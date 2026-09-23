## What and why

<!-- What changes, and what problem it solves. Link the issue if there is one. -->

Closes #

## Checklist

<!-- Tick what applies. An unticked box is fine if you say why. -->

- [ ] The PR title is a valid Conventional Commit - **we squash-merge, so the title becomes the
      commit on `main`** (`docs/contributing/commit-convention.md`)
- [ ] `tools/lint/run_all.sh` passes locally
- [ ] Host tests added or updated for any new logic in `core/`
- [ ] Public API carries Doxygen comments (`EXTRACT_ALL = NO` means undocumented entities fail CI)
- [ ] New user-visible strings are i18n keys authored in `en-US`, with `pt-BR` updated
- [ ] No new version literals - use `nsx::core::version::kString`
- [ ] Layering respected: `core -> platform -> infra -> domain -> ui -> app`

## Architecture decisions

- [ ] This changes no recorded decision
- [ ] This adds or supersedes an ADR (included in this PR, index row added)

## Device testing

<!-- Anything touching platform/, infra/, ui/ or the update path cannot be covered by host
     tests. Say what you ran and on what. See docs/contributing/testing.md. -->

- Console model: <!-- Erista / Mariko / not applicable -->
- Firmware / CFW:
- Smoke checklist items exercised:
