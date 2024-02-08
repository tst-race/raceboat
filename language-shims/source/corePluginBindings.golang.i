%module(directors="1") corePluginBindingsGolang

%include <typemaps.i>

// stuff in %{ ... %} is passed directly to output
%{
#include <utility>
#include <string>
#include "race/common/ChannelId.h"
#include "race/common/RaceHandle.h"
#include "race/Race.h"
#include "race/common/RaceLog.h"
using namespace RaceLib;
%}

// SWIG wraps these %include'ed files, %templates, etc
// optionally remove some of these and tell SWIG what to wrap

%include "stdint.i"
%include "stl.i"


%template(ByteVector) std::vector<uint8_t>;
%template(StatusConnectionPair) std::pair<RaceLib::ApiStatus, RaceLib::ConnectionObject>;
%template(StatusStringPair) std::pair<RaceLib::ApiStatus, std::string>;
%template(StatusSwigConnectionPair) std::pair<RaceLib::ApiStatus, RaceLib::SwigConnectionObject>;

%include "race/common/ChannelId.h"
%include "race/common/RaceHandle.h"
%include "race/common/RaceLog.h"
%include "race/Race.h"

%inline %{
// mitigate circular deps that prohibit including Race.h as well as duplicate wrapper defs for Race
namespace RaceLib {
struct ReadResult {
    ApiStatus status;
    std::vector<uint8_t> result;
};

struct SwigConnectionObject: public ConnectionObject {
    // default / copy ctors necessary for swig
    SwigConnectionObject() {}
    SwigConnectionObject(const ConnectionObject &that) : ConnectionObject(that) {}

    SwigConnectionObject(std::shared_ptr<Core> core, OpHandle handle) :
        ConnectionObject(core, handle) {}
    
    virtual ~SwigConnectionObject() {}

      // SWIG fails when wrapping the read() return pair
    ReadResult readSwig() {
        auto result = read();
        return ReadResult { result.first, result.second };
    }
};


// accept needs to return a SwigConnectionObject 
struct SwigAcceptObject: public AcceptObject {
    std::pair<ApiStatus, SwigConnectionObject> acceptSwig(int) {
        auto pair = accept();
        return { pair.first, pair.second };
    }
};


// SWIG doesn't support std::tuple returned by Race::listen()
struct ListenResult {
    ApiStatus status; 
    LinkAddress linkAddr;
    SwigAcceptObject acceptObject;
};

struct RaceSwig: public Race {
    RaceSwig(std::string race_dir, ChannelParamStore params) : 
        Race(race_dir, params) {}
    RaceSwig(std::shared_ptr<Core> core) : Race(core) {}
    virtual ~RaceSwig() {}

    ListenResult listenSwig(ReceiveOptions options) {
        auto tuple = listen(options);
        return ListenResult { std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple) };
    }

    std::pair<ApiStatus, SwigConnectionObject> dial_strSwig(SendOptions options, std::string message) {
        auto pair = dial_str(options, message);
        return { pair.first, pair.second };
    }
};
}  // namespace RaceLib
%}

%template(SwigConnVector) std::vector<SwigConnectionObject>; // go doesn't support generic containers
