#include "event_queue.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <sstream>

// ============================================================
// Constructor
// ============================================================
EventQueue::EventQueue(IStorage* storage)
    : storage_(storage) {}

// ============================================================
// Enqueue — event daalo queue mein
// ============================================================
Status EventQueue::enqueue(Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Queue full? Backpressure policy
    if (queue_.size() >= MAX_QUEUE_SIZE) {
        std::cout << "[EVENT_QUEUE] ERROR: queue full ("
                  << MAX_QUEUE_SIZE << " events)\n";
        return Status::ERR_FULL;
    }

    // Stable ID assign karo — storage se pehle
    event.event_id       = next_event_id_++;
    event.sequence_number = next_sequence_++;
    event.committed      = false;

    // Pehle storage mein likho — durability ke liye
    auto bytes = serialize(event);
    Span<const uint8_t> span{bytes.data(), bytes.size()};
    Status s = storage_->append_record(LOG_EVENTS, span);
    if (s != Status::OK) {
        std::cout << "[EVENT_QUEUE] storage write FAILED"
                  << " event_id=" << event.event_id << "\n";
        return s;
    }

    // Phir memory mein daalo
    queue_.push_back(event);
    total_enqueued_++;

    std::cout << "[EVENT_QUEUE] enqueued event_id="
              << event.event_id
              << " type=" << event_type_str(event.type)
              << " seq=" << event.sequence_number
              << " pending=" << queue_.size() << "\n";

    return Status::OK;
}

// ============================================================
// Get Pending — uncommitted events nikalo
// ============================================================
std::vector<Event> EventQueue::get_pending(size_t max_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Event> result;

    for (auto& e : queue_) {
        if (!e.committed) {
            result.push_back(e);
            if (result.size() >= max_count) break;
        }
    }

    std::cout << "[EVENT_QUEUE] get_pending returning "
              << result.size() << " events\n";
    return result;
}

// ============================================================
// Mark Committed — server ne acknowledge kiya
// ============================================================
Status EventQueue::mark_committed(uint64_t event_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& e : queue_) {
        if (e.event_id == event_id) {
            if (e.committed) {
                return Status::OK;
            }

            // Persist the acknowledgement first. If this fails, leave the
            // in-memory event pending so SyncTask safely retries it.
            Status storage_status = storage_->mark_record_committed(
                LOG_EVENTS,
                (RecordId)event_id);
            if (storage_status != Status::OK) {
                std::cout << "[EVENT_QUEUE] commit persistence FAILED"
                          << " event_id=" << event_id << "\n";
                return storage_status;
            }

            e.committed = true;
            total_committed_++;

            std::cout << "[EVENT_QUEUE] committed event_id="
                      << event_id
                      << " total_committed=" << total_committed_
                      << "\n";
            return Status::OK;
        }
    }

    std::cout << "[EVENT_QUEUE] WARN: event_id="
              << event_id << " not found for commit\n";
    return Status::ERR_NOT_FOUND;
}

// ============================================================
// Pending Count
// ============================================================
size_t EventQueue::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (auto& e : queue_)
        if (!e.committed) count++;
    return count;
}

// ============================================================
// Recovery — reboot ke baad storage se reload
// ============================================================
Status EventQueue::recover() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[EVENT_QUEUE] starting recovery...\n";

    // recover() is intentionally idempotent. This also makes a simulated
    // reboot easy to test without duplicating records in RAM.
    queue_.clear();
    next_event_id_ = 1;
    next_sequence_ = 1;

    struct Visitor : IRecordVisitor {
        EventQueue* q;
        int recovered = 0;
        int skipped   = 0;

        bool on_record(RecordId id,
                       Span<const uint8_t> data) override {
            Event e;
            if (q->deserialize(data.data, data.len, e)) {
                if (!e.committed) {
                    if (q->queue_.size() >= MAX_QUEUE_SIZE) {
                        std::cout << "[EVENT_QUEUE] recovery stopped: "
                                  << "RAM queue capacity reached\n";
                        return false;
                    }
                    q->queue_.push_back(e);
                    // Sequence tracking update karo
                    if (e.event_id >= q->next_event_id_)
                        q->next_event_id_ = e.event_id + 1;
                    if (e.sequence_number >= q->next_sequence_)
                        q->next_sequence_ = e.sequence_number + 1;
                    recovered++;
                } else {
                    skipped++;
                }
            }
            return true; // continue scanning
        }
    };

    Visitor v;
    v.q = this;
    Status s = storage_->scan_records(LOG_EVENTS, v);

    std::cout << "[EVENT_QUEUE] recovery complete:"
              << " recovered=" << v.recovered
              << " skipped_committed=" << v.skipped
              << " next_event_id=" << next_event_id_
              << "\n";
    return s;
}

