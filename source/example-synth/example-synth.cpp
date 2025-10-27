#ifndef LOG_EXPR
#	include <iostream>
#	define LOG_EXPR(expr) std::cout << #expr " = " << (expr) << std::endl;
#endif

#include "example-synth.h"

clap_process_status ExampleSynth::pluginProcess(const clap_process *process) {
	for (uint32_t outPort = 0; outPort < process->audio_outputs_count; ++outPort) {
		auto &outBuffer = process->audio_outputs[outPort];
		if (outPort < process->audio_inputs_count) {
			auto &inBuffer = process->audio_inputs[outPort];
			// Copy input
			for (uint32_t outC = 0; outC < outBuffer.channel_count; ++outC) {
				uint32_t inC = outC%(inBuffer.channel_count);
				if (outBuffer.data32 && inBuffer.data32) {
					memcpy(outBuffer.data32[outC], inBuffer.data32[inC], process->frames_count*4);
				}
			}
		} else {
			// Zero
			for (uint32_t outC = 0; outC < outBuffer.channel_count; ++outC) {
				if (outBuffer.data32) {
					memset(outBuffer.data32[outC], 0, process->frames_count*4);
				}
			}
		}
	}

	auto &synthOut = process->audio_outputs[0];
	
	noteManager.startBlock();
	auto processNoteTasks = [&](auto &tasks) {
		float sustainAmp = std::pow(10, sustainDb.value/20);
		
		for (auto &note : tasks) {
			auto &osc = oscillators[note.voiceIndex];

			auto hz = 440*std::exp2((note.key - 69)/12);
			auto targetNormFreq = hz/sampleRate;

			auto portamentoMs = 10;
			auto portamentoSlew = 1/(portamentoMs*0.001f*sampleRate + 1);

			if (note.state == NoteManager::stateDown) {
				// Start new note
				osc = {};
				osc.normFreq = targetNormFreq;
			} else if (note.state == NoteManager::stateLegato) {
				// Restart attack from wherever the decay got to
				osc.attackRelease *= osc.decay;
				osc.decay = 1;
			}
			
			auto arMs = (note.released() ? 50 : 2);
			auto arSlew = 1/(arMs*0.001f*sampleRate + 1);
			auto targetAr = (note.released() ? 0 : note.velocity/4);
			// decay rate
			auto decayMs = 10 + 490*note.velocity*note.velocity;
			auto decaySlew = 1/(decayMs*0.001f*sampleRate + 1);
			
			auto processTo = note.processTo;
			if (note.state == NoteManager::stateKill) { // This note is about to be stolen
				// minimum 1ms fade-out
				processTo = std::max<uint32_t>(note.processTo, note.processFrom + sampleRate*0.001);
				// unless we'd hit the end of the block
				processTo = std::min(processTo, process->frames_count);
				
				// Decay -60dB in the time we have
				float samples = processTo - note.processFrom;
				arSlew = 7/(samples + 7);
				targetAr = 0;
			}
			
			for (uint32_t i = note.processFrom; i < processTo; ++i) {
				osc.attackRelease += (targetAr - osc.attackRelease)*arSlew;
				osc.decay += (sustainAmp - osc.decay)*decaySlew;
				osc.normFreq += (targetNormFreq - osc.normFreq)*portamentoSlew;
				osc.phase += osc.normFreq;
				auto amp = osc.attackRelease*osc.decay;
				auto v = amp*std::sin(float(2*M_PI)*osc.phase);
				// stereo out
				synthOut.data32[0][i] += v;
				synthOut.data32[1][i] += v;
			}
			osc.phase -= std::floor(osc.phase);

			if (note.released() && osc.canStop()) {
				noteManager.stop(note, process->out_events);
			}
		}
	};

	auto *eventsIn = process->in_events;
	auto *eventsOut = process->out_events;
	uint32_t eventCount = eventsIn->size(eventsIn);
	for (uint32_t i = 0; i < eventCount; ++i) {
		auto *event = eventsIn->get(eventsIn, i);
		if (auto newNote = noteManager.wouldStart(event)) {
			bool foundLegato = false;
			if (polyphony.value == 0) {
				for (auto &otherNote : noteManager) {
					if (!otherNote.released() || otherNote.ageAt(event->time) < sampleRate*0.01f) {
						processNoteTasks(noteManager.legato(*newNote, otherNote, eventsOut));
						foundLegato = true;
						break;
					}
				}
			}
			if (!foundLegato) {
				processNoteTasks(noteManager.start(*newNote, eventsOut));
			}
		} else if (auto endNote = noteManager.wouldRelease(event)) {
			processNoteTasks(noteManager.release(*endNote));
		}
		processEvent(event);
		eventsOut->try_push(eventsOut, event);
	}
	
	processNoteTasks(noteManager.processTo(process->frames_count));
	
	return CLAP_PROCESS_CONTINUE;
}
