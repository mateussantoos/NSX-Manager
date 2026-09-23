# Handoff fixtures

The state machine `nsx::core::Handoff` drives, tested without a console.

| Fixture | Must produce |
|---|---|
| `valid.json` | a swap may proceed |
| `exhausted.json` | `attempts == max_attempts` - do **not** retry; take the rollback branch and restore `backup_nro` |
| `relative-path.json` | every path rejected. This is the predecessor's bug preserved as a test: `app-forwarder/source/main.cpp:28` opened `"forwarder.conf"` relative to the CWD, worked only because hbmenu happens to `chdir`, and on failure produced empty strings that were then passed to `rename()`. Paths in a handoff are absolute or the handoff is invalid. |
