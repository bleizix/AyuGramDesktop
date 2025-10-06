// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu_state.h"

namespace AyuState {

std::unordered_map<PeerId, std::unordered_set<MsgId>> hiddenMessages;

// used for fake media load (loads deleted instead)
// key - fake documentId, value - path to deleted media
std::unordered_map<uint64, QString> documentIds;

void hide(PeerId peerId, MsgId messageId) {
	hiddenMessages[peerId].insert(messageId);
}

void hide(not_null<HistoryItem*> item) {
	hide(item->history()->peer->id, item->id);
}

bool isHidden(PeerId peerId, MsgId messageId) {
	const auto it = hiddenMessages.find(peerId);
	if (it != hiddenMessages.end()) {
		return it->second.contains(messageId);
	}
	return false;
}

bool isHidden(not_null<HistoryItem*> item) {
	return isHidden(item->history()->peer->id, item->id);
}

void setFakeDocument(uint64 id, QString path) {
	documentIds[id] = path;
}
QString fakeDocument(uint64 id) {
	if (auto path = documentIds.find(id); path != documentIds.end()) {
		return path->second;
	}
	return QString();
}

}
