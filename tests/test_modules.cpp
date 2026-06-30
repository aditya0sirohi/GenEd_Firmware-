#include "../firmware/diagnostics/diagnostics.h"
#include "../firmware/hal/sim/hal_sim.h"
#include "../firmware/ota/ota.h"
#include "../firmware/power/power_manager.h"
#include "../firmware/rules_engine/rules_engine.h"
#include <iostream>
#include <string>

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << message << "\n";
    if (!condition) failures++;
}

Event event_at(EventType type, uint64_t time_ms) {
    Event event;
    event.type = type;
    event.monotonic_ms = time_ms;
    event.occurred_at_utc_ms = time_ms;
    event.device_id = "test_device";
    event.student_session_id = "test_session";
    return event;
}

void test_rules_engine() {
    RulesEngine rules;
    rules.process_primary_event(event_at(EventType::SESSION_STARTED, 0));
    rules.process_primary_event(event_at(EventType::HELP_REQUESTED, 1000));
    rules.process_primary_event(event_at(EventType::HELP_REQUESTED, 2000));
    rules.process_primary_event(event_at(EventType::HELP_REQUESTED, 3000));

    auto repeated = rules.get_and_clear_derived_events();
    expect(repeated.size() == 1 &&
           repeated[0].type == EventType::REPEATED_HELP_REQUESTS,
           "three help requests produce one repeated-help event");

    rules.process_primary_event(
        event_at(EventType::INACTIVITY_DETECTED, 603000));
    auto inactive = rules.get_and_clear_derived_events();
    expect(inactive.size() == 2,
           "inactivity after repeated help produces drop and struggle");
    if (inactive.size() == 2) {
        expect(inactive[0].type == EventType::ENGAGEMENT_DROP,
               "first inactivity result is engagement drop");
        expect(inactive[1].type == EventType::STRUGGLE_DETECTED,
               "second inactivity result is struggle detected");
    }
}

void test_power_manager() {
    SimPower power;
    PowerManager manager(&power);

    manager.evaluate_state(0, false);
    expect(manager.get_state() ==
               SystemPowerState::DISCONNECTED_BUFFERING,
           "disconnection selects buffering power state");

    manager.evaluate_state(600000, true);
    expect(manager.get_state() == SystemPowerState::CONNECTED_IDLE,
           "long idle period selects connected-idle state");

    power.inject_brownout();
    manager.evaluate_state(0, true);
    expect(manager.get_state() == SystemPowerState::RECOVERY,
           "brownout risk selects recovery state");
}

void test_diagnostics() {
    SimStorage storage;
    DiagnosticsFramework diagnostics(&storage);
    diagnostics.add_trace_entry("test", "started");
    diagnostics.add_trace_entry("test", "completed");
    std::string json = diagnostics.export_compact_json();
    expect(json.find("\"trace_count\":2") != std::string::npos,
           "diagnostics exports trace count");
    expect(json.find("completed") != std::string::npos,
           "diagnostics exports recent traces");

    expect(diagnostics.record_crash("watchdog", "SyncTask") == Status::OK,
           "crash record is persisted");
    uint8_t buffer[128]{};
    expect(storage.read_blob("last_crash", {buffer, sizeof(buffer)}) ==
               Status::OK,
           "crash record can be recovered");
}

void test_ota_host_model() {
    SimNetwork network;
    SimStorage storage;
    SimSecurity security;
    network.connect({"test_wifi", "test_password"});

    OtaManager ota(&network, &storage, &security);
    expect(ota.begin_download("http://test/image.bin") == Status::OK,
           "OTA host model stores inactive image");
    expect(ota.verify_signature(1) == Status::OK,
           "OTA host model verifies image");
    expect(ota.mark_slot_active_and_reboot() == Status::OK,
           "OTA writes pending slot atomically");
    expect(ota.confirm_boot() == Status::OK && ota.active_slot() == 1,
           "healthy boot confirms inactive slot");

    SimStorage bad_storage;
    SimSecurity bad_security;
    bad_security.force_signature_fail = true;
    OtaManager bad_ota(&network, &bad_storage, &bad_security);
    bad_ota.begin_download("http://test/bad.bin");
    expect(bad_ota.verify_signature(1) == Status::ERR_SECURITY &&
               bad_ota.get_state() == OtaState::FAILED,
           "bad OTA signature is rejected");
}
} // namespace

int main() {
    test_rules_engine();
    test_power_manager();
    test_diagnostics();
    test_ota_host_model();
    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "All module tests passed\n";
    return 0;
}
