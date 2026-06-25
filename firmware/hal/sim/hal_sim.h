#pragma once
#include "../include/hal.h"
#include <map>
#include <vector>
#include <mutex>
#include <random>
#include <chrono>
#include <string>

// ============================================================
// SimStorage — in-memory flash simulation
// ============================================================
class SimStorage : public IStorage {
public:
    bool corrupt_next_write = false;

    Status read_blob(Key key, Span<uint8_t> out) override;
    Status write_blob_atomic(Key key, Span<const uint8_t> data) override;
    Status append_record(LogId log, Span<const uint8_t> record) override;
    Status scan_records(LogId log, IRecordVisitor& visitor) override;
    Status mark_record_committed(LogId log, RecordId id) override;
    Status inject_corruption_for_test(Key key, CorruptionMode mode) override;

private:
    std::mutex mutex_;
    std::map<std::string, std::vector<uint8_t>> blobs_;
    struct Record {
        RecordId id;
        std::vector<uint8_t> data;
        bool committed = false;
    };
    std::map<LogId, std::vector<Record>> logs_;
    RecordId next_id_ = 1;
    uint32_t write_count_ = 0;
};

// ============================================================
// SimNetwork — fake WiFi + HTTP
// ============================================================
class SimNetwork : public INetwork {
public:
    Status       connect(WifiCredentials credentials) override;
    NetworkState state() const override;
    Status       post_json(Endpoint endpoint, Headers headers,
                           Span<const uint8_t> body,
                           HttpResponse* out) override;
    void         set_fault_profile(NetworkFaultProfile profile) override;

    void force_disconnect();
    void force_connect();

private:
    NetworkState       state_ = NetworkState::DISCONNECTED;
    NetworkFaultProfile fault_ = {0.0f, 5000, false};
};

// ============================================================
// SimTime — system clock
// ============================================================
class SimTime : public ITime {
public:
    Timestamp     now_utc() override;
    MonotonicTime monotonic_now() override;
    Status        sync_time() override;

private:
    std::chrono::steady_clock::time_point boot_time_ =
        std::chrono::steady_clock::now();
};

// ============================================================
// SimPower — fake battery
// ============================================================
class SimPower : public IPower {
public:
    BatteryState battery_state() override;
    WakeReason   wake_reason() override;
    Status       enter_sleep(SleepMode mode,
                             Duration max_duration) override;

    void set_battery_percent(uint8_t pct);
    void inject_brownout();

private:
    uint8_t    battery_pct_  = 85;
    bool       brownout_     = false;
    WakeReason wake_reason_  = WakeReason::RESET;
};

// ============================================================
// SimIO — fake button + LED + buzzer
// ============================================================
class SimIO : public IIO {
public:
    ButtonState read_button(ButtonId button) override;
    Status      set_led(LedId led, LedPattern pattern) override;
    Status      play_buzzer(BuzzerPattern pattern) override;

    void press_button(ButtonId button);
    void release_button(ButtonId button);
    void long_press_button(ButtonId button);

    LedPattern    current_led     = LedPattern::OFF;
    BuzzerPattern current_buzzer  = BuzzerPattern::OFF;

private:
    std::map<int, ButtonState> button_states_;
};

// ============================================================
// SimSecurity — fake crypto
// ============================================================
class SimSecurity : public ISecurity {
public:
    bool force_signature_fail = false;

    Status random_bytes(Span<uint8_t> out) override;
    Status sha256(Span<const uint8_t> data, Hash* out) override;
    Status verify_signature(PublicKeyId key,
                            Span<const uint8_t> data,
                            Signature sig) override;
    Status read_secure_secret(SecretId id,
                              Span<uint8_t> out) override;
private:
    std::mt19937 rng_{42};
};

// ============================================================
// Factory + global accessors for fault injection
// ============================================================
HAL         create_sim_hal();
SimStorage*  get_sim_storage();
SimNetwork*  get_sim_network();
SimPower*    get_sim_power();
SimIO*       get_sim_io();
SimSecurity* get_sim_security();