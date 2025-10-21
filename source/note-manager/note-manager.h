#pragma once

#include "clap/events.h"

#include <vector>

struct NoteManager {
	enum Event{eventDown, eventLegato, eventContinue, eventUp, eventRelease};

	struct Note {
		size_t polyIndex;
		float key, velocity;
		int32_t noteId;
		int16_t baseKey, port;

		uint32_t processFrom = 0, processTo = 0;

		Event event = eventDown;
		
		bool released() const {
			return event == eventUp || event == eventRelease;
		}
		
		bool match(const clap_event_note &clapEvent) const {
			if (clapEvent.note_id >= 0) return noteId == clapEvent.note_id;
			return baseKey == clapEvent.key;
		}
		bool match(const Note &other) const {
			if (noteId >= 0 || other.noteId >= 0) return noteId == other.noteId;
			return baseKey == other.baseKey;
		}
	};
	
	NoteManager(size_t polyphony=16) {
		notes.reserve(polyphony);
		tasks.reserve(polyphony);
		for (size_t i = 0; i < polyphony; ++i) {
			polyIndexQueue.push_back(polyphony - 1 - i);
		}
	}
	
	size_t polyphony() const {
		return notes.capacity();
	}
	
	void reset() {
		notes.resize(0);
		tasks.resize(0);
		auto polyphony = notes.capacity();
		polyIndexQueue.resize(0);
		for (size_t i = 0; i < polyphony; ++i) {
			polyIndexQueue.push_back(polyphony - 1 - i);
		}
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
				} else if (n.event == eventUp) {
					n.event = eventRelease;
				}
			}
		}
	}
	
	bool processEvent(const clap_event_header *event) {
		tasks.resize(0);
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return false;
		if (event->type == CLAP_EVENT_NOTE_ON) {
			auto &noteEvent = *(const clap_event_note *)event;
			for (auto &n : notes) {
				if (n.match(noteEvent)) return true;
			}
			if (notes.size() < notes.capacity()) {
				Note newNote{
					.polyIndex=polyIndexQueue.back(),
					.noteId=noteEvent.note_id,
					.key=float(noteEvent.key),
					.velocity=float(noteEvent.velocity),
					.baseKey=noteEvent.key,
					.port=noteEvent.port_index,
					.processFrom=event->time,
					.processTo=event->time,
					.event=eventDown
				};
				notes.push_back(newNote);
				polyIndexQueue.pop_back();
			}
			return true;
		} else if (event->type == CLAP_EVENT_NOTE_OFF || event->type == CLAP_EVENT_NOTE_CHOKE) {
			auto &noteEvent = *(const clap_event_note *)event;
			for (auto &n : notes) {
				if (!n.match(noteEvent)) continue;
				if (n.processFrom < event->time) {
					n.processTo = event->time;
					tasks.push_back(n);
					n.processFrom = event->time;
				}
				n.event = eventUp;
				if (n.noteId < 0) {
					// This lets us close a particular note
					// and we're not using this since without note IDs
					// we can't have per-note modulation
					n.baseKey = -1 - int(n.polyIndex);
				}
				return true;
			}
			return true; // couldn't find a matching note - probably means we ran out of polyphony
		}
		return false;
	}
	
	void stop(const Note &noteToStop, const clap_output_events *eventsOut) {
		for (auto &n : notes) {
			if (n.match(noteToStop)) {
				if (n.noteId >= 0) {
					// Let the host know the note isn't available for modulation any more
					clap_event_note stopEvent{
						.header={
							.size=sizeof(clap_event_note),
							.time=n.processFrom,
							.space_id=CLAP_CORE_EVENT_SPACE_ID,
							.type=CLAP_EVENT_NOTE_END,
							.flags=CLAP_EVENT_DONT_RECORD
						},
						.note_id=n.noteId,
						.port_index=n.port,
						.channel=n.port,
						.key=n.baseKey,
						.velocity=0
					};
					eventsOut->try_push(eventsOut, &stopEvent.header);
				}
				
				polyIndexQueue.push_back(n.polyIndex);
				if (notes.size() <= 1) { // final note
					notes.resize(0);
				} else {
					if (&n != &notes.back()) {
						// Move the last note into this slot
						n = notes.back();
					}
					notes.pop_back();
				}
				return;
			}
		}
	}
	
	std::vector<Note> tasks;
private:
	std::vector<Note> notes;
	std::vector<size_t> polyIndexQueue;
};
