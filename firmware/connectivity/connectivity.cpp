#include "connectivity.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

ConnectivityManager::ConnectivityManager(INetwork* network) : network_(network) {}

Status ConnectivityManager::initialize(const WifiCredentials& creds) {
    credentials_ = creds;
    Status status = network_->connect(creds);
    retry_count_ = status == Status::OK ? 0 : 1;
    return status;
}

NetworkState ConnectivityManager::get_current_state() const {
    return network_->state();
}

void ConnectivityManager::check_and_reconnect() {
    if (network_->state() == NetworkState::DISCONNECTED) {
        apply_backoff_policy();
        if (network_->connect(credentials_) == Status::OK) {
            retry_count_ = 0;
        }
    }
}

Status ConnectivityManager::send_telemetry_batch(Span<const uint8_t> payload, HttpResponse* out_response) {
    Endpoint endpoint{"http://localhost:5000/telemetry"};
    Headers headers{"dev_123", "sim_device_token_dev_123"};

    // Keep retries bounded so a caller can cancel between attempts.
    Status status = Status::ERR_TIMEOUT;
    for (int attempt = 0; attempt < 3; ++attempt) {
        status = network_->post_json(
            endpoint, headers, payload, out_response);
        if (status == Status::OK) return status;
        if (attempt < 2)
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100 * (1 << attempt)));
    }
    return status;
}

void ConnectivityManager::apply_backoff_policy() {
    retry_count_++;
    int delay_ms = std::min(30000, 1000 * (1 << std::min(retry_count_, 5)));
    std::cout << "[CONNECTIVITY] reconnect attempt=" << retry_count_
              << " backoff_ms=" << delay_ms << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}
