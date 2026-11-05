#!/usr/bin/env python3
"""
Equivalence test for the variables of the Blueprint's "Weather pages" branch.

Renders `weather_units` and `parameters` of two versions of the Blueprint with mocked Home Assistant state,
for many unit attributes and forecast values, and compares what is sent to the panel: the units and, for
each visible field, the value and icon texts. Meant for refactorings that must not change what the panel
shows (issue #657): run it against the version before the change.

It also renders the forecast values of a page (`forecast_day`, then `temp_min`, `temp_max` and the other
values of the day) from mocked forecasts with missing, empty, `none`, non-numeric and numeric fields. These
must render the same in both versions, except where the reference fails with a template error: there the
tested version must render (a fix), and an error in the tested version always counts as a difference.

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

import datetime
import itertools
import math
import random
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

# Forecast values of a page, rendered from `forecast_day` (in order)
FORECAST_VALUE_NAMES = ("temp_min", "temp_max", "precipitation", "precipitation_probability", "pressure",
                        "wind_speed", "hours_of_sun", "uv_index", "thunderstorm_probability", "humidity")
# Numeric fields of a forecast slot, and the values they take in the mocked forecasts (... = field missing)
FORECAST_FIELDS = ("temperature", "templow", "precipitation", "precipitation_probability", "pressure",
                   "wind_speed", "hours_of_sun", "uv_index", "thunderstorm_probability", "humidity")
FORECAST_SAMPLES = (..., None, "", "unknown", 0, 0.0, -2.0, 3.5, "4.5", 12)
FORECAST_CASES = 1500
# Slots per day of each forecast type (hourly kept short to keep the test fast)
FORECAST_TYPES = {"daily": (12,), "twice_daily": (6, 18), "hourly": (0, 6, 12, 18)}
NOW = datetime.datetime(2026, 11, 5, 9, 30, tzinfo=datetime.timezone.utc)


def is_number(value):
    """Home Assistant's `is_number`: a finite number, or a string holding one."""
    try:
        return math.isfinite(float(value))
    except (ValueError, TypeError):
        return False


def ha_float(value, default=None):
    """Home Assistant's `float` filter: fails on invalid input unless a default is given."""
    try:
        return float(value)
    except (ValueError, TypeError):
        if default is None:
            raise ValueError(f"float got invalid input {value!r}") from None
        return default


def as_timestamp(value):
    """Home Assistant's `as_timestamp`, for datetimes and ISO strings."""
    if isinstance(value, datetime.datetime):
        return value.timestamp()
    return datetime.datetime.fromisoformat(str(value)).timestamp()


def setup_environment(hass):
    """Add the Home Assistant functions, filters and tests the weather pages use to the mocked environment."""
    hass.env.globals.update(is_number=is_number, now=lambda: NOW, timedelta=datetime.timedelta,
                            today_at=lambda time: NOW.replace(hour=0, minute=0, second=0, microsecond=0))
    hass.env.tests["is_number"] = is_number
    hass.env.filters.update(
        float=ha_float,
        as_timestamp=as_timestamp,
        as_datetime=lambda value: datetime.datetime.fromtimestamp(value, datetime.timezone.utc),
        timestamp_utc=lambda value: datetime.datetime.fromtimestamp(value, datetime.timezone.utc).isoformat(),
    )


def load_forecast(branch, path):
    """Return the variables building `forecast_day` for a page, its fallback for today, and the values of the day."""
    builder = find(branch, lambda d: "datetime_is_string" in d and "forecast_day" in d)
    fallback = find(branch, lambda d: isinstance(d.get("forecast_day"), str)
                    and "page_index == 0" in d["forecast_day"] and "datetime_is_string" not in d)
    values = find(branch, lambda d: "temp_min" in d and "temp_max" in d)
    if builder is None or fallback is None or values is None:
        sys.exit(f"ERROR: the forecast variables were not found in {path}")
    return builder, fallback["forecast_day"], {name: values[name] for name in FORECAST_VALUE_NAMES}


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
    branch_parts = (units["weather_units"], params["parameters"], repeat["for_each"], global_step["variables"]["nextion"])
    return branch_parts, load_forecast(branch, path)


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


