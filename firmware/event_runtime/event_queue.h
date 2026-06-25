#pragma once
#include "event.h"
#include "../hal/include/hal.h"
#include <vector>
#include <mutex>
#include <functional>

// ============================================================
// EventQueue — durable event queue
// Dil hai poore system ka — sab events yahan aate hain
// ============================================================
class EventQueue {
public:
    explicit EventQueue(IStorage* storage);

    // --------------------------------------------------------
    // Core operations
    // --------------------------------------------------------

    // Naya event daalo — ID assign hoti hai automatically
    // Storage mein likhta hai pehle, phir memory mein
    Status enqueue(Event& event);

    // Uncommitted events nikalo — SyncTask use karta hai
    std::vector<Event> get_pending(size_t max_count = 10);

    // Server ne acknowledge kiya — mark as committed
    Status mark_committed(uint64_t event_id);

    // Kitne events pending hain abhi
    size_t pending_count() const;

    // --------------------------------------------------------
    // Recovery — reboot ke baad storage se reload
    // --------------------------------------------------------
    Status recover();

    // --------------------------------------------------------
    // Compaction — committed events hata do space ke liye
    // --------------------------------------------------------
    Status compact();

    // --------------------------------------------------------
    // Diagnostics
    // --------------------------------------------------------
    uint32_t total_enqueued()  const { return total_enqueued_;  }
    uint32_t total_committed() const { return total_committed_; }
    uint64_t oldest_pending_age_ms(uint64_t now_ms) const;

private:
    IStorage*          storage_;
    std::vector<Event> queue_;
    mutable std::mutex mutex_;

    uint64_t next_event_id_     = 1;
    uint32_t next_sequence_     = 1;
    uint32_t total_enqueued_    = 0;
    uint32_t total_committed_   = 0;

    // Max queue size — 512KB RAM budget ke hisaab se
    static constexpr size_t MAX_QUEUE_SIZE = 100;

    // Event → bytes (storage ke liye)
    std::vector<uint8_t> serialize(const Event& e) const;

    // Bytes → Event (recovery ke liye)
    bool deserialize(const uint8_t* data,
                     size_t len,
                     Event& out) const;
};