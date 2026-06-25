#pragma once
#include "../hal/include/hal.h"

enum class SystemPowerState {
    ACTIVE,
    CONNECTED_IDLE,
    DISCONNECTED_BUFFERING,
    LOW_BATTERY,
    SLEEP,
    RECOVERY
};

class PowerManager {
public:
    PowerManager(IPower* power_hal);

    // Core state evaluation
    Status evaluate_state(uint64_t idle_time_ms, bool is_connected);
    SystemPowerState get_state() const;
    
    // Actions
    Status enter_sleep(SleepMode mode, Duration max_duration);

private:
    IPower* power_;
    SystemPowerState current_state_ = SystemPowerState::ACTIVE;
};