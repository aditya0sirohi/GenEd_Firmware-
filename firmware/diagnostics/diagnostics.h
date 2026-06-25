#pragma once
#include "../hal/include/hal.h"
#include <string>
#include <vector>

enum class LogSeverity {
    INFO,
    WARNING,
    CRITICAL
};

struct DiagnosticMetrics {
    uint64_t uptime_ms;
    size_t free_heap_bytes;
    size_t queue_depth;
    int16_t rssi_dbm;
    uint8_t battery_percent;
};

class DiagnosticsFramework {
public:
    DiagnosticsFramework(IStorage* storage);

    // Core diagnostics operations
    Status log_error(const std::string& error_code, LogSeverity severity, const std::string& module);
    Status record_crash(const std::string& reset_reason, const std::string& active_task);
    
    // Event tracing ring buffer
    void add_trace_entry(const std::string& task_name, const std::string& message);
    std::string export_compact_json();

    // Health metrics
    DiagnosticMetrics get_current_metrics();

private:
    IStorage* storage_;
    static constexpr size_t RING_BUFFER_MAX = 100;
    std::vector<std::string> trace_ring_buffer_;
    size_t current_ring_index_ = 0;
};