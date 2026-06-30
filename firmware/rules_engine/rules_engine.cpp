#include "rules_engine.h"
#include <algorithm>
#include <iostream>

RulesEngine::RulesEngine() {}

void RulesEngine::process_primary_event(const Event& event) {
    if (event.type == EventType::SESSION_STARTED) {
        help_request_times_.clear();
        repeated_help_emitted_ = false;
        engagement_drop_emitted_ = false;
        last_press_time_ms_ = event.monotonic_ms;
        return;
    }

    if (event.type == EventType::HELP_REQUESTED) {
        last_press_time_ms_ = event.monotonic_ms;
        help_request_times_.push_back(event.monotonic_ms);

        const uint64_t cutoff =
            event.monotonic_ms > HELP_WINDOW_MS
                ? event.monotonic_ms - HELP_WINDOW_MS
                : 0;
        help_request_times_.erase(
            std::remove_if(help_request_times_.begin(),
                           help_request_times_.end(),
                           [cutoff](uint64_t time) { return time < cutoff; }),
            help_request_times_.end());
        check_for_repeated_requests(event);
    }

    if (event.type == EventType::INACTIVITY_DETECTED) {
        check_for_engagement_drop(event);
    }
}

std::vector<Event> RulesEngine::get_and_clear_derived_events() {
    std::vector<Event> out = derived_events_queue_;
    derived_events_queue_.clear();
    return out;
}

Event RulesEngine::derived_from(const Event& source, EventType type) const {
    Event result = source;
    result.event_id = 0;
    result.sequence_number = 0;
    result.committed = false;
    result.type = type;
    result.payload.clear();
    return result;
}

void RulesEngine::check_for_repeated_requests(const Event& source) {
    if (help_request_times_.size() >= 3 && !repeated_help_emitted_) {
        Event derived = derived_from(
            source, EventType::REPEATED_HELP_REQUESTS);
        derived.payload["count"] =
            std::to_string(help_request_times_.size());
        derived.payload["window_ms"] = std::to_string(HELP_WINDOW_MS);
        derived_events_queue_.push_back(derived);
        repeated_help_emitted_ = true;
    }
}

void RulesEngine::check_for_engagement_drop(const Event& source) {
    uint64_t inactive_for =
        source.monotonic_ms >= last_press_time_ms_
            ? source.monotonic_ms - last_press_time_ms_
            : 0;

    if (inactive_for >= INACTIVITY_THRESHOLD_MS &&
        !engagement_drop_emitted_) {
        Event drop = derived_from(source, EventType::ENGAGEMENT_DROP);
        drop.payload["inactive_ms"] = std::to_string(inactive_for);
        derived_events_queue_.push_back(drop);
        engagement_drop_emitted_ = true;

        if (repeated_help_emitted_) {
            Event struggle =
                derived_from(source, EventType::STRUGGLE_DETECTED);
            struggle.payload["reason"] =
                "repeated_help_requests_and_inactivity";
            derived_events_queue_.push_back(struggle);
        }
    }
}
