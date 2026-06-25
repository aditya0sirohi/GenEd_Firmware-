#pragma once
#include "../hal/include/hal.h"
#include <string>

enum class OtaState {
    IDLE,
    DOWNLOADING,
    VERIFYING,
    APPLYING,
    PENDING_REBOOT,
    ROLLBACK
};

class OtaManager {
public:
    OtaManager(INetwork* network, IStorage* storage, ISecurity* crypto);

    // Boot lifecycle
    Status validate_current_boot();
    
    // Update flow
    Status check_for_updates(const std::string& current_version);
    Status begin_download(const std::string& update_url);
    Status verify_signature(PublicKeyId key);
    Status mark_slot_active_and_reboot();
    
    // Recovery
    Status trigger_rollback();

    OtaState get_state() const { return current_state_; }

private:
    INetwork* network_;
    IStorage* storage_;
    ISecurity* crypto_;
    
    OtaState current_state_ = OtaState::IDLE;
    size_t downloaded_bytes_ = 0;
    
    // Active vs Inactive boot slots (A/B OTA)
    int current_active_slot_ = 0; 
};