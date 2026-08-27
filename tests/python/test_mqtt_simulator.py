import importlib.util
import json
from pathlib import Path

import pytest


ROOT = Path(__file__).parents[2]
spec = importlib.util.spec_from_file_location("mqtt_simulator", ROOT / "scripts" / "mqtt_simulator.py")
simulator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(simulator)


@pytest.mark.parametrize("scenario,terminal", [("success", "SUCCESS"), ("error", "ERROR"), ("rollback", "ROLLBACK")])
def test_scenario_emits_attributes_and_terminal_state(scenario, terminal):
    events = simulator.scenario_events(scenario)
    assert events[0][0] == simulator.ATTRIBUTES_TOPIC
    assert events[0][1]["active_slot"] == "A"
    assert events[-1][0] == simulator.TELEMETRY_TOPIC
    assert events[-1][1]["ota_state"] == terminal
    json.dumps(events[-1][1])


def test_dashboard_definition_contains_required_widgets():
    dashboard = json.loads((ROOT / "docs" / "thingsboard" / "dashboard.json").read_text())
    keys = {widget["key"] for widget in dashboard["widgets"]}
    assert {"active_slot", "app_version", "ota_state", "ota_progress", "ota_error"} <= keys
