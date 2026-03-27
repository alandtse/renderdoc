# Contributing to RenderDoc

This document is split up and organised into several sections to aid reading and linking. For small changes like one-line fixes or minor tweaks then you can just read the [quick start section](#quick-start) below.

Don't worry about reading all of these documents end-to-end and getting everything perfect the first time. The point of this information isn't to be restrictive about rules and reject contributions, but to give people guidance and help about how to contribute. I'm happy to help out with any changes needed to get your PR ready to merge, until you get the hang of things. If you're unfamiliar with git and need help making any changes, feel free to ask as well!

If you're a regular contributor or if you have a larger amount of code to change, please do read through these as it will make life easier for everyone if you to follow along with these guidelines from the start.

## Code of Conduct

I want to ensure that anyone can contribute to RenderDoc with only the next bug to worry about. For that reason the project has adopted the [contributor covenent](CODE_OF_CONDUCT.md) as a code of conduct to be enforced for anyone taking part in RenderDoc development. This includes any comments on issues or any public discussion e.g. in the #renderdoc IRC channel or discord server.

If you have any queries or concerns in this regard you can [open an issue](https://github.com/alandtse/renderdoc/issues) on this fork's repository.

## Use of LLMs / "AI"

**This fork permits the use of LLMs and AI-assisted tooling in development.** This is the primary policy difference from the upstream RenderDoc project, which prohibits LLM use entirely. Contributors to this fork may use LLM assistance when writing code or documentation.

## Acceptable use of RenderDoc

RenderDoc is a tool intended for debugging your own projects and programs, those to which you have true ownership of. Use and abuse of RenderDoc for illegal or unethical uses including but not limited to capturing copyrighted programs that you do not own the rights to will not be tolerated. Any questions or issues related to any such use will not be answered and no support will be provided.

## Copyright / Contributor License Agreement

This repository contains code under two licenses. Files inherited from upstream RenderDoc remain under their original **MIT License** ([LICENSE.md](../LICENSE.md)) — do not modify their copyright notices. Fork-specific contributions (new files and modifications made in this fork) are licensed under **GPL-3.0-or-later** ([COPYING](../COPYING)).

By submitting a pull request you accept the [Contributor License Agreement](../CONTRIBUTOR_LICENSE_AGREEMENT.md). **Read the full CLA before contributing.** Key points:

- Your contribution is licensed **GPL-3.0-or-later** in this fork.
- For upstream reintegration you pre-authorize a **copyright assignment to Baldur Karlsson** (the upstream maintainer), which is what the upstream project requires. This is not merely a license grant — it transfers copyright ownership. Once assigned, the upstream maintainer may use the contribution in any way copyright law permits, including proprietary use.
- This assignment is exercisable only while the upstream project remains OSI-licensed, and is irrevocable for any contribution already incorporated.

New fork-specific files should carry a GPL-3.0-or-later header with your copyright.

## Upstream-first policy

**Before opening a PR here, consider whether your change is eligible for upstream.**

If your change:
- Does not use LLM-assisted development, **and**
- Follows all [upstream contribution guidelines](https://github.com/baldurk/renderdoc/blob/v1.x/docs/CONTRIBUTING.md), **and**
- Is not fork-specific (e.g. not tied to a fork-only feature)

then submit it to [baldurk/renderdoc](https://github.com/baldurk/renderdoc) directly. Changes accepted upstream will flow into this fork automatically via the daily sync. This keeps the fork diff small and maximises the benefit to the broader RenderDoc community.

Submit here only if:
- Your change uses LLM tooling and cannot go upstream, **or**
- It builds on a fork-specific feature, **or**
- Upstream has declined or is unlikely to accept it.

When in doubt, try upstream first. You can always submit here if it is declined.

## Contributing information

1. [Dependencies](CONTRIBUTING/Dependencies.md)
2. [Compiling](CONTRIBUTING/Compiling.md)
3. [Preparing commits](CONTRIBUTING/Preparing-Commits.md)
4. [Developing a change](CONTRIBUTING/Developing-Change.md)
5. [Testing](CONTRIBUTING/Testing.md)
6. [Code Explanation](CONTRIBUTING/Code-Explanation.md)
7. [Filing issues](CONTRIBUTING/Filing-Issues.md)
8. [Asking Questions](CONTRIBUTING/Questions.md)

## Quick Start

The two things you'll need to bear in mind for a small change are the [commit message](CONTRIBUTING/Preparing-Commits.md#commit-messages) and [code formatting](CONTRIBUTING/Preparing-Commits.md#code-formatting).

Commit messages should have a first line with a **maximum of 72 characters**, then a gap, then if you need it a longer explanation in any format you want. The reason for this is that limiting the first line to 72 characters means that `git log` and github's history always displays the full message without it being truncated.

For more information, check the section about [commit messages](CONTRIBUTING/Preparing-Commits.md#commit-messages).

Code should be formatted using **clang-format 15.0**. The reason we fix a specific version of clang-format is that unfortunately different versions can format code in different ways using the same config file, so this would cause problems with automatic verification of code formatting.

For more information, check the section about [code formatting](CONTRIBUTING/Preparing-Commits.md#code-formatting).

**DO NOT** create "draft" pull requests ever. These are a pointless anti-feature from github and provide zero value and have zero purpose. If your code is not ready to merge, do not create a pull request at all.
