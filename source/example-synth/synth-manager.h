#pragma once

#include "clap/events.h"

#include <vector>
#include <optional>

/* This helper handles CLAP note events, and returns "note tasks", which are sub-blocks for processing each note.  A note's tasks will have a consistent `voiceIndex` (up to the specified polyphony), exclusive to that note it's `.stop()`ed or stolen.

When you hand it an event (and it returns `true`), it returns tasks to process any affected notes up to that point.  You can also request all notes be processed up to a certain block index, which should be used for completing a block, or for any sample-accurate parameter/etc. changes which affect all notes.

It implements voice-stealing based on time since a note's release (if released) or attack.  This is represented by a note-task with `stateKill`.  The length (`processFrom`/`processTo`) of this task will not overlap with the new note - which unavoidably means it *may* be 0, in which case you can process a bit more to avoid clicks at your discretion.
*/
struct SynthManager {
	enum State{stateDown, stateLegato, stateContinue, stateUp, stateRelease, stateKill};

	struct Note {
		// Note info
		size_t voiceIndex;
		float key, velocity;
		int16_t port, channel;
		// Processing task info
		State state = stateDown;
		uint32_t processFrom, processTo;
		
		bool released() const {
			return state == stateUp || state == stateRelease || state == stateKill;
		}
		
		bool match(const clap_event_note &clapEvent) const {
			if (clapEvent.note_id != -1) return clapEvent.note_id == noteId;
			if (released()) return false;
			if (clapEvent.port_index >= 0 && clapEvent.port_index != port) return false;
			if (clapEvent.channel >= 0 && clapEvent.channel != channel) return false;
			if (clapEvent.key >= 0 && clapEvent.key != baseKey) return false;
			return true;
		}
		// `other` may contain wildcards (-1), but `this` must not
		bool match(const Note &other) const {
			if (other.noteId != -1) return noteId == other.noteId;
			if (released()) return false;
			if (other.port != -1 && other.port != port) return false;
			if (other.channel != -1 && other.channel != channel) return false;
			if (other.baseKey != -1 && other.baseKey != baseKey) return false;
			return true;
		}
		
		float killCost() const {
			return 1.0f/(age + 1) + 10 - (int)state;
		}
		
		size_t ageAt(uint32_t timeInBlock) const {
			return age + (timeInBlock - processFrom);
		}
	private:
		friend class SynthManager;

		Note(size_t voiceIndex, const clap_event_note &e) : voiceIndex(voiceIndex), key(e.key), velocity(e.velocity), port(e.port_index), channel(e.channel), processFrom(e.header.time), processTo(e.header.time), noteId(e.note_id), baseKey(e.key) {}

		int32_t noteId;
		int16_t baseKey;
		size_t age = 0; // since start/legato/up
	};
	
	SynthManager(size_t polyphony=64) {
		notes.reserve(polyphony);
		tasks.reserve(polyphony);
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
	const std::vector<Note> & processTo(uint32_t frames) {
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
		return tasks;
	}
	
	// Gets a note ready, but don't do anything with it yet
	std::optional<Note> wouldStart(const clap_event_header *event) const {
		if (event->space_id == CLAP_CORE_EVENT_SPACE_ID && event->type == CLAP_EVENT_NOTE_ON) {
			auto &noteEvent = *(const clap_event_note *)event;
			Note newNote{size_t(-1), noteEvent};
			if (newNote.noteId < 0) {
				newNote.noteId = -int32_t(internalNoteId);
				if (++internalNoteId >= 0x7FFFFFFF) internalNoteId = 2;
			}
			return {newNote};
		}
		return {};
	}

	// You should call this if you're not using a note-on, so the host gets a NOTE_END
	void ignore(const Note &newNote, const clap_output_events *eventsOut) {
		sendNoteEnd(newNote, eventsOut);
	}
	
	const std::vector<Note> & start(const Note &newNote, const clap_output_events *eventsOut) {
		tasks.clear();
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
			killNote.processTo = newNote.processFrom;
			// Push this task even if it's zero length
			tasks.push_back(killNote);
			stop(killNote, eventsOut);
		}

		notes.push_back(newNote);
		// We had at least one note left in capacity, so this is safe
		notes.back().voiceIndex = voiceIndexQueue.back();
		voiceIndexQueue.pop_back();
		return tasks;
	}
	
	const std::vector<Note> & legato(const Note &newNote, const Note &existingNote, const clap_output_events *eventsOut) {
		tasks.clear();
		for (auto &n : notes) {
			if (n.match(existingNote)) {
				// Process the note
				addTask(n, newNote.processFrom);
				sendNoteEnd(n, eventsOut); // release the old note ID

				auto voiceIndex = existingNote.voiceIndex;
				n = newNote;
				n.voiceIndex = voiceIndex;
				n.state = stateLegato;
				n.age = 0;
				break;
			}
		}
		return tasks;
	}

	std::optional<Note> wouldRelease(const clap_event_header *event) const {
		if (event->space_id != CLAP_CORE_EVENT_SPACE_ID) return {};
		if (event->type == CLAP_EVENT_NOTE_OFF || event->type == CLAP_EVENT_NOTE_CHOKE) {
			auto &noteEvent = *(const clap_event_note *)event;
			return {Note{size_t(-1), noteEvent}}; // still includes any wildcards
		}
		return {};
	}

	const std::vector<Note> & release(const Note &releaseNote) {
		// If this is a note-end event (or we don't care) then use the timestamp we already have
		return release(releaseNote, releaseNote.processFrom);
	}

	const std::vector<Note> & release(const Note &releaseNote, uint32_t atBlockTime) {
		tasks.clear();
		for (auto &n : notes) {
			if (n.match(releaseNote)) {
				addTask(n, atBlockTime);
				n.state = stateUp;
				n.age = 0;
				// Stop unless the note ID is a wildcard
				if (releaseNote.noteId != -1) break;
			}
		}
		return tasks;
	}

	// Start or stop notes as appropriate
	const std::vector<Note> & processEvent(const clap_event_header *event, const clap_output_events *eventsOut) {
		auto newNote = wouldStart(event);
		if (newNote) return start(*newNote, eventsOut);
		
		auto releaseNote = wouldRelease(event);
		if (releaseNote) return release(*releaseNote);

		tasks.clear();
		return tasks;
	}
	
	// This note has finished - we no longer want any other tasks about it, and its voice can be reassigned
	void stop(const Note &noteToStop, const clap_output_events *eventsOut) {
		for (auto &n : notes) {
			if (n.match(noteToStop)) {
				sendNoteEnd(n, eventsOut);
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

	const std::vector<Note> & activeNotes() const {
		return notes;
	}
	
	auto begin() const {
		return notes.begin();
	}
	auto end() const {
		return notes.end();
	}
	
private:
	mutable uint32_t internalNoteId = 2;
	std::vector<Note> notes;
	std::vector<Note> tasks;
	std::vector<size_t> voiceIndexQueue;
	
	void addTask(Note &n, uint32_t processTo) {
		// Skip zero-length tasks for non-event states only
		if (n.processFrom >= processTo && (n.state == stateContinue || n.state == stateRelease)) return;
		n.processTo = processTo;
		tasks.push_back(n);
		n.age += (processTo - n.processFrom);
		n.processFrom = processTo;
	}
	
	void sendNoteEnd(const Note &n, const clap_output_events *eventsOut) {
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
	}
};
