#include "hal_sim.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

// ============================================================
// Global instances
// ============================================================
static SimStorage  g_storage;
static SimNetwork  g_network;
static SimTime     g_time;
static SimPower    g_power;
static SimIO       g_io;
static SimSecurity g_security;

HAL create_sim_hal() {
    return HAL{
        &g_storage,
        &g_network,
        &g_time,
        &g_power,
        &g_io,
        &g_security
    };
}

SimStorage*  get_sim_storage()  { return &g_storage;  }
SimNetwork*  get_sim_network()  { return &g_network;  }
SimPower*    get_sim_power()    { return &g_power;    }
SimIO*       get_sim_io()       { return &g_io;       }
SimSecurity* get_sim_security() { return &g_security; }

// ============================================================
// SimStorage
// ============================================================
Status SimStorage::read_blob(Key key, Span<uint8_t> out) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = blobs_.find(key);
    if (it == blobs_.end()) return Status::ERR_NOT_FOUND;
    size_t copy_len = std::min(out.len, it->second.size());
    memcpy(out.data, it->second.data(), copy_len);
    return Status::OK;
}

Status SimStorage::write_blob_atomic(Key key,
                                      Span<const uint8_t> data) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_count_++;

    if (corrupt_next_write) {
        corrupt_next_write = false;
        std::cout << "[SIM_STORAGE] Corruption injected on key: "
                  << key << "\n";
        return Status::ERR_CORRUPT;
    }

    blobs_[key] = std::vector<uint8_t>(data.data,
                                        data.data + data.len);
    std::cout << "[SIM_STORAGE] write_blob_atomic key="
              << key << " size=" << data.len
              << " total_writes=" << write_count_ << "\n";
    return Status::OK;
}

Status SimStorage::append_record(LogId log,
                                  Span<const uint8_t> record) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_count_++;

    if (corrupt_next_write) {
        corrupt_next_write = false;
        std::cout << "[SIM_STORAGE] Corruption injected on append"
                  << " log=" << log << "\n";
        return Status::ERR_CORRUPT;
    }

    Record r;
    r.id   = next_id_++;
    r.data = std::vector<uint8_t>(record.data,
                                   record.data + record.len);
    logs_[log].push_back(r);

    std::cout << "[SIM_STORAGE] append_record log=" << log
              << " record_id=" << r.id
              << " size=" << record.len << "\n";
    return Status::OK;
}

Status SimStorage::scan_records(LogId log,
                                 IRecordVisitor& visitor) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = logs_.find(log);
    if (it == logs_.end()) return Status::OK;

    for (auto& r : it->second) {
        Span<const uint8_t> span{r.data.data(), r.data.size()};
        if (!visitor.on_record(r.id, span)) break;
    }
    return Status::OK;
}

Status SimStorage::mark_record_committed(LogId log,
                                          RecordId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = logs_.find(log);
    if (it == logs_.end()) return Status::ERR_NOT_FOUND;

    for (auto& r : it->second) {
        if (r.id == id) {
            r.committed = true;
            std::cout << "[SIM_STORAGE] record committed log="
                      << log << " id=" << id << "\n";
            return Status::OK;
        }
    }
    return Status::ERR_NOT_FOUND;
}

Status SimStorage::inject_corruption_for_test(Key key,
                                               CorruptionMode mode) {
    std::cout << "[SIM_STORAGE] Fault injection armed for key: "
              << key << "\n";
    corrupt_next_write = true;
    return Status::OK;
}

// ============================================================
// SimNetwork
// ============================================================
Status SimNetwork::connect(WifiCredentials credentials) {
    if (fault_.force_disconnect) {
        std::cout << "[SIM_NETWORK] connect BLOCKED by fault\n";
        return Status::ERR_TIMEOUT;
    }
    state_ = NetworkState::CONNECTED;
    std::cout << "[SIM_NETWORK] connected to: "
              << credentials.ssid << "\n";
    return Status::OK;
}

NetworkState SimNetwork::state() const {
    return state_;
}

Status SimNetwork::post_json(Endpoint endpoint,
                              Headers headers,
                              Span<const uint8_t> body,
                              HttpResponse* out) {
    if (state_ != NetworkState::CONNECTED ||
        fault_.force_disconnect) {
        std::cout << "[SIM_NETWORK] post_json FAILED — disconnected\n";
        return Status::ERR_TIMEOUT;
    }

    // Simulate packet loss
    if (fault_.packet_loss_rate > 0.0f) {
        float r = static_cast<float>(rand()) / RAND_MAX;
        if (r < fault_.packet_loss_rate) {
            std::cout << "[SIM_NETWORK] packet DROPPED url="
                      << endpoint.url << "\n";
            return Status::ERR_TIMEOUT;
        }
    }

    std::cout << "[SIM_NETWORK] POST " << endpoint.url
              << " device=" << headers.device_id
              << " body_size=" << body.len << "\n";

    // Mock response
    if (out) {
        out->status_code = 200;
        out->body        = "{\"status\":\"ok\"}";
    }
    return Status::OK;
}

void SimNetwork::set_fault_profile(NetworkFaultProfile profile) {
    fault_ = profile;
    std::cout << "[SIM_NETWORK] fault profile set:"
              << " loss=" << profile.packet_loss_rate
              << " timeout=" << profile.timeout_ms
              << " force_disconnect=" << profile.force_disconnect
              << "\n";
}

void SimNetwork::force_disconnect() {
    state_                = NetworkState::DISCONNECTED;
    fault_.force_disconnect = true;
    std::cout << "[SIM_NETWORK] force DISCONNECTED\n";
}