def run_forecast(hass, forecast_parts, context):
    """Render the forecast values of a page as the branch does; a template error is returned as the result."""
    builder, fallback, values = forecast_parts
    try:
        context = dict(context, datetime_is_string=render(hass, builder["datetime_is_string"], context))
        context["forecast_day"] = render(hass, builder["forecast_day"], context)
        if not (len(context["forecast_day"]) > 0 or context["page_index"] == 0):
            return "no forecast"
        context["forecast_day"] = render(hass, fallback, context)
        if not len(context["forecast_day"]) > 0:
            return "no forecast"
        return {name: render(hass, source, context) for name, source in values.items()}
    except Exception as error:  # pylint: disable=broad-except - any template error is a result
        return f"ERROR {type(error).__name__}: {error}"


def forecast_cases():
    """Forecasts of every type, each field random among the samples, for every page and with weather states."""
    rng = random.Random(657)
    midnight = NOW.replace(hour=0, minute=0, second=0, microsecond=0)
    for case in range(FORECAST_CASES):
        hours = list(FORECAST_TYPES.values())[case % len(FORECAST_TYPES)]
        forecast = []
        for day in range(rng.choice((0, 1, 3))):
            for hour in hours:
                slot = {"datetime": (midnight + datetime.timedelta(days=day, hours=hour)).isoformat(),
                        "condition": "sunny"}
                for field in FORECAST_FIELDS:
                    sample = rng.choice(FORECAST_SAMPLES)
                    if sample is not ...:
                        slot[field] = sample
                forecast.append(slot)
        attributes = {field: rng.choice(FORECAST_SAMPLES[1:]) for field in ("temperature", "wind_speed")}
        yield forecast, rng.randint(0, 3), attributes


def compare_forecast(reference, candidate):
    """Run every forecast case through both versions; return (cases, fixed, differences)."""
    hass = MockHomeAssistant()
    setup_environment(hass)
    total = fixed = differences = 0
    for forecast, page_index, attributes in forecast_cases():
        hass.db = {WEATHER_ENTITY: MockState(WEATHER_ENTITY, "sunny", attributes)}
        context = {"forecast": forecast, "page_index": page_index, "weather_entity": WEATHER_ENTITY}
        total += 1
        expected = run_forecast(hass, reference, context)
        actual = run_forecast(hass, candidate, context)
        # An error in the tested version is always a difference, even when the reference fails the same way
        failed = str(actual).startswith("ERROR")
        if expected == actual and not failed:
            continue
        if str(expected).startswith("ERROR") and not failed:
            fixed += 1
            continue
        differences += 1
        if differences <= MAX_REPORTED_DIFFERENCES:
            print(f"DIFF forecast={forecast} page_index={page_index} weather={attributes}\n"
                  f"  reference: {expected!r}\n  tested:    {actual!r}")
    return total, fixed, differences


def compare(reference, candidate):
    """Run every case through both versions; return (cases, differences)."""
    hass = MockHomeAssistant()
    setup_environment(hass)
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
    reference, reference_forecast = load_branch(sys.argv[1])
    candidate, candidate_forecast = load_branch(sys.argv[2] if len(sys.argv) == 3 else "nspanel_easy_blueprint.yaml")
    total, differences = compare(reference, candidate)
    print(f"Units and fields: {total:,} cases, {differences:,} differences")
    forecast_total, fixed, forecast_differences = compare_forecast(reference_forecast, candidate_forecast)
    print(f"Forecast values: {forecast_total:,} cases, {forecast_differences:,} differences, "
          f"{fixed:,} fixed (reference fails, tested renders)")
    return 1 if differences or forecast_differences else 0


if __name__ == "__main__":
    sys.exit(main())
