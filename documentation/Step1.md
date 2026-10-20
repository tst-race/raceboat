# Step 1: Introduce `LinkRole`/`LinkDirectionality`/`ConnEstablishment` primitives

## Goal

Mechanical, behavior-preserving refactor of `ConnectionStateMachine.cpp`'s
existing create-vs-load / send-vs-recv-vs-bidi logic to be expressed in terms
of a new small value-type layer, as a first step toward the broader
role-based link-selection redesign discussed in `link-management.md`.

## Changes made

- **New file**: `source/state-machine/LinkEstablishment.h`
  - `enum class LinkRole { Creator, Loader }` — who creates vs. loads the link.
  - `enum class LinkDirectionality { Send, Recv, Bidi }` — which direction(s)
    the connection carries.
  - `struct ConnEstablishment` — holds a `LinkRole` + `LinkDirectionality`,
    with:
    - `fromLegacy(creating, sending, bidirectional)` — derives the new type
      from the existing three legacy bools, so no call sites needed to
      change their inputs.
    - `isCreator()`, `isBidi()`, `isSend()` — convenience predicates that
      mirror the boolean checks previously scattered across
      `ConnectionStateMachine.cpp`.
    - `toLinkType()` — maps directly to the `LinkType` (`LT_SEND`/`LT_RECV`/
      `LT_BIDI`) passed to `plugin.openConnection`, replacing the inline
      ternary that used to compute this.

- **`source/state-machine/ConnectionStateMachine.h`**
  - Added `ConnEstablishment establishment;` member to `ApiConnContext`,
    alongside (not replacing) the existing `create`/`send`/`bidirectional`
    bools. The legacy bools are left in place and still set, since other
    code (logging, `updateLinkStatusChanged`, etc.) reads them and this step
    is scoped to be additive/behavior-preserving only.

- **`source/state-machine/ConnectionStateMachine.cpp`**
  - `updateConnStateMachineStart` and `updateConnStateMachineStartBidi` now
    also populate `establishment` via `ConnEstablishment::fromLegacy(...)`
    using the same inputs that set the legacy bools.
  - `StateConnActivated::enter()`: replaced `if (ctx.create)` with
    `if (ctx.establishment.isCreator())` for the createLink/loadLinkAddress
    branch.
  - `StateConnLinkEstablished::enter()`: replaced `if (!ctx.send && ...)`
    with `if (!ctx.establishment.isSend() && ...)` for the received-address
    validation branch, and replaced the inline
    `ctx.bidirectional ? LT_BIDI : (ctx.send ? LT_SEND : LT_RECV)`-style
    ternary with `ctx.establishment.toLinkType()` / `isBidi()`.

No other files were touched. No state-transition tables, event names, or
control flow were changed — only the *source* of the three booleans
consulted at two call sites was swapped from raw fields to the new
`ConnEstablishment` accessors.

## Build

Rebuilt via the documented 3-step sequence to pick up the SDK source change
without touching the `:latest` tags:

1. `raceboat-builder:latest` → `./build.sh` (compiled cleanly, no errors/warnings
   introduced by this change).
2. `raceboat-plugin-builder-image/build_image.sh --version=working`
3. `raceboat-runtime-image` via `docker buildx build --build-context
   ...plugin-builder=docker-image://...:working ... -t ...runtime:working`

Both `:working` images built successfully (confirmed via `docker images`
timestamps).

## Regression suite results (all 8 scenarios, run against `:working` images)

| # | Scenario | Result |
|---|----------|--------|
| 1 | `racebird-client-connect` | PASSED |
| 2 | `decomposed-client-connect-multi-client` | PASSED |
| 3 | `bootstrap-decomposed-racebird` | PASSED (final-link topology: OK) |
| 4 | `bootstrap-racebird-multi-client` | PASSED (final-link topology: OK) |
| 5 | `bootstrap-racebird-decomposed-multi-client` | PASSED (final-link topology: OK) |
| 6 | `bootstrap-decomposed-decomposed-multi-client` | PASSED (final-link topology: SKIPPED — known same-gid limitation, not a regression) |
| 7 | `bootstrap-racebird-decomposed` (single-client) | PASSED (final-link topology: OK) |
| 8 | `bootstrap-decomposed-decomposed` (single-client) | PASSED (final-link topology: SKIPPED — same known limitation) |

All 8 scenarios were executed fresh in this session (not assumed from a prior
turn) with `run_scenario.py --image-tag working`, and each printed
`INTEGRATION TEST PASSED ✓` plus the expected topology verdict. No
regressions were introduced by the refactor.

`bootstrap-racebird-racebird` (both single- and multi-client) was **not**
re-tested here — it is already documented as a known-broken combo
(independent, deeper data-plane bug, see `link-management.md` and repo
memory) and is intentionally excluded from this suite's pass/fail gate.

## Issues uncovered

None. This step was purely mechanical: the two call sites that previously
read `ctx.create`/`ctx.send`/`ctx.bidirectional` directly now read the
equivalent derived value from `ctx.establishment`, and `fromLegacy()`
reproduces the exact same truth table as before
(`bidirectional ? Bidi : (sending ? Send : Recv)`, `creating ? Creator :
Loader`). No behavior change was expected or observed.

One pre-existing hygiene issue was encountered and worked around (not
caused by this change): docker leaves root-owned files under
`test/integration/generated/<scenario-id>/`, which `generate_scenario.py`'s
`shutil.rmtree` cannot remove on a subsequent run without `sudo rm -rf`
first. This is already documented in `test/integration/README.md`'s
Troubleshooting section.

## Assessment / carry-forward

- The two remaining legacy bools still read directly in
  `ConnectionStateMachine.cpp` outside the two refactored sites (e.g.
  `ctx.bidirectional` in logging, `ctx.create`/`ctx.send` in
  `updateLinkStatusChanged` callers elsewhere) were intentionally left
  alone in this step to keep the diff mechanical and low-risk. A later step
  should finish migrating all remaining reads to `establishment` and then
  consider removing the now-redundant legacy bools entirely.
- `ConnEstablishment` currently only wraps the *outcome* of the existing
  role decision (via `fromLegacy`); it does not yet make the decision
  itself. The next step per the migration plan is to move
  `shouldCreateSender`/`shouldCreateReceiver`-equivalent decision logic
  behind a `resolveRole`-style function that produces a `ConnEstablishment`
  directly (particularly for the `LD_BIDI`-with-explicit-override and the
  final-slot non-merged-branch inconsistency flagged in
  `link-management.md`), rather than deriving it after the fact from
  booleans computed elsewhere.
- No test failures require remediation right now — the suite is green.
