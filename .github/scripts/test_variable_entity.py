#!/usr/bin/env python3
"""
Equivalence test for the Blueprint's "Variable Entity" anchor (&variable_entity).

Renders the anchor of two versions of the Blueprint with mocked Home Assistant
state, for many entities, states and attributes, and compares every variable
the reference version defines. Meant for refactorings that must not change
what the panel shows (issue #657): run it against the version before the change.

The mock covers what the anchor uses (states, state_attr, has_value and the
`list`/`match`/`search` tests) and approximates how Home Assistant parses a
rendered template (Python literals become values). It is not a full Home
Assistant template environment.

Usage:
  test_variable_entity.py REFERENCE_BLUEPRINT [BLUEPRINT]

  REFERENCE_BLUEPRINT  Blueprint before the change, e.g. from
                       `git show origin/dev:nspanel_easy_blueprint.yaml > /tmp/reference.yaml`
  BLUEPRINT            Blueprint to test (default: nspanel_easy_blueprint.yaml)

Requires PyYAML and Jinja2 (`pip install pyyaml jinja2`).
Exit code: 0 when both render the same, 1 on any difference.
"""

import ast
import itertools
import re
import sys

import jinja2
import yaml

ANCHOR_ALIAS = "Variable Entity"
GLOBALS_ALIAS = "Global variables"

# Global variables the anchor reads (static data, no templates)
GLOBAL_NAMES = ("nextion", "enum", "device_class_icons", "all_icon_names", "special_icons", "entity_state_styles")

# Values for the Blueprint inputs the anchor reads (`!input` tags)
INPUT_VALUES = {
    "icon_color_fallback_on": [76, 175, 80],
    "icon_color_fallback_off": [90, 90, 90],
}

MAX_REPORTED_DIFFERENCES = 20


class BlueprintLoader(yaml.SafeLoader):
    """SafeLoader resolving `!input` tags to the values in INPUT_VALUES."""


BlueprintLoader.add_multi_constructor(
    "!", lambda loader, suffix, node: INPUT_VALUES.get(loader.construct_scalar(node)))


def find(node, predicate):
    """First mapping under node matching predicate (depth-first), or None."""
    if isinstance(node, dict):
        if predicate(node):
            return node
        children = node.values()
    elif isinstance(node, list):
        children = node
    else:
        return None
    for child in children:
        found = find(child, predicate)
        if found is not None:
            return found
    return None


def load_anchor(path):
    """Return the anchor's variables and the global variables it reads."""
    with open(path, encoding="utf-8") as f:
        data = yaml.load(f, Loader=BlueprintLoader)  # nosec B506 - SafeLoader subclass
    anchor = find(data.get("actions"), lambda d: d.get("alias") == ANCHOR_ALIAS)
    global_step = find(data.get("actions"), lambda d: d.get("alias") == GLOBALS_ALIAS)
    if anchor is None or global_step is None:
        sys.exit(f"ERROR: '{ANCHOR_ALIAS}' or '{GLOBALS_ALIAS}' not found in {path}")
    globals_ = {name: global_step["variables"][name] for name in GLOBAL_NAMES if name in global_step["variables"]}
    return anchor["variables"], globals_


class MockState:
    """A Home Assistant state object, as far as the anchor uses it."""

    def __init__(self, entity_id, state, attributes):
        self.entity_id = entity_id
        self.domain = entity_id.split(".")[0]
        self.state = state
        self.attributes = attributes
        self.name = attributes.get("friendly_name", entity_id)


class MockStates:
    """`states`: callable (`states(entity_id)`), indexable (`states[entity_id]`) and by domain (`states.sun`)."""

    def __init__(self, hass):
        self._hass = hass

    def __call__(self, entity_id):
        state = self._hass.db.get(entity_id)
        return state.state if state else "unknown"

    def __getitem__(self, entity_id):
        return self._hass.db.get(entity_id)

    def __getattr__(self, domain):
        if domain.startswith("_") or domain == "jinja_pass_arg":
            raise AttributeError(domain)
        return [state for entity_id, state in self._hass.db.items() if entity_id.startswith(domain + ".")]


class MockHomeAssistant:
    """Jinja environment with the Home Assistant functions and tests the anchor uses, over a swappable state db."""

    def __init__(self):
        self.db = {}
        self._compiled = {}
        self.env = jinja2.Environment(extensions=["jinja2.ext.loopcontrols"])  # nosec B701 - renders no HTML
        self.env.globals.update(
            states=MockStates(self),
            state_attr=lambda entity_id, name: (self.db[entity_id].attributes.get(name)
                                                if entity_id in self.db else None),
            has_value=lambda entity_id: (entity_id in self.db
                                         and self.db[entity_id].state not in ("unknown", "unavailable")),
            int=int,
            none=None,
        )
        self.env.tests["list"] = lambda value: isinstance(value, list)
        self.env.tests["match"] = lambda value, pattern: isinstance(value, str) and re.match(pattern, value) is not None
        self.env.tests["search"] = lambda value, pattern: (isinstance(value, str)
                                                           and re.search(pattern, value) is not None)

    def template(self, source):
        """Compiled template, compiled once per source."""
        if source not in self._compiled:
            self._compiled[source] = self.env.from_string(source)
        return self._compiled[source]


def parse_result(text):
    """Approximate Home Assistant's parsing of a rendered template: Python literals become values."""
    stripped = text.strip()
    try:
        return ast.literal_eval(stripped)
    except (ValueError, SyntaxError):
        return stripped


