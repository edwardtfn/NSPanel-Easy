# How AI Is Used in This Project

AI tools (mainly Claude by Anthropic) assist with maintaining this project.
This page describes how, so you know what to expect.

## What Is Automated

- **Pull request reviews:** [Sourcery](https://github.com/apps/sourcery-ai) and [CodeRabbit](https://github.com/apps/coderabbitai)
  automatically review pull requests. Their comments are suggestions; they do not approve or merge anything.

Nothing else is triggered automatically. No bot replies to issues or discussions on its own.

## What Is AI-Assisted

- Drafting answers to issues and discussions
- Writing or refactoring code, documentation, and tests
- Investigating bugs and reviewing logs

In all cases, the work is started manually by the maintainer. Each answer is reviewed, and often edited,
before it is posted. Each code change is reviewed and tested before it is merged.

## Identifying AI Posts

Posts made through the AI integration always appear under the
[panel-easy-assistant](https://github.com/apps/panel-easy-assistant) bot account
(shown as `panel-easy-assistant[bot]`), never under my personal account ([@edwardtfn](https://github.com/edwardtfn)).
The bot is a GitHub App owned and operated by me, and it only acts when I trigger it.

Comments under my personal account are my own, though I sometimes use AI to help draft them before posting.

I still personally read every issue, discussion, and pull request, and I remain responsible for everything
posted or merged here.

## Your Data

Content you post publicly here (text, logs, configuration) may be shared with an AI provider when I ask for help
analyzing it. Never post secrets such as API keys, passwords, Wi-Fi credentials, or tokens.
Redact them before sharing logs or YAML.

Private vulnerability reports, submitted as described in the [Security Policy](https://github.com/edwardtfn/NSPanel-Easy/security/policy), are never shared with AI tools.

## Contributions Using AI

AI-assisted contributions are welcome. You are expected to understand, test, and stand behind the code you submit.
See [CONTRIBUTING.md](https://github.com/edwardtfn/NSPanel-Easy/blob/main/CONTRIBUTING.md) for the general contribution guidelines.
