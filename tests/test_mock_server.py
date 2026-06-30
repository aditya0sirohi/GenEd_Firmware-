import unittest

from tools import mock_server


class MockServerTests(unittest.TestCase):
    def setUp(self):
        self.client = mock_server.app.test_client()
        self.client.post("/debug/reset")

    def test_duplicate_event_is_idempotent(self):
        event = {
            "device_id": "dev_test",
            "event_id": "evt_1",
            "sequence_number": 1,
            "event_type": "help_requested",
        }
        first = self.client.post("/telemetry", json=event)
        second = self.client.post("/telemetry", json=event)
        stored = self.client.get("/debug/events").get_json()

        self.assertEqual(first.status_code, 200)
        self.assertFalse(first.get_json()["duplicate"])
        self.assertTrue(second.get_json()["duplicate"])
        self.assertEqual(stored["total"], 1)

    def test_batch_returns_only_acknowledged_ids(self):
        events = [
            {"device_id": "dev_test", "event_id": str(index)}
            for index in range(1, 4)
        ]
        response = self.client.post(
            "/telemetry/batch", json={"events": events}
        )
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.get_json()["acknowledged"], ["1", "2"])


if __name__ == "__main__":
    unittest.main()

