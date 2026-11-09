#!/usr/bin/env python3
"""Create or update the comment of a CI report on a pull request.

Each report has a single comment per PR, found by a hidden marker, so later runs
update it instead of adding new comments.

Meant for workflow_run workflows, which have a write token also for PRs from forks.
The PR number comes from an artifact of the triggering run, so it is untrusted:
it is used only when that PR's head is the commit of the triggering run. A
different head also means a newer push, whose own run updates the comment.

Usage:
  upsert_pr_comment.py --pr-number-file FILE --marker NAME --body-file FILE [--update-only]

  --pr-number-file FILE  file with the PR number (from the triggering run's artifact)
  --marker NAME          report id, stored as a hidden marker in the comment
  --body-file FILE       Markdown body of the comment
  --update-only          only update an existing comment, never create one

Environment:
  GH_TOKEN             token with pull-requests: write
  GITHUB_REPOSITORY    owner/repo (set by GitHub Actions)
  HEAD_SHA             head commit of the triggering run
"""

import argparse
import json
import os
import re
import sys
import urllib.request

API_URL = "https://api.github.com"
BOT_LOGIN = "github-actions[bot]"
# GitHub's limit is 65,536 characters; keep a margin for the marker and footers
MAX_BODY_CHARS = 60000


def api(method, path, data=None):
    """Call the GitHub REST API and return the decoded JSON response."""
    request = urllib.request.Request(
        f"{API_URL}/{path}",
        data=json.dumps(data).encode("utf-8") if data is not None else None,
        method=method,
        headers={
            "Authorization": f"Bearer {os.environ['GH_TOKEN']}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def read_pr_number(path):
    """Read and validate the PR number from the (untrusted) artifact file."""
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read(32).strip()
    if not re.fullmatch(r"[1-9][0-9]{0,9}", text):
        raise ValueError(f"invalid PR number in {path}")
    return int(text)


def find_comment(repo, pr_number, marker):
    """Return the id of the existing report comment, or None."""
    page = 1
    while True:
        comments = api("GET", f"repos/{repo}/issues/{pr_number}/comments?per_page=100&page={page}")
        for comment in comments:
            if comment["user"]["login"] == BOT_LOGIN and comment["body"].startswith(marker):
                return comment["id"]
        if len(comments) < 100:
            return None
        page += 1


def main():
    parser = argparse.ArgumentParser(description="Create or update the comment of a CI report on a PR.")
    parser.add_argument("--pr-number-file", required=True)
    parser.add_argument("--marker", required=True)
    parser.add_argument("--body-file", required=True)
    parser.add_argument("--update-only", action="store_true")
    args = parser.parse_args()

    if not re.fullmatch(r"[a-z0-9-]+", args.marker):
        print("::error::The marker must contain only lowercase letters, digits and hyphens")
        return 1
    repo = os.environ["GITHUB_REPOSITORY"]
    head_sha = os.environ["HEAD_SHA"]
    marker = f"<!-- ci-report:{args.marker} -->"

    try:
        pr_number = read_pr_number(args.pr_number_file)
    except (OSError, ValueError) as error:
        print(f"::error::{error}")
        return 1

    pr_head = api("GET", f"repos/{repo}/pulls/{pr_number}")["head"]["sha"]
    if pr_head != head_sha:
        print(f"PR #{pr_number} head is {pr_head}, not {head_sha}: skipping")
        return 0

    with open(args.body_file, encoding="utf-8", errors="replace") as f:
        body = f"{marker}\n{f.read(MAX_BODY_CHARS)}"

    comment_id = find_comment(repo, pr_number, marker)
    if comment_id is not None:
        api("PATCH", f"repos/{repo}/issues/comments/{comment_id}", {"body": body})
        print(f"Updated comment {comment_id} on PR #{pr_number}")
    elif args.update_only:
        print(f"No comment to update on PR #{pr_number}: skipping")
    else:
        comment_id = api("POST", f"repos/{repo}/issues/{pr_number}/comments", {"body": body})["id"]
        print(f"Created comment {comment_id} on PR #{pr_number}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
