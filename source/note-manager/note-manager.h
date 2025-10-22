#pragma once

#include "clap/events.h"

#include <vector>

struct NoteManager {
	enum State{stateDown, stateLegato, stateContinue, stateUp, stateRelease, stateKill};

	struct Note {
		size_t polyIndex;
		float key, velocity;
		int32_t noteId;
		int16_t baseKey, port, channel;

		uint32_t processFrom = 0, processTo = 0;

		State state = stateDown;
		size_t age = 0;
		
		bool released() const {
			return state == stateUp || state == stateRelease || state == stateKill;
		}
		
		bool match(const clap_event_note &clapEvent) const {
			if (clapEvent.note_id >= 0 && clapEvent.note_id != noteId) return false;
			if (clapEvent.port_index >= 0 && clapEvent.port_index != port) return false;
			if (clapEvent.channel >= 0 && clapEvent.channel != channel) return false;
			if (clapEvent.key >= 0 && clapEvent.key != baseKey) return false;
			return true;
		}
		bool match(const Note &other) const {
			return noteId == other.noteId;
		}
		
		float killCost() const {
			return 1.0f/(age + 1) + 10 - (int)state;
		}
	};
	
	NoteManager(size_t polyphony=16) {
		notes.reserve(polyphony);
		tasks.reserve(std::max<size_t>(2, polyphony)); // we might need to handle a kill and note-start from a single event
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
				n.age += (n.processTo - n.processFrom);
				n.processFrom = frames;
				if (n.state == stateDown || n.state == stateLegato) {
					n.state = stateContinue;
				} else if (n.state == stateUp) {
					n.state = stateRelease;
				}
			}
		}
	}
	
	bool processEvent(const clap_event_header *event, const clap_output_events *eventsOut) {
		tasks.resize(0);
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return false;
		if (event->type == CLAP_EVENT_NOTE_ON) {
			auto &noteEvent = *(const clap_event_note *)event;
			for (auto &n : notes) {
				if (n.match(noteEvent)) {
					// Retrigger (legato) an existing note
					if (n.processFrom < event->time) {
						n.processTo = event->time;
						if (n.processTo > n.processFrom) {
							tasks.push_back(n);
							n.age += (n.processTo - n.processFrom);
						}
						n.processFrom = event->time;
					}
					n.state = stateLegato;
					return true;
				}
			}
			if (notes.size() >= notes.capacity()) {
				// Kill an existing note
				size_t killIndex = 0;
				float killCost = 1e10;
				for (size_t i = 0; i < notes.size(); ++i) {
					auto cost = notes[i].killCost();
					if (cost < killCost) {
						killIndex = i;
						killCost = cost;
					}
				}
				auto &killNote = notes[killIndex];
				killNote.state = stateKill;
				killNote.processTo = event->time;
				// Push this task even if it's zero length
				tasks.push_back(killNote);
				stop(killNote, eventsOut);
			}
			Note newNote{
				.polyIndex=polyIndexQueue.back(),
				.noteId=noteEvent.note_id,
				.key=float(noteEvent.key),
				.velocity=float(noteEvent.velocity),
				.baseKey=noteEvent.key,
				.port=noteEvent.port_index,
				.processFrom=event->time,
				.processTo=event->time,
				.state=stateDown
			};
			if (newNote.noteId < 0) {
				newNote.noteId = -int32_t(internalNoteId);
				if (++internalNoteId >= 0x7FFFFFFF) internalNoteId = 2;
			}
			notes.push_back(newNote);
			polyIndexQueue.pop_back();
			return true;
		} else if (event->type == CLAP_EVENT_NOTE_OFF || event->type == CLAP_EVENT_NOTE_CHOKE) {
			auto &noteEvent = *(const clap_event_note *)event;
			for (auto &n : notes) {
				if (!n.match(noteEvent)) continue;
				if (n.processFrom < event->time) {
					n.processTo = event->time;
					if (n.processTo > n.processFrom) {
						tasks.push_back(n);
						n.age += (n.processTo - n.processFrom);
					}
					n.processFrom = event->time;
				}
				n.state = stateUp;
				if (n.noteId < 0) {
					// This lets us close a particular note
					// and we're not using this since without note IDs
					// we can't have per-note modulation
					n.baseKey = -1 - int(n.polyIndex);
				}
				// Only return if the note ID isn't a wildcard
				if (noteEvent.note_id >= 0) return true;
			}
			return true;
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
	uint32_t internalNoteId = 2;
	std::vector<Note> notes;
	std::vector<size_t> polyIndexQueue;
};
