#pragma once

#include "clap/events.h"

#include <vector>

struct NoteManager {
	enum Event{eventDown, eventLegato, eventContinue, eventUp, eventChoke, eventRelease};

	struct Note {
		size_t index;
		uint32_t noteId;
		float key = 60;

		uint32_t processFrom = 0, processTo = 0;

		Event event = eventDown;
	};
	
 	NoteManager(size_t polyphony=16) {
		notes.reserve(polyphony);
		tasks.reserve(polyphony);
		for (size_t i = 0; i < polyphony; ++i) {
			indexQueue.push_back(i);
		}
	}
	
	void reset() {
		notes.resize(0);
		tasks.resize(0);
	}
	
	void startBlock() {
		tasks.resize(0);
		for (auto &n : notes) n.processFrom = n.processTo = 0;
	}
	void processTo(uint32_t frames) {
		tasks.resize(0);
		for (auto &n : notes) {
			if (n.processFrom < frames) {
				n.processTo = frames;
				tasks.push_back(n);
				n.processFrom = frames;
				if (n.event == eventDown || n.event == eventLegato) {
					n.event = eventContinue;
				} else {
					n.event = eventRelease;
				}
			}
		}
	}
	
	bool processEvent(const clap_event_header *event) {
		tasks.resize(0);
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return false;
		if (event->type == CLAP_EVENT_NOTE_ON) {
			auto &noteEvent = *(clap_event_note *)event;
			for (auto &n : notes) {
				if (n.noteId == noteEvent.note_id) return false;
			}
			if (notes.size() < notes.capacity()) {
				Note newNote{
					.index=indexQueue.back(),
					.noteId=noteEvent.note_id,
					.key=noteEvent.key,
					.processFrom=event->time,
					.processTo=event->time,
					.down=true
				};
				notes.push_back(newNote);
				newNote.event = eventDown;
				indexQueue.pop_back();
			}
		}
		return false;
	}
		
	std::vector<Note> tasks;
private:
	std::vector<Note> notes;
	std::vector<size_t> indexQueue;
};
