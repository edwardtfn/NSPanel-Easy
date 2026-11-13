# Contributing to NSPanel Easy

Thank you for your interest in contributing to NSPanel Easy.
This document covers everything you need to know before opening a pull request.
All participants are expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

---

## Table of Contents

- [PR Title Format](#pr-title-format)
- [Labels](#labels)
- [Commit Messages](#commit-messages)
- [Code Style](#code-style)
- [Branching](#branching)
- [AI-Assisted Contributions](#ai-assisted-contributions)

---

## PR Title Format

Every pull request title **must** follow this format:

```text
<prefix>: <Description starting with a capital letter>
```

- The prefix is **always lowercase**.
- A colon and a single space separate the prefix from the description.
- The description starts with a **capital letter**.
- **No trailing period.**

### Allowed Prefixes

| Prefix | When to use |
| --- | --- |
| `fix:` | Something was broken and is now corrected |
| `feat:` | A new user-facing feature or capability |
| `improve:` | An enhancement to existing behavior that is neither a bug fix nor a new feature |
| `ci:` | Changes limited to GitHub Actions workflows or CI scripts |
| `docs:` | Documentation-only changes |
| `style:` | Cosmetic or formatting changes with no logic impact |
| `build:` | Build system, dependencies, or packaging |
| `refactor:` | Internal restructuring with no behavior change |
| `test:` | Test additions or corrections only |
| `chore:` | Maintenance tasks that do not fit any other category |

### Examples

```text
fix: Correct baud rate negotiation timeout on EU model
feat: Add per-page brightness control
improve: Reduce boot time by deferring TFT version check
ci: Add PR title validation workflow
docs: Document release prefix conventions
```

### Why This Matters

PR titles become GitHub Release titles verbatim. Users reading the release history
use the prefix to understand at a glance whether an update is a bug fix, a new
feature, or an internal change. Automated pipelines built by community members
also parse these prefixes to classify releases — a non-conforming title will
break those integrations.

A required CI check enforces this format on every PR. The check runs on
`.github/workflows/validate_pr_title.yml`.

---

## Labels

You do not need to label your pull request: a workflow (`.github/workflows/labeler.yml`) does it,
and keeps the labels up to date when the title or the files change.

**By type**, from the prefix of the title. These labels decide the section of the release notes
a pull request is listed in (`.github/release.yml`):

| Prefix | Label | Section of the release notes |
| --- | --- | --- |
| `fix:` | `Bug` | Bug Fixes |
| `feat:` | `Enhancement` | New Features |
| `improve:` | `Improvement` | Improvements |
| `docs:` | `Documentation` | Documentation |
| `ci:` | `ci` | CI & Automation |
| `style:` | `Style` | Style & Formatting |
| `build:` | `Build` | Build System |
| `refactor:` | `Refactor` | Refactoring |
| `test:` | `Test` | Tests |
| `chore:` | `Chore` | Chores |

**By area**, from the files changed (rules in `.github/labeler.yml`). A pull request gets one label
for each area it touches:

| Label | Files |
| --- | --- |
| `Area: Blueprint` | `nspanel_easy_blueprint.yaml` |
| `Area: Firmware` | `esphome/`, `components/`, `nspanel_esphome*.yaml` |
| `Area: TFT` | `hmi/src/` |
| `Area: Documentation` | any `*.md` |
| `Area: CI` | `.github/`, `.rules/`, `hmi/dev/scripts/` |
| `Area: Tests` | `.test/` |

The two sets are separate on purpose: a feature that also updates the documentation is labelled
`Enhancement` and `Area: Documentation`, not `Documentation`.

The workflow cannot label a pull request from a fork, as its token is read-only there.
A maintainer adds those labels by hand.

---

## Commit Messages

Individual commit messages within a PR follow the same prefix convention and
format as PR titles. Squash commits inherit the PR title, so keeping them
consistent is the most important thing.

---

## Code Style

- **C++**: 2-space indentation; formatted with `clang-format` (config at `.clang-format`).
  The CI workflow applies and auto-commits fixes.
- **YAML**: Validated with `yamllint` (config at `.rules/yamllint.yml`).
- **Markdown**: Validated with `markdownlint-cli2` (config at `.rules/.markdownlint.jsonc`).
  The CI workflow applies and auto-commits fixes.

All linters run automatically on pull requests. You do not need to run them locally,
but doing so before pushing will save you a round trip.

---

## Branching

Work on a feature branch and open a PR targeting `dev`.
Keep branches short-lived and scoped to a single logical change.

---

## AI-Assisted Contributions

AI-assisted contributions are welcome. You are expected to understand, test, and stand behind the code you submit.

For how AI is used by the maintainer in this project, see [How AI Is Used in This Project](docs/ai_usage.md).
