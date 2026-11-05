#!/usr/bin/env python3
"""
Equivalence test for the refresh loop of the Blueprint's "Media player settings page" branch.

Runs two iterations of the loop of two versions of the Blueprint with mocked Home Assistant state and compares
what the panel receives: the actions called in each iteration (full refresh or progress resync) with their
data, and the state carried to the next iteration. Meant for refactorings that must not change what the
panel shows (issue #657): run it against the version before the change.

The cases cover the attributes the loop reads (state, mute, volume, title, artist, duration, position and its
update time, supported features, group players), missing, `None`, invalid and valid, and a second iteration
with the same or a changed state, a few seconds or more than a minute later.

Home Assistant parses each rendered template on its own, so a title or artist that looks like a Python literal
(for example `1979` or `True`) became a number or a boolean when rendered as a separate field, and was then
dropped as not being text. For those cases the tested version must send the text as it is (a fix), and the
rest of what the panel receives must match.

Usage:
  test_media_player_page.py REFERENCE_BLUEPRINT [BLUEPRINT]

  REFERENCE_BLUEPRINT  Blueprint before the change, e.g. from
                       `git show origin/dev:nspanel_easy_blueprint.yaml > /tmp/reference.yaml`
  BLUEPRINT            Blueprint to test (default: nspanel_easy_blueprint.yaml)

Requires PyYAML and Jinja2, pinned in .github/requirements.txt (`pip install -r .github/requirements.txt`).
Exit code: 0 when both render the same, 1 on any difference.
"""

import datetime
import random
import sys

import yaml

from test_variable_entity import BlueprintLoader, MockHomeAssistant, MockState, find, render

LOOP_ALIAS = "Refresh page in a loop"
BRANCH_ALIAS = "Media player settings page"
GLOBALS_ALIAS = "Global variables"
ENTITY = "media_player.kitchen"

CASES = 3000
SEED = 657
MAX_REPORTED_DIFFERENCES = 20
NOW = datetime.datetime(2026, 11, 5, 18, 0, 0, tzinfo=datetime.timezone.utc)

# Values each attribute takes in the cases (... = attribute missing)
ATTRIBUTE_SAMPLES = {
    "is_volume_muted": (..., None, True, False),
    "volume_level": (..., None, 0, 0.5, 0.333, 1, "0.4", "loud"),
    "media_title": (..., None, "", "unknown", "Song", "Song | Live", 'Say "hi"', "It's me", "Ação"),
    "media_artist": (..., None, "", "unavailable", "Artist", "Band \\ Co"),
    "media_duration": (..., None, 0, 200, "200", 200.7),
    "media_position": (..., None, 0, 50, 250, "30"),
    "media_position_updated_at": (..., None, "", "unknown", "unavailable", NOW - datetime.timedelta(seconds=30), NOW),
    "supported_features": (..., None, 0, 21437, 152463, "5"),
    "mass_player_type": (..., "group", "player"),
}
STATE_SAMPLES = ("playing", "paused", "idle", "off", "on", "unavailable", "unknown")
# Titles and artists that Home Assistant parsed as other types when rendered as separate fields
LITERAL_TEXTS = ("1979", "True", "None", "3.5", "[1, 2]")


def ha_round(value, precision=0, method="common", default=...):
    """Home Assistant's `round` filter: fails on invalid input unless a default is given."""
    try:
        return round(float(value), precision)
    except (ValueError, TypeError):
        if default is ...:
            raise ValueError(f"round got invalid input {value!r}") from None
        return default


def ha_as_timestamp(value, default=...):
    """Home Assistant's `as_timestamp`: datetimes and date strings, fails otherwise unless a default is given."""
    try:
        if isinstance(value, datetime.datetime):
            return value.timestamp()
        return datetime.datetime.fromisoformat(str(value)).timestamp()
    except (ValueError, TypeError):
        if default is ...:
            raise ValueError(f"as_timestamp got invalid input {value!r}") from None
        return default


def load(path):
    """Return the loop's steps and the global `enum` variable."""
    with open(path, encoding="utf-8") as f:
        data = yaml.load(f, Loader=BlueprintLoader)  # nosec B506 - SafeLoader subclass
    branch = find(data.get("actions"), lambda d: d.get("alias") == BRANCH_ALIAS)
    loop = find(branch, lambda d: d.get("alias") == LOOP_ALIAS)
    global_step = find(data.get("actions"), lambda d: d.get("alias") == GLOBALS_ALIAS)
    if loop is None or global_step is None:
        sys.exit(f"ERROR: '{BRANCH_ALIAS} > {LOOP_ALIAS}' or '{GLOBALS_ALIAS}' not found in {path}")
    return loop["repeat"]["sequence"], global_step["variables"]["enum"]


def is_true(hass, condition, context):
    """Whether a condition (template or list of templates) holds."""
    conditions = condition if isinstance(condition, list) else [condition]
    return all(render(hass, item, context) is True for item in conditions)


def run_sequence(hass, steps, context, sent):
    """Run the steps that matter for the panel: variables, choose and actions (with their rendered data)."""
    for step in steps:
        if "variables" in step:
            for name, value in step["variables"].items():
                context[name] = render(hass, value, context)
        elif "choose" in step:
            for option in step["choose"]:
                if is_true(hass, option["conditions"], context):
                    run_sequence(hass, option["sequence"], context, sent)
                    break
        elif "action" in step and step["action"] != "homeassistant.update_entity":
            sent.append((render(hass, step["action"], context), render(hass, step.get("data", {}), context)))


def run(hass, loop, enum, iterations):
    """Run the loop once per (db, now) iteration and return what each iteration sent and carried over."""
    context = {"enum": enum, "entity_id": ENTITY, "action_prefix": "panel", "last_signature": "",
               "last_full_refresh": 0, "wait": {"completed": True}}
    results = []
    for db, now in iterations:
        hass.db = db
        hass.env.globals["now"] = lambda now=now: now
        sent = []
        run_sequence(hass, loop, context, sent)
        results.append((sent, context["last_signature"], context["last_full_refresh"], context["wait_completed"]))
    return results


def random_state(rng, literal_text=None):
    """A mocked media player state; literal_text replaces the title or the artist when given."""
    attributes = {name: rng.choice(samples) for name, samples in ATTRIBUTE_SAMPLES.items()}
    if literal_text is not None:
        attributes[rng.choice(("media_title", "media_artist"))] = literal_text
    return {ENTITY: MockState(ENTITY, rng.choice(STATE_SAMPLES),
                              {name: value for name, value in attributes.items() if value is not ...})}


def cases():
    """(iterations, literal_text): two iterations each, the second with the same or a changed state."""
    rng = random.Random(SEED)
    for index in range(CASES):
        literal_text = rng.choice(LITERAL_TEXTS) if index % 10 == 0 else None
        first = random_state(rng, literal_text)
        # With a literal-looking text, the signature differs from the reference, and so would the choice between a
        # full refresh and a resync after a state change; the same state keeps that choice comparable.
        second = first if literal_text is not None or rng.random() < 0.5 else random_state(rng)
        later = NOW + datetime.timedelta(seconds=rng.choice((5, 59, 60, 61, 120)))
        yield [(first, NOW), (second, later)], literal_text


def without_text(results):
    """Results with the title, the artist and the signature dropped."""
    stripped = []
    for sent, _signature, last_full_refresh, wait_completed in results:
        actions = []
        for action, data in sent:
            data = dict(data)
            for key in ("media_title", "media_artist"):
                data.pop(key, None)
            actions.append((action, data))
        stripped.append((actions, last_full_refresh, wait_completed))
    return stripped


def sends_text(results, literal_text, enum):
    """Whether the first full refresh sends the literal text as title or artist (unless it is an unknown state)."""
    if literal_text in enum["states"]["unknown"]:
        return True
    for action, data in results[0][0]:
        if action.endswith("_page_media_player"):
            return literal_text in (str(data.get("media_title")), str(data.get("media_artist")))
    return False


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    reference = load(sys.argv[1])
    candidate = load(sys.argv[2] if len(sys.argv) == 3 else "nspanel_easy_blueprint.yaml")
    hass = MockHomeAssistant()
    hass.env.filters["round"] = ha_round
    hass.env.filters["as_timestamp"] = ha_as_timestamp
    hass.env.globals["as_timestamp"] = ha_as_timestamp
    hass.env.tests["boolean"] = lambda value: isinstance(value, bool)

    differences = fixed = total = 0
    for iterations, literal_text in cases():
        total += 1
        expected = run(hass, *reference, iterations)
        try:
            actual = run(hass, *candidate, iterations)
        except Exception as error:  # pylint: disable=broad-except - any error is a difference
            actual = f"ERROR: {error!r}"
        if literal_text is None:
            same = actual == expected
        else:
            same = (not isinstance(actual, str) and sends_text(actual, literal_text, candidate[1])
                    and without_text(actual) == without_text(expected))
            fixed += same
        if not same:
            differences += 1
            if differences <= MAX_REPORTED_DIFFERENCES:
                print(f"DIFF literal_text={literal_text!r}\n"
                      f"  states:    {[{k: (s.state, s.attributes) for k, s in db.items()} for db, _ in iterations]}\n"
                      f"  reference: {expected}\n  tested:    {actual}")
    print(f"{total} cases, {differences} differences, {fixed} fixed (title or artist looking like a literal)")
    return 1 if differences else 0


if __name__ == "__main__":
    sys.exit(main())
