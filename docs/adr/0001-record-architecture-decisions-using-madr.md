---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["process", "docs"]
---

# 0001. Record architecture decisions using MADR

## Context and Problem Statement

NSX Manager is a rewrite whose foundations are being laid all at once: build system, source
layout, versioning, update mechanism, trust model, licensing. In six months nobody - including
the author - will remember why each was chosen, and the predecessor offers a concrete warning:
its `.gitmodules` declared three submodules while the sources were committed directly, and one
declared path did not exist at all. Nobody could tell whether that was a decision or an accident.

We need a durable, greppable record of *why*, not just *what*.

## Decision Drivers

* A decision must be reconstructible by someone who was not there.
* The **rejected** option and its reason matter as much as the chosen one - that is what stops a
  settled question being reopened.
* Low enough ceremony that it actually gets done by a single maintainer.
* Machine-checkable, so the record cannot silently drift from reality.

## Considered Options

* Trimmed MADR 4.0 in `docs/adr/`
* Nygard-style ADRs (Context / Decision / Consequences)
* A wiki or design docs in an external tool
* No formal records - rely on commit messages and code comments

## Decision Outcome

Chosen option: **trimmed MADR 4.0**, because every foundational decision in this project has a
genuine runner-up, and MADR is the only lightweight format that forces the runner-up to be
written down.

Conventions:

* `docs/adr/NNNN-kebab-case-title.md`, four-digit, allocated sequentially.
* **Numbers are never reused and never renumbered.** Links rot otherwise.
* Front-matter carries `status`, `date`, `deciders`, `supersedes`, `superseded-by`, `tags`.
* Lifecycle: `proposed -> accepted -> deprecated`, or `proposed -> rejected`, or
  `accepted -> superseded`. A superseded ADR is **never deleted or edited** beyond its
  front-matter - it is the historical record.
* `docs/adr/README.md` holds the index table.
* MADR's optional `Confirmation` and `Validation` sections are dropped as ceremony.

### Consequences

* Good, because the rejected alternative is captured while the reasoning is fresh.
* Good, because `tools/lint/check_adr_index.sh` fails CI when the index and the files disagree,
  when a status is invalid, or when a `superseded-by` link is not reciprocal - so the record
  cannot rot into fiction.
* Good, because `CONTRIBUTING.md` can point at a concrete process for proposing an architectural
  change.
* Bad, because MADR is more verbose than Nygard. Accepted: verbosity is the cost of capturing the
  runner-up, which is the whole point.
* Neutral, because 14 ADRs written at once is unusual. They document a foundation laid at once.

## Pros and Cons of the Options

### Trimmed MADR 4.0

* Good, because `Considered Options` and `Pros and Cons of the Options` make the alternatives
  explicit rather than implicit.
* Good, because the front-matter is machine-readable, enabling the CI index check.
* Good, because it is a recognised standard - a new contributor does not need to learn a local
  invention.
* Bad, because a trivial decision costs more to write up than it is worth. Mitigated by only
  using ADRs for genuinely architectural choices.

### Nygard ADRs

* Good, because they are shorter and faster to write.
* Bad, because the format never names the rejected option. "We will use CMake" does not tell you
  that the devkitPro Makefile was considered and why it lost, so the question returns.

### A wiki or external tool

* Good, because richer formatting and easier cross-linking.
* Bad, because it is not versioned with the code, cannot be reviewed in a pull request, and drifts
  the moment the code changes.
* Bad, because it cannot be checked by CI.

### No formal records

* Good, because zero overhead.
* Bad, because this is precisely how the predecessor arrived at a state where nobody could
  distinguish a decision from an accident.

## More Information

* [MADR](https://adr.github.io/madr/)
* Index and lifecycle: [`docs/adr/README.md`](README.md)
* Enforcement: [`tools/lint/check_adr_index.sh`](../../tools/lint/check_adr_index.sh)

Revisit if the ADR count grows past roughly 50 and the index becomes unwieldy, or if a second
maintainer finds the format an obstacle rather than an aid.
