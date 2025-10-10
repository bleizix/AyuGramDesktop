// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu_mapper.h"

#include "apiwrap.h"
#include "telegram_helpers.h"
#include "api/api_text_entities.h"
#include "ayu/data/ayu_database.h"
#include "ayu/data/entities.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "mtproto/connection_abstract.h"
#include "mtproto/details/mtproto_dump_to_text.h"

namespace AyuMapper {

constexpr auto kMessageFlagUnread = 0x00000001;
constexpr auto kMessageFlagOut = 0x00000002;
constexpr auto kMessageFlagForwarded = 0x00000004;
constexpr auto kMessageFlagReply = 0x00000008;
constexpr auto kMessageFlagMention = 0x00000010;
constexpr auto kMessageFlagContentUnread = 0x00000020;
constexpr auto kMessageFlagHasMarkup = 0x00000040;
constexpr auto kMessageFlagHasEntities = 0x00000080;
constexpr auto kMessageFlagHasFromId = 0x00000100;
constexpr auto kMessageFlagHasMedia = 0x00000200;
constexpr auto kMessageFlagHasViews = 0x00000400;
constexpr auto kMessageFlagHasBotId = 0x00000800;
constexpr auto kMessageFlagIsSilent = 0x00001000;
constexpr auto kMessageFlagIsPost = 0x00004000;
constexpr auto kMessageFlagEdited = 0x00008000;
constexpr auto kMessageFlagHasPostAuthor = 0x00010000;
constexpr auto kMessageFlagIsGrouped = 0x00020000;
constexpr auto kMessageFlagFromScheduled = 0x00040000;
constexpr auto kMessageFlagHasReactions = 0x00100000;
constexpr auto kMessageFlagHideEdit = 0x00200000;
constexpr auto kMessageFlagRestricted = 0x00400000;
constexpr auto kMessageFlagHasReplies = 0x00800000;
constexpr auto kMessageFlagIsPinned = 0x01000000;
constexpr auto kMessageFlagHasTTL = 0x02000000;
constexpr auto kMessageFlagInvertMedia = 0x08000000;
constexpr auto kMessageFlagHasSavedPeer = 0x10000000;

template<typename MTPObject>
std::vector<char> serializeObject(MTPObject object) {
	mtpBuffer buffer;
	object.write(buffer);

	const auto from = reinterpret_cast<char*>(buffer.data());
	const auto end = from + buffer.size() * sizeof(mtpPrime);

	std::vector<char> entities(from, end);
	return entities;
}

template<typename MTPObject>
MTPObject deserializeObject(std::vector<char> serialized) {
	gsl::span<char> span(serialized.data(), serialized.size());

	auto from = reinterpret_cast<const mtpPrime*>(span.data());
	const auto end = from + span.size() / sizeof(mtpPrime);

	MTPObject data;
	if (!data.read(from, end)) {
		LOG(("AyuMapper: Failed to deserialize object"));
	}
	return data;
}

std::pair<std::string, std::vector<char>> serializeTextWithEntities(not_null<HistoryItem*> item) {
	if (item->emptyText()) {
		return std::make_pair("", std::vector<char>());
	}
	auto textWithEntities = item->originalText();


	std::vector<char> entities;
	if (!textWithEntities.entities.empty()) {
		const auto mtpEntities = Api::EntitiesToMTP(
			&item->history()->session(),
			textWithEntities.entities,
			Api::ConvertOption::WithLocal);

		entities = serializeObject(mtpEntities);
	}

	return std::make_pair(textWithEntities.text.toStdString(), entities);
}

MTPVector<MTPMessageEntity> deserializeTextWithEntities(std::vector<char> serialized) {
	return deserializeObject<MTPVector<MTPMessageEntity>>(serialized);
}

std::vector<char> serializeAttribute(MTPVector<MTPDocumentAttribute> attribute) {
	return serializeObject(attribute);
}

MTPVector<MTPDocumentAttribute> deserializeAttribute (std::vector<char> serialized) {
	return deserializeObject<MTPVector<MTPDocumentAttribute>>(serialized);
}


int mapItemFlagsToMTPFlags(not_null<HistoryItem*> item) {
	int flags = 0;

	const auto thread = item->topic()
							? reinterpret_cast<Data::Thread*>(item->topic())
							: item->history();
	if (item->unread(thread)) {
		flags |= kMessageFlagUnread;
	}

	if (item->out()) {
		flags |= kMessageFlagOut;
	}

	if (item->Get<HistoryMessageForwarded>()) {
		flags |= kMessageFlagForwarded;
	}

	if (item->Get<HistoryMessageReply>()) {
		flags |= kMessageFlagReply;
	}

	if (item->mentionsMe()) {
		flags |= kMessageFlagMention;
	}

	if (item->isUnreadMedia()) {
		flags |= kMessageFlagContentUnread;
	}

	if (item->definesReplyKeyboard()) {
		flags |= kMessageFlagHasMarkup;
	}

	if (!item->originalText().entities.empty()) {
		flags |= kMessageFlagHasEntities;
	}

	if (item->displayFrom()) {
		// todo: maybe wrong
		flags |= kMessageFlagHasFromId;
	}

	if (item->media()) {
		flags |= kMessageFlagHasMedia;
	}

	if (item->hasViews()) {
		flags |= kMessageFlagHasViews;
	}

	if (item->viaBot()) {
		flags |= kMessageFlagHasBotId;
	}

	if (item->isSilent()) {
		flags |= kMessageFlagIsSilent;
	}

	if (item->isPost()) {
		flags |= kMessageFlagIsPost;
	}

	if (item->Get<HistoryMessageEdited>()) {
		flags |= kMessageFlagEdited;
	}

	if (item->Get<HistoryMessageSigned>()) {
		flags |= kMessageFlagHasPostAuthor;
	}

	if (item->groupId()) {
		flags |= kMessageFlagIsGrouped;
	}

	if (item->isScheduled()) {
		flags |= kMessageFlagFromScheduled;
	}

	if (!item->reactions().empty()) {
		flags |= kMessageFlagHasReactions;
	}

	if (item->hideEditedBadge()) {
		flags |= kMessageFlagHideEdit;
	}

	if (item->hasPossibleRestrictions()) {
		flags |= kMessageFlagRestricted;
	}

	if (item->repliesCount() > 0) {
		flags |= kMessageFlagHasReplies;
	}

	if (item->isPinned()) {
		flags |= kMessageFlagIsPinned;
	}

	if (item->ttlDestroyAt() > 0) {
		flags |= kMessageFlagHasTTL;
	}

	if (item->invertMedia()) {
		flags |= kMessageFlagInvertMedia;
	}

	if (item->savedFromSender()) {
		// todo: maybe wrong
		flags |= kMessageFlagHasSavedPeer;
	}

	return flags;
}

MTPDmessage::Flags mapToMTPFlags(int ayuFlags) {
	MTPDmessage::Flags mtpFlags;

	if (ayuFlags & kMessageFlagOut) mtpFlags |= MTPDmessage::Flag::f_out;
	if (ayuFlags & kMessageFlagMention) mtpFlags |= MTPDmessage::Flag::f_mentioned;
	if (ayuFlags & kMessageFlagContentUnread) mtpFlags |= MTPDmessage::Flag::f_media_unread;
	if (ayuFlags & kMessageFlagIsSilent) mtpFlags |= MTPDmessage::Flag::f_silent;
	if (ayuFlags & kMessageFlagIsPost) mtpFlags |= MTPDmessage::Flag::f_post;
	if (ayuFlags & kMessageFlagFromScheduled) mtpFlags |= MTPDmessage::Flag::f_from_scheduled;
	if (ayuFlags & kMessageFlagHideEdit) mtpFlags |= MTPDmessage::Flag::f_edit_hide;
	if (ayuFlags & kMessageFlagIsPinned) mtpFlags |= MTPDmessage::Flag::f_pinned;
	if (ayuFlags & kMessageFlagInvertMedia) mtpFlags |= MTPDmessage::Flag::f_invert_media;

	if (ayuFlags & kMessageFlagHasFromId) mtpFlags |= MTPDmessage::Flag::f_from_id;
	if (ayuFlags & kMessageFlagForwarded) mtpFlags |= MTPDmessage::Flag::f_fwd_from;
	if (ayuFlags & kMessageFlagHasBotId) mtpFlags |= MTPDmessage::Flag::f_via_bot_id;
	if (ayuFlags & kMessageFlagReply) mtpFlags |= MTPDmessage::Flag::f_reply_to;
	if (ayuFlags & kMessageFlagHasMedia) mtpFlags |= MTPDmessage::Flag::f_media;
	if (ayuFlags & kMessageFlagHasMarkup) mtpFlags |= MTPDmessage::Flag::f_reply_markup;
	if (ayuFlags & kMessageFlagHasEntities) mtpFlags |= MTPDmessage::Flag::f_entities;
	if (ayuFlags & kMessageFlagHasViews) mtpFlags |= MTPDmessage::Flag::f_views;
	if (ayuFlags & kMessageFlagHasReplies) mtpFlags |= MTPDmessage::Flag::f_replies;
	if (ayuFlags & kMessageFlagEdited) mtpFlags |= MTPDmessage::Flag::f_edit_date;
	if (ayuFlags & kMessageFlagHasPostAuthor) mtpFlags |= MTPDmessage::Flag::f_post_author;
	if (ayuFlags & kMessageFlagIsGrouped) mtpFlags |= MTPDmessage::Flag::f_grouped_id;
	if (ayuFlags & kMessageFlagHasReactions) mtpFlags |= MTPDmessage::Flag::f_reactions;
	if (ayuFlags & kMessageFlagRestricted) mtpFlags |= MTPDmessage::Flag::f_restriction_reason;
	if (ayuFlags & kMessageFlagHasTTL) mtpFlags |= MTPDmessage::Flag::f_ttl_period;
	if (ayuFlags & kMessageFlagHasSavedPeer) mtpFlags |= MTPDmessage::Flag::f_saved_peer_id;

	mtpFlags |= MTPDmessage::Flag::f_legacy;

	return mtpFlags;
}

MTPMessage mapToMTP(const AyuMessageBase &message, not_null<PeerData*> peer) {
	// no media at all, only text

	const auto flags = mapToMTPFlags(message.flags);

	auto dialogId = MTP_long(getDialogIdFromPeer(peer));
	auto peerId = peer->asChat()
					  ? MTP_peerChat(dialogId)
					  : peer->asUser()
							? MTP_peerUser(dialogId)
							: peer->asChannel()
								  ? MTP_peerChannel(dialogId)
								  : MTPpeer();

	auto fromId = MTP_long(message.fromId);
	auto fromIdPeer = message.fromId < 0 ? MTP_peerChannel(fromId) : MTP_peerUser(fromId);

	return MTP_message(
		MTP_flags(flags),
		MTP_int(message.messageId),
		(flags & MTPDmessage::Flag::f_from_id) ? fromIdPeer : MTPPeer(),
		MTP_int(0), // from_boosts_applied
		peerId,
		(flags & MTPDmessage::Flag::f_saved_peer_id) ? MTP_peerUser(MTP_long(message.userId)) : MTPPeer(),
		MTPMessageFwdHeader(),
		(flags & MTPDmessage::Flag::f_via_bot_id) ? MTP_long(0) : MTP_long(0),
		MTP_long(0),// via_business_bot_id
		/*(flags & MTPDmessage::Flag::f_reply_to) ? MTP_messageReplyHeader(MTP_flags(MTPDmessageReplyHeader::Flag::f_reply_to_peer_id), MTP_int(message.replyMessageId), MTPPeer(), MTP_int(0)) : */
		MTPMessageReplyHeader(),
		MTP_int(message.date),
		MTP_string(message.text),
		(flags & MTPDmessage::Flag::f_media) ? MTP_messageMediaEmpty() : MTPMessageMedia(),
		(flags & MTPDmessage::Flag::f_reply_markup) ? MTP_replyKeyboardHide(MTP_flags(0)) : MTPReplyMarkup(),
		(flags & MTPDmessage::Flag::f_entities) ? MTP_vector<MTPMessageEntity>() : MTP_vector<MTPMessageEntity>(),
		(flags & MTPDmessage::Flag::f_views) ? MTP_int(message.views) : MTP_int(0),
		MTP_int(0),// forwards
		MTPMessageReplies(),
		(flags & MTPDmessage::Flag::f_edit_date) ? MTP_int(message.editDate) : MTP_int(0),
		(flags & MTPDmessage::Flag::f_post_author) ? MTP_string(message.postAuthor) : MTPstring(),
		(flags & MTPDmessage::Flag::f_grouped_id) ? MTP_long(message.groupedId) : MTP_long(0),
		(flags & MTPDmessage::Flag::f_reactions)
			? MTP_messageReactions(MTP_flags(0),
								   MTP_vector<MTPReactionCount>(),
								   MTP_vector<MTPMessagePeerReaction>(),
								   MTP_vector<MTPMessageReactor>())
			: MTPMessageReactions(),
		(flags & MTPDmessage::Flag::f_restriction_reason)
			? MTP_vector<MTPRestrictionReason>()
			: MTP_vector<MTPRestrictionReason>(),
		(flags & MTPDmessage::Flag::f_ttl_period) ? MTP_int(0) : MTP_int(0),
		MTP_int(0),// quick_reply_shortcut_id
		MTP_long(0),// effect
		MTPFactCheck(),
		MTP_int(0),// report_delivery_until_date
		MTP_long(0),// paid_message_stars
		MTPSuggestedPost()
	);
}

MessageFlags mapToItemFlags(const AyuMessageBase &message) {
	MessageFlags result;
	const int ayuFlags = message.flags;

	result |= MessageFlag::HistoryEntry;

	if (ayuFlags & kMessageFlagOut) result |= MessageFlag::Outgoing;
	if (ayuFlags & kMessageFlagMention) result |= MessageFlag::MentionsMe;
	if (ayuFlags & kMessageFlagContentUnread) result |= MessageFlag::MediaIsUnread;
	if (ayuFlags & kMessageFlagIsSilent) result |= MessageFlag::Silent;
	if (ayuFlags & kMessageFlagIsPost) result |= MessageFlag::Post;
	if (ayuFlags & kMessageFlagFromScheduled) result |= MessageFlag::IsOrWasScheduled;
	if (ayuFlags & kMessageFlagHideEdit) result |= MessageFlag::HideEdited;
	if (ayuFlags & kMessageFlagIsPinned) result |= MessageFlag::Pinned;
	if (ayuFlags & kMessageFlagInvertMedia) result |= MessageFlag::InvertMedia;
	if (ayuFlags & kMessageFlagRestricted) result |= MessageFlag::HasRestrictions;
	if (ayuFlags & kMessageFlagHasFromId) result |= MessageFlag::HasFromId;
	if (ayuFlags & kMessageFlagHasPostAuthor) result |= MessageFlag::HasPostAuthor;
	if (ayuFlags & kMessageFlagHasViews) result |= MessageFlag::HasViews;
	if (ayuFlags & kMessageFlagHasMarkup) result |= MessageFlag::HasReplyMarkup;
	if (ayuFlags & kMessageFlagReply || ayuFlags & kMessageFlagHasReplies) result |= MessageFlag::HasReplyInfo;


	return result;
}

std::vector<not_null<HistoryItem*>> hookDeletedItems(
	not_null<History*> history,
	std::vector<not_null<HistoryItem*>> originalItems) {
	if (originalItems.empty()) {
		return originalItems;
	}

	std::sort(originalItems.begin(),
			  originalItems.end(),
			  [](not_null<HistoryItem*> a, not_null<HistoryItem*> b)
			  {
				  return a->id < b->id;
			  });
	ID minId = originalItems.front()->id.bare;
	ID maxId = originalItems.back()->id.bare;

	const auto peer = history->peer.get();
	const ID userId = history->session().userId().bare;
	const ID dialogId = getDialogIdFromPeer(peer);
	auto deletedFromDb = AyuDatabase::getDeletedMessages(userId, dialogId, 0, minId, maxId, 200);

	if (deletedFromDb.empty()) {
		return originalItems;
	}

	originalItems.reserve(originalItems.size() + deletedFromDb.size());
	for (const auto &deletedMsg : deletedFromDb) {
		const auto id = FullMsgId(peer->id, deletedMsg.messageId);

		if (history->owner().message(id)) {
			continue;
		}

		auto item = history->createItem(
			id.msg.bare,
			mapToMTP(deletedMsg, history->peer),
			mapToItemFlags(deletedMsg),
			true,
			false
		);
		item->setDeleted();
		originalItems.emplace_back(item);
	}

	std::sort(originalItems.begin(),
			  originalItems.end(),
			  [](not_null<HistoryItem*> a, not_null<HistoryItem*> b)
			  {
				  return a->id < b->id;
			  });

	return originalItems;
}
}
