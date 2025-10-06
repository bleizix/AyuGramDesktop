// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "entities.h"

#include "history/history_item_edition.h"

constexpr int DOCUMENT_TYPE_NONE = 0;
constexpr int DOCUMENT_TYPE_PHOTO = 1;
constexpr int DOCUMENT_TYPE_STICKER = 2;
constexpr int DOCUMENT_TYPE_FILE = 3;
namespace AyuMessages {

void addEditedMessage(not_null<HistoryItem *> item);
std::vector<AyuMessageBase> getEditedMessages(not_null<HistoryItem*> item, ID minId, ID maxId, int totalLimit);
bool hasRevisions(not_null<HistoryItem*> item);

void addDeletedMessage(not_null<HistoryItem*> item);
std::vector<AyuMessageBase> getDeletedMessages(not_null<PeerData*> peer, ID topicId, ID minId, ID maxId, int totalLimit);
bool hasDeletedMessages(not_null<PeerData*> peer, ID topicId);


template <typename MessageType, typename AddFunc>
void processMediaAndAddToDb(
	not_null<HistoryItem*> item,
	MessageType message,
	AddFunc addFunction);
}
