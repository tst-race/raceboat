# Link Management in Raceboat's Bootstrap/Channel State Machines

This document traces how the raceboat SDK decides *how many* physical links
get created for a given channel, *who* creates vs loads each one, and *how*
that decision differs between the plain (non-bootstrap) "channel" API and the
bootstrap-connect ("initial"/"final" slot) API. It also proposes concrete
changes to make link counts and creator/loader roles consistent and
verifiable across arbitrary channel types, rather than the current
per-callsite hardcoding.

Source references are relative to `raceboat/source/`.

## 1. The decision primitives (`state-machine/ApiContext.cpp`)

```cpp
bool ApiContext::shouldCreate(const ChannelId &channelId, bool useForRecv) {
  ChannelProperties props = manager.getCore().getChannelManager().getChannelProperties(channelId);
  bool shouldCreate = props.linkDirection == LD_CREATOR_TO_LOADER;
  if (useForRecv) return not shouldCreate;
  return shouldCreate;
}
bool ApiContext::shouldCreateSender(const ChannelId &channelId)   { return shouldCreate(channelId, false); }
bool ApiContext::shouldCreateReceiver(const ChannelId &channelId) { return shouldCreate(channelId, true); }

bool ApiContext::shouldUseSingleBidiLink(const ChannelId &sendChannel, const ChannelId &recvChannel) {
  if (sendChannel != recvChannel) return false;
  ChannelProperties props = manager.getCore().getChannelManager().getChannelProperties(sendChannel);
  if (props.linkDirection != LD_BIDI) return false;
  if (props.transmissionType != TT_UNICAST) return false;
  return true;
}
```

**`shouldCreateSender`/`shouldCreateReceiver` are role-blind.** They only
consult a channel's *static manifest* `linkDirection` property
(`LD_BIDI`, `LD_CREATOR_TO_LOADER`, `LD_LOADER_TO_CREATOR`) - the same
channel, called from the same function, returns the *identical* answer
whether the caller is the listener/creator or the dialer/loader. For a
`LD_CREATOR_TO_LOADER`/`LD_LOADER_TO_CREATOR` channel this is enough (the
manifest already encodes an asymmetric role). For a symmetric `LD_BIDI`
channel it structurally cannot break the tie - both sides compute the same
boolean. Every bug fixed this session and last (creator/loader ambiguity for
merged final/init links, the response-JSON gating bug, the
`WaitingForConnections` readiness-gate bug) traces back to code trying to use
these two functions to resolve a decision they cannot resolve, and having to
be replaced with a hardcoded role-based boolean at that specific call site.

`shouldUseSingleBidiLink` is the one function in this file that *can* make a
reliable decision, because it only needs static channel properties (both
sides agree a `LD_BIDI` + `TT_UNICAST` channel *can* merge send+recv into one
physical link) - it never has to decide *who* creates it, only *whether*
merging is possible.

## 2. Two structurally different "merge" mechanisms

This is the most important architectural fact in the codebase for
understanding link counts.

### 2a. Non-bootstrap "channel" slot - a dedicated Bidi path

`ListenStateMachine.cpp` / `DialStateMachine.cpp` call
`ApiManagerInternal::startConnStateMachineBidi(...)`, which calls
`ApiConnContext::updateConnStateMachineStartBidi()` and sets
`ctx.bidirectional = true`. Later, in `ConnectionStateMachine.cpp`
`StateConnLinkEstablished::enter()`:

```cpp
LinkType linkType;
if (ctx.bidirectional) {
  linkType = LT_BIDI;
} else {
  linkType = ctx.send ? LT_SEND : LT_RECV;
}
SdkResponse response = plugin.openConnection(openConnHandle, linkType, ctx.linkId, "{}", 0, 0, 0);
```

This path genuinely tells the plugin to open an `LT_BIDI` connection. One
physical link, one connection object, one connSM handle used for both
directions (`ctx.sendConnSMHandle = ctx.recvConnSMHandle` at the context
level too).

