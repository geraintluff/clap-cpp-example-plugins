#pragma once

#include "clap/events.h"

#include <vector>

/* This is a helper class which handles CLAP note events, and returns "note tasks", which are sub-blocks for processing each note.  A note's tasks will have a consistent `voiceIndex` (up to the specified polyphony), exclusive to that note it's `.stop()`ed or stolen.

When you hand it an event (and it returns `true`), it queues up tasks for processing any affected notes.  You can also request all notes be processed up to a certain block index, which should be used for completing a block, or for any sample-accurate parameter/etc. changes which affect all notes.

It implements voice-stealing based on time since a note's release (if released) or attack.  This is represented by a note-task with `stateKill`.  The length (`processFrom`/`processTo`) of this task will not overlap with the new note - which unavoidably means it *may* be 0, in which case you can process a bit more to avoid clicks at your discretion.
*/
struct SynthManager {
	enum State{stateDown, stateLegato, stateContinue, stateUp, stateRelease, stateKill};

	struct Note {
		// Note info
		size_t voiceIndex;
		float key, velocity;
		int16_t port, channel;
		// Task info
		State state = stateDown;
		uint32_t processFrom, processTo;
		
		Note(size_t voiceIndex, const clap_event_note &e) : voiceIndex(voiceIndex), key(e.key), velocity(e.velocity), port(e.port_index), channel(e.channel), processFrom(e.header.time), processTo(e.header.time), noteId(e.note_id), baseKey(e.key) {}

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
	private:
		friend class SynthManager;
		int32_t noteId;
		int16_t baseKey;
		size_t age = 0;
	};
	
	SynthManager(size_t polyphony=64) {
		notes.reserve(polyphony);
		tasks.reserve(std::max<size_t>(2, polyphony)); // Even in the monophonic case, we might need to handle both a kill and note-start
		for (size_t i = 0; i < polyphony; ++i) {
			voiceIndexQueue.push_back(polyphony - 1 - i);
		}
	}
	
	size_t polyphony() const {
		return notes.capacity();
	}
	
	void reset() {
		notes.clear();
		tasks.clear();
		auto polyphony = notes.capacity();
		voiceIndexQueue.clear();
		for (size_t i = 0; i < polyphony; ++i) {
			voiceIndexQueue.push_back(polyphony - 1 - i);
		}
	}
	
	void startBlock() {
		tasks.clear();
		for (auto &n : notes) n.processFrom = n.processTo = 0;
	}
	void processTo(uint32_t frames) {
		tasks.clear();
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
		tasks.clear();
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
			Note newNote{voiceIndexQueue.back(), noteEvent};
			if (newNote.noteId < 0) {
				newNote.noteId = -int32_t(internalNoteId);
				if (++internalNoteId >= 0x7FFFFFFF) internalNoteId = 2;
			}
			notes.push_back(newNote);
			voiceIndexQueue.pop_back();
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
				// Only stop if the note ID isn't a wildcard
				if (noteEvent.note_id >= 0) break;
			}
		}
		return tasks.size() > 0;
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
				
				voiceIndexQueue.push_back(n.voiceIndex);
				if (notes.size() <= 1) { // final note
					notes.clear();
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
	std::vector<size_t> voiceIndexQueue;
};
