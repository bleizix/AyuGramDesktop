// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/data/messages_storage.h"

#include "ayu/data/ayu_database.h"
#include "ayu/features/forward/ayu_sync.h"
#include "ayu/utils/ayu_mapper.h"
#include "ayu/utils/telegram_helpers.h"

#include "base/unixtime.h"
#include "crl/crl_async.h"

#include "data/data_forum_topic.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers.h"

#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"

#include "main/main_session.h"



namespace AyuMessages {


template <typename MessageType, typename AddFunc>
void processMediaAndAddToDb(
	not_null<HistoryItem*> item,
	MessageType message,
	AddFunc addFunction) {


	// from
	// void DocumentData::setattributes(const QVector<MTPDocumentAttribute> &attributes)
	// in data_document.cp
	const auto mapAttributes = [=]
	{
		MTPVector<MTPDocumentAttribute> att;
		if (const auto doc = item->media()->document()) {
		    if (const auto sizes = doc->dimensions; sizes.width() > 0 && sizes.height() > 0) {
		        att.v.emplace_back(MTP_documentAttributeImageSize(MTP_int(sizes.width()), MTP_int(sizes.height())));
		    }
			if (doc->isAnimation()) {
		        att.v.emplace_back(MTP_documentAttributeAnimated());
		    }
			if (doc->sticker()) {
		        if (const auto sticker = doc->sticker()) {
		            if (sticker->isLottie()) {
		                 att.v.emplace_back(MTP_documentAttributeSticker(
		                    MTP_flags(MTPDdocumentAttributeSticker::Flag::f_mask_coords),
		                    MTP_string(sticker->alt),
		                    MTP_inputStickerSetID(MTP_long(sticker->set.id), MTP_long(sticker->set.accessHash)),
		                    MTPMaskCoords()));
		            } else if (sticker->setType == Data::StickersType::Emoji) {
		                auto flags = doc->isPremiumSticker()
            				? MTPDdocumentAttributeCustomEmoji::Flag::f_free
            				: MTPDdocumentAttributeCustomEmoji::Flag(0);

		                // if (doc->useTextColor()) {
		                //     flags |= MTPDdocumentAttributeCustomEmoji::Flag::f_text_color;
		                // }
		                att.v.emplace_back(MTP_documentAttributeCustomEmoji(
		                    MTP_flags(flags),
		                    MTP_string(sticker->alt),
		                    MTP_inputStickerSetID(
		                        MTP_long(sticker->set.id),
		                        MTP_long(sticker->set.accessHash))));
		            } else {
		                 att.v.emplace_back(MTP_documentAttributeSticker(
		                    MTP_flags(MTPDdocumentAttributeSticker::Flag::f_mask_coords),
		                    MTP_string(sticker->alt),
		                    MTP_inputStickerSetID(MTP_long(sticker->set.id), MTP_long(sticker->set.accessHash)),
		                    MTPMaskCoords()));
		            }
		        }
		    }
			if (doc->isVideoFile() || doc->round()) {
		        auto flags = MTPDdocumentAttributeVideo::Flags();
		        if (doc->round()) {
		            flags |= MTPDdocumentAttributeVideo::Flag::f_round_message;
		        }
		        if (doc->isSilentVideo()) {
		            flags |= MTPDdocumentAttributeVideo::Flag::f_nosound;
		        }
		        const auto video = doc->video();
		        att.v.emplace_back(MTP_documentAttributeVideo(
		            MTP_flags(flags),
		            MTP_double(doc->duration() / 1000.),
		            MTP_int(doc->dimensions.width()),
		            MTP_int(doc->dimensions.height()),
		            MTP_int(doc->videoPreloadPrefix()),
		            MTP_double(0),
		            MTP_string(video ? video->codec : QString())));
		    }
			if (doc->isSong()) {
		        const auto song = doc->song();
		        auto flags = MTPDdocumentAttributeAudio::Flags(0);
		        if (!song->title.isEmpty()) {
		            flags |= MTPDdocumentAttributeAudio::Flag::f_title;
		        }
		        if (!song->performer.isEmpty()) {
		            flags |= MTPDdocumentAttributeAudio::Flag::f_performer;
		        }
		        att.v.emplace_back(MTP_documentAttributeAudio(
		            MTP_flags(flags),
		            MTP_int(doc->duration()),
		            MTP_string(song->title),
		            MTP_string(song->performer),
		            MTP_bytes()));
		    }
			if (doc->isVoiceMessage()) {
		        const auto voice = doc->voice();
    			auto flags = MTPDdocumentAttributeAudio::Flags(0);
		        flags = MTPDdocumentAttributeAudio::Flag::f_voice;
		        if (!voice->waveform.isEmpty()) {
		            flags |= MTPDdocumentAttributeAudio::Flag::f_waveform;
		        }
		        att.v.emplace_back(MTP_documentAttributeAudio(
		            MTP_flags(flags),
		            MTP_int(doc->duration()),
		            MTP_string(QString()),
		            MTP_string(QString()),
		            MTP_bytes(QByteArray())));
		    }
			if (!doc->filename().isEmpty()) {
		        att.v.emplace_back(MTP_documentAttributeFilename(MTP_string(doc->filename())));
		    }
		}
		return att;
	};

	if (const auto media = item->media(); media && mediaDownloadable(media)) {
		Main::Session* session = &item->history()->session();

		if (!media->photo()) {
			crl::async([=, message = std::move(message)]() mutable
			{
				AyuSync::loadDocuments(session, {item});

				QString mediaPathStr = AyuSync::filePath(session, media);
				std::string mediaPath = mediaPathStr.toStdString();

				message.mediaPath = mediaPath;
				message.documentType = DOCUMENT_TYPE_PHOTO;
				addFunction(message);
			});
			return;

		}

		int documentType = 0;
		std::string mimeType;

		if (const auto document = media->document()) {
			documentType = DOCUMENT_TYPE_FILE;
			mimeType = document->mimeString().toStdString();
		} else
		if (media->photo()) {
			documentType = DOCUMENT_TYPE_PHOTO;
		}
		MTPVector<MTPDocumentAttribute> attributes = mapAttributes();
		if (!attributes.v.empty()) {
			std::vector<char> serialized = AyuMapper::serializeAttribute(attributes);
			message.documentAttributesSerialized = serialized;
		}



		crl::async([=, message = std::move(message)]() mutable {

			AyuSync::loadDocuments(session, {item});

			QString mediaPathStr = AyuSync::filePath(session, media);
			std::string mediaPath = mediaPathStr.toStdString();

			message.mediaPath = mediaPath;
			message.documentType = documentType;
			if (documentType == DOCUMENT_TYPE_FILE) {
				message.mimeType = mimeType;
			}

			addFunction(message);
		});
	} else {

		crl::async([addFunction = std::move(addFunction), message = std::move(message)]() mutable {
			addFunction(message);
		});
	}
}

template<typename DerivedMessage>
std::vector<AyuMessageBase> convertToBase(const std::vector<DerivedMessage> &messages) {
	std::vector<AyuMessageBase> based;
	based.reserve(messages.size());
	for (const auto &msg : messages) {
		based.push_back(static_cast<AyuMessageBase>(msg));
	}
	return based;
}

void map(not_null<HistoryItem*> item, AyuMessageBase &message) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;