- **Listener** (`ListenStateMachine.cpp` `StateListenInitial`): calls
  `startConnStateMachineBidi(..., creating=true)` **unconditionally** - it
  never calls `shouldUseSingleBidiLink()` at all, it just assumes the
  "channel" slot's channel can be merged.
- **Dialer** (`DialStateMachine.cpp` `StateDialInitial`): *does* check
  `ctx.usingSingleBidiConnection = ctx.shouldUseSingleBidiLink(send_channel, recv_channel)`
  first. If true, uses the Bidi path with `creating=false`; if false, falls
  back to a separate directional `startConnStateMachine(..., creating=true, sending=false)`
  call for a standalone recv connection (and a separate send setup
  elsewhere in the same file).

This is a real asymmetry: the listener assumes mergeability without
checking; the dialer checks and can disagree. It hasn't caused an observed
failure in the scenarios exercised so far (both `racebird-client-connect`
and `decomposed-client-connect*` pass), but it means correctness currently
depends on every "channel"-slot plugin happening to be `LD_BIDI` (true for
both racebird and decomposed-exemplars today).

### 2b. Bootstrap "initial"/"final" slots - directional connSMs aliased together

`BootstrapListenStateMachine.cpp`, `BootstrapDialStateMachine.cpp`, and
`BootstrapPreConduitStateMachine.cpp` **never call
`startConnStateMachineBidi`**. When `shouldUseSingleBidiLink()` is true, they
call the plain **directional** `startConnStateMachine(..., sending=true)`
once and then manually alias the context fields to fake a merged connection:

```cpp
// StateBootstrapPreConduitAccepted (listener side, final slot)
const bool create = true;  // hardcoded, see below
ctx.finalSendConnSMHandle = ctx.manager.startConnStateMachine(
    ctx.handle, ctx.opts.final_send_channel, ctx.opts.final_send_role, "", create, /*sending=*/true);
ctx.finalRecvConnSMHandle = ctx.finalSendConnSMHandle;   // alias - NOT a real bidi connSM
ctx.finalRecvConnId = ctx.finalSendConnId;
ctx.finalUsingSingleBidiConnection = true;
```

The same pattern (one directional `startConnStateMachine` call, then
`finalRecvConnSMHandle = finalSendConnSMHandle` / `initRecvConnSMHandle =
initSendConnSMHandle`) is repeated in `BootstrapDialStateMachine.cpp` for the
dialer's init and final links. Every place downstream that needs to know
"is this actually one merged link" checks the dedicated
`ctx.finalUsingSingleBidiConnection`/`ctx.initUsingSingleBidiConnection`
booleans set by this aliasing - never `ctx.bidirectional`, which stays
`false` on these directional contexts.

