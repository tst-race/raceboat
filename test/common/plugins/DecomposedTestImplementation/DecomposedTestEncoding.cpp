
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
// 

#include <common/RaceLog.h>
#include <decomposed/IEncodingComponent.h>

#define TRACE_METHOD(...) TRACE_METHOD_BASE(DecomposedTestEncoding, ##__VA_ARGS__)

class DecomposedTestEncoding : public IEncodingComponent {
public:
    explicit DecomposedTestEncoding(IEncodingSdk *sdk) : sdk(sdk) {
        TRACE_METHOD();
        sdk->updateState(COMPONENT_STATE_STARTED);
    }

    virtual ComponentStatus onUserInputReceived(RaceHandle /* handle */, bool /* answered */,
                                                const std::string & /* response */) override {
        TRACE_METHOD();
        return COMPONENT_OK;
    }

    virtual EncodingProperties getEncodingProperties() override {
        TRACE_METHOD();
        return {0, "application/octet-stream"};
    }

    virtual SpecificEncodingProperties getEncodingPropertiesForParameters(
        const EncodingParameters & /* params */) override {
        TRACE_METHOD();
        return {1000000};
    }

    virtual ComponentStatus encodeBytes(RaceHandle handle, const EncodingParameters & /* params */,
                                        const std::vector<uint8_t> &bytes) override {
        TRACE_METHOD(bytes.size());
        sdk->onBytesEncoded(handle, bytes, ENCODE_OK);
        return COMPONENT_OK;
    }

    virtual ComponentStatus decodeBytes(RaceHandle handle,
                                        const EncodingParameters & /* params */,
                                        const std::vector<uint8_t> & bytes) override {
        TRACE_METHOD(bytes.size());
        sdk->onBytesDecoded(handle, bytes, ENCODE_OK);
        return COMPONENT_OK;
    }

private:
    IEncodingSdk *sdk;
};

IEncodingComponent *createEncoding(const std::string & /* encoding */, IEncodingSdk *sdk,
                                   const std::string & /* roleName */,
                                   const PluginConfig & /* pluginConfig */) {
    return new DecomposedTestEncoding(sdk);
}

void destroyEncoding(IEncodingComponent *component) {
    delete component;
}
