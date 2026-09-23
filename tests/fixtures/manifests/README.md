# Manifest fixtures

Each file is one rejection reason. Together they are the specification for
`nsx::core::parseManifest` - "what does the client do with a hostile or broken `update.json`"
is answered here rather than discovered in production.

| Fixture | Must produce |
|---|---|
| `valid.json` | a parsed manifest with two assets |
| `truncated.json` | `ManifestUnreadable` - the JSON ends mid-token |
| `future-schema.json` | refusal: `schema_version` 2 is newer than this client understands; the UI must tell the user to update manually rather than guess |
| `bad-schema.json` | refusal: wrong types throughout (`tag` numeric, `mandatory` a string, `assets` a string, `version` unparseable) |
| `missing-sha.json` | refusal: an asset without `sha256` can never be verified, so it must never be downloadable |
| `hostile-paths.json` | both assets rejected: one escapes its directory via `../`, the other is served over plaintext `http://` |

None of these may throw. A parser that terminates on bad input is the predecessor's bug
(`main_frame.cpp:57-72`, where `std::stoi` on a date-like tag raised an uncaught
`std::out_of_range`), and it is exactly what these fixtures exist to prevent.
