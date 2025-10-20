#pragma once

#include "clap/events.h"

#include <vector>

struct NoteManager {
	struct Note {
		uint32_t noteId;
		size_t index;

		bool down = true;
		float note;
	private:
		friend class NoteManager;
		// What needs processing on it
		uint32_t processedTo = 0;
	};
	struct Task {
		Note note;
		uint32_t from, to;
	};
	
 	NoteManager(size_t polyphony=16) {
		notes.reserve(polyphony);
		tasks.reserve(polyphony);
	}
	
	void startBlock() {
		tasks.resize(0);
		for (auto &n : notes) n.processedTo = 0;
	}
	void processTo(uint32_t frames) {
		tasks.resize(0);
		for (auto &n : notes) {
			if (n.processedTo < frames) {
				tasks.push_back({n, n.processedTo, frames});
				n.processedTo = frames;
			}
		}
	}
	
	bool processEvent(const clap_event_header *event) {
		tasks.resize(0);
		return false;
	}
	
	
	
	std::vector<Note> tasks;
private:
	std::vector<Note> notes;
};
