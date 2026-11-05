#!/usr/bin/env python3
"""
Equivalence test for the variables of the Blueprint's "Weather pages" branch.

Renders `weather_units` and `parameters` of two versions of the Blueprint with mocked Home Assistant state,
for many unit attributes and forecast values, and compares what is sent to the panel: the units and, for
each visible field, the value and icon texts. Meant for refactorings that must not change what the panel
shows (issue #657): run it against the version before the change.

The variables themselves may differ in value types (Home Assistant parses each rendered template on its
own, so a field rendered as a separate template can become a number), so `parameters` is compared after
the `repeat` that sends it to the panel, as the panel receives it.

Usage:
  test_weather_variables.py REFERENCE_BLUEPRINT [BLUEPRINT]

  REFERENCE_BLUEPRINT  Blueprint before the change, e.g. from
                       `git show origin/dev:nspanel_easy_blueprint.yaml > /tmp/reference.yaml`
  BLUEPRINT            Blueprint to test (default: nspanel_easy_blueprint.yaml)

Requires PyYAML and Jinja2, pinned in .github/requirements.txt (`pip install -r .github/requirements.txt`).
Exit code: 0 when both render the same, 1 on any difference.
"""

import itertools
import math
import sys

import yaml

from test_variable_entity import BlueprintLoader, MockHomeAssistant, MockState, find, parse_result, render

BRANCH_ALIAS = "Weather pages"
GLOBALS_ALIAS = "Global variables"
WEATHER_ENTITY = "weather.test"

# Forecast values the `parameters` variable reads, as the forecast variables provide them
VALUE_NAMES = ("hours_of_sun", "thunderstorm_probability", "precipitation", "precipitation_probability",
               "uv_index", "wind_speed", "humidity", "pressure")
UNIT_NAMES = ("humidity", "hours_of_sun", "precipitation", "precipitation_probability", "pressure",
              "thunderstorm_probability", "uv_index", "wind_speed")

MAX_REPORTED_DIFFERENCES = 20


def is_number(value):
    """Home Assistant's `is_number`: a finite number, or a string holding one."""
    try:
        return math.isfinite(float(value))
    except (ValueError, TypeError):
        return False


def load_branch(path):
    """Return the branch's `weather_units` and `parameters` sources, the `for_each` sending them, and `nextion`."""
    with open(path, encoding="utf-8") as f:
        data = yaml.load(f, Loader=BlueprintLoader)  # nosec B506 - SafeLoader subclass
    branch = find(data.get("actions"), lambda d: d.get("alias", "").startswith(BRANCH_ALIAS))
    global_step = find(data.get("actions"), lambda d: d.get("alias") == GLOBALS_ALIAS)
    if branch is None or global_step is None:
        sys.exit(f"ERROR: '{BRANCH_ALIAS}' or '{GLOBALS_ALIAS}' not found in {path}")
    units = find(branch, lambda d: isinstance(d.get("weather_units"), (str, dict)))
    params = find(branch, lambda d: isinstance(d.get("parameters"), (str, list)))
    repeat = find(branch, lambda d: "parameters" in str(d.get("for_each", "")))
    if units is None or params is None or repeat is None:
        sys.exit(f"ERROR: 'weather_units', 'parameters' or its 'for_each' not found in {path}")
    return units["weather_units"], params["parameters"], repeat["for_each"], global_step["variables"]["nextion"]


def run_branch(hass, branch, db, context):
    """Render the units and parameters, then the texts sent to the panel for each visible field."""
    units_source, params_source, for_each, nextion = branch
    hass.db = db
    context = dict(context, nextion=nextion)
    context["weather_units"] = render(hass, units_source, context)
    context["parameters"] = render(hass, params_source, context)
    items = parse_result(hass.template(for_each).render(**context))
    sent = []
    for item in items:
        repeat = {"item": item}
        sent.append(tuple(str(parse_result(hass.template("{{ repeat.item.%s }}" % key).render(repeat=repeat)))
                          for key in ("value", "icon")))
    return context["weather_units"], sent


def unit_cases():
    """Unit attributes of the weather entity: none, all set, and each one missing, empty or set alone."""
    full = {name: f"u_{name}" for name in UNIT_NAMES}
    cases = [{}, full]
    for name in UNIT_NAMES:
        cases.append({key: value for key, value in full.items() if key != name})
        cases.append(dict(full, **{name: ""}))
        cases.append(dict(full, **{name: None}))
        cases.append({name: "mm"})
    return cases


def value_cases():
    """Forecast values: all missing, all set, and each one with values of every kind while the rest vary."""
    samples = (None, 0, 0.0, 3.0, 12.5, -1.0, 1013.0)
    cases = [dict.fromkeys(VALUE_NAMES), dict.fromkeys(VALUE_NAMES, 5.0)]
    for name, sample in itertools.product(VALUE_NAMES, samples):
        cases.append(dict(dict.fromkeys(VALUE_NAMES), **{name: sample}))
        cases.append(dict(dict.fromkeys(VALUE_NAMES, 2.0), **{name: sample}))
    for precipitation, probability in itertools.product(samples, samples):
        cases.append(dict(dict.fromkeys(VALUE_NAMES), precipitation=precipitation,
                          precipitation_probability=probability))
    return cases


def compare(reference, candidate):
    """Run every case through both versions; return (cases, differences)."""
    hass = MockHomeAssistant()
    hass.env.globals.update(is_number=is_number)
    total = differences = 0
    entities = [(WEATHER_ENTITY, attributes) for attributes in unit_cases()]
    entities += [(None, {}), ("weather.missing", None)]
    for (weather_entity, attributes), values, compact in itertools.product(
            entities, value_cases(), (False, True)):
        db = {}
        if attributes is not None and weather_entity == WEATHER_ENTITY:
            db[WEATHER_ENTITY] = MockState(WEATHER_ENTITY, "sunny",
                                           {f"{key}_unit": value for key, value in attributes.items()})
        context = dict(values, weather_entity=weather_entity, compact_unit_display=compact)
        total += 1
        expected = run_branch(hass, reference, db, context)
        actual = run_branch(hass, candidate, db, context)
        if expected != actual:
            differences += 1
            if differences <= MAX_REPORTED_DIFFERENCES:
                print(f"DIFF entity={weather_entity} units={attributes} values={values} compact={compact}\n"
                      f"  reference: {expected!r}\n  tested:    {actual!r}")
    return total, differences


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    reference = load_branch(sys.argv[1])
    candidate = load_branch(sys.argv[2] if len(sys.argv) == 3 else "nspanel_easy_blueprint.yaml")
    total, differences = compare(reference, candidate)
    print(f"{total:,} cases, {differences:,} differences")
    return 1 if differences else 0


if __name__ == "__main__":
    sys.exit(main())
