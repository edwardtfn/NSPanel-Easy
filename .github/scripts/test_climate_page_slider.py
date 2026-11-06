#!/usr/bin/env python3
"""
Equivalence test for the Blueprint's climate page slider and values block (`&climate-update_slider`).

Runs the block of two versions of the Blueprint with mocked Home Assistant state and compares what the panel
receives (the `page_climate` action with its data) and the `hvac_modes` variable it leaves for the climate
buttons. Meant for refactorings that must not change what the panel shows (issue #657): run it against the
version before the change.

The cases cover the climate entity (a Home Assistant climate, the embedded thermostat, the page's own climate,
missing or empty) and the attributes the block reads: current and target temperatures (single and range),
supported features, the temperature step under both names, minimum and maximum temperatures (Celsius and
Fahrenheit ranges), the HVAC action and the state, each missing, `None`, invalid or valid.

Usage:
  test_climate_page_slider.py REFERENCE_BLUEPRINT [BLUEPRINT]

  REFERENCE_BLUEPRINT  Blueprint before the change, e.g. from
                       `git show origin/dev:nspanel_easy_blueprint.yaml > /tmp/reference.yaml`
  BLUEPRINT            Blueprint to test (default: nspanel_easy_blueprint.yaml)

Requires PyYAML and Jinja2, pinned in .github/requirements.txt (`pip install -r .github/requirements.txt`).
Exit code: 0 when both render the same, 1 on any difference.
"""

import math
import random
import sys

import yaml

from test_variable_entity import BlueprintLoader, MockHomeAssistant, MockState, find, render

BLOCK_CONDITION = "{{ not (climate_entity == climate and embedded_climate) }}"
BLOCK_ACTION_SUFFIX = "_page_climate"
GLOBALS_ALIAS = "Global variables"
THERMOSTAT_EMBEDDED = "climate.nspanel_thermostat"
CLIMATE = "climate.living"
OTHER_CLIMATE = "climate.office"

CASES = 4000
SEED = 657
MAX_REPORTED_DIFFERENCES = 20

# Values each attribute takes in the cases (... = attribute missing)
ATTRIBUTE_SAMPLES = {
    "current_temperature": (..., None, 21.46, "21.5", 70, "unknown"),
    "supported_features": (..., None, 0, 1, 2, 3, 17, 385, "18"),
    "temperature": (..., None, 21, 21.25, "22.5", "unknown"),
    "target_temp_high": (..., None, 24, 75.5, "x"),
    "target_temp_low": (..., None, 18, 65.25, "x"),
    "target_temp_step": (..., None, 0, 0.5, 1, "0.1", -0.5, "x"),
    "target_temperature_step": (..., None, 0.5, 0.1, 2),
    "min_temp": (..., None, 7, "5", -10, 7.33, 45),
    "max_temp": (..., None, 35, "30", 95, 5),
    "hvac_action": (..., None, "heating", "cooling", "idle", "off", "drying", "fan", "unknown", ""),
    "hvac_modes": (..., None, ["off", "heat"], ["off", "heat_cool", "cool", "auto"]),
}
STATE_SAMPLES = ("heat", "cool", "heat_cool", "auto", "dry", "fan_only", "off", "unavailable", "unknown")
ENTITY_SAMPLES = (CLIMATE, OTHER_CLIMATE, THERMOSTAT_EMBEDDED, "")
OUTER_HVAC_MODES_SAMPLES = ("", None, ["off", "heat"])


def ha_round(value, precision=0, method="common", default=...):
    """Home Assistant's `round` filter: an integer for precision 0, fails on invalid input unless a default is given."""
    try:
        value = round(float(value), precision)
    except (ValueError, TypeError):
        if default is ...:
            raise ValueError(f"round got invalid input {value!r}") from None
        return default
    return int(value) if precision == 0 else value


def ha_is_number(value):
    """Home Assistant's `is_number`: a finite float can be made from the value."""
    try:
        return math.isfinite(float(value))
    except (ValueError, TypeError):
        return False


def is_block(node):
    """The slider block: the `if` with its condition whose `then` calls the `page_climate` action."""
    return (node.get("if") == BLOCK_CONDITION
            and any(isinstance(step, dict) and str(step.get("action", "")).endswith(BLOCK_ACTION_SUFFIX)
                    for step in node.get("then", [])))


def load(path):
    """Return the block's `then` steps and the global `enum` variable."""
    with open(path, encoding="utf-8") as f:
        data = yaml.load(f, Loader=BlueprintLoader)  # nosec B506 - SafeLoader subclass
    block = find(data.get("actions"), is_block)
    global_step = find(data.get("actions"), lambda d: d.get("alias") == GLOBALS_ALIAS)
    if block is None or global_step is None:
        sys.exit(f"ERROR: the climate slider block or '{GLOBALS_ALIAS}' not found in {path}")
    return block["then"], global_step["variables"]["enum"]


def run(hass, steps, enum, case):
    """Run the block's steps: variables in order, and the actions with their rendered data."""
    hass.db = case["db"]
    context = dict(case["context"], enum=enum, action_prefix="panel", thermostat_embedded=THERMOSTAT_EMBEDDED)
    sent = []
    for step in steps:
        if "variables" in step:
            for name, value in step["variables"].items():
                context[name] = render(hass, value, context)
        elif "action" in step:
            sent.append((render(hass, step["action"], context), render(hass, step.get("data", {}), context)))
    return sent, context.get("hvac_modes")


def cases():
    """Random cases (seeded) over the entity, its state and attributes."""
    rng = random.Random(SEED)
    for _ in range(CASES):
        climate_entity = rng.choice(ENTITY_SAMPLES)
        attributes = {name: rng.choice(samples) for name, samples in ATTRIBUTE_SAMPLES.items()}
        db = {}
        if climate_entity and rng.random() < 0.9:
            db[climate_entity] = MockState(climate_entity, rng.choice(STATE_SAMPLES),
                                           {name: value for name, value in attributes.items() if value is not ...})
        yield {
            "db": db,
            "context": {
                "climate_entity": climate_entity,
                "climate": rng.choice((CLIMATE, THERMOSTAT_EMBEDDED, "")),
                "embedded_climate": rng.choice((True, False)),
                "hvac_modes": rng.choice(OUTER_HVAC_MODES_SAMPLES),
            },
        }


def run_safely(hass, version, case):
    """Run one version; an error becomes a comparable result, so the same error in both versions is no difference."""
    try:
        return run(hass, *version, case)
    except Exception as error:  # pylint: disable=broad-except - any error is a result to compare
        return f"ERROR: {error!r}"


def main():
    """Compare both versions over all cases and report the differences."""
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    reference = load(sys.argv[1])
    candidate = load(sys.argv[2] if len(sys.argv) == 3 else "nspanel_easy_blueprint.yaml")
    hass = MockHomeAssistant()
    hass.env.filters["round"] = ha_round
    hass.env.filters["bitwise_and"] = lambda first, second: int(first) & int(second)
    hass.env.globals["is_number"] = ha_is_number

    differences = total = 0
    for case in cases():
        total += 1
        expected = run_safely(hass, reference, case)
        actual = run_safely(hass, candidate, case)
        if actual != expected:
            differences += 1
            if differences <= MAX_REPORTED_DIFFERENCES:
                print(f"DIFF context={case['context']}\n"
                      f"  states:    {[(k, s.state, s.attributes) for k, s in case['db'].items()]}\n"
                      f"  reference: {expected}\n  tested:    {actual}")
    print(f"{total} cases, {differences} differences")
    return 1 if differences else 0


if __name__ == "__main__":
    sys.exit(main())
