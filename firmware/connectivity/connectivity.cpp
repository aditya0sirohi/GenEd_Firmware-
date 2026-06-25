#include "connectivity.h"
#include <iostream>

ConnectivityManager::ConnectivityManager(INetwork* network) : network_(network) {}

Status ConnectivityManager::initialize(const WifiCredentials& creds) {
    credentials_ = creds;
    std::cout << "[CONNECTIVITY Stub] Initializing with SSID: " << creds.ssid << "\n";
    // TODO: Trigger initial connection attempt
    return network_->connect(creds);
}

NetworkState ConnectivityManager::get_current_state() const {
    return network_->state();
}

void ConnectivityManager::check_and_reconnect() {
    if (network_->state() == NetworkState::DISCONNECTED) {
        std::cout << "[CONNECTIVITY Stub] Disconnected. Attempting reconnect...\n";
        // TODO: Implement exponential backoff retry logic here using apply_backoff_policy()
        apply_backoff_policy();
        network_->connect(credentials_);
    }
}

Status ConnectivityManager::send_telemetry_batch(Span<const uint8_t> payload, HttpResponse* out_response) {
    // TODO: Implement TLS wrapper and robust retry policies for HTTP POST
    Endpoint endpoint{"http://localhost:5000/telemetry"};
    Headers headers{"dev_123", "sim_device_token_dev_123"};
    
    return network_->post_json(endpoint, headers, payload, out_response);
}

void ConnectivityManager::apply_backoff_policy() {
    // TODO: Increment retry_count_ and calculate exponential backoff delay (e.g., 2s, 4s, 8s, up to 30s)
    retry_count_++;
}