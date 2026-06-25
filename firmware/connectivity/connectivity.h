#pragma once
#include "../hal/include/hal.h"
#include "../event_runtime/event.h"
#include <string>

class ConnectivityManager {
public:
    ConnectivityManager(INetwork* network);

    // Core operations
    Status initialize(const WifiCredentials& creds);
    NetworkState get_current_state() const;
    void check_and_reconnect();

    // High-level request wrapper (handles retries and TLS abstractions)
    Status send_telemetry_batch(Span<const uint8_t> payload, HttpResponse* out_response);

private:
    INetwork* network_;
    WifiCredentials credentials_;
    int retry_count_ = 0;
    uint64_t last_attempt_ms_ = 0;

    void apply_backoff_policy();
};