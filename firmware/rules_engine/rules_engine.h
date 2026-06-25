#pragma once
#include <string>
#include <vector>

// Note: Assuming an Event struct exists in your event_runtime module
struct Event {
    std::string type;
    uint64_t timestamp_ms;
};

class RulesEngine {
public:
    RulesEngine();

    // Ingest primary hardware events
    void process_primary_event(const Event& event);
    
    // Poll for any new smart/derived events (like "struggling")
    std::vector<Event> get_and_clear_derived_events();

private:
    int recent_button_presses_ = 0;
    uint64_t last_press_time_ms_ = 0;
    
    std::vector<Event> derived_events_queue_;
    
    void check_for_repeated_requests();
    void check_for_engagement_drop(uint64_t current_time_ms);
};