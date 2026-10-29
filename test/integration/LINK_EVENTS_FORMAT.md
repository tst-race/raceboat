# Link Event Specifications

`link_topology.py` originally hardcoded the "listener creates the final link,
connector loads it" assumption directly in Python, which breaks silently for
any scenario that legitimately reverses those roles (e.g. a
`linkDirectionOverrides` scenario like
`bootstrap-decomposed-decomposed-final-l2c`) and gives a test author no way
to describe what a *new* scenario's link sequence is actually supposed to
look like without editing that script.

A **link event spec** is a small, plugin-agnostic YAML file that a scenario
author writes alongside a `scenarios/<id>.json` file to declare the expected
order of link `CREATE`/`LOAD` operations. `link_events.py` reads it, greps the
scenario's own debug logs for the matching events, and prints a human-readable
PASS/FAIL report - no Python changes required to describe a new scenario's
topology.

## File location

`scenarios/link_events/<scenario-id>.yaml`, matching the `scenarios/<scenario-id>.json`
it describes. This file is **optional**: if it doesn't exist, `run_scenario.py`
simply skips the check.

## Format

```yaml
steps:
  1:
    - {node: listener, action: CREATE, channel: twoSixIndirectComposition, slot: initial}
  2:
    - {node: dialer, action: LOAD,   channel: twoSixIndirectComposition, slot: initial}
    - {node: dialer, action: CREATE, channel: twoSixIndirectComposition, slot: final}
  3:
    - {node: listener, action: LOAD,   channel: twoSixIndirectComposition, slot: final}
```

The top-level `steps` key maps a **step number** to a **list of events**.

* Step numbers just need to be unique; they're sorted numerically before
  checking, so file order doesn't matter.
* **All events listed under the same step may occur in any relative order or
  interleaving.** Concretely: every event in step *N* must be observed
  (by log timestamp) before *any* event in step *N+1* - but two events in the
  same step have no ordering requirement relative to each other.
* **The spec is exhaustive.** Every link a plugin creates or loads must be
  claimed by some step; a link event the spec doesn't account for (an extra
  occurrence, or a different node/channel/slot) fails the check as
  `[UNEXPECTED]`. Counts are exact.

### Event fields

| Field     | Required | Description |
|-----------|----------|-------------|
| `node`    | yes | A literal node `id` from the scenario's `nodes[]` list, **or** a role keyword: `listener`/`server`, `connector`/`client`. A role keyword expands to one independent requirement *per node currently in the scenario with that role* - e.g. in a multi-client scenario, `connector` means "each connector, independently," not "any one connector." |
| `action`  | yes | `CREATE` or `LOAD` (case-insensitive). `CREATE` matches a `LINK_CREATED` status, `LOAD` a `LINK_LOADED` status. |
| `channel` | yes | The channel gid exactly as declared in the owning plugin's `source/manifest.json` under `channel_properties` (e.g. `obfs4`, `twoSixIndirectComposition`) - **not** the scenario's plugin/registry name (`racebird`, `decomposed-exemplars`), since one plugin can expose several channel gids. |
| `slot`    | no (default `channel`) | Which race-cli phase this event belongs to: `channel` (single-phase `client-connect`/`server-connect` mode), `initial`, or `final` (the two phases of `bootstrap-connect` mode). Always set this explicitly for bootstrap-connect scenarios, since `initial` and `final` can otherwise share the same channel gid. |
| `count`   | no (default `1`) | How many independent occurrences of this exact `(node, action, channel, slot)` tuple must be observed within this step (per matching node, if `node` is a role keyword). This is the number of *links*, e.g. 2 for a channel that opens separate send and recv links, or one per client for a listener that makes a private link per connection. |

The same `(node, action, channel, slot)` tuple can legitimately appear in
more than one step (e.g. a listener creating a fresh link per connecting
client, at different points in time) - each occurrence is matched against a
*separate* log line, consumed in timestamp order, so later steps never reuse
a log line already claimed by an earlier step.

## How it maps to raceboat log lines

`ApiManagerInternal::onLinkStatusChanged` (see `source/api-managers/ApiManager.cpp`)
is the one place every plugin's link status passes through, decomposed or
monolithic. For each `LINK_CREATED`/`LINK_LOADED` it logs exactly one line:

```
LinkEvent: status=<LINK_CREATED|LINK_LOADED> slot=<slot> channel=<channelGid> linkId=<id>
```

`slot` comes from the `Socket::establish(..., slot)` call that requested the
connection (untagged call sites report `channel`). `link_events.py` greps each
node's `raceboat_<role>.log` for these lines, parses their leading timestamp
(`2026-09-28 12:50:01.932223: ...`), and maps `LINK_CREATED` -> `CREATE`,
`LINK_LOADED` -> `LOAD`.

Because the line is emitted per link status event, reusing an existing link
(e.g. a listener `accept()`ing another client on its listen link) produces no
event, and one link is never counted twice.

## Running standalone

```bash
python3 link_events.py --scenario-id bootstrap-decomposed-decomposed-final-l2c
```

This defaults `--spec` to `scenarios/link_events/<scenario-id>.yaml` and
`--logs-dir` to `generated/<scenario-id>/logs` (i.e. it re-checks the most
recent run's logs without re-running the scenario). Both can be overridden
explicitly.

`run_scenario.py` also calls this automatically after every run, for any
scenario that has a matching `scenarios/link_events/<id>.yaml` file, and folds
a FAILED result into the overall test exit code.

## Example

See [scenarios/link_events/bootstrap-decomposed-decomposed-final-l2c.yaml](scenarios/link_events/bootstrap-decomposed-decomposed-final-l2c.yaml)
for a full worked example (including inline commentary) checked against a
real run of that scenario.
