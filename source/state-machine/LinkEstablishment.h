// Copyright 2023 Two Six Technologies
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "race/common/LinkType.h"

namespace Raceboat {

// Whether this side of a connection creates the link (and publishes/owns its
// address) or loads an address obtained from the peer.
enum class LinkRole {
  Creator,
  Loader,
};

// Which direction(s) of traffic a connection carries over its link.
enum class LinkDirectionality {
  Send,
  Recv,
  Bidi,
};

// The two orthogonal decisions - who creates the link and which direction(s)
// it serves - that together determine how ConnectionStateMachine establishes
// a link/connection and which LinkType it requests from the plugin. This is
// a pure value type derived from (and, for now, kept in sync with) the
// legacy create/send/bidirectional bools so existing callers are unaffected.
struct ConnEstablishment {
  LinkRole role = LinkRole::Loader;
  LinkDirectionality directionality = LinkDirectionality::Send;

  static ConnEstablishment fromLegacy(bool creating, bool sending,
                                      bool bidirectional) {
    ConnEstablishment est;
    est.role = creating ? LinkRole::Creator : LinkRole::Loader;
    est.directionality = bidirectional  ? LinkDirectionality::Bidi
                        : sending       ? LinkDirectionality::Send
                                        : LinkDirectionality::Recv;
    return est;
  }

  bool isCreator() const { return role == LinkRole::Creator; }
  bool isBidi() const { return directionality == LinkDirectionality::Bidi; }
  // Bidi connections carry send traffic too, mirroring the legacy
  // `send=true` used for bidirectional connections.
  bool isSend() const { return directionality != LinkDirectionality::Recv; }

  LinkType toLinkType() const {
    switch (directionality) {
    case LinkDirectionality::Bidi:
      return LT_BIDI;
    case LinkDirectionality::Recv:
      return LT_RECV;
    case LinkDirectionality::Send:
    default:
      return LT_SEND;
    }
  }
};

// Which side of a conversation a state machine represents when it needs to
// resolve the LinkRole for a merged/Bidi link. LD_BIDI channels have no
// manifest-derivable creator/loader answer - shouldCreateSender()/
// shouldCreateReceiver() return the same value regardless of caller - so
// every merged-link call site across channel and bootstrap mode hardcoded
// the same convention independently. This centralizes that one convention.
enum class ModeRole {
  Listener,
  Dialer,
};

// The listener always creates a merged/Bidi link; the dialer always loads
// it (and, for bootstrap's initial/final slots, learns its address via the
// hello/response exchange rather than the manifest).
inline LinkRole resolveBidiRole(ModeRole modeRole) {
  return modeRole == ModeRole::Listener ? LinkRole::Creator : LinkRole::Loader;
}

} // namespace Raceboat
