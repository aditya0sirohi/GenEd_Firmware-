#include "power_manager.h"
#include <iostream>

PowerManager::PowerManager(IPower* power_hal) : power_(power_hal) {}

Status PowerManager::evaluate_state(uint64_t idle_time_ms, bool is_connected) {
    BatteryState bat = power_->battery_state();

    SystemPowerState next = SystemPowerState::ACTIVE;
    if (bat.brownout_risk) {
        next = SystemPowerState::RECOVERY;
    } else if (bat.percent <= 20) {
        next = SystemPowerState::LOW_BATTERY;
    } else if (!is_connected) {
        next = SystemPowerState::DISCONNECTED_BUFFERING;
    } else if (idle_time_ms >= 600000) {
        next = SystemPowerState::CONNECTED_IDLE;
    }

    if (next != current_state_) {
        std::cout << "[POWER] state " << static_cast<int>(current_state_)
                  << " -> " << static_cast<int>(next) << "\n";
        current_state_ = next;
    }
    return Status::OK;
}

SystemPowerState PowerManager::get_state() const {
    return current_state_;
}

Status PowerManager::enter_sleep(SleepMode mode, Duration max_duration) {
    std::cout << "[POWER] preparing system for sleep mode\n";
    current_state_ = SystemPowerState::SLEEP;
    // EventQueue appends synchronously, so no queued storage write needs
    // flushing in the current host model.
    return power_->enter_sleep(mode, max_duration);
}
