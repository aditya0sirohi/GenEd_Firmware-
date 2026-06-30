#include "ota.h"
#include <iostream>
#include <vector>

OtaManager::OtaManager(INetwork* network, IStorage* storage, ISecurity* crypto) 
    : network_(network), storage_(storage), crypto_(crypto) {}

Status OtaManager::validate_current_boot() {
    uint8_t pending_slot = 0;
    Status status = storage_->read_blob(
        "ota_pending_slot", {&pending_slot, 1});
    if (status == Status::OK) {
        current_state_ = OtaState::PENDING_CONFIRM;
        std::cout << "[OTA] boot is pending health confirmation"
                  << " slot=" << static_cast<int>(pending_slot) << "\n";
    } else if (status != Status::ERR_NOT_FOUND) {
        return status;
    }
    return Status::OK;
}

Status OtaManager::check_for_updates(const std::string& current_version) {
    std::string request = "{\"version\":\"" + current_version + "\"}";
    HttpResponse response;
    Status status = network_->post_json(
        {"http://localhost:5000/ota/check"}, {"dev_123", "sim_token"},
        {reinterpret_cast<const uint8_t*>(request.data()), request.size()},
        &response);
    if (status != Status::OK) return status;
    return response.status_code == 200 ? Status::OK : Status::ERR_HARDWARE;
}

Status OtaManager::begin_download(const std::string& update_url) {
    current_state_ = OtaState::DOWNLOADING;
    // The host model stores a deterministic stand-in image. The ESP32 HAL
    // will replace this with streamed partition writes.
    std::string image = "simulated_firmware_image:" + update_url;
    downloaded_bytes_ = image.size();
    Status status = storage_->write_blob_atomic(
        "ota_inactive_image",
        {reinterpret_cast<const uint8_t*>(image.data()), image.size()});
    if (status != Status::OK) {
        current_state_ = OtaState::FAILED;
        downloaded_bytes_ = 0;
    }
    return status;
}

Status OtaManager::verify_signature(PublicKeyId key) {
    current_state_ = OtaState::VERIFYING;
    if (downloaded_bytes_ == 0) {
        current_state_ = OtaState::FAILED;
        return Status::ERR_INVALID;
    }

    std::vector<uint8_t> image(downloaded_bytes_);
    Status status = storage_->read_blob(
        "ota_inactive_image", {image.data(), image.size()});
    if (status != Status::OK) {
        current_state_ = OtaState::FAILED;
        return status;
    }

    Hash hash{};
    status = crypto_->sha256({image.data(), image.size()}, &hash);
    if (status == Status::OK) {
        status = crypto_->verify_signature(
            key, {image.data(), image.size()}, image_signature_);
    }
    if (status != Status::OK) current_state_ = OtaState::FAILED;
    return status;
}

Status OtaManager::mark_slot_active_and_reboot() {
    if (current_state_ != OtaState::VERIFYING)
        return Status::ERR_INVALID;

    uint8_t pending_slot =
        static_cast<uint8_t>(current_active_slot_ == 0 ? 1 : 0);
    Status status = storage_->write_blob_atomic(
        "ota_pending_slot", {&pending_slot, 1});
    if (status == Status::OK)
        current_state_ = OtaState::PENDING_REBOOT;
    return status;
}

Status OtaManager::confirm_boot() {
    if (current_state_ != OtaState::PENDING_CONFIRM &&
        current_state_ != OtaState::PENDING_REBOOT)
        return Status::ERR_INVALID;

    uint8_t pending_slot = 0;
    Status status = storage_->read_blob(
        "ota_pending_slot", {&pending_slot, 1});
    if (status != Status::OK) return status;

    status = storage_->write_blob_atomic(
        "ota_active_slot", {&pending_slot, 1});
    if (status == Status::OK) {
        current_active_slot_ = pending_slot;
        current_state_ = OtaState::CONFIRMED;
    }
    return status;
}

Status OtaManager::trigger_rollback() {
    current_state_ = OtaState::ROLLBACK;
    uint8_t active_slot = static_cast<uint8_t>(current_active_slot_);
    return storage_->write_blob_atomic(
        "ota_pending_slot", {&active_slot, 1});
}