**Consequence:** even in the "merged" bootstrap case, the plugin's
`openConnection()` is called with `linkType = LT_SEND` (never `LT_BIDI`),
because `ctx.bidirectional` is always false for connSMs started via
`startConnStateMachine`. One physical link is created (matching the
non-bootstrap path's link count), but the connection object opened on it is
directional at the plugin API surface, not truly bidirectional.

This is the leading suspect for why combo 1 (racebird as **both** initial
and final in bootstrap-connect mode) still fails after all other fixes this
session: it is the only tested combination that pushes a merged **final**
link through this aliased-directional path for a channel whose Go plugin
implementation may behave differently for a `LT_SEND`-typed accepted
connection than the always-truly-bidi non-bootstrap path does (which is
exercised successfully by `racebird-client-connect`). This was root-caused
far enough to identify the mechanism, but not far enough to prove the exact
line inside `RacebirdPlugin.go` responsible - see
`/memories/repo/racebird-bootstrap-single-link-fixes.md` for the full
investigation log, including a fix attempt that was reverted because it
regressed two previously-passing scenarios.

## 3. Where "create vs load" is hardcoded today

| Slot | Merge? | Listener/Creator | Dialer/Loader | File |
|---|---|---|---|---|
| channel | assumed always | `create=true` (`startConnStateMachineBidi`, no compatibility check) | `create=false` if `shouldUseSingleBidiLink`, else separate directional | `ListenStateMachine.cpp` / `DialStateMachine.cpp` |
| initial | if `shouldUseSingleBidiLink` | `create=true` hardcoded | `create=false` hardcoded | `BootstrapListenStateMachine.cpp` `StateBootstrapListenInitial` / `BootstrapDialStateMachine.cpp` `StateBootstrapDialInitial` |
| final | if `shouldUseSingleBidiLink` | `create=true` hardcoded (both send+recv even when non-merged) | `create=false` hardcoded (both send+recv even when non-merged) | `BootstrapPreConduitStateMachine.cpp` `StateBootstrapPreConduitAccepted` / `BootstrapDialStateMachine.cpp` |

Every one of these hardcodings exists because `shouldCreateSender`/
`shouldCreateReceiver` could not be trusted for `LD_BIDI` channels. They are
correct today only because every call site was manually audited and patched
during this and the prior debugging session - there is no mechanism that
would catch a *new* call site repeating the same role-blind mistake.

## 4. Measured link counts (from `link_topology.py`, empirically confirmed)

**Final slot**, per connector node:

| Channel | Merged? | Listener creates | Connector loads |
|---|---|---|---|
| racebird (`obfs4`, `LD_BIDI`+`TT_UNICAST`) | yes | 1 total (OS `accept()` multiplexes, no extra calls per client) | 1 |
| decomposed-exemplars (`twoSixIndirectComposition`, `LD_BIDI`+`TT_MULTICAST`) | no | 2 per connector (separate send+recv) | 2 per connector |

**Initial slot**, listener side:

| Channel | Listener behavior |
|---|---|
| racebird | creates 1 link once, ever; native TCP `accept()` handles additional clients with zero extra SDK-level calls |
| decomposed-exemplars | creates 1 link once, then **loads the same address again once per accepted client** (no native multi-accept concept for whiteboard polling, so the SDK's multi-client fix re-invokes `startConnStateMachine(creating=false)` per additional client to reuse the link) |

This last row is *not* a merge-logic decision at all - it's a side effect of
which channels have a native multiplexed-accept concept (TCP-like) versus
which need the SDK to manufacture one via repeated loads (poll-based
whiteboard channels). It is currently indistinguishable, by log-grepping
alone, from ordinary per-client final-link creation when both slots share a
channel gid - which is why `link_topology.py`'s final-link check has to
`[SKIPPED]` that case rather than assert counts (see §5.3 below).

## 5. Suggested approaches for consistency and detectability

### 5.1 Stop asking role-blind functions to make role-based decisions

Replace `shouldCreateSender(channelId)`/`shouldCreateReceiver(channelId)`
calls in bootstrap code with an explicit `bool isListener`/`bool isCreator`
parameter threaded down from the state machine that already knows its own
role (every state already knows whether it is `BootstrapListen*` or
`BootstrapDial*`). Keep `shouldCreate()` itself for the one case it's
actually valid - resolving `LD_CREATOR_TO_LOADER`/`LD_LOADER_TO_CREATOR`
channels, where the manifest genuinely encodes an asymmetric role - but gate
its use behind `props.linkDirection != LD_BIDI`, and require an explicit
role argument for the `LD_BIDI` case instead of silently returning `false`
for both sides. This turns "you forgot to hardcode the role at this new call
site" from a silent deadlock/wrong-behavior into a compile-time-visible
missing-argument error.

### 5.2 Unify the two merge mechanisms

Migrate bootstrap's init/final merge path off the directional
`startConnStateMachine` + manual-alias pattern and onto
`startConnStateMachineBidi` (the same mechanism `Listen`/`DialStateMachine`
already use successfully). Concretely:
- Add a `startBootstrapConnStateMachineBidi`-style helper (or extend
  `startConnStateMachineBidi` to accept the extra bootstrap bookkeeping
  `BootstrapListenStateMachine.cpp`/`BootstrapPreConduitStateMachine.cpp`
  currently do by hand) so `ctx.bidirectional = true` and `linkType = LT_BIDI`
  actually reach the plugin for a merged bootstrap link, matching what a
  non-bootstrap merged channel gets.
- This is very likely the real fix for combo 1 (racebird as both initial and
  final), since it removes the `LT_SEND`-only connection as a variable
  entirely rather than continuing to patch individual gates around it.
- **Caution informed by this session:** any change in this area has proven
  to be timing-sensitive - a smaller, more localized attempt at fixing just
  the `WaitingForConnections` readiness gate regressed two
  previously-passing scenarios (`bootstrap-decomposed-racebird`,
  `bootstrap-racebird-multi-client`) without fixing combo 1. Any future
  attempt must run the full regression suite (all 8 scenarios under
  `raceboat/test/integration/scenarios/`) before being considered safe, not
  just the specific combo being fixed.

### 5.3 Make link/connection log lines self-describing instead of position-dependent

Today, verifying link counts (`link_topology.py`) requires grepping
`createLink`/`loadLinkAddress` debug lines keyed only by `channelGid`, with
no indication of which **slot** (channel/initial/final) or **role**
(listener/connector) triggered the call. This works by coincidence when
initial and final use different channel gids, and has to be skipped
entirely when they don't (see the `bootstrap-decomposed-decomposed*`
scenarios). Two complementary fixes:
- Thread the slot name (`"channel"`/`"initial"`/`"final"`) into
  `startConnStateMachine`/`startConnStateMachineBidi` as an optional
  parameter purely for logging, and include it in the
  `CompositeWrapper`/`LoaderWrapper` create/load debug lines.
- Alternatively/additionally, emit one structured, greppable summary line
  per completed link (slot, role, channelGid, created-vs-loaded, connector
  count if known) from a single, central place (e.g.
  `ApiManagerInternal::onLinkStatusChanged`) rather than relying on
  `TRACE_METHOD`/ad hoc `helper::logInfo` calls scattered across each state.

This would let `link_topology.py` assert *real* counts for every
combination (including same-gid initial+final) instead of degrading to
`[SKIPPED]`, and would make future regressions in this area visible via the
existing test suite rather than requiring manual log correlation like the
one done this session.

### 5.4 Decouple "channel establishment" and "per-message idle" timeouts from a single scenario knob

Combos 2 and 3's failures this session were not link-count bugs at all, but
a single hardcoded `--timeout` value (originally 15s, copied into every
bootstrap scenario JSON) being too short for decomposed-exemplars' ~10s
whiteboard-polling period. A channel-agnostic per-scenario `timeout` cannot
simultaneously be tight enough to fail fast for a broken fast channel and
loose enough to tolerate a slow polling-based one. Consider deriving a
default `--timeout` (or a per-channel override) from the channel's own
`period_s`/transmission properties (already present in `ChannelProperties`)
rather than a single flat number copied across every scenario file
regardless of which plugins are involved.

### 5.5 Add an explicit "native multi-accept" channel capability instead of an emergent one

Whether a channel needs the SDK to manufacture repeated `loadLinkAddress`
calls to admit additional clients on a shared init link (decomposed) versus
handling it natively via OS-level `accept()` (racebird) is currently an
emergent property of the plugin's own implementation, not something the SDK
or manifest declares. Making this an explicit, queryable property (even a
simple boolean in `ChannelProperties`) would let both the multi-accept logic
itself and `link_topology.py`'s verification be driven by a declared fact
instead of an assumption re-derived from log timing each time a new channel
is exercised in this mode.
