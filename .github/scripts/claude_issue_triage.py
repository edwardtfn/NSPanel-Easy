#!/usr/bin/env python3
"""Deterministic steps around the AI issue triage (.github/workflows/claude_issue_triage.yml).

The AI agent never gets a tool that writes to GitHub. This script does what it cannot:

  fetch  Collects what the agent reads (the issue, the labels it may use and the other
         issues) into a folder of the checkout, and decides whether the agent runs at all
         (rate limits against issue floods).
  apply  Validates the agent's structured answer and applies it to the issue: labels from
         an allowlist only, links to possible duplicates, and one sanitized comment.

Everything the agent returns is treated as untrusted: issue text is written by the public
and may try to steer the agent (prompt injection). Whatever it answers, the worst outcome
is a wrong label or a misleading (but link-free and mention-free) comment on that issue.

Usage:
  claude_issue_triage.py fetch --issue N --out DIR
  claude_issue_triage.py apply --issue N [--dry-run]

Environment:
  GH_TOKEN             token with issues: read (fetch) or issues: write (apply)
  GITHUB_REPOSITORY    owner/repo (set by GitHub Actions)
  GITHUB_OUTPUT        step outputs file (fetch writes run_agent and skip_reason)
  TRIAGE_RESULT        the agent's structured answer, as JSON (apply)
"""

import argparse
import datetime
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

API = "https://api.github.com"

# -----------------------------------------------------------------------------
# Labels the agent may apply. Everything else (stale, wontfix, duplicate, invalid,
# Effort: *, ...) stays a maintainer decision.
# -----------------------------------------------------------------------------
LABEL_PREFIXES = ("Add-on: ", "Area: ", "Core: ", "Hardware: ", "HW: ", "Page: ")
LABEL_EXACT = {
    "Blueprint",
    "Bluetooth in use",
    "Boot/Initializing",
    "Bug",
    "Build",
    "Chips",
    "Compiling",
    "Customization",
    "Design",
    "Documentation",
    "Enhancement",
    "First installation",
    "Framework",
    "Improvement",
    "Migrating",
    "Performance",
    "question",
    "Upload TFT",
}
IMPACT_LABELS = {"low": "Impact: low", "medium": "Impact: medium", "high": "Impact: high"}
MISSING_INFO_LABEL = "\U0001F440 Missing information"
MAX_TOPIC_LABELS = 6
MAX_DUPLICATES = 3

# -----------------------------------------------------------------------------
# Rate limits: the agent is skipped (the issue is left for manual triage) above these.
# -----------------------------------------------------------------------------
MAX_ISSUES_PER_AUTHOR_24H = 3
MAX_ISSUES_REPO_24H = 15

# -----------------------------------------------------------------------------
# Context given to the agent
# -----------------------------------------------------------------------------
MAX_ISSUE_PAGES = 10          # 100 issues/PRs per page, most recently updated first
ISSUE_BODY_LIMIT = 20000      # Characters kept from the triaged issue's body
OTHER_BODY_LIMIT = 600        # Characters kept from each other open issue's body
CLOSED_ISSUES_MAX_AGE_DAYS = 365

# -----------------------------------------------------------------------------
# Comment sanitizing
# -----------------------------------------------------------------------------
COMMENT_LIMIT = 3000
COMMENT_MARKER = "<!-- claude-issue-triage -->"
AI_USAGE_URL = "https://github.com/edwardtfn/NSPanel-Easy/blob/main/docs/ai_usage.md"
MAINTAINER_ASSOCIATIONS = {"OWNER", "MEMBER", "COLLABORATOR"}
# Links are kept only when they point to the project or to the official upstream projects.
# This also covers ESPHome remote packages/components ("github://owner/repo/..."): a
# suggestion to install code from anywhere else must never reach users.
ALLOWED_GITHUB_OWNERS = {"edwardtfn", "esphome", "home-assistant"}
ALLOWED_HOSTS = {
    "edwardtfn.github.io",
    "esphome.io",
    "home-assistant.io",
    "www.home-assistant.io",
    "community.home-assistant.io",
    "developers.home-assistant.io",
    "nextion.tech",
}
LINK_RE = re.compile(r"(?i)\b(?:[a-z][a-z0-9+.-]*://|www\.)[^\s<>()\[\]`\"']+")
SECRET_RES = (
    re.compile(r"sk-ant-[A-Za-z0-9_-]{8,}"),
    re.compile(r"gh[pousr]_[A-Za-z0-9]{20,}"),
    re.compile(r"github_pat_[A-Za-z0-9_]{20,}"),
    re.compile(r"[A-Za-z0-9+/=_-]{80,}"),  # Long opaque blobs (encoded data)
)


def api(method, path, token, payload=None):
    """Call the GitHub REST API and return the decoded JSON answer."""
    data = json.dumps(payload).encode() if payload is not None else None
    request = urllib.request.Request(API + path, data=data, method=method)
    request.add_header("Authorization", f"Bearer {token}")
    request.add_header("Accept", "application/vnd.github+json")
    request.add_header("X-GitHub-Api-Version", "2022-11-28")
    if data is not None:
        request.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(request, timeout=30) as response:
        body = response.read()
    return json.loads(body) if body else None


