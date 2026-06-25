#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <sstream>

#include "../hal/include/hal.h"
#include "../hal/sim/hal_sim.h"
#include "../event_runtime/event_queue.h"

// ============================================================
// Global State
// ============================================================
static HAL hal;
static EventQueue* g_queue = nullptr;

// Device info
static const std::string DEVICE_ID      = "dev_123";
static const std::string SESSION_ID     = "sess_001";
static const std::string FW_VERSION     = "0.1.0";

// System flags — tasks inhe check karte hain
static std::atomic<bool> g_running         {true};
static std::atomic<bool> g_connected       {false};
static std::atomic<bool> g_low_battery     {false};
static std::atomic<bool> g_ota_in_progress {false};
static std::atomic<bool> g_session_active  {false};

// Watchdog — har task apna timestamp update karta hai
static std::mutex        g_watchdog_mutex;
static std::map<std::string, uint64_t> g_watchdog_timestamps;

inline uint64_t now_ms() {
    return hal.time->monotonic_now();
}

inline void watchdog_kick(const std::string& task_name) {
    std::lock_guard<std::mutex> lock(g_watchdog_mutex);
    g_watchdog_timestamps[task_name] = now_ms();
}

// ============================================================
// Helper — Event banana
// ============================================================
Event make_event(EventType type,
                 std::map<std::string,std::string> payload = {}) {
    Event e;
    e.type                = type;
    e.device_id           = DEVICE_ID;
    e.student_session_id  = SESSION_ID;
    e.occurred_at_utc_ms  = hal.time->now_utc();
    e.monotonic_ms        = hal.time->monotonic_now();
    e.firmware_version    = FW_VERSION;
    e.battery_pct         = hal.power->battery_state().percent;
    e.payload             = payload;
    return e;
}

// ============================================================
// TASK 1: TelemetryTask
// Button press detect karo → event banao → queue mein daalo
// ============================================================
void TelemetryTask() {
    std::cout << "[TELEMETRY] task started\n";

    // Session start event
    Event session_start = make_event(EventType::SESSION_STARTED);
    g_queue->enqueue(session_start);
    g_session_active = true;

    ButtonState last_state = ButtonState::RELEASED;
    uint64_t    last_activity_ms = now_ms();
    int         help_count = 0;
    uint64_t    help_window_start = now_ms();

    while (g_running) {
        watchdog_kick("TelemetryTask");

        // Button check
        ButtonState current = hal.io->read_button(
            ButtonId::HELP_BUTTON);

        if (current == ButtonState::PRESSED &&
            last_state == ButtonState::RELEASED) {

            // Help requested event
            Event e = make_event(EventType::HELP_REQUESTED,
                {{"source", "button"},
                 {"press_duration_ms", "500"}});
            g_queue->enqueue(e);
            hal.io->set_led(LedId::STATUS_LED,
                            LedPattern::BLINK_FAST);
            hal.io->play_buzzer(BuzzerPattern::BEEP_SHORT);

            help_count++;
            last_activity_ms = now_ms();

            // Rules engine — repeated help requests?
            uint64_t window_elapsed = now_ms() - help_window_start;
            if (help_count >= 3 && window_elapsed <= 300000) {
                Event derived = make_event(
                    EventType::REPEATED_HELP_REQUESTS,
                    {{"count",    std::to_string(help_count)},
                     {"window_ms",std::to_string(window_elapsed)}});
                g_queue->enqueue(derived);
                std::cout << "[TELEMETRY] derived: "
                          << "repeated_help_requests\n";
            }
        }

        // Button pressed (generic)
        if (current == ButtonState::PRESSED &&
            last_state == ButtonState::RELEASED) {
            Event e = make_event(EventType::BUTTON_PRESSED,
                {{"button_id", "HELP_BUTTON"}});
            g_queue->enqueue(e);
        }

        last_state = current;

        // Inactivity check — 10 seconds for simulation
        if (g_session_active &&
            (now_ms() - last_activity_ms) > 10000) {
            Event e = make_event(EventType::INACTIVITY_DETECTED,
                {{"idle_ms",
                  std::to_string(now_ms() - last_activity_ms)}});
            g_queue->enqueue(e);
            last_activity_ms = now_ms(); // reset

            // Engagement drop check
            Event drop = make_event(EventType::ENGAGEMENT_DROP,
                {{"reason", "inactivity"}});
            g_queue->enqueue(drop);
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));
    }

    // Session end
    Event session_end = make_event(EventType::SESSION_ENDED);
    g_queue->enqueue(session_end);
    g_session_active = false;
    std::cout << "[TELEMETRY] task stopped\n";
}

