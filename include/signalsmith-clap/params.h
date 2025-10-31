#pragma once

#include "clap/events.h"
#include "clap/ext/params.h"

#include <atomic>
#include <functional>

namespace signalsmith { namespace clap {

/* A parameter object which can send gesture/value events back to the host when needed.

It can also track whether its value has been sent to the UI or not, but doesn't specify how that should be done.
*/
struct Param {
	double value = 0;
	clap_param_info info;
	const char *formatString = "%.2f";
	std::function<std::string(double)> formatFn;
	const char *key; // useful when debugging, or when an integer key is awkward
	
	// User interactions which we need to send as events to the host
	std::atomic_flag sentValue = ATOMIC_FLAG_INIT;
	std::atomic_flag sentGestureStart = ATOMIC_FLAG_INIT;
	std::atomic_flag sentGestureEnd = ATOMIC_FLAG_INIT;

	// Value change which we might need to send to the UI
	std::atomic_flag sentUiState = ATOMIC_FLAG_INIT;

	Param(const char *key, const char *name, clap_id paramId, double min, double initial, double max) : key(key), value(initial) {
		info = {
			.id=paramId,
			.flags=CLAP_PARAM_IS_AUTOMATABLE,
			.cookie=this,
			.name={}, // assigned below
			.module={},
			.min_value=min,
			.max_value=max,
			.default_value=initial
		};
		std::strncpy(info.name, name, CLAP_NAME_SIZE);
		
		sentValue.test_and_set();
		sentGestureStart.test_and_set();
		sentGestureEnd.test_and_set();
	}
	
	void setValueFromEvent(const clap_event_param_value &paramEvent) {
		value = paramEvent.value;
		sentUiState.clear();
	}

	void sendEvents(const clap_output_events *outEvents) {
		if (!sentGestureStart.test_and_set()) {
			clap_event_param_gesture event{
				.header={
					.size=sizeof(clap_event_param_gesture),
					.time=0,
					.space_id=CLAP_CORE_EVENT_SPACE_ID,
					.type=CLAP_EVENT_PARAM_GESTURE_BEGIN,
					.flags=CLAP_EVENT_IS_LIVE
				},
				.param_id=info.id
			};
			outEvents->try_push(outEvents, &event.header);
		}
		if (!sentValue.test_and_set()) {
			clap_event_param_value event{
				.header={
					.size=sizeof(clap_event_param_value),
					.time=0,
					.space_id=CLAP_CORE_EVENT_SPACE_ID,
					.type=CLAP_EVENT_PARAM_VALUE,
					.flags=CLAP_EVENT_IS_LIVE
				},
				.param_id=info.id,
				.cookie=this,
				.note_id=-1,
				.port_index=-1,
				.channel=-1,
				.key=-1,
				.value=value
			};
			outEvents->try_push(outEvents, &event.header);
		}
		if (!sentGestureEnd.test_and_set()) {
			clap_event_param_gesture event{
				.header={
					.size=sizeof(clap_event_param_gesture),
					.time=0,
					.space_id=CLAP_CORE_EVENT_SPACE_ID,
					.type=CLAP_EVENT_PARAM_GESTURE_END,
					.flags=CLAP_EVENT_IS_LIVE
				},
				.param_id=info.id
			};
			outEvents->try_push(outEvents, &event.header);
		}
	}
};

}} // namespace