def render(hass, node, context):
    """Render a variable's value like Home Assistant: templates in strings, recursively in dicts and lists."""
    if isinstance(node, str) and ("{{" in node or "{%" in node):
        return parse_result(hass.template(node).render(**context))
    if isinstance(node, dict):
        return {key: render(hass, value, context) for key, value in node.items()}
    if isinstance(node, list):
        return [render(hass, value, context) for value in node]
    return node


def run_anchor(hass, variables, globals_, db, inputs):
    """Render the anchor's variables in order, each seeing the ones before it."""
    hass.db = db
    context = dict(globals_, **inputs)
    for name, value in variables.items():
        context[name] = render(hass, value, context)
    return context


def entity_cases():
    """(entity_id, state, attributes) to test, covering every domain and state the anchor handles."""
    cases = []
    for state in ("disarmed", "disarming", "armed_home", "armed_away", "armed_night", "armed_vacation",
                  "armed_bypass", "armed_custom_bypass", "pending", "arming", "triggered", "unknown", "unavailable"):
        cases.append(("alarm_control_panel.test", state, {}))
    for state, hvac_action in itertools.product(
            ("off", "heat", "cool", "heat_cool", "auto", "dry", "fan_only", "unknown", "unavailable"),
            ("off", "heating", "cooling", "drying", "fan", "idle", "preheating", "defrosting", None, "unknown", "")):
        cases.append(("climate.test", state, {} if hvac_action is None else {"hvac_action": hvac_action}))
    for state, attributes in itertools.product(
            ("on", "off", "unavailable"),
            ({}, {"rgb_color": [10, 20, 30]}, {"rgb_color": "invalid"}, {"color": "#112233"}, {"color": [1, 2, 3]})):
        cases.append(("light.test", state, attributes))
    for state in ("locked", "unlocked", "jammed", "locking", "unknown"):
        cases.append(("lock.test", state, {}))
    for state in ("active", "paused", "idle"):
        cases.append(("timer.test", state, {}))
    for state, mode in itertools.product(
            ("off", "eco", "gas", "performance", "unknown"),
            ("off", "on", "eco", "electric", "gas", "heat_pump", "performance", "high_demand", "other", None)):
        cases.append(("water_heater.test", state, {} if mode is None else {"operation_mode": mode}))
    for state, action in itertools.product(("on", "off"), (None, "off", "humidifying", "drying", "idle")):
        cases.append(("humidifier.test", state, {} if action is None else {"action": action}))
    for state, device_class in itertools.product(("on", "off", "unavailable"),
                                                 (None, "door", "battery", "moisture", "nonexistent")):
        cases.append(("binary_sensor.test", state, {"device_class": device_class} if device_class else {}))
    for state, device_class in itertools.product(("21.5", "0", "unknown", "text"), (None, "temperature", "humidity")):
        cases.append(("sensor.test", state, {"device_class": device_class} if device_class else {}))
    for state, icon in itertools.product(("on", "off"),
                                         (None, "mdi:lightbulb", "mdi:nonexistent", "lightbulb", "mdi:blank")):
        cases.append(("switch.test", state, {"icon": icon} if icon else {}))
    for state in ("open", "closed", "opening"):
        cases.append(("cover.test", state, {"device_class": "garage"}))
    return cases


OVERLAPS = (
    {"icon": None, "icon_color": None, "name": None},
    {"icon": "mdi:fan", "icon_color": [1, 2, 3], "name": "Custom"},
    {"icon": "mdi:nonexistent", "icon_color": "invalid", "name": ""},
)
FORCED_ICON_STATES = (None, "on", "off")
INVALID_ENTITY_IDS = (None, "", "x", "light.missing")


def compare(reference, candidate):
    """Run every case through both anchors; return (cases, differences)."""
    ref_vars, ref_globals = reference
    new_vars, new_globals = candidate
    hass = MockHomeAssistant()
    total = differences = 0

    def check(label, db, inputs):
        nonlocal total, differences
        total += 1
        expected = run_anchor(hass, ref_vars, ref_globals, db, inputs)
        actual = run_anchor(hass, new_vars, new_globals, db, inputs)
        for name in ref_vars:
            if expected.get(name) != actual.get(name):
                differences += 1
                if differences <= MAX_REPORTED_DIFFERENCES:
                    print(f"DIFF {label}: {name}\n"
                          f"  reference: {expected.get(name)!r}\n  tested:    {actual.get(name)!r}")

    for (entity_id, state, attributes), overlap, forced in itertools.product(
            entity_cases(), OVERLAPS, FORCED_ICON_STATES):
        attributes = dict(attributes, friendly_name="Friendly name")
        db = {entity_id: MockState(entity_id, state, attributes)}
        check(f"{entity_id}={state} {attributes} overlap={overlap} forced={forced}", db,
              {"entity_id": entity_id, "overlap": overlap, "forced_icon_state": forced})
    for entity_id in INVALID_ENTITY_IDS:
        check(f"invalid entity {entity_id!r}", {},
              {"entity_id": entity_id, "overlap": OVERLAPS[1], "forced_icon_state": None})
    return total, differences


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    reference = load_anchor(sys.argv[1])
    candidate = load_anchor(sys.argv[2] if len(sys.argv) == 3 else "nspanel_easy_blueprint.yaml")
    total, differences = compare(reference, candidate)
    print(f"{total:,} cases, {len(reference[0])} variables each, {differences:,} differences")
    return 1 if differences else 0


if __name__ == "__main__":
    sys.exit(main())
