#!/usr/bin/env python3
"""
Equivalence test for the variables of the Blueprint's "API Subscriptions" step.

Renders the variables of two versions of the Blueprint, up to `api_subscriptions`, with mocked Home Assistant
state and Blueprint inputs, and compares the resulting list: the subscriptions pushed to the panel (entity,
page, component, attribute, icon, icon color and inverted flag), in order. Meant for refactorings that must not
change what the panel subscribes to (issue #657): run it against the version before the change.

The cases cover the weather, outdoor and indoor temperature inputs (empty, single entity or list, visible or
not), the temperature calibration reference (empty, sensor, climate, the panel's own entities, with and without
the embedded thermostat), the wake-up sensors (none, a single entity or a list with gaps), and the chips and
custom buttons (missing, empty, invalid and valid entities, with and without icon and color).

Usage:
  test_api_subscriptions.py REFERENCE_BLUEPRINT [BLUEPRINT]

  REFERENCE_BLUEPRINT  Blueprint before the change, e.g. from
                       `git show origin/dev:nspanel_easy_blueprint.yaml > /tmp/reference.yaml`
  BLUEPRINT            Blueprint to test (default: nspanel_easy_blueprint.yaml)

Requires PyYAML and Jinja2, pinned in .github/requirements.txt (`pip install -r .github/requirements.txt`).
Exit code: 0 when both render the same, 1 on any difference.
"""

import random
import sys

import yaml

from test_variable_entity import MockHomeAssistant, MockState, find, render

STEP_ALIAS = "API Subscriptions"
GLOBALS_ALIAS = "Global variables"
RESULT_NAME = "api_subscriptions"

NSPANEL_TEMP = "sensor.nspanel_temperature"
THERMOSTAT_EMBEDDED = "climate.nspanel_thermostat"
WEATHER_ENTITY = "weather.home"

CASES = 4000
SEED = 657
MAX_REPORTED_DIFFERENCES = 20

# Values each kind of input takes in the cases
ENTITY_SAMPLES = (None, "", "invalid", "sensor.a", "binary_sensor.door", "light.kitchen", "climate.living")
ICON_SAMPLES = (None, "", "mdi:lightbulb", "mdi:not-an-icon")
COLOR_SAMPLES = (None, [], [255, 0, 0], [1, 2])
TEMP_ENTITY_SAMPLES = ("", "sensor.outdoor", ["sensor.outdoor"], [], "climate.living", ["climate.living"],
                       WEATHER_ENTITY)
CALIBRATION_SAMPLES = ("", "sensor.reference", ["sensor.reference"], [], "climate.reference",
                       ["climate.reference"], NSPANEL_TEMP, THERMOSTAT_EMBEDDED, "invalid")
WAKE_UP_SAMPLES = (None, [], "", "binary_sensor.motion", ["binary_sensor.motion"],
                   ["binary_sensor.motion", "", "binary_sensor.door"], ["", "", "binary_sensor.door"])


class InputRef:
    """An `!input` tag, resolved per case."""

    def __init__(self, name):
        self.name = name


class InputLoader(yaml.SafeLoader):
    """SafeLoader keeping `!input` tags as InputRef."""


InputLoader.add_multi_constructor("!", lambda loader, suffix, node: InputRef(loader.construct_scalar(node)))


def resolve(node, inputs):
    """Replace every InputRef under node with the case's value (None when not set, like an empty input)."""
    if isinstance(node, InputRef):
        return inputs.get(node.name)
    if isinstance(node, dict):
        return {key: resolve(value, inputs) for key, value in node.items()}
    if isinstance(node, list):
        return [resolve(value, inputs) for value in node]
    return node


