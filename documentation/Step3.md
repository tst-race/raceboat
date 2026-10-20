# Step 3: Migrate bootstrap's initial/final links onto ConnEstablishment (unify the merge mechanism)

## Goal

Per `link-management.md` §5.2 ("Unify the two merge mechanisms"): migrate
bootstrap's merged init/final links off the directional
`startConnStateMachine` + manual field-aliasing pattern
(`finalRecvConnSMHandle = finalSendConnSMHandle`) and onto genuine
`startConnStateMachineBidi` calls - the same mechanism `ListenStateMachine`/
`DialStateMachine` already use successfully (Step 2). This is the change
flagged as most likely to actually fix combo 1 (racebird as both initial and
final channel in bootstrap-connect mode), and the riskiest/most
timing-sensitive step in the migration, per prior-session history in
`/memories/repo/racebird-bootstrap-single-link-fixes.md`.

## Changes made

### 1. Merge-mechanism unification (mechanical, per the established pattern)

- **`BootstrapDialStateMachine.cpp`**
  - `StateBootstrapDialInitial::enter()`: when `initUsesSingleBidiLink`, the
    init-send connSM is now started via `startConnStateMachineBidi` with
    `ConnEstablishment{LinkRole::Loader, LinkDirectionality::Bidi}` instead
    of the directional `startConnStateMachine(..., sending=true)`. The
    non-merged branch is untouched.
  - `StateBootstrapDialRecvResponse::enter()`: the dialer's real merged
    final-link creation (loading the address the listener sent in its hello
    response) now uses `startConnStateMachineBidi` with
    `ConnEstablishment{LinkRole::Loader, LinkDirectionality::Bidi}` instead
    of directional `startConnStateMachine(..., sending=true)`.
  - The dead `if (create)` branches in `StateBootstrapDialInitial` for the
    final-link merged case (`create` is hardcoded `false` and never taken)
    were left untouched - they are unreachable and out of scope for this
    step.

- **`BootstrapListenStateMachine.cpp`**
  - `StateBootstrapListenInitial::enter()`: when `create && initUsesSingleBidiLink`
    (listener always creates the merged init link), now uses
    `startConnStateMachineBidi` with
    `ConnEstablishment{LinkRole::Creator, LinkDirectionality::Bidi}` instead
    of directional `startConnStateMachine(..., sending=true)`. The
    non-merged/loading branch is untouched.

- **`BootstrapPreConduitStateMachine.cpp`**
  - `StateBootstrapPreConduitAccepted::enter()`: the listener's merged final
    link (previously a hardcoded `const bool create = true` directional
    `startConnStateMachine` call, with two now-provably-dead `else`
    branches removed since `create` was always true) now uses
    `startConnStateMachineBidi` with
    `ConnEstablishment{LinkRole::Creator, LinkDirectionality::Bidi}`.

In every case, the post-creation bookkeeping (`finalRecvConnSMHandle =
finalSendConnSMHandle`, `ctx.finalUsingSingleBidiConnection = true`, etc.)
was left unchanged - only the *mechanism* used to establish the single
underlying connSM changed, from directional-aliased-as-bidi to genuinely
bidi. All three files now `#include "LinkEstablishment.h"`.

### 2. A required, closely-related state-transition fix (discovered via testing, not part of the original mechanical plan)

