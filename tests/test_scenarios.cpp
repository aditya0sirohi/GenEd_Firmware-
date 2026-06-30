#include "../firmware/event_runtime/event_queue.h"
#include "../firmware/hal/sim/hal_sim.h"
#include "../firmware/power/power_manager.h"
#include <iostream>
#include <string>

namespace {
Event make_event(EventType type, uint64_t time) {
    Event event;
    event.type = type;
    event.device_id = "scenario_device";
    event.student_session_id = "scenario_session";
    event.occurred_at_utc_ms = time;
    event.monotonic_ms = time;
    return event;
}

bool connectivity_loss() {
    SimStorage storage;
    SimNetwork network;
    EventQueue queue(&storage);
    network.connect({"test_wifi", "test_password"});

    Event first = make_event(EventType::SESSION_STARTED, 1);
    Event second = make_event(EventType::HELP_REQUESTED, 2);
    if (queue.enqueue(first) != Status::OK ||
        queue.enqueue(second) != Status::OK)
        return false;

    network.force_disconnect();
    HttpResponse response;
    std::string body = "{}";
    Status offline = network.post_json(
        {"http://test/telemetry"}, {"device", "token"},
        {reinterpret_cast<const uint8_t*>(body.data()), body.size()},
        &response);
    if (offline != Status::ERR_TIMEOUT || queue.pending_count() != 2)
        return false;

    network.force_connect();
    for (const Event& event : queue.get_pending()) {
        Status sent = network.post_json(
            {"http://test/telemetry"}, {"device", "token"},
            {reinterpret_cast<const uint8_t*>(body.data()), body.size()},
            &response);
        if (sent != Status::OK ||
            queue.mark_committed(event.event_id) != Status::OK)
            return false;
    }

    EventQueue after_reboot(&storage);
    return after_reboot.recover() == Status::OK &&
           after_reboot.pending_count() == 0;
}

bool partial_ack() {
    SimStorage storage;
    EventQueue queue(&storage);
    for (uint64_t i = 1; i <= 3; ++i) {
        Event event = make_event(EventType::HELP_REQUESTED, i);
        if (queue.enqueue(event) != Status::OK) return false;
    }

    // Simulate a server acknowledging only the first two event IDs.
    if (queue.mark_committed(1) != Status::OK ||
        queue.mark_committed(2) != Status::OK)
        return false;

    EventQueue after_reboot(&storage);
    if (after_reboot.recover() != Status::OK) return false;
    auto pending = after_reboot.get_pending();
    return pending.size() == 1 && pending[0].event_id == 3;
}

bool flash_corruption() {
    SimStorage storage;
    EventQueue queue(&storage);
    Event damaged = make_event(EventType::HELP_REQUESTED, 1);
    storage.inject_corruption_for_test(
        "event_log", CorruptionMode::PARTIAL_WRITE);
    if (queue.enqueue(damaged) != Status::ERR_CORRUPT ||
        queue.pending_count() != 0)
        return false;

    Event healthy = make_event(EventType::HELP_REQUESTED, 2);
    return queue.enqueue(healthy) == Status::OK &&
           queue.pending_count() == 1;
}

bool low_battery() {
    SimPower power;
    PowerManager manager(&power);
    power.set_battery_percent(15);
    manager.evaluate_state(0, true);
    if (manager.get_state() != SystemPowerState::LOW_BATTERY)
        return false;

    power.set_battery_percent(85);
    manager.evaluate_state(0, true);
    return manager.get_state() == SystemPowerState::ACTIVE;
}

bool bad_ota_signature() {
    SimSecurity security;
    security.force_signature_fail = true;
    uint8_t image[] = {1, 2, 3};
    Signature signature{};
    return security.verify_signature(
        0, {image, sizeof(image)}, signature) == Status::ERR_SECURITY;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: test_scenarios <scenario>\n";
        return 2;
    }

    std::string name = argv[1];
    bool passed = false;
    if (name == "connectivity_loss") passed = connectivity_loss();
    else if (name == "partial_ack") passed = partial_ack();
    else if (name == "flash_corruption") passed = flash_corruption();
    else if (name == "low_battery") passed = low_battery();
    else if (name == "bad_ota_signature") passed = bad_ota_signature();
    else {
        std::cerr << "Unknown scenario: " << name << "\n";
        return 2;
    }

    std::cout << "[SCENARIO] " << name << ": "
              << (passed ? "PASS" : "FAIL") << "\n";
    return passed ? 0 : 1;
}

