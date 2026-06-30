#include "../firmware/event_runtime/event_queue.h"
#include "../firmware/hal/sim/hal_sim.h"
#include <iostream>
#include <string>

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
    if (condition) {
        std::cout << "[PASS] " << message << "\n";
    } else {
        std::cerr << "[FAIL] " << message << "\n";
        failures++;
    }
}

Event make_test_event(EventType type = EventType::HELP_REQUESTED) {
    Event event;
    event.type = type;
    event.device_id = "test_device";
    event.student_session_id = "test_session";
    event.occurred_at_utc_ms = 1000;
    event.monotonic_ms = 100;
    event.correlation_id = 77;
    event.payload["source"] = "button";
    event.payload["value"] = "uses|reserved;characters=too";
    return event;
}

void test_enqueue_and_recover() {
    SimStorage storage;
    EventQueue queue(&storage);
    Event event = make_test_event();

    expect(queue.enqueue(event) == Status::OK, "enqueue succeeds");
    expect(event.event_id == 1, "stable event ID is assigned");
    expect(event.sequence_number == 1, "sequence number is assigned");

    EventQueue recovered(&storage);
    expect(recovered.recover() == Status::OK, "recovery succeeds");
    auto pending = recovered.get_pending();
    expect(pending.size() == 1, "unacknowledged event is recovered");
    if (!pending.empty()) {
        expect(pending[0].correlation_id == 77,
               "correlation ID survives recovery");
        expect(pending[0].payload == event.payload,
               "event payload survives recovery");
    }
}

void test_acknowledged_event_is_not_replayed() {
    SimStorage storage;
    EventQueue queue(&storage);
    Event event = make_test_event();

    expect(queue.enqueue(event) == Status::OK, "event for ack test enqueues");
    expect(queue.mark_committed(event.event_id) == Status::OK,
           "acknowledgement is persisted");

    EventQueue recovered(&storage);
    expect(recovered.recover() == Status::OK, "post-ack recovery succeeds");
    expect(recovered.pending_count() == 0,
           "acknowledged event is not replayed");
}

void test_failed_write_is_not_queued() {
    SimStorage storage;
    EventQueue queue(&storage);
    Event event = make_test_event();

    storage.inject_corruption_for_test(
        "event_log", CorruptionMode::PARTIAL_WRITE);
    expect(queue.enqueue(event) == Status::ERR_CORRUPT,
           "injected storage failure reaches caller");
    expect(queue.pending_count() == 0,
           "failed durable write is not added to RAM queue");
}

void test_recovery_is_idempotent() {
    SimStorage storage;
    EventQueue queue(&storage);
    Event event = make_test_event();
    queue.enqueue(event);

    expect(queue.recover() == Status::OK, "first recovery succeeds");
    expect(queue.recover() == Status::OK, "second recovery succeeds");
    expect(queue.pending_count() == 1,
           "repeated recovery does not duplicate events");
}
} // namespace

int main() {
    test_enqueue_and_recover();
    test_acknowledged_event_is_not_replayed();
    test_failed_write_is_not_queued();
    test_recovery_is_idempotent();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "All event queue tests passed\n";
    return 0;
}

