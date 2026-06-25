#include "rules_engine.h"
#include <iostream>

RulesEngine::RulesEngine() {}

void RulesEngine::process_primary_event(const Event& event) {
    // TODO: Store event in a local rolling window buffer
    if (event.type == "BUTTON_PRESS") {
        recent_button_presses_++;
        // TODO: Update last_press_time_ms_
        check_for_repeated_requests();
    }
}

std::vector<Event> RulesEngine::get_and_clear_derived_events() {
    std::vector<Event> out = derived_events_queue_;
    derived_events_queue_.clear();
    return out;
}

void RulesEngine::check_for_repeated_requests() {
    // TODO: If recent_button_presses_ > 3 within 10 seconds, 
    // push a "REPEATED_HELP_REQUEST" event into derived_events_queue_.
}

void RulesEngine::check_for_engagement_drop(uint64_t current_time_ms) {
    // TODO: If no events processed for > 15 minutes during an active session,
    // push an "ENGAGEMENT_DROP" event into derived_events_queue_.
}