def is_allowed_label(name):
    """Whether the agent may apply this label (impact and missing-info have their own fields)."""
    return name in LABEL_EXACT or name.startswith(LABEL_PREFIXES)


def set_output(name, value):
    """Write a step output (or print it when not running in GitHub Actions)."""
    path = os.environ.get("GITHUB_OUTPUT")
    if path:
        with open(path, "a", encoding="utf-8") as handle:
            handle.write(f"{name}={value}\n")
    print(f"{name}={value}")


# =============================================================================
# fetch
# =============================================================================
def fetch(repo, token, number, out_dir):
    """Write the agent's context to out_dir and decide whether the agent runs."""
    os.makedirs(out_dir, exist_ok=True)
    now = datetime.datetime.now(datetime.timezone.utc)

    issue = api("GET", f"/repos/{repo}/issues/{number}", token)
    if "pull_request" in issue:
        set_output("run_agent", "false")
        set_output("skip_reason", "not an issue")
        return
    author = issue["user"]["login"]

    labels = []
    page = 1
    while True:
        batch = api("GET", f"/repos/{repo}/labels?per_page=100&page={page}", token)
        labels.extend({"name": item["name"], "description": item.get("description") or ""} for item in batch if is_allowed_label(item["name"]))
        if len(batch) < 100:
            break
        page += 1

    others = []
    recent_total = 0
    recent_author = 0
    for page in range(1, MAX_ISSUE_PAGES + 1):
        batch = api("GET", f"/repos/{repo}/issues?state=all&sort=updated&direction=desc&per_page=100&page={page}", token)
        for item in batch:
            if "pull_request" in item or item["number"] == number:
                continue
            created = datetime.datetime.fromisoformat(item["created_at"].replace("Z", "+00:00"))
            if (now - created).total_seconds() < 86400:
                recent_total += 1
                if item["user"]["login"] == author:
                    recent_author += 1
            entry = {
                "number": item["number"],
                "title": item["title"],
                "state": item["state"],
                "labels": [label["name"] for label in item["labels"]],
            }
            if item["state"] == "open":
                entry["body_excerpt"] = (item.get("body") or "")[:OTHER_BODY_LIMIT]
            else:
                updated = datetime.datetime.fromisoformat(item["updated_at"].replace("Z", "+00:00"))
                if (now - updated).days > CLOSED_ISSUES_MAX_AGE_DAYS:
                    continue
                entry["state_reason"] = item.get("state_reason")
            others.append(entry)
        if len(batch) < 100:
            break

    with open(os.path.join(out_dir, "issue.json"), "w", encoding="utf-8") as handle:
        json.dump(
            {
                "number": issue["number"],
                "title": issue["title"],
                "author": author,
                "labels": [label["name"] for label in issue["labels"]],
                "body": (issue.get("body") or "")[:ISSUE_BODY_LIMIT],
            },
            handle,
            indent=2,
            ensure_ascii=False,
        )
    with open(os.path.join(out_dir, "labels.json"), "w", encoding="utf-8") as handle:
        json.dump(labels, handle, indent=2, ensure_ascii=False)
    with open(os.path.join(out_dir, "other_issues.json"), "w", encoding="utf-8") as handle:
        json.dump(others, handle, indent=1, ensure_ascii=False)

    # The triaged issue itself counts towards both limits
    if recent_author + 1 > MAX_ISSUES_PER_AUTHOR_24H:
        set_output("run_agent", "false")
        set_output("skip_reason", "too many issues from this author in 24 h")
    elif recent_total + 1 > MAX_ISSUES_REPO_24H:
        set_output("run_agent", "false")
        set_output("skip_reason", "too many issues in the repository in 24 h")
    else:
        set_output("run_agent", "true")


# =============================================================================
# apply
# =============================================================================
def link_is_allowed(link):
    """Whether a link found in the agent's comment may be shown to users."""
    candidate = link if "://" in link else "https://" + link
    try:
        parsed = urllib.parse.urlsplit(candidate)
    except ValueError:
        return False
    scheme = parsed.scheme.lower()
    host = (parsed.hostname or "").lower()
    if scheme == "github":
        # ESPHome shorthand: github://owner/repo/path@ref (the owner is parsed as the host)
        return host in ALLOWED_GITHUB_OWNERS
    if scheme != "https":
        return False
    if host in ("github.com", "raw.githubusercontent.com"):
        segments = [segment for segment in parsed.path.split("/") if segment]
        return bool(segments) and segments[0].lower() in ALLOWED_GITHUB_OWNERS
    return host in ALLOWED_HOSTS


def sanitize_comment(text):
    """Make the agent's comment safe to post: no foreign links, mentions, HTML or secrets."""
    text = text.replace("\r\n", "\n").strip()[:COMMENT_LIMIT]
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    text = re.sub(r"<[A-Za-z/!][^>]*>", "", text)            # HTML tags
    text = text.replace("![", "[")                           # No embedded images
    for pattern in SECRET_RES:
        text = pattern.sub("[removed]", text)
    text = LINK_RE.sub(lambda match: match.group(0) if link_is_allowed(match.group(0).rstrip(".,;:!?")) else "[link removed]", text)
    text = re.sub(r"(?<![\w/])@(?=[A-Za-z0-9])", "@\u200b", text)  # No user or team mentions
    return text.strip()