	message.userId = userId;
	message.dialogId = getDialogIdFromPeer(item->history()->peer);
	message.groupedId = item->groupId().raw();
	message.peerId = item->history()->peer->id.value & PeerId::kChatTypeMask;
	message.fromId = item->from()->id.value & PeerId::kChatTypeMask;
	if (item->topic()) {
		message.topicId = item->topicRootId().bare;
	} else {
		message.topicId = 0;
	}
	message.messageId = item->id.bare;
	message.date = item->date();
	message.flags = AyuMapper::mapItemFlagsToMTPFlags(item);

	if (const auto edited = item->Get<HistoryMessageEdited>()) {
		message.editDate = edited->date;
	} else {
		message.editDate = base::unixtime::now();
	}

	message.views = item->viewsCount();
	message.fwdFlags = 0;
	message.fwdFromId = 0;
	// message.fwdName
	message.fwdDate = 0;
	// message.fwdPostAuthor
	if (const auto msgsigned = item->Get<HistoryMessageSigned>()) {
		message.postAuthor = msgsigned->author.toStdString();
	}
	message.replyFlags = 0;
	message.replyMessageId = 0;
	message.replyPeerId = 0;
	message.replyTopId = 0;
	message.replyForumTopic = false;
	// message.replySerialized
	// message.replyMarkupSerialized
	message.entityCreateDate = base::unixtime::now();

	auto serializedText = AyuMapper::serializeTextWithEntities(item);
	message.text = serializedText.first;
	message.textEntities = serializedText.second;
}

void addEditedMessage(not_null<HistoryItem *> item) {
	EditedMessage message;
	map(item, message);

	if (message.text.empty() && !item->media()) {
		return;
	}


    // This call now works correctly with the improved helper function.
	processMediaAndAddToDb(item, std::move(message), &AyuDatabase::addEditedMessage);
}

std::vector<AyuMessageBase> getEditedMessages(not_null<HistoryItem*> item, ID minId, ID maxId, int totalLimit) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto msgId = item->id.bare;

	return convertToBase(AyuDatabase::getEditedMessages(userId, dialogId, msgId, minId, maxId, totalLimit));
}

bool hasRevisions(not_null<HistoryItem*> item) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto msgId = item->id.bare;

	return AyuDatabase::hasRevisions(userId, dialogId, msgId);
}

void addDeletedMessage(not_null<HistoryItem*> item) {
	DeletedMessage message;
	map(item, message);

	if (message.text.empty() && !item->media()) {
		return;
	}

    processMediaAndAddToDb(item, std::move(message), &AyuDatabase::addDeletedMessage);
}

std::vector<AyuMessageBase>
getDeletedMessages(not_null<PeerData*> peer, ID topicId, ID minId, ID maxId, int totalLimit) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	return convertToBase(
		AyuDatabase::getDeletedMessages(userId, getDialogIdFromPeer(peer), topicId, minId, maxId, totalLimit));
}

bool hasDeletedMessages(not_null<PeerData*> peer, ID topicId) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	return AyuDatabase::hasDeletedMessages(userId, getDialogIdFromPeer(peer), topicId);
}

}
