---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["i18n", "ui"]
---

# 0012. Use Borealis i18n with en-US as the source of truth

## Context and Problem Statement

The predecessor's internationalisation was fiction. It shipped twelve locale directories, and:

* eight of them - `de`, `es`, `fr`, `it`, `ja`, `pl`, `zh-CN`, `zh-TW` - were **byte-identical to
  each other**, all containing Brazilian Portuguese;
* `en-US/menus.json` also contained Portuguese;
* `ja/menus.json` literally read *"O {} ({}) usa a licenca GPL-3.0"*;
* only `brls.json`, ten lines of `ok`/`back`/`exit` inherited from the framework, was genuinely
  translated.

Meanwhile the strings users actually saw were **hardcoded Portuguese literals in nine `.cpp`
files** - `tools_tab.cpp`, `cheats_tab.cpp`, `saves_tab.cpp`, `ports_tab.cpp`,
`sysmodules_page.cpp`, `ftp_page.cpp`, `dns_test_page.cpp`, `credits_tab.cpp`,
`splash_page.cpp` - which bypassed the i18n system entirely.

The application claimed twelve languages and spoke one, and no locale could be added without
editing C++.

Meanwhile this project's stated policy is that everything is standardised in English, while the
actual user base is Brazilian.

## Decision Drivers

* English is the project language; Portuguese is the user language. Both must be first class.
* No user-visible string may be a literal in a `.cpp` file.
* A locale must be addable without touching code.
* A locale must not be able to claim coverage it does not have.

## Considered Options

* Borealis i18n with `en-US` as source of truth and `pt-BR` as a shipped translation
* Borealis i18n with `pt-BR` as source of truth and `en-US` as the translation
* `pt-BR` only, dropping the fake locales
* gettext / `.po` files

## Decision Outcome

Chosen option: **Borealis i18n, with `assets/i18n/en-US/` as the source of truth.**

* Every user-visible string is a key. `nsx::ui::i18n` exposes key constants so a typo is a
  compile error rather than a missing string at runtime.
* `en-US` is **authored first and reviewed in the pull request**. It is what a reviewer reads.
* `pt-BR` is a **first-class shipped translation**, updated in the same pull request, because
  that is who uses this software.
* **A locale directory exists only if it is genuinely translated.** No placeholder locales, ever.
* Keys are `dotted.lower.case`, grouped by screen: `update.progress.verifying`.

`tools/lint/check_i18n.py` fails CI when a locale is missing a key present in `en-US`, has an
extra key, or has a different `{}` placeholder **count** for the same key. The placeholder check
matters: a translator dropping a `{}` is a formatting crash on a device, not a cosmetic issue.

### Consequences

* Good, because the source strings are readable by anyone reviewing the code, satisfying the
  English-standardisation goal without abandoning Brazilian users.
* Good, because the fake-locale failure is now mechanically impossible - a locale either has
  every key or CI fails.
* Good, because adding a language is a pull request touching only JSON.
* Good, because hardcoded literals are caught in review and, once the UI layer exists, can be
  linted for.
* Bad, because every string change is now two files, not one. That is the cost of not shipping
  a lie.
* Bad, because `en-US` being canonical means a Portuguese-thinking author writes English first.
  Accepted deliberately: it is the project language, and it keeps the reviewable artefact
  readable.
* Neutral, because only two locales will ship for a long time. Two real locales beat twelve fake
  ones.

## Pros and Cons of the Options

### en-US source of truth, pt-BR translation

* Good, because code, comments, commits and strings are all in one language.
* Good, because contributors who do not read Portuguese can still review UI changes.
* Bad, because the primary audience's language is the derived one, so a `pt-BR` wording issue is
  caught one step later.

### pt-BR source of truth, en-US translation

* Good, because the audience's language gets authored and reviewed first.
* Bad, because it contradicts the project-wide English standard and makes UI review inaccessible
  to non-Portuguese speakers.
* Bad, because `en-US` would drift into being the neglected one - which is exactly how the
  predecessor ended up with Portuguese inside `en-US/menus.json`.

### pt-BR only

* Good, because it is the honest minimum: one locale, fully translated, no pretence.
* Good, because it halves the work per string.
* Bad, because it forecloses an English UI without a later migration, and English is the project
  language.

### gettext

* Good, because it is mature, with plural forms, context and a real translator toolchain.
* Bad, because Borealis already has an i18n system, so this would mean bridging two.
* Bad, because `.po` tooling is heavy for two locales and adds a build dependency.

## More Information

* [`tools/lint/check_i18n.py`](../../tools/lint/check_i18n.py)
* [`assets/i18n/`](../../assets/i18n/)
* [`PROJECT_SCOPE.md` section 17](../PROJECT_SCOPE.md#17-language-policy)

Revisit if a third locale is genuinely offered by a contributor - at that point plural-form
handling may justify reconsidering gettext.