void SimNetwork::force_connect() {
    state_                = NetworkState::CONNECTED;
    fault_.force_disconnect = false;
    std::cout << "[SIM_NETWORK] force CONNECTED\n";
}

// ============================================================
// SimTime
// ============================================================
Timestamp SimTime::now_utc() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

MonotonicTime SimTime::monotonic_now() {
    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast
                   <std::chrono::milliseconds>(now - boot_time_);
    return elapsed.count();
}

Status SimTime::sync_time() {
    std::cout << "[SIM_TIME] time sync OK (simulated)\n";
    return Status::OK;
}

// ============================================================
// SimPower
// ============================================================
BatteryState SimPower::battery_state() {
    return BatteryState{battery_pct_, false, brownout_};
}

WakeReason SimPower::wake_reason() {
    return wake_reason_;
}

Status SimPower::enter_sleep(SleepMode mode,
                              Duration max_duration) {
    std::cout << "[SIM_POWER] entering sleep mode="
              << (mode == SleepMode::DEEP ? "DEEP" : "LIGHT")
              << " duration=" << max_duration << "ms\n";
    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            std::min(max_duration, (Duration)500)));
    wake_reason_ = WakeReason::RTC_TIMER;
    std::cout << "[SIM_POWER] woke up from sleep\n";
    return Status::OK;
}

void SimPower::set_battery_percent(uint8_t pct) {
    battery_pct_ = pct;
    std::cout << "[SIM_POWER] battery set to "
              << (int)pct << "%\n";
}

void SimPower::inject_brownout() {
    brownout_ = true;
    std::cout << "[SIM_POWER] BROWNOUT injected\n";
}

// ============================================================
// SimIO
// ============================================================
ButtonState SimIO::read_button(ButtonId button) {
    auto it = button_states_.find((int)button);
    if (it == button_states_.end())
        return ButtonState::RELEASED;
    return it->second;
}

Status SimIO::set_led(LedId led, LedPattern pattern) {
    current_led = pattern;
    const char* p = "UNKNOWN";
    switch(pattern) {
        case LedPattern::OFF:          p = "OFF";          break;
        case LedPattern::SOLID_GREEN:  p = "SOLID_GREEN";  break;
        case LedPattern::SOLID_ORANGE: p = "SOLID_ORANGE"; break;
        case LedPattern::SOLID_BLUE:   p = "SOLID_BLUE";   break;
        case LedPattern::BLINK_SLOW:   p = "BLINK_SLOW";   break;
        case LedPattern::BLINK_FAST:   p = "BLINK_FAST";   break;
    }
    std::cout << "[SIM_IO] LED → " << p << "\n";
    return Status::OK;
}

Status SimIO::play_buzzer(BuzzerPattern pattern) {
    current_buzzer = pattern;
    const char* p = "UNKNOWN";
    switch(pattern) {
        case BuzzerPattern::OFF:          p = "OFF";          break;
        case BuzzerPattern::BEEP_SHORT:   p = "BEEP_SHORT";   break;
        case BuzzerPattern::BEEP_LONG:    p = "BEEP_LONG";    break;
        case BuzzerPattern::BEEP_WARNING: p = "BEEP_WARNING"; break;
    }
    std::cout << "[SIM_IO] BUZZER → " << p << "\n";
    return Status::OK;
}

void SimIO::press_button(ButtonId button) {
    button_states_[(int)button] = ButtonState::PRESSED;
    std::cout << "[SIM_IO] button PRESSED\n";
}

void SimIO::release_button(ButtonId button) {
    button_states_[(int)button] = ButtonState::RELEASED;
    std::cout << "[SIM_IO] button RELEASED\n";
}

void SimIO::long_press_button(ButtonId button) {
    button_states_[(int)button] = ButtonState::LONG_PRESSED;
    std::cout << "[SIM_IO] button LONG_PRESSED\n";
}

// ============================================================
// SimSecurity
// ============================================================
Status SimSecurity::random_bytes(Span<uint8_t> out) {
    for (size_t i = 0; i < out.len; i++)
        out.data[i] = rng_() & 0xFF;
    return Status::OK;
}

Status SimSecurity::sha256(Span<const uint8_t> data, Hash* out) {
    // Fake hash — real SHA256 nahi, sirf simulation ke liye
    if (!out) return Status::ERR_INVALID;
    memset(out->bytes, 0, 32);
    for (size_t i = 0; i < data.len; i++)
        out->bytes[i % 32] ^= data.data[i];
    std::cout << "[SIM_SECURITY] sha256 computed (simulated)\n";
    return Status::OK;
}

Status SimSecurity::verify_signature(PublicKeyId key,
                                      Span<const uint8_t> data,
                                      Signature sig) {
    if (force_signature_fail) {
        std::cout << "[SIM_SECURITY] signature INVALID"
                  << " (fault injected)\n";
        return Status::ERR_SECURITY;
    }
    std::cout << "[SIM_SECURITY] signature VALID (simulated)\n";
    return Status::OK;
}

Status SimSecurity::read_secure_secret(SecretId id,
                                        Span<uint8_t> out) {
    // Fake device token
    std::string token = "sim_device_token_dev_123";
    size_t copy_len   = std::min(out.len, token.size());
    memcpy(out.data, token.data(), copy_len);
    std::cout << "[SIM_SECURITY] read_secure_secret id="
              << id << "\n";
    return Status::OK;
}