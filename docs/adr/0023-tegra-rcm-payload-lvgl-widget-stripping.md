---
status: "accepted"
date: 2026-09-26
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["payload", "rcm", "footprint"]
---

# 0023. Tegra RCM payload LVGL widget stripping

## Context and Problem Statement

Tegra X1 hardware recovery mode (RCM) loads initial payloads into IRAM with an absolute size limit of 126,296 bytes (0x1ECFC).
Any RCM binary exceeding this limit causes Tegra bootrom verification failure and payload injection failure.
The standalone RCM payload in `apps/rcm-payload/` includes BDK and the LVGL embedded graphics library.
As features expand, keeping the compiled binary safely below this hardware threshold requires aggressive dead-code elimination
and widget exclusion.

## Decision Drivers

* Strict compliance with the 126,296-byte IRAM hardware buffer limit.
* Retention of basic boot logo and recovery progress displays without superfluous GUI widgets.
* Full preservation of ADR-0011 license isolation (BDK GPL-2.0-only isolated from NSX Manager GPLv3).

## Considered Options

* Include default LVGL full widget suite.
* Exclude non-essential widgets via build configuration (`lv_objx.mk` and `lv_conf.h`).

## Decision Outcome

Chosen option: **disable and exclude non-essential LVGL widgets in `apps/rcm-payload/bdk/libs/`**.

The following unused widgets are disabled in `lv_conf.h` and excluded from `lv_objx.mk`:
`lv_calendar.c`, `lv_chart.c`, `lv_gauge.c`, `lv_spinbox.c`, `lv_table.c`, `lv_tileview.c`, `lv_win.c`, `lv_roller.c`, `lv_preload.c`.
Essential primitives (labels, images, containers, bars) are retained.
This reduces the compiled payload binary to 75,809 bytes, providing over 50 KB of safety margin below the 126,296-byte hardware limit.

### Consequences

* Positive: Payload size drops to 75,809 bytes (60% of hardware capacity).
* Positive: Faster bootup and payload execution time in recovery mode.
* Positive: Compliant with hardware constraints and ADR-0011 license boundaries.