Testing `bootstrap-racebird-racebird` (combo 1) after the merge-mechanism
change above showed the **listener** hang at
`STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS`: the newly-genuine
Bidi final connSM fires `EVENT_CONN_STATE_MACHINE_LINK_ESTABLISHED` (not
`EVENT_CONN_STATE_MACHINE_CONNECTED` - a merged Bidi link is a listening
socket, and `LINK_ESTABLISHED` is what signals its address is ready,
`CONNECTED` won't fire until a peer actually dials in), but the state engine
had no transition registered for that event in that state, so the address
readiness update happened in the context but the state was never re-entered
to notice it - a permanent hang, confirmed via debug log:
`event EVENT_CONN_STATE_MACHINE_LINK_ESTABLISHED not registered for state
STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS`.

Fix: added the missing self-loop transition in
`BootstrapPreConduitStateEngine`'s constructor:
```cpp
declareStateTransition(STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS,
                       EVENT_CONN_STATE_MACHINE_LINK_ESTABLISHED,
                       STATE_BOOTSTRAP_PRE_CONN_OBJ_WAITING_FOR_CONNECTIONS);
```

**This is the same fix a prior session attempted and reverted** (see
memory), but that attempt was made against the *old* aliased-directional
mechanism, where two different context fields (`finalSendConnSMHandle`/
`finalRecvConnSMHandle`, aliased to the same handle but conceptually
representing two directions) could each independently fire events in ways
that interacted badly with a naively-added self-loop, regressing two
previously-passing scenarios. With the merge mechanism now unified onto a
single genuine Bidi connSM (this step), that structural cause is gone, and
the same transition addition was verified clean against the full regression
suite (see below) - no regressions this time.

The equivalent gap exists on the dialer side too
(`STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS`/
`STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS` also lack a
`LINK_ESTABLISHED` transition and log the same "not registered" line), but
it did not block combo 1 in practice - the dialer's own `CONNECTED` event
already satisfies its wait gates before `LINK_ESTABLISHED` would matter. Left
unchanged in this step since it isn't required for any currently-tested
scenario; flagged as a latent gap for a future step if a scenario ever
depends on it.

## Build

Same documented 3-step sequence as Steps 1-2. Two build/test cycles were
needed: one for the mechanical merge-mechanism change, and one more after
adding the transition fix above.

## Regression suite results (all runs fresh against `:working` images)

| # | Scenario | Result |
|---|----------|--------|
| 1 | `racebird-client-connect` | PASSED |
| 2 | `decomposed-client-connect-multi-client` | PASSED |
| 3 | `bootstrap-decomposed-racebird` | PASSED (topology: OK) |
| 4 | `bootstrap-racebird-multi-client` | PASSED (topology: OK) |
| 5 | `bootstrap-racebird-decomposed-multi-client` | PASSED (topology: OK) |
| 6 | `bootstrap-decomposed-decomposed-multi-client` | PASSED (topology: SKIPPED, known limitation) |
| 7 | `bootstrap-racebird-decomposed` (single-client) | PASSED (topology: OK) |
| 8 | `bootstrap-decomposed-decomposed` (single-client) | PASSED (topology: SKIPPED, known limitation) |
| 9 | **`bootstrap-racebird-racebird` (combo 1, single-client)** | **PASSED** (was known-broken since prior sessions) |
| 10 | **`bootstrap-racebird-racebird-multi-client` (combo 1, multi-client)** | **PASSED** (was known-broken since prior sessions) |

All 10 scenarios were executed fresh in this session via
`run_scenario.py --image-tag working`, each showing `INTEGRATION TEST PASSED ✓`
plus the expected topology verdict.

## Issues uncovered during verification (both investigated, neither a regression)

1. **`decomposed-client-connect-multi-client` timed out (4 min) on first
   attempt after the initial mechanical change**, alongside an unrelated
   `bootstrap-decomposed-racebird` immediate `[MISMATCH]` failure in the same
   batch. Root cause: the *previous* scenario in the loop
   (`decomposed-client-connect-multi-client` itself) got killed by the outer
   `timeout` before its own cleanup ran, leaving `dxclient`/`dxclient2`/
   `dxserver` containers running and contending for resources with the next
   scenario's containers. Confirmed by: force-removing the leftover
   containers/network and re-running both scenarios in isolation - both
   passed cleanly (`bootstrap-decomposed-racebird` in 63s,
   `decomposed-client-connect-multi-client` in 59s). No code change made;
   this is a test-harness hygiene issue (a hung scenario's containers must be
   force-cleaned before the next scenario runs), not a product regression.
2. **The state-transition gap described in section 2 above** was itself
   "uncovered during verification" (it wasn't part of the planned mechanical
   diff) - it was root-caused via debug-log inspection rather than
   guessed-and-checked, confirmed to be the sole blocker via the log line
   quoted above, fixed, and then the *entire* regression suite (not just
   combo 1) was re-run to gate the fix, per the task's explicit instruction.

## Assessment / carry-forward

- **Combo 1 (`bootstrap-racebird-racebird`) is fixed**, both single- and
  multi-client variants. This closes out the investigation documented across
  multiple prior sessions in
  `/memories/repo/racebird-bootstrap-single-link-fixes.md`.
- The dialer-side `LINK_ESTABLISHED`-not-registered gap (noted above) is
  latent but currently harmless for all 10 scenarios in the suite. A future
  step should add the equivalent self-loop transitions to
  `BootstrapDialStateEngine` (`STATE_BOOTSTRAP_DIAL_WAITING_FOR_CONNECTIONS`
  and `STATE_BOOTSTRAP_DIAL_WAITING_FOR_FINAL_CONNECTIONS`) for symmetry and
  to close off the same class of bug proactively, gated behind the full
  suite as always.
- Per `link-management.md` §5.1, the next natural step is replacing
  `shouldCreateSender`/`shouldCreateReceiver` call sites in bootstrap code
  with an explicit role parameter (the state machines already know their own
  role), now that the merge mechanism itself is unified - this would let the
  remaining hardcoded `create = true`/`create = false` constants in these
  three files be expressed as real role-derived decisions instead of
  comments explaining why they're hardcoded.
- No test failures require remediation now - the suite (10/10 scenarios) is
  green.
