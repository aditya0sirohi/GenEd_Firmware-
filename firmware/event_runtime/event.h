#pragma once
#include <cstdint>
#include <string>
#include <map>

// ============================================================
// Event Types — spec se exactly
// ============================================================
enum class EventType : uint32_t {
    // Hardware events
    SESSION_STARTED = 1,
    SESSION_ENDED,
    HELP_REQUESTED,
    BUTTON_PRESSED,
    INACTIVITY_DETECTED,
    CONNECTIVITY_LOST,
    CONNECTIVITY_RESTORED,
    OTA_STARTED,
    OTA_SUCCEEDED,
    OTA_FAILED,
    POWER_LOW,
    DEVICE_REBOOTED,

    // Derived events — rules engine banata hai
    REPEATED_HELP_REQUESTS,
    ENGAGEMENT_DROP,
    STRUGGLE_DETECTED
};

// ============================================================
// Event — har ek event ka structure
// ============================================================
struct Event {
    // Identity
    uint64_t    event_id        = 0;   // stable ID — storage se pehle assign
    uint64_t    correlation_id  = 0;   // related events chain karne ke liye
    uint32_t    sequence_number = 0;   // upload order maintain karne ke liye

    // Type
    EventType   type;

    // Timing
    uint64_t    occurred_at_utc_ms  = 0;  // UTC milliseconds
    uint64_t    monotonic_ms        = 0;  // boot se kitna time

    // Session
    std::string device_id;
    std::string student_session_id;

    // Payload — flexible key-value
    std::map<std::string, std::string> payload;

    // Diagnostics snapshot at time of event
    uint8_t     battery_pct      = 0;
    int16_t     rssi_dbm         = 0;
    std::string firmware_version = "0.1.0";

    // Internal tracking
    bool        committed        = false;  // server ne acknowledge kiya?
};

// ============================================================
// Event Type → String (logging ke liye)
// ============================================================
inline const char* event_type_str(EventType t) {
    switch(t) {
        case EventType::SESSION_STARTED:        return "session_started";
        case EventType::SESSION_ENDED:          return "session_ended";
        case EventType::HELP_REQUESTED:         return "help_requested";
        case EventType::BUTTON_PRESSED:         return "button_pressed";
        case EventType::INACTIVITY_DETECTED:    return "inactivity_detected";
        case EventType::CONNECTIVITY_LOST:      return "connectivity_lost";
        case EventType::CONNECTIVITY_RESTORED:  return "connectivity_restored";
        case EventType::OTA_STARTED:            return "ota_started";
        case EventType::OTA_SUCCEEDED:          return "ota_succeeded";
        case EventType::OTA_FAILED:             return "ota_failed";
        case EventType::POWER_LOW:              return "power_low";
        case EventType::DEVICE_REBOOTED:        return "device_rebooted";
        case EventType::REPEATED_HELP_REQUESTS: return "repeated_help_requests";
        case EventType::ENGAGEMENT_DROP:        return "engagement_drop";
        case EventType::STRUGGLE_DETECTED:      return "struggle_detected";
        default:                                return "unknown";
    }
}

// ============================================================
// Log IDs — har module ka alag log
// ============================================================
enum LogIds : uint32_t {
    LOG_EVENTS      = 1,  // main event queue
    LOG_DIAGNOSTICS = 2,  // diagnostics ring buffer
    LOG_OTA         = 3,  // OTA progress
    LOG_CRASH       = 4   // crash records — reboot pe bhi bachta hai
};