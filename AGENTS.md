# AGENTS.md

Guidance for AI coding agents working on NSPanel Easy. Human contributors should read [CONTRIBUTING.md](CONTRIBUTING.md) first; this file complements it and does not replace it.

## Project overview

NSPanel Easy is an open-source integration of the Sonoff NSPanel with Home Assistant. It has three layers that must stay in sync:

| Layer | Location | Language |
| --- | --- | --- |
| ESPHome firmware | `nspanel_esphome*.yaml`, `esphome/`, `components/nspanel_easy/` | YAML, C++, Python (codegen) |
| Home Assistant Blueprint | `nspanel_easy_blueprint.yaml` | YAML, Jinja2 |
| Nextion display (HMI/TFT) | `hmi/` | Nextion Editor project files |

Supported display models: EU (landscape), US portrait, and US landscape.

## Repository layout

- `nspanel_esphome.yaml` - Entry point users include; pulls packages from `esphome/`.
- `esphome/` - ESPHome packages, split by concern (`hw_*`, `page_*`, `addon_*`, `api*`, `core`, `standard`, `version`).
- `components/nspanel_easy/` - External component (C++ sources and `__init__.py`).
  Page-specific code is gated by `NSPANEL_EASY_PAGE_*` build flags defined in the matching `esphome/nspanel_esphome_page_*.yaml`.
- `nspanel_easy_blueprint.yaml` - Single Blueprint file (very large; edit surgically).
- `hmi/` - `.hmi` sources and compiled `.tft` files. `hmi/dev/` holds developer tooling, fonts, images and the `nextion2text` output.
- `docs/` - User documentation. Update it whenever user-visible behavior changes.
- `.test/` - ESPHome configurations used by the CI build matrix.
- `prebuilt/` - Prebuilt firmware configuration and binaries (experimental).
- `versioning/` - Version files managed by CI. Do not edit.
- `.github/` - Workflows, issue templates, CI scripts and pinned Python tooling (`requirements.txt`).
- `.rules/` - Linter configurations (yamllint, markdownlint, markdown link check).

## Build and validation

Run the relevant checks before proposing a change:

```bash
# One-time setup (Ubuntu/Debian; markdownlint-cli2 requires Node.js/npm)
sudo apt-get update && sudo apt-get install -y clang-format
npm install --global markdownlint-cli2
pip install -r .github/requirements.txt
pip install esphome  # Intentionally not pinned in requirements.txt

# C++ formatting (config: .clang-format, ColumnLimit 120); same scope as validate_clang_format.yml
find ./components/nspanel_easy ./.test/unit \( -name '*.h' -o -name '*.c' -o -name '*.cpp' \) -print0 | xargs -0 -r clang-format --style=file -i

# YAML lint (max line length 200)
yamllint -c ./.rules/yamllint.yml .

# Python lint
flake8 --max-line-length=200 components/nspanel_easy

# Markdown lint (config: .rules/.markdownlint.jsonc)
markdownlint-cli2 --config .rules/.markdownlint.jsonc "**/*.md"

# Compile one of the CI test configurations
esphome compile .test/esphome_idf_basic.yaml
```

CI builds every configuration in `.test/` against ESPHome latest and dev.
When a change affects a feature that is only included by specific packages (climate, cover, Bluetooth, customizations, Arduino), compile the matching `.test/` file, not only the basic one.

## Versioning and compatibility

- Versioning is CalVer (`YYYY.M.seq`) and fully automated by `.github/workflows/versioning.yml` on push to `main`. Never edit `versioning/` or bump version numbers manually.
- The three layers check each other's versions at runtime.
  When a change makes one layer depend on a newer version of another, bump the relevant minimum in the same PR:
  - `min_blueprint_version`, `min_tft_version` and `min_esphome_compiler_version` in `esphome/nspanel_esphome_version.yaml`.
  - `min_version` (Home Assistant) in the Blueprint header.
  - `min_esphome_version` in the Blueprint variables, when the Blueprint needs a newer ESPHome firmware (e.g. a new API action or a new field in an existing one).
  - Set `min_blueprint_version` or `min_esphome_version` to `next` to require the release being created; the versioning workflow resolves it.
- API actions are versioned as `api_version`:
  `"MAJOR.MINOR"` in `esphome/nspanel_esphome_version.yaml` (provided by the firmware) and `[MAJOR, MINOR]` in the Blueprint variables (used by the Blueprint):
  - Adding an action: bump the firmware MINOR. When the Blueprint starts calling it, raise the Blueprint MINOR and set `min_esphome_version` to `next`, as only panel events carry the API version.
  - Removing an action or changing its parameters: bump MAJOR (and reset MINOR to 0) in both, and set `min_blueprint_version` to `next`.
- Never use `yq` for in-place writes on `nspanel_easy_blueprint.yaml` (corrupts Unicode escapes),
  `esphome/nspanel_esphome_version.yaml` or `.github/ISSUE_TEMPLATE/bug.yml` (strips blank lines, normalizes merge keys). Use `sed` for simple replacements or a dedicated Python script.

## Commits and pull requests

- Title format (enforced by `validate_pr_title.yml`): `<prefix>: <Description starting with a capital letter>`, lowercase prefix, no trailing period.
- Allowed prefixes: `fix`, `feat`, `improve`, `ci`, `docs`, `style`, `build`, `refactor`, `test`, `chore`. Use `improve` when behavior is unchanged but the implementation is better.
- PR titles become release titles verbatim; write them for end users.
- Reference related issues in the description (`Closes #N` when resolved; a plain `#N` reference otherwise).
- Do not hard-wrap text in PR descriptions, issues or release notes.
- Keep PRs scoped to one logical change. Unrelated fixes go in separate PRs.
- Target `main` from a short-lived feature branch.

