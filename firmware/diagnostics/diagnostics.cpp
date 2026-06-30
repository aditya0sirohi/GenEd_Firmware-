#include "diagnostics.h"
#include "../event_runtime/event.h"
#include <chrono>
#include <iostream>
#include <sstream>

DiagnosticsFramework::DiagnosticsFramework(IStorage* storage) : storage_(storage) {
    trace_ring_buffer_.reserve(RING_BUFFER_MAX);
}

Status DiagnosticsFramework::log_error(const std::string& error_code, LogSeverity severity, const std::string& module) {
    std::string record = "{\"error_code\":\"" + error_code +
        "\",\"severity\":" + std::to_string(static_cast<int>(severity)) +
        ",\"module\":\"" + module + "\"}";
    Span<const uint8_t> bytes{
        reinterpret_cast<const uint8_t*>(record.data()), record.size()};
    Status status = storage_->append_record(LOG_DIAGNOSTICS, bytes);
    if (status == Status::OK)
        add_trace_entry(module, "error:" + error_code);
    return status;
}

Status DiagnosticsFramework::record_crash(const std::string& reset_reason, const std::string& active_task) {
    std::string record = "{\"reset_reason\":\"" + reset_reason +
        "\",\"active_task\":\"" + active_task + "\"}";
    Span<const uint8_t> bytes{
        reinterpret_cast<const uint8_t*>(record.data()), record.size()};
    return storage_->write_blob_atomic("last_crash", bytes);
}

void DiagnosticsFramework::add_trace_entry(const std::string& task_name, const std::string& message) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t timestamp_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string entry = std::to_string(timestamp_ms) + "|" +
                        task_name + "|" + message;

    std::lock_guard<std::mutex> lock(mutex_);
    if (trace_ring_buffer_.size() < RING_BUFFER_MAX) {
        trace_ring_buffer_.push_back(entry);
    } else {
        trace_ring_buffer_[current_ring_index_] = entry;
        current_ring_index_ = (current_ring_index_ + 1) % RING_BUFFER_MAX;
    }
}

std::string DiagnosticsFramework::export_compact_json() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream json;
    json << "{\"trace_count\":" << trace_ring_buffer_.size()
         << ",\"traces\":[";

    for (size_t i = 0; i < trace_ring_buffer_.size(); ++i) {
        size_t index = trace_ring_buffer_.size() < RING_BUFFER_MAX
            ? i
            : (current_ring_index_ + i) % RING_BUFFER_MAX;
        if (i != 0) json << ",";
        json << "\"";
        for (char c : trace_ring_buffer_[index]) {
            if (c == '"' || c == '\\') json << '\\';
            json << c;
        }
        json << "\"";
    }
    json << "]}";
    return json.str();
}

DiagnosticMetrics DiagnosticsFramework::get_current_metrics() {
    // Host simulation cannot query ESP-IDF heap/stack watermarks. Values that
    // require those APIs remain explicit host estimates.
    return DiagnosticMetrics{0, 512000, 0, -60, 100};
}