// ============================================================
// TASK 2: SyncTask
// Queue se events nikalo → Flask server pe bhejo
// ============================================================
void SyncTask() {
    std::cout << "[SYNC] task started\n";

    while (g_running) {
        watchdog_kick("SyncTask");

        if (!g_connected) {
            std::cout << "[SYNC] offline — buffering ("
                      << g_queue->pending_count()
                      << " pending)\n";
            std::this_thread::sleep_for(
                std::chrono::milliseconds(2000));
            continue;
        }

        auto pending = g_queue->get_pending(5);
        if (pending.empty()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
            continue;
        }

        std::cout << "[SYNC] uploading "
                  << pending.size() << " events\n";

        for (auto& event : pending) {
            // JSON payload banana
            std::string body = "{";
            body += "\"schema_version\":\"device_event.v1\",";
            body += "\"device_id\":\"" + event.device_id + "\",";
            body += "\"event_id\":\""
                 + std::to_string(event.event_id) + "\",";
            body += "\"sequence_number\":"
                 + std::to_string(event.sequence_number) + ",";
            body += "\"event_type\":\""
                 + std::string(event_type_str(event.type)) + "\",";
            body += "\"diagnostics\":{";
            body += "\"battery_pct\":"
                 + std::to_string(event.battery_pct) + ",";
            body += "\"firmware_version\":\""
                 + event.firmware_version + "\"}}";

            Span<const uint8_t> span{
                reinterpret_cast<const uint8_t*>(body.data()),
                body.size()};

            Headers headers;
            headers.device_id    = DEVICE_ID;
            headers.device_token = "sim_device_token_dev_123";

            Endpoint endpoint;
            endpoint.url = "http://localhost:5000/telemetry";

            HttpResponse resp;
            Status s = hal.network->post_json(
                endpoint, headers, span, &resp);

            if (s == Status::OK && resp.status_code == 200) {
                g_queue->mark_committed(event.event_id);
            } else {
                std::cout << "[SYNC] upload FAILED event_id="
                          << event.event_id
                          << " — will retry\n";
                break; // Retry baad mein
            }
        }

        // Compact after successful batch
        g_queue->compact();

        std::this_thread::sleep_for(
            std::chrono::milliseconds(500));
    }
    std::cout << "[SYNC] task stopped\n";
}

// ============================================================
// TASK 3: DiagnosticsTask
// Health metrics log karo
// ============================================================
void DiagnosticsTask() {
    std::cout << "[DIAGNOSTICS] task started\n";

    while (g_running) {
        watchdog_kick("DiagnosticsTask");

        auto battery = hal.power->battery_state();
        uint64_t uptime = now_ms();

        std::cout << "[DIAGNOSTICS] uptime="
                  << uptime << "ms"
                  << " battery=" << (int)battery.percent << "%"
                  << " pending=" << g_queue->pending_count()
                  << " connected=" << g_connected.load()
                  << " ota=" << g_ota_in_progress.load()
                  << "\n";

        std::this_thread::sleep_for(
            std::chrono::milliseconds(5000));
    }
    std::cout << "[DIAGNOSTICS] task stopped\n";
}

// ============================================================
// TASK 4: PowerTask
// Battery monitor karo, states manage karo
// ============================================================
void PowerTask() {
    std::cout << "[POWER] task started\n";

    while (g_running) {
        watchdog_kick("PowerTask");

        auto battery = hal.power->battery_state();

        if (battery.percent <= 20 && !g_low_battery) {
            g_low_battery = true;
            Event e = make_event(EventType::POWER_LOW,
                {{"battery_pct",
                  std::to_string(battery.percent)}});
            g_queue->enqueue(e);
            hal.io->set_led(LedId::STATUS_LED,
                            LedPattern::BLINK_SLOW);
            hal.io->play_buzzer(BuzzerPattern::BEEP_WARNING);
            std::cout << "[POWER] LOW BATTERY — "
                      << (int)battery.percent << "%\n";

            // OTA pause karo low battery mein
            if (g_ota_in_progress) {
                std::cout << "[POWER] pausing OTA"
                          << " due to low battery\n";
            }
        }

        if (battery.brownout_risk) {
            std::cout << "[POWER] BROWNOUT risk detected!\n";
        }

        if (battery.percent > 20 && g_low_battery) {
            g_low_battery = false;
            std::cout << "[POWER] battery recovered\n";
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(2000));
    }
    std::cout << "[POWER] task stopped\n";
}

// ============================================================
// TASK 5: UiTask
// Device state ke hisaab se LED aur buzzer chalao
// ============================================================
void UiTask() {
    std::cout << "[UI] task started\n";

    while (g_running) {
        watchdog_kick("UiTask");

        if (g_low_battery) {
            hal.io->set_led(LedId::STATUS_LED,
                            LedPattern::BLINK_SLOW);
        } else if (g_ota_in_progress) {
            hal.io->set_led(LedId::STATUS_LED,
                            LedPattern::SOLID_BLUE);
        } else if (g_connected) {
            hal.io->set_led(LedId::STATUS_LED,
                            LedPattern::SOLID_GREEN);
        } else {
            hal.io->set_led(LedId::STATUS_LED,
                            LedPattern::SOLID_ORANGE);
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(200));
    }
    std::cout << "[UI] task stopped\n";
}