## General coding rules

- Keep the project lean: do not add sensors, globals or entities when the data is already reachable through an existing path. No speculative abstractions.
- Never remove existing comments or inline documentation unless explicitly asked.
- Prefer explicit over implicit: explicit enum comparisons instead of range checks, and explicit `if`/`else if` branches instead of a catch-all `else` where it improves defense in depth.
- No trailing whitespace, LF line endings, newline at end of file.
- Deliver complete edits. Do not leave placeholders such as `// ... rest unchanged`.

## C++ (components/nspanel_easy)

Follow ESPHome's C++ conventions and the repository `.clang-format`.

- Namespace: `esphome::nspanel_easy`. Close namespaces with a comment (`}  // namespace esphome::nspanel_easy`).
- Headers use `#pragma once`. Page-specific files are wrapped in `#ifdef NSPANEL_EASY_PAGE_<NAME>`.
- Always comment what a preprocessor `#else` or `#endif` closes (e.g. `#endif  // NSPANEL_EASY_PAGE_UTILITIES`).
- Document public declarations with Doxygen blocks (`@brief`, `@param`, `@return`) and use trailing `///<` comments for constants and struct members. Keep trailing comments aligned.
- Comment non-obvious includes with what they provide (e.g. `#include "pages.h"  // For page_names and get_page_id`).
- Numeric parsing: use `parse_number<T>()` (returns `optional<T>`); never `stof`, `stoi` or `atoi`.
- Format strings: `PRId32`/`PRIu32` for fixed-width integers, `%zu` for `size_t`.
- Compare `std::string` with `==`; use `strcmp()` with `.c_str()` only against `char[]`.
- Bit-field members cannot be bound to references; use `static_cast<bool>(member)` when passing them.
- Use `get_page_id()` / `get_object_id()` compile-time lookups instead of `${PAGE_*_ID}` substitutions in new code.
- Prefer `ExternalRAMAllocator` (PSRAM) for large or growing containers.
- The loop task stack is static; check stack usage with `uxTaskGetStackHighWaterMark(nullptr)`, not heap functions.

## ESPHome YAML (esphome/)

- One concern per package file; follow the existing `nspanel_esphome_<area>_<name>.yaml` naming.
- New optional functionality should be opt-in, following the existing `include_action_*` pattern.
- `wait_for_trigger` does not support a dynamic `entity_id`.
- Nextion `on_setup` fires once per ESPHome run and cannot detect a display restart mid-run.
- Keep lines within 200 characters and use yamllint-clean syntax (`on:` keys need `# yamllint disable-line rule:truthy` where applicable).

## Blueprint (nspanel_easy_blueprint.yaml)

- Action calls to the panel use `nspanel_name`, not `device_name`.
- Validate entity IDs with `"." in entity_id` combined with `states[entity_id] is not none`.
- `has_value()` tests for a non-unknown/unavailable state, not existence.
- Jinja2 `~` is string concatenation, not bitwise NOT.
- `default()` arguments are evaluated eagerly; nested attribute access inside them can still raise. Use `default(x, true)` to also catch `None`.
- Use `select("search", ...)` for string lists, not `selectattr(None, "search", ...)`.
- `supported_color_modes` returns enum objects; map with `map(attribute='value') | list` before comparing.
- Compare versions as integer tuples, never lexicographically.
- Do not confuse `last_changed` with `last_updated`.
- `ServiceNotFound` is re-raised by Home Assistant even with `continue_on_error`.

## Nextion HMI (hmi/)

- `.hmi` and `.tft` files are binary. Do not attempt to edit them directly.
  Propose Nextion event code as text for the maintainer to apply in Nextion Editor, and review changes through the generated text in `hmi/dev/nextion2text/`.
- `if` conditions and commands such as `fill` and `cirs` do not accept inline arithmetic. Pre-compute values into scratch variables (`sys0`..`sys2`, `sya0`..`sya1`) first.
- Array indexes must also be pre-computed: `sys2=sys0+11` then `vis b[sys2],0`, never `vis b[sys0+11],0`.
- Variable names are limited to 14 characters.
- Escape sequences (`\uXXXX`, `\xNN`) are not supported in event code at runtime.
- Draw `fill` before `cirs` to avoid overwriting rounded corners.
- `vis` state is not preserved across page navigation, while `.txt` and `.pco` of global components are.
- `rest` always returns to the boot page and resets globals to the values declared in `Program.s`.
- Component names referenced from ESPHome must match the declared names exactly; wrong names produce silent `0x1A` errors.
- Icons come from the MDI font pipeline in `hmi/dev/fonts/`; `all_icons.h` and `all_icons.json` are generated and must not be edited by hand.

## Documentation

- Update `docs/` in the same PR when behavior, options or requirements visible to users change.
- Markdown must pass markdownlint (line length 200).
- Write for end users: describe behavior as it actually is, including limitations.

## Things agents must not do

- Edit files under `versioning/` or bump versions manually.
- Edit generated files (`all_icons.h`, `all_icons.json`, `hmi/dev/nextion2text/`) instead of their sources.
- Commit compiled binaries (`.tft`, `.bin`) unless explicitly asked.
- Reformat or restructure unrelated code in the same change.
- Remove comments, docs or existing opt-in guards without being asked.