// ============================================================
// Compaction — committed events hata do
// ============================================================
Status EventQueue::compact() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t before = queue_.size();

    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
            [](const Event& e) { return e.committed; }),
        queue_.end()
    );

    size_t after = queue_.size();

    std::cout << "[EVENT_QUEUE] compaction:"
              << " removed=" << (before - after)
              << " remaining=" << after << "\n";

    return Status::OK;
}

// ============================================================
// Oldest Pending Age
// ============================================================
uint64_t EventQueue::oldest_pending_age_ms(
    uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& e : queue_) {
        if (!e.committed && e.occurred_at_utc_ms > 0) {
            return now_ms - e.occurred_at_utc_ms;
        }
    }
    return 0;
}

// ============================================================
// Serialize — Event → bytes
// Simple format: [event_id|seq|type|utc_ms|mono_ms|
//                 committed|device_id|session_id|
//                 battery|rssi|fw_version|correlation_id|payload]
// Production mein protobuf ya CBOR use hoga
// ============================================================
namespace {
std::string escape_field(const std::string& input) {
    static const char* hex = "0123456789ABCDEF";
    std::string output;
    for (unsigned char c : input) {
        if (c == '%' || c == '|' || c == ';' || c == '=') {
            output += '%';
            output += hex[(c >> 4) & 0x0F];
            output += hex[c & 0x0F];
        } else {
            output += static_cast<char>(c);
        }
    }
    return output;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::string unescape_field(const std::string& input) {
    std::string output;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            int high = hex_value(input[i + 1]);
            int low = hex_value(input[i + 2]);
            if (high >= 0 && low >= 0) {
                output += static_cast<char>((high << 4) | low);
                i += 2;
                continue;
            }
        }
        output += input[i];
    }
    return output;
}
} // namespace

std::vector<uint8_t> EventQueue::serialize(
    const Event& e) const {

    std::string s;
    s += std::to_string(e.event_id)        + "|";
    s += std::to_string(e.sequence_number) + "|";
    s += std::to_string((uint32_t)e.type)  + "|";
    s += std::to_string(e.occurred_at_utc_ms) + "|";
    s += std::to_string(e.monotonic_ms)    + "|";
    s += std::to_string(e.committed ? 1:0) + "|";
    s += e.device_id                       + "|";
    s += e.student_session_id              + "|";
    s += std::to_string(e.battery_pct)     + "|";
    s += std::to_string(e.rssi_dbm)        + "|";
    s += escape_field(e.firmware_version)  + "|";
    s += std::to_string(e.correlation_id)  + "|";

    bool first = true;
    for (const auto& item : e.payload) {
        if (!first) s += ";";
        s += escape_field(item.first) + "=" + escape_field(item.second);
        first = false;
    }

    return std::vector<uint8_t>(s.begin(), s.end());
}

// ============================================================
// Deserialize — bytes → Event
// ============================================================
bool EventQueue::deserialize(const uint8_t* data,
                              size_t len,
                              Event& out) const {
    std::string s(reinterpret_cast<const char*>(data), len);
    std::vector<std::string> parts;
    std::string token;

    for (char c : s) {
        if (c == '|') {
            parts.push_back(token);
            token.clear();
        } else {
            token += c;
        }
    }
    parts.push_back(token);

    if (parts.size() < 11) return false;

    try {
        out.event_id           = std::stoull(parts[0]);
        out.sequence_number    = std::stoul(parts[1]);
        out.type               = (EventType)std::stoul(parts[2]);
        out.occurred_at_utc_ms = std::stoull(parts[3]);
        out.monotonic_ms       = std::stoull(parts[4]);
        out.committed          = parts[5] == "1";
        out.device_id          = parts[6];
        out.student_session_id = parts[7];
        out.battery_pct        = (uint8_t)std::stoul(parts[8]);
        out.rssi_dbm           = (int16_t)std::stoi(parts[9]);
        out.firmware_version   = unescape_field(parts[10]);

        // Fields added after the initial prototype are optional so records
        // written by the earlier format can still be recovered.
        if (parts.size() >= 12)
            out.correlation_id = std::stoull(parts[11]);

        if (parts.size() >= 13 && !parts[12].empty()) {
            std::stringstream payload_stream(parts[12]);
            std::string item;
            while (std::getline(payload_stream, item, ';')) {
                size_t equals = item.find('=');
                if (equals == std::string::npos) continue;
                std::string key = unescape_field(item.substr(0, equals));
                std::string value = unescape_field(item.substr(equals + 1));
                out.payload[key] = value;
            }
        }
    } catch (...) {
        return false;
    }

    return true;
}
