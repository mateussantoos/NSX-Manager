---
status: "accepted"
date: 2026-09-26
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["core", "logging"]
---

# 0020. Structured file and console logging

## Context and Problem Statement

Previous iterations of NSX Manager relied exclusively on standard output (`stdout`/`stderr`) and ad-hoc `printf` statements.
On Nintendo Switch consoles running Horizon OS, `stdout` output is lost once an applet terminates unless an nxlink server
or USB debugging shell is connected. When troubleshooting system-level maintenance failures, incomplete CFW migrations,
or SD card archive-bit issues, persistent post-mortem diagnostics are essential.

## Decision Drivers

* Thread-safe, non-blocking diagnostic logging across background worker jobs and the UI thread.
* Dual output: real-time console stdout for development and persistent SD storage logging for field diagnostics.
* Safe degradation: logging must never panic, block, or corrupt state if SD storage is unmounted, read-only, or full.
* Size management: automated log rotation or file capping to prevent unbounded SD card space consumption.

## Considered Options

* Heavy external logging libraries (e.g., spdlog, boost.log).
* Ad-hoc `std::printf` with manual redirect wrappers.
* Lightweight, thread-safe first-party `FileLogger` in `core/log`.

## Decision Outcome

Chosen option: **lightweight, thread-safe first-party `FileLogger` in `core/log`**.

`FileLogger` provides structured formatted output:
`[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [ThreadID] Message`
Output is simultaneously written to standard output and appended to `sdmc:/switch/nsx-manager/nsx.log`.
The logger is thread-safe using standard mutex locking, caps file growth at a configurable limit (default 1 MB),
and provides graceful error handling when SD storage is unmounted or write-protected.

### Consequences

* Positive: Diagnostic traces survive application exits and system reboots.
* Positive: Zero external runtime dependencies; fully host-testable with in-memory or temporary file backends.
* Positive: Strictly bounded disk footprint with size capping and atomic flush.
