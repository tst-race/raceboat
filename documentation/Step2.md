# Step 2: Migrate channel mode (Listen/DialStateMachine) onto ConnEstablishment

## Goal

Extend Step 1's `LinkRole`/`LinkDirectionality`/`ConnEstablishment` primitives
(introduced in `ConnectionStateMachine.cpp`) to the non-bootstrap "channel
mode" state machines — `ListenStateMachine.cpp` and `DialStateMachine.cpp` —
which already correctly use `startConnStateMachineBidi` for merged links.
This step is scoped to be low-risk: it does **not** touch the shared
`ApiManagerInternal::startConnStateMachine(Bidi)` interface (still bool-based),
and does **not** touch Bootstrap* state machines (higher-risk, deferred to a
later step).

## Changes made

- **`source/state-machine/ListenStateMachine.cpp`**
  - `#include "LinkEstablishment.h"` added.
  - `StateListenInitial::enter()`: the hardcoded `true` passed to
    `startConnStateMachineBidi` (the very first listener connection, which
    always creates the link) is now expressed as
    `ConnEstablishment{LinkRole::Creator, LinkDirectionality::Bidi}.isCreator()`.
  - `StateListenWaiting::enter()`: the hardcoded `false` passed to
    `startConnStateMachineBidi` for subsequent `accept()`s that reuse the
    first connection's link is now
    `ConnEstablishment{LinkRole::Loader, LinkDirectionality::Bidi}.isCreator()`.

- **`source/state-machine/DialStateMachine.cpp`**
  - `#include "LinkEstablishment.h"` added.
  - `StateDialInitial::enter()`, merged-bidi branch: hardcoded `false` passed
    to `startConnStateMachineBidi` is now
    `ConnEstablishment{LinkRole::Loader, LinkDirectionality::Bidi}.isCreator()`.
  - `StateDialInitial::enter()`, non-merged branch: hardcoded `true, false`
    (creating, sending) passed to `startConnStateMachine` for the separate
    recv connection is now
    `ConnEstablishment{LinkRole::Creator, LinkDirectionality::Recv}` via
    `.isCreator()`/`.isSend()`.
  - `StateDialWaitingForSendConnection::enter()`: hardcoded `false, true`
    passed to `startConnStateMachine` for the separate send connection is now
    `ConnEstablishment{LinkRole::Loader, LinkDirectionality::Send}` via
    `.isCreator()`/`.isSend()`.

In every case, the manager-facing call sites (`startConnStateMachine`,
`startConnStateMachineBidi`) are unchanged — only the *source* of the
booleans passed to them was swapped from bare literals to named
`ConnEstablishment` values, which is exactly the same pattern used in Step 1.
No state-transition tables, event names, or control flow were touched.
`ApiListenContext`/`ApiDialContext` were not modified (no new members added);
the `ConnEstablishment` values are local to each `enter()` call.

## Build

Rebuilt via the documented 3-step sequence:
1. `raceboat-builder:latest` → `./build.sh` — compiled cleanly.
2. `raceboat-plugin-builder-image/build_image.sh --version=working --platform-x86_64 --namespace=ghcr.io/tst-race/raceboat`
3. `raceboat-runtime-image` via `docker buildx build --build-context
   ghcr.io/tst-race/raceboat/raceboat-plugin-builder:latest=docker-image://ghcr.io/tst-race/raceboat/raceboat-plugin-builder:working
   -f raceboat-runtime-image/Dockerfile -t ghcr.io/tst-race/raceboat/raceboat-runtime:working --load .`

(Note: the plugin-builder image must be built with `--namespace=ghcr.io/tst-race/raceboat`
explicitly, otherwise it defaults to a local `docker.io/library/...` tag that
the runtime Dockerfile's `--build-context` override won't match.)

## Regression suite results (all 8 scenarios, run against `:working` images)

| # | Scenario | Result | Time |
|---|----------|--------|------|
| 1 | `racebird-client-connect` | PASSED | 36s |
| 2 | `decomposed-client-connect-multi-client` | PASSED (x2 rerun, see below) | 59s |
| 3 | `bootstrap-decomposed-racebird` | PASSED (topology: OK) | 63s |
| 4 | `bootstrap-racebird-multi-client` | PASSED (topology: OK) | 73s |
| 5 | `bootstrap-racebird-decomposed-multi-client` | PASSED (topology: OK) | 84s |
| 6 | `bootstrap-decomposed-decomposed-multi-client` | PASSED (topology: SKIPPED — known limitation) | 84s |
| 7 | `bootstrap-racebird-decomposed` (single-client) | PASSED (topology: OK) | 64s |
| 8 | `bootstrap-decomposed-decomposed` (single-client) | PASSED (topology: SKIPPED — known limitation) | 84s |

`bootstrap-racebird-racebird` intentionally excluded (pre-existing known-broken
combo, unrelated to this step).

## Issue uncovered and resolved during verification: one-off test flake (not a code regression)

The first attempt to run `decomposed-client-connect-multi-client` against the
Step-2 image appeared to hang indefinitely (`dxclient2`'s test stub blocked
for 10+ minutes instead of completing within its own ~30-60s timeout budget).
This was investigated rather than dismissed, since it directly exercised the
`StateListenWaiting` reuse branch this step modified:

1. Inspected the diff line-by-line: the boolean passed to
   `startConnStateMachineBidi` (`reuseEstablishment.isCreator()`) is
   mathematically identical to the original hardcoded `false`
   (`LinkRole::Loader` → `isCreator()` → `false`) — no logic change is
   possible here.
2. **Bisected empirically**: `git stash`ed just the Step 2 changes, rebuilt
   `:working` from Step-1-only source, and ran the same scenario — PASSED
   cleanly in 59s.
3. Restored the Step 2 changes (`git stash pop`), rebuilt `:working` again,
   and reran the same scenario **twice** — PASSED cleanly both times (59s,
   58s).

This confirms the original hang was environmental flakiness (most likely
docker/whiteboard contention from the back-to-back rebuild-and-test cycles
run earlier in this session), not a regression introduced by this step. This
matches the "highly timing-sensitive" warning already on file for this
scenario. No code change was made in response; recorded here for the record
per the instruction to assess whether failures need remediation now or
later — **remediation is not needed**, since the bisection proved the code
is not the cause.

## Assessment / carry-forward

- Channel-mode (Listen/Dial) call sites now consistently express their
  create/load and direction decisions via `ConnEstablishment`, matching the
  pattern established in `ConnectionStateMachine.cpp`. The only remaining
  bool-based surface in this path is the shared `ApiManagerInternal`
  interface itself (`startConnStateMachine`/`startConnStateMachineBidi`
  signatures), which still take `bool creating`/`bool sending` — left
  unchanged in this step to avoid rippling into every other caller
  (`Receive`/`Send`/`SendReceive`/`Resume`/`PreConduit`/`Bootstrap*`).
- Next step per the migration plan: decide whether to (a) change the shared
  manager interface itself to accept `ConnEstablishment` directly (removing
  the bool-conversion boilerplate at every call site, but touching all
  callers including the higher-risk Bootstrap* files), or (b) migrate
  `PreConduitStateMachine.cpp`/`ReceiveStateMachine.cpp`/`SendStateMachine.cpp`/
  `SendReceiveStateMachine.cpp`/`ResumeStateMachine.cpp` next (same low-risk
  mechanical pattern as this step, still without touching the shared
  interface), saving the Bootstrap* state machines and the interface change
  itself for last, per the original risk-ordering rationale.
- No test failures require remediation right now — the suite is green.
