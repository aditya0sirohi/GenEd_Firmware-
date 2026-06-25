#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

// ============================================================
// Core Types
// ============================================================

enum class Status {
    OK = 0,
    ERR_NOT_FOUND,
    ERR_CORRUPT,
    ERR_FULL,
    ERR_TIMEOUT,
    ERR_INVALID,
    ERR_HARDWARE,
    ERR_SECURITY,
    ERR_BUSY
};

// Simple span — pointer + length
template<typename T>
struct Span {
    T*     data;
    size_t len;
};

using Key      = const char*;
using LogId    = uint32_t;
using RecordId = uint64_t;

// ============================================================
// IStorage
// ============================================================

struct IRecordVisitor {
    virtual bool on_record(RecordId id,
                           Span<const uint8_t> data) = 0;
    virtual ~IRecordVisitor() = default;
};

enum class CorruptionMode {
    FLIP_BYTE,
    ZERO_SECTOR,
    PARTIAL_WRITE
};

class IStorage {
public:
    virtual Status read_blob(Key key,
                             Span<uint8_t> out) = 0;
    virtual Status write_blob_atomic(Key key,
                             Span<const uint8_t> data) = 0;
    virtual Status append_record(LogId log,
                             Span<const uint8_t> record) = 0;
    virtual Status scan_records(LogId log,
                             IRecordVisitor& visitor) = 0;
    virtual Status mark_record_committed(LogId log,
                             RecordId id) = 0;
    virtual Status inject_corruption_for_test(Key key,
                             CorruptionMode mode) = 0;
    virtual ~IStorage() = default;
};

// ============================================================
// INetwork
// ============================================================

struct WifiCredentials {
    std::string ssid;
    std::string password;
};

enum class NetworkState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DEGRADED,
    CAPTIVE_PORTAL
};

struct Endpoint {
    std::string url;
};

struct Headers {
    std::string device_id;
    std::string device_token;
};

struct HttpResponse {
    int         status_code;
    std::string body;
};

struct NetworkFaultProfile {
    float packet_loss_rate;   // 0.0 - 1.0
    int   timeout_ms;
    bool  force_disconnect;
};

class INetwork {
public:
    virtual Status       connect(WifiCredentials credentials) = 0;
    virtual NetworkState state() const = 0;
    virtual Status       post_json(Endpoint endpoint,
                                   Headers headers,
                                   Span<const uint8_t> body,
                                   HttpResponse* out) = 0;
    virtual void         set_fault_profile(
                                   NetworkFaultProfile profile) = 0;
    virtual ~INetwork() = default;
};

// ============================================================
// ITime
// ============================================================

using Timestamp    = uint64_t;  // Unix epoch ms
using MonotonicTime = uint64_t; // ms since boot

class ITime {
public:
    virtual Timestamp    now_utc() = 0;
    virtual MonotonicTime monotonic_now() = 0;
    virtual Status       sync_time() = 0;
    virtual ~ITime() = default;
};

// ============================================================
// IPower
// ============================================================

struct BatteryState {
    uint8_t percent;       // 0-100
    bool    charging;
    bool    brownout_risk;
};

enum class WakeReason {
    BUTTON_PRESS,
    RTC_TIMER,
    NETWORK_EVENT,
    RESET,
    UNKNOWN
};

enum class SleepMode {
    LIGHT,   // quick wake
    DEEP     // low power, slower wake
};

using Duration = uint32_t;  // milliseconds

class IPower {
public:
    virtual BatteryState battery_state() = 0;
    virtual WakeReason   wake_reason() = 0;
    virtual Status       enter_sleep(SleepMode mode,
                                     Duration max_duration) = 0;
    virtual ~IPower() = default;
};

// ============================================================
// IIO
// ============================================================

enum class ButtonId {
    HELP_BUTTON
};

enum class ButtonState {
    RELEASED,
    PRESSED,
    LONG_PRESSED
};

enum class LedId {
    STATUS_LED
};

enum class LedPattern {
    OFF,
    SOLID_GREEN,
    SOLID_ORANGE,
    SOLID_BLUE,
    BLINK_SLOW,
    BLINK_FAST
};

enum class BuzzerPattern {
    OFF,
    BEEP_SHORT,
    BEEP_LONG,
    BEEP_WARNING
};

class IIO {
public:
    virtual ButtonState read_button(ButtonId button) = 0;
    virtual Status      set_led(LedId led,
                                LedPattern pattern) = 0;
    virtual Status      play_buzzer(BuzzerPattern pattern) = 0;
    virtual ~IIO() = default;
};

// ============================================================
// ISecurity
// ============================================================

using PublicKeyId = uint32_t;
using SecretId    = uint32_t;

struct Hash {
    uint8_t bytes[32];  // SHA-256
};

struct Signature {
    uint8_t bytes[64];  // Ed25519
};

class ISecurity {
public:
    virtual Status random_bytes(Span<uint8_t> out) = 0;
    virtual Status sha256(Span<const uint8_t> data,
                          Hash* out) = 0;
    virtual Status verify_signature(PublicKeyId key,
                                    Span<const uint8_t> data,
                                    Signature sig) = 0;
    virtual Status read_secure_secret(SecretId id,
                                      Span<uint8_t> out) = 0;
    virtual ~ISecurity() = default;
};

// ============================================================
// HAL Bundle — sab ek jagah
// ============================================================

struct HAL {
    IStorage*  storage;
    INetwork*  network;
    ITime*     time;
    IPower*    power;
    IIO*       io;
    ISecurity* security;
};