// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/ui/message_history/history_item.h"
#include "ayu/data/entities.h"

#include "api/api_chat_participants.h"
#include "api/api_text_entities.h"
#include "ayu/utils/ayu_mapper.h"
#include "ayu/ui/message_history/history_inner.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "data/data_channel.h"
#include "data/data_file_origin.h"
#include "data/data_forum_topic.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"

#include "data/data_photo.h"
#include "data/data_photo_media.h"

#include "ayu/utils/ayu_mapper.h"
#include "history/history_item_helpers.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/basic_click_handlers.h"
#include "ui/text/text_utilities.h"
#include "scheme.h"
#include "ayu/ayu_state.h"
#include "ayu/data/messages_storage.h"
#include "base/random.h"

namespace MessageHistory {

OwnedItem::OwnedItem(std::nullptr_t) {
}

OwnedItem::OwnedItem(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<HistoryItem*> data)
	: _data(data), _view(_data->createView(delegate)) {
}

OwnedItem::OwnedItem(OwnedItem &&other)
	: _data(base::take(other._data)), _view(base::take(other._view)) {
}

OwnedItem &OwnedItem::operator=(OwnedItem &&other) {
	_data = base::take(other._data);
	_view = base::take(other._view);
	return *this;
}

OwnedItem::~OwnedItem() {
	clearView();
	if (_data) {
		_data->destroy();
	}
}

void OwnedItem::refreshView(
	not_null<HistoryView::ElementDelegate*> delegate) {
	_view = _data->createView(delegate);
}

void OwnedItem::clearView() {
	_view = nullptr;
}

void GenerateItems(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<History*> history,
	AyuMessageBase message,
	Fn<void(OwnedItem item, TimeId sentDate, MsgId)> callback) {
	PeerData *from = history->owner().userLoaded(message.fromId);
	if (!from) {
		from = history->owner().channelLoaded(message.fromId);
	}
	if (!from) {
		from = reinterpret_cast<PeerData*>(history->owner().chatLoaded(message.fromId));
	}
	const auto date = message.entityCreateDate;
	const auto addPart = [&](
		not_null<HistoryItem*> item,
		TimeId sentDate = 0,
		MsgId realId = MsgId())
	{
		return callback(OwnedItem(delegate, item), sentDate, realId);
	};

	const auto makeSimpleTextMessage = [&](TextWithEntities &&text)
	{
		base::flags<MessageFlag> flags = MessageFlag::AdminLogEntry;
		if (from) {
			flags |= MessageFlag::HasFromId;
		} else {
			flags |= MessageFlag::HasPostAuthor;
		}
		if (!message.postAuthor.empty()) {
			flags |= MessageFlag::HasPostAuthor;
		}

		const auto path = QString::fromStdString(message.mediaPath);
		MTPMessageMedia media = MTP_messageMediaEmpty();
		if (const auto file = QFile(path); !path.isEmpty() && file.exists()) {

			const auto randomId = base::RandomValue<uint64>();
			AyuState::setFakeDocument(randomId, path);

			if (message.documentType == DOCUMENT_TYPE_PHOTO) {

				// we'll hook into downloading process with
				// fake document id and return file as ready
				// with our deleted media's path
				const auto reader = QImageReader(path);
				const auto size = reader.size();


				auto sizes = QVector<MTPPhotoSize>();
				sizes.emplace_back(MTP_photoSize(MTP_string("y"), MTP_int(size.width()), MTP_int(size.height()), MTP_int(file.size())));
				auto videoSizes = QVector<MTPVideoSize>();
				videoSizes.emplace_back(MTP_videoSize(MTP_flags(0), MTP_string("y"), MTP_int(1080), MTP_int(1080), MTP_int(1080), MTP_double(.0)));


				media = MTP_messageMediaPhoto(MTP_flags(MTPDmessageMediaPhoto::Flag::f_photo),
					MTP_photo(MTP_flags(0),
						MTP_long(randomId),
						MTP_long(randomId),
						MTP_bytes(QByteArray()),
						MTP_int(message.date),
						MTP_vector<MTPPhotoSize>(sizes),
						MTP_vector<MTPVideoSize>(videoSizes),
						MTP_int(1337)
						),
				MTPint()
				);
			} else if (message.documentType == DOCUMENT_TYPE_FILE) {

				auto sizes = QVector<MTPPhotoSize>();
				sizes.emplace_back(MTP_photoSize(MTP_string("y"), MTP_int(1080), MTP_int(1080), MTP_int(file.size())));
				auto videoSizes = QVector<MTPVideoSize>();
				videoSizes.emplace_back(MTP_videoSize(MTP_flags(0), MTP_string("v"), MTP_int(1080), MTP_int(1080), MTP_int(file.size()), MTP_double(.0)));

				QVector<MTPDocumentAttribute> attributes;

				if (message.documentAttributesSerialized.size()) {
					auto attr = AyuMapper::deserializeAttribute(message.documentAttributesSerialized);
					attributes = attr.v;
				}



				media = MTP_messageMediaDocument(
				MTP_flags(MTPDmessageMediaDocument::Flag::f_document),
				MTP_document(
					MTP_flags(0),
					MTP_long(randomId),
					MTP_long(randomId),
					MTP_bytes(QByteArray()),
					MTP_int(message.date),
					MTP_string(message.mimeType),
					MTP_long(file.size()),
					MTP_vector<MTPPhotoSize>(sizes),
					MTP_vector<MTPVideoSize>(videoSizes),
					MTP_int(1337),
					MTP_vector(attributes)
					),
				MTPVector<MTPDocument>(), // alt_documents
				MTPPhoto(),
				MTPint(), // video_timestamp
				MTPint());
			}


		}

		return history->makeMessage({
										.id = history->nextNonHistoryEntryId(),
										.flags = flags,
										.from = from ? from->id : 0,
										.date = date,
										.postAuthor = !message.postAuthor.empty()
														  ? QString::fromStdString(message.postAuthor)
														  : from
																? QString()
																: QString("unknown user: %1").arg(message.fromId),
									},
									std::move(text),
									media
									);
	};

	const auto addSimpleTextMessage = [&](TextWithEntities &&text)
	{
		addPart(makeSimpleTextMessage(std::move(text)));
	};

	const auto text = QString::fromStdString(message.text);
	auto textAndEntities = Ui::Text::WithEntities(text);
	const auto entities = AyuMapper::deserializeTextWithEntities(message.textEntities);
	textAndEntities.entities = Api::EntitiesFromMTP(&history->session(), entities.v);
	addSimpleTextMessage(std::move(textAndEntities));
}

} // namespace MessageHistory
