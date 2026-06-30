#pragma once
#include <cstdint>
#include <vector>
#include "../event_runtime/event.h"

class RulesEngine {
public:
    RulesEngine();

    // Ingest primary hardware events
    void process_primary_event(const Event& event);
    
    // Poll for any new smart/derived events (like "struggling")
    std::vector<Event> get_and_clear_derived_events();

private:
    static constexpr uint64_t HELP_WINDOW_MS = 300000;
    static constexpr uint64_t INACTIVITY_THRESHOLD_MS = 600000;

    std::vector<uint64_t> help_request_times_;
    uint64_t last_press_time_ms_ = 0;
    bool repeated_help_emitted_ = false;
    bool engagement_drop_emitted_ = false;
    
    std::vector<Event> derived_events_queue_;
    
    void check_for_repeated_requests(const Event& source);
    void check_for_engagement_drop(const Event& source);
    Event derived_from(const Event& source, EventType type) const;
};