def load(path):
    """Return the variables of the step, in order up to the result, and the global `home_page` variable."""
    with open(path, encoding="utf-8") as f:
        data = yaml.load(f, Loader=InputLoader)  # nosec B506 - SafeLoader subclass
    step = find(data.get("actions"), lambda d: d.get("alias") == STEP_ALIAS)
    global_step = find(data.get("actions"), lambda d: d.get("alias") == GLOBALS_ALIAS)
    if step is None or global_step is None:
        sys.exit(f"ERROR: '{STEP_ALIAS}' or '{GLOBALS_ALIAS}' not found in {path}")
    variables = []
    for item in step["sequence"]:
        found = item.get("variables") if isinstance(item, dict) else None
        if found is None:
            continue
        variables.extend(found.items())
        if RESULT_NAME in found:
            return variables, global_step["variables"]["home_page"]
    sys.exit(f"ERROR: '{RESULT_NAME}' not found in '{STEP_ALIAS}' of {path}")


def run(hass, variables, home_page, case):
    """Render the variables in order, each seeing the ones before it, and return the result."""
    hass.db = case["db"]
    context = {
        "nspaneltemp": NSPANEL_TEMP,
        "thermostat_embedded": THERMOSTAT_EMBEDDED,
        "home_page": render(hass, resolve(home_page, case["inputs"]), {}),
    }
    for name, value in variables:
        context[name] = render(hass, resolve(value, case["inputs"]), context)
    return context[RESULT_NAME]


def cases():
    """Random cases (seeded) over the inputs and the entities present."""
    rng = random.Random(SEED)
    for _ in range(CASES):
        inputs = {
            "weather_entity": rng.choice(("", WEATHER_ENTITY, None)),
            "outdoortemp": rng.choice(TEMP_ENTITY_SAMPLES),
            "home_outdoor_temp_visible": rng.choice((True, False)),
            "indoortemp": rng.choice(TEMP_ENTITY_SAMPLES),
            "home_indoor_temp_visible": rng.choice((True, False)),
            "temp_calibration_reference": rng.choice(CALIBRATION_SAMPLES),
            "thermostat_use_temp_reference": rng.choice((True, False)),
            "wake_up_sensors": rng.choice(WAKE_UP_SAMPLES),
        }
        for index in range(1, 8):
            prefix = f"chip{index:02d}"
            inputs[prefix] = rng.choice(ENTITY_SAMPLES)
            inputs[prefix + "_inverted"] = rng.choice((True, False, None))
            inputs[prefix + "_icon"] = rng.choice(ICON_SAMPLES)
            inputs[prefix + "_icon_color"] = rng.choice(COLOR_SAMPLES)
        for index in range(1, 8):
            prefix = f"home_custom_button{index:02d}"
            inputs[prefix] = rng.choice(ENTITY_SAMPLES)
            inputs[prefix + "_icon"] = rng.choice(ICON_SAMPLES)
            inputs[prefix + "_name"] = rng.choice((None, "", "Name"))
        db = {}
        if rng.random() < 0.7:
            db[WEATHER_ENTITY] = MockState(WEATHER_ENTITY, "sunny", {"temperature": 7})
        if rng.random() < 0.5:
            db[THERMOSTAT_EMBEDDED] = MockState(THERMOSTAT_EMBEDDED, "heat", {"current_temperature": 21})
        yield {"inputs": inputs, "db": db}


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    reference = load(sys.argv[1])
    candidate = load(sys.argv[2] if len(sys.argv) == 3 else "nspanel_easy_blueprint.yaml")
    hass = MockHomeAssistant()
    hass.env.filters["combine"] = lambda *dicts: {key: value for d in dicts for key, value in d.items()}

    differences = 0
    total = 0
    for case in cases():
        total += 1
        expected = run(hass, *reference, case)
        try:
            actual = run(hass, *candidate, case)
        except Exception as error:  # pylint: disable=broad-except - any error is a difference
            actual = f"ERROR: {error!r}"
        if actual != expected:
            differences += 1
            if differences <= MAX_REPORTED_DIFFERENCES:
                print(f"DIFF inputs={case['inputs']} db={sorted(case['db'])}\n  reference: {expected}\n"
                      f"  tested:    {actual}")
    print(f"{total} cases, {differences} differences")
    return 1 if differences else 0


if __name__ == "__main__":
    sys.exit(main())