def apply(repo, token, number, result, dry_run):
    """Validate the agent's answer and apply it to the issue."""
    if not isinstance(result, dict):
        sys.exit("The triage result is not a JSON object")

    issue = api("GET", f"/repos/{repo}/issues/{number}", token)
    if "pull_request" in issue or issue["state"] != "open":
        print("Not an open issue, nothing to do")
        return

    # -------------------------------------------------------------------------
    # Labels: only existing, allowlisted ones; never removes a label
    # -------------------------------------------------------------------------
    existing = set()
    page = 1
    while True:
        batch = api("GET", f"/repos/{repo}/labels?per_page=100&page={page}", token)
        existing.update(item["name"] for item in batch)
        if len(batch) < 100:
            break
        page += 1

    labels = []
    for name in result.get("labels") or []:
        if isinstance(name, str) and name in existing and is_allowed_label(name) and name not in labels:
            labels.append(name)
    labels = labels[:MAX_TOPIC_LABELS]
    impact = IMPACT_LABELS.get(result.get("impact"))
    already_has_impact = any(label["name"].startswith("Impact: ") for label in issue["labels"])
    if impact and impact in existing and not already_has_impact:
        labels.append(impact)
    if result.get("needs_info") is True and MISSING_INFO_LABEL in existing:
        labels.append(MISSING_INFO_LABEL)

    # -------------------------------------------------------------------------
    # Possible duplicates: only real issues of this repository
    # -------------------------------------------------------------------------
    duplicates = []
    for candidate in result.get("duplicates") or []:
        if isinstance(candidate, bool) or not isinstance(candidate, int) or candidate == number or candidate in duplicates:
            continue
        try:
            other = api("GET", f"/repos/{repo}/issues/{candidate}", token)
        except urllib.error.HTTPError:
            continue
        if "pull_request" not in other:
            duplicates.append(candidate)
        if len(duplicates) >= MAX_DUPLICATES:
            break

    # -------------------------------------------------------------------------
    # Comment: none on the maintainers' own issues, or when there is nothing to say
    # -------------------------------------------------------------------------
    text = result.get("comment")
    text = sanitize_comment(text) if isinstance(text, str) else ""
    body = None
    if (text or duplicates) and issue.get("author_association") not in MAINTAINER_ASSOCIATIONS:
        parts = [
            COMMENT_MARKER,
            f"Hi @{issue['user']['login']}, thanks for opening this issue. I am an automated AI assistant doing a first-pass triage for @edwardtfn, "
            "who reads every issue. Take this as a first hint, not as the final answer.",
        ]
        if duplicates:
            parts.append("**Possibly related:** " + ", ".join(f"#{item}" for item in duplicates) + ". Please check whether one of them describes the same thing.")
        if text:
            parts.append(text)
        parts.append(f"---\n*Generated by AI and not reviewed before posting. See [How AI Is Used in This Project]({AI_USAGE_URL}).*")
        body = "\n\n".join(parts)

    print("Labels to add:", labels)
    print("Possible duplicates:", duplicates)
    print("Comment:\n" + (body or "(none)"))
    if dry_run:
        print("Dry run: nothing was changed")
        return

    if labels:
        api("POST", f"/repos/{repo}/issues/{number}/labels", token, {"labels": labels})
    if body:
        # A re-run updates the previous triage comment instead of adding a new one
        previous = None
        page = 1
        while previous is None:
            batch = api("GET", f"/repos/{repo}/issues/{number}/comments?per_page=100&page={page}", token)
            previous = next((item for item in batch if item["user"]["type"] == "Bot" and COMMENT_MARKER in (item.get("body") or "")), None)
            if len(batch) < 100:
                break
            page += 1
        if previous:
            api("PATCH", f"/repos/{repo}/issues/comments/{previous['id']}", token, {"body": body})
        else:
            api("POST", f"/repos/{repo}/issues/{number}/comments", token, {"body": body})


def main():
    """Entry point."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    fetch_parser = commands.add_parser("fetch")
    fetch_parser.add_argument("--issue", type=int, required=True)
    fetch_parser.add_argument("--out", required=True)
    apply_parser = commands.add_parser("apply")
    apply_parser.add_argument("--issue", type=int, required=True)
    apply_parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    repo = os.environ["GITHUB_REPOSITORY"]
    token = os.environ["GH_TOKEN"]
    if args.command == "fetch":
        fetch(repo, token, args.issue, args.out)
    else:
        try:
            result = json.loads(os.environ.get("TRIAGE_RESULT") or "")
        except json.JSONDecodeError:
            sys.exit("TRIAGE_RESULT is not valid JSON")
        apply(repo, token, args.issue, result, args.dry_run)


if __name__ == "__main__":
    main()