// ============================================================
// TASK 6: OtaTask
// Update check karo, download karo, verify karo, boot karo
// ============================================================
void OtaTask() {
    std::cout << "[OTA] task started\n";

    while (g_running) {
        watchdog_kick("OtaTask");

        // Low battery mein OTA mat karo
        if (g_low_battery) {
            std::cout << "[OTA] skipping check — low battery\n";
            std::this_thread::sleep_for(
                std::chrono::milliseconds(30000));
            continue;
        }

        if (!g_connected) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(30000));
            continue;
        }

        // Check for update
        Headers headers;
        headers.device_id    = DEVICE_ID;
        headers.device_token = "sim_device_token_dev_123";

        Endpoint endpoint;
        endpoint.url = "http://localhost:5000/ota/check";

        HttpResponse resp;
        std::string body = "{\"version\":\"" + FW_VERSION + "\"}";
        Span<const uint8_t> span{
            reinterpret_cast<const uint8_t*>(body.data()),
            body.size()};

        Status s = hal.network->post_json(
            endpoint, headers, span, &resp);

        if (s == Status::OK && resp.status_code == 200) {
            if (resp.body.find("no_update") == std::string::npos) {
                std::cout << "[OTA] update available: "
                          << resp.body << "\n";

                g_ota_in_progress = true;
                Event started = make_event(EventType::OTA_STARTED);
                g_queue->enqueue(started);

                // Simulate download + verify
                std::cout << "[OTA] downloading chunks...\n";
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(2000));

                // Signature verify
                uint8_t fake_data[] = {0x01, 0x02, 0x03};
                Signature fake_sig  = {};
                Status verify = hal.security->verify_signature(
                    0,
                    Span<const uint8_t>{fake_data, 3},
                    fake_sig);

                if (verify == Status::OK) {
                    std::cout << "[OTA] signature valid"
                              << " — ready to boot\n";
                    Event success = make_event(
                        EventType::OTA_SUCCEEDED);
                    g_queue->enqueue(success);
                } else {
                    std::cout << "[OTA] signature INVALID"
                              << " — rollback\n";
                    Event failed = make_event(
                        EventType::OTA_FAILED,
                        {{"reason", "invalid_signature"}});
                    g_queue->enqueue(failed);
                }

                g_ota_in_progress = false;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(30000));
    }
    std::cout << "[OTA] task stopped\n";
}

// ============================================================
// Boot Sequence
// ============================================================
bool boot_sequence() {
    std::cout << "\n=== GenEd Companion Firmware v"
              << FW_VERSION << " ===\n";
    std::cout << "[BOOT] starting...\n";

    // Step 1: HAL init
    hal = create_sim_hal();
    std::cout << "[BOOT] HAL initialized\n";

    // Step 2: Event queue init + recover
    static EventQueue queue(hal.storage);
    g_queue = &queue;
    Status s = g_queue->recover();
    if (s != Status::OK) {
        std::cout << "[BOOT] queue recovery FAILED\n";
        return false;
    }
    std::cout << "[BOOT] event queue recovered\n";

    // Step 3: Network connect
    WifiCredentials creds{"GenEd_WiFi", "password123"};
    s = hal.network->connect(creds);
    if (s == Status::OK) {
        g_connected = true;
        std::cout << "[BOOT] network connected\n";
    } else {
        std::cout << "[BOOT] network unavailable"
                  << " — buffering mode\n";
    }

    // Step 4: Time sync
    hal.time->sync_time();

    // Step 5: Boot event
    Event reboot = make_event(EventType::DEVICE_REBOOTED,
        {{"reason", "power_on"}});
    g_queue->enqueue(reboot);

    std::cout << "[BOOT] boot sequence complete\n\n";
    return true;
}

// ============================================================
// Main
// ============================================================
int main() {
    if (!boot_sequence()) {
        std::cerr << "[MAIN] boot failed — exiting\n";
        return 1;
    }

    // Start all 6 tasks
    std::thread t1(TelemetryTask);
    std::thread t2(SyncTask);
    std::thread t3(DiagnosticsTask);
    std::thread t4(PowerTask);
    std::thread t5(UiTask);
    std::thread t6(OtaTask);

    std::cout << "[MAIN] all tasks running."
              << " Press Enter to stop.\n";
    std::cin.get();

    // Graceful shutdown
    g_running = false;

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();

    std::cout << "[MAIN] shutdown complete\n";
    return 0;
}