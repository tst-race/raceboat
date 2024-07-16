# The decomposed interface

The decomposed interface consists of three separate interfaces: the encoding interface, the transport interface, and the usermodel interface. An object that implements one of the interfaces is called a component. There is a corresponding type of component for each of the three interfaces. A decomposed channel (i.e. a channel created using the decomposed interface) consists of at least one of each of the types of components, although multiple encoding components are allowed. There is a component manager that utilizes the components and handles additional logic such as scheduling what bytes are encoded and when.

An encoding component is responsible for encoding and decoding the bytes sent. The specific encoding component provided will determine how the bytes are encoded/decoded. Some examples are no-op, base64, jpg, or video.

A transport component is responsible for all network related functions. This includes sending bytes over the network, but depending on the implementation may also include stuff such as generating background traffic to legitimate sites, logging into an account, or scraping a web page where another node may have posted data for this node.

A User model component is responsible for scheduling when transport actions occur. The actions the user model creates are most commonly fetching and posting, but may also include other actions if necessary. One concept related to the need for user models is 'behavioral independence'. This is the idea that the behavior of a transport is statistically independent from the behavior of the user that is sending messages.


# Implementing a decomposed encoding

## The Encoding SDK Interface
This is the interface that the encoding uses to communicate with the rest of the race application. The core implements this interface and the encoding is given an object that implements it in the `createPluginEncoding()` call.

---
```c++
ChannelResponse updateState(ComponentState state);
```
Inform the component manager of a change in state.

NOTE: This MUST be called with the state `COMPONENT_STARTED` before the component can be used. This should be done in either the constructor or `onUserInputReceived()` if the component requires user input before starting.

---
```c++
ChannelResponse requestPluginUserInput(const std::string &key, const std::string &prompt, bool cache);
```
`requestPluginUserInput()` is used to get user modifiable setting / configuration values. This is commonly called during component activation and completion of component activation delayed until the response is received. The return value of this function will contain a handle that matches a future `onUserInputReceived()` call into the plugin.

These calls should correspond to the `channel_parameters` section of the manifest file.

This is for component specific parameters.

---
```c++
ChannelResponse requestCommonUserInput(const std::string &key);
```
`requestCommonUserInput()` is used to get user modifiable setting / configuration values. This is commonly called during component activation and completion of component activation delayed until the response is received. The return value of this function will contain a handle that matches a future `onUserInputReceived()` call into the plugin.

These calls should correspond to the `channel_parameters` section of the manifest file.

This is for some common parameters that are used by multiple plugins or components.

See [Valid Common User Input](#valid-common-user-input) for valid keys.

---
```c++
ChannelResponse onBytesEncoded(RaceHandle handle, const std::vector<uint8_t> &bytes, EncodingStatus status);
```
Tell the component manager that encoding has completed. This must be called in response to a `encodeBytes()` call. Status may be either `ENCODE_OK`, indicating the encoding succeeded, or `ENCODE_FAILED`, indicating that encoding was not successful. `handle` must match the handle supplied to the corresponding `encodeBytes()` call. `bytes` must contain the encoded bytes if status is `ENCODE_OK`.

---
```c++
ChannelResponse onBytesDecoded(RaceHandle handle, const std::vector<uint8_t> &bytes, EncodingStatus status);
```
Tell the component manager that decoding has completed. This must be called in response to a `decodeBytes()` call. Status may be either `ENCODE_OK` (yes, it's 'encode' even though this is decoding), indicating the decoding succeeded, or `ENCODE_FAILED`, indicating that decoding was not successful. `handle` must match the handle supplied to the corresponding `decodeBytes()` call. `bytes` must contain the decoded bytes if status is `ENCODE_OK`.

---


## The Encoding Component Interface

This is the interface that the rest of the RACE application uses to communicate with the user model. The user model must implement this interface and return the implementing object in the `createUserModel()` call.

---
```c++
ComponentStatus onUserInputReceived(RaceHandle handle, bool answered, const std::string &response);
```
Receive a response to a previous `requestPluginUserInput()` or `requestCommonUserInput()` call. The handle will match the handle returned in the `SdkResponse` object of the call that this is responding to.


---
```c++
EncodingProperties getEncodingProperties();
```
The component manager will call this to get some properties of the encoder. The properties contain the time taken to encode, and what type of encoding it is.

The time taken to encode should be a pessimistic estimate. It is used to calculate how long before an action is taken will the encoding start. The units are seconds and may be fractional.

The type of encoding should be a mime-type string. Examples include: "text/plain", "application/octet-stream", and "image/png".


---
```c++
SpecificEncodingProperties getEncodingPropertiesForParameters(const EncodingParameters &params);
```
The component manager will call this to get some properties of the encoder given some parameters. The encoding parameters may contain encoder specific information if the transport is compatible. The properties returned should contain the number of bytes that can be encoded into the content that will be returned for the supplied parameters.


---
```c++
ComponentStatus encodeBytes(RaceHandle handle, const EncodingParameters &params, const std::vector<uint8_t> &bytes);
```
Encode bytes into a message. `onBytesEncoded()` should be called once the encoding is complete.


---
```c++
ComponentStatus decodeBytes(RaceHandle handle, const EncodingParameters &params, const std::vector<uint8_t> &bytes);
```
Decode bytes from a message. `onBytesDecoded()` should be called once the decoding is complete.

---
# Implementing a decomposed transport

## The Transport SDK Interface
This is the interface that the transport uses to communicate with the rest of the race application. The core implements this interface and the transport is given an object that implements it in the `createPluginTransport()` call.

---
```c++
ChannelResponse updateState(ComponentState state);
```
Inform the component manager of a change in state.

NOTE: This MUST be called with the state `COMPONENT_STARTED` before the component can be used. This should be done in either the constructor or `onUserInputReceived()` if the component requires user input before starting.


---
```c++
ChannelResponse requestPluginUserInput(const std::string &key, const std::string &prompt, bool cache);
```
`requestPluginUserInput()` is used to get user modifiable setting / configuration values. This is commonly called during component activation and completion of component activation delayed until the response is received. The return value of this function will contain a handle that matches a future `onUserInputReceived()` call into the plugin.

These calls should correspond to the `channel_parameters` section of the manifest file.

This is for component specific parameters.

---
```c++
ChannelResponse requestCommonUserInput(const std::string &key);
```
`requestCommonUserInput()` is used to get user modifiable setting / configuration values. This is commonly called during component activation and completion of component activation delayed until the response is received. The return value of this function will contain a handle that matches a future `onUserInputReceived()` call into the plugin.

These calls should correspond to the `channel_parameters` section of the manifest file.

This is for some common parameters that are used by multiple plugins or components.

See [Valid Common User Input](#valid-common-user-input) for valid keys.

---
```c++
ChannelProperties getChannelProperties();
```
Get the channel properties listed in the channel properties json.

See [channel properties documentation](./ChannelAndLinkProperties.md) for details about channel properties.

---
```c++
ChannelResponse onLinkStatusChanged(RaceHandle handle, const LinkID &linkId, LinkStatus status, const LinkParameters &params);
```
Update the status of a link. `onLinkStatusChanged()` must be called in response to `createLinkFromAddress()`, `createLink()`, `loadLinkAddress()`, `loadLinkAddresses()`, or `destroyLink()`. It may also be called with `LINK_DESTROYED` if the link fails and cannot be reestablished separately from those any of those calls.

If the call is a response to a previous call, the handle should be match the handle supplied in the original call, otherwise it should be `NULL_RACE_HANDLE`.

`params` may be used when creating or loading to communicate information about this link to the user model.

---
```c++
ChannelResponse onPackageStatusChanged(RaceHandle handle, PackageStatus status);
```
`onPackageStatusChanged()` must be called in response to `doAction()`. If the package was successfully sent, the status should be `PACKAGE_SENT`. Otherwise, `PACKAGE_FAILED_GENERIC` must be called.

The component manager currently does not support `PACKAGE_RECEIVED`.

---
```c++
ChannelResponse onEvent(const Event &event);
```
Inform the user model of some implementation defined event that occurred.

---
```c++
ChannelResponse onReceive(const LinkID &linkId, const EncodingParameters &params, const std::vector<uint8_t> &bytes);
```
`onReceive()` must be called when a package is received on the specified link. The received package must be byte for byte identical to a package supplied in `enqueueContent()`.

Fields of the `params` object other than link id must also match the properties supplied in `enqueueContent()`. This should be handled by the user model informing the transport of any relevant properties.

---

## The Transport Component Interface

This is the interface that the rest of the RACE application uses to communicate with the transport. The transport must implement this interface and return the implementing object in the `createUserModel()` call.

---
```c++
ComponentStatus onUserInputReceived(RaceHandle handle, bool answered, const std::string &response);
```
Receive a response to a previous `requestPluginUserInput()` or `requestCommonUserInput()` call. The handle will match the handle returned in the `SdkResponse` object of the call that this is responding to.

---
```c++
TransportProperties getTransportProperties();
```
The transport properties must contain all supported actions and what type of encodings are necessary for each action.

---
```c++
LinkProperties getLinkProperties(const LinkID &linkId);
```

Returns the link properties for the specified link.

See the Channel and link properties documentation for more details about link properties.


---
```c++
ComponentStatus createLink(RaceHandle handle, const LinkID &linkId);
```
Create a new link. `onLinkStatusChanged()` must be called with either `LINK_CREATED` if creating the link was successful, or `LINK_DESTROYED` if creating the link failed and the resulting link is unusable. The handle supplied to `onLinkStatusChanged()` must match the handle passed in to `createLink()`.

---
```c++
ComponentStatus createLinkFromAddress(RaceHandle handle, const LinkID &linkId, const std::string &linkAddress);
```
Create a new link with the specified link address. `onLinkStatusChanged()` must be called with either `LINK_CREATED` if creating the link was successful, or `LINK_DESTROYED` if creating the link failed and the resulting link is unusable. The handle supplied to `onLinkStatusChanged()` must match the handle passed in to `createLinkFromAddress()`.

---
```c++
ComponentStatus loadLinkAddress(RaceHandle handle, const LinkID &linkId, const std::string &linkAddress);
```
Load a new link with the specified link address. `onLinkStatusChanged()` must be called with either `LINK_LOADED` if creating the link was successful, or `LINK_DESTROYED` if creating the link failed and the resulting link is unusable. The handle supplied to `onLinkStatusChanged()` must match the handle passed in to `loadLinkAddress()`.

---
```c++
ComponentStatus loadLinkAddresses(RaceHandle handle, const LinkID &linkId, const std::vector<std::string> &linkAddress);
```
If the channel supports loading multicast addresses, this should behave similarly to `loadLinkAddress()`. If this channel does not support multiple addresses, `onLinkStatusChanged()` must be called with the passed in handle and status set to `LINK_DESTROYED`.

---
```c++
ComponentStatus destroyLink(RaceHandle handle, const LinkID &linkId);
```
This call should clean up all resources related to the specified link. A call to `onLinkStatusChanged()` with status equal to `LINK_DESTROYED` must be issued for this link.

---
```c++
std::vector<EncodingParameters> getActionParams(const Action &action);
```
Get encoding parameters for each encoding associated with an action. The encodings associated with the action should match the list returned in `getTransportProperties()`

---
```c++
ComponentStatus enqueueContent(const EncodingParameters &params, const Action &action, const std::vector<uint8_t> &content);
```
Supply content to be utilized when performing an action.

---
```c++
ComponentStatus dequeueContent(const Action &action);
```
Remove previously enqueued content for the specified action

---
```c++
ComponentStatus doAction(const std::vector<RaceHandle> &handles, const Action &action);
```
Perform the action and send any content associated with the action. Once the action has been performed, call `onPackageStatusChanged()` for each of the handles provided.

---

# Implementing a decomposed usermodel

## The User Model SDK Interface

This is the interface that the user model uses to communicate with the rest of the race application. The core implements this interface and the user model is given an object that implements it in the `createPluginUserModel()` call.

---
```c++
ChannelResponse updateState(ComponentState state);
```
Inform the component manager of a change in state.

NOTE: This MUST be called with the state `COMPONENT_STARTED` before the component can be used. This should be done in either the constructor or `onUserInputReceived()` if the component requires user input before starting.


---
```c++
ChannelResponse requestPluginUserInput(const std::string &key, const std::string &prompt, bool cache);
```
`requestPluginUserInput()` is used to get user modifiable setting / configuration values. This is commonly called during component activation and completion of component activation delayed until the response is received. The return value of this function will contain a handle that matches a future `onUserInputReceived()` call into the plugin.

These calls should correspond to the `channel_parameters` section of the manifest file.

This is for component specific parameters.

---
```c++
ChannelResponse requestCommonUserInput(const std::string &key);
```
`requestCommonUserInput()` is used to get user modifiable setting / configuration values. This is commonly called during component activation and completion of component activation delayed until the response is received. The return value of this function will contain a handle that matches a future `onUserInputReceived()` call into the plugin.

These calls should correspond to the `channel_parameters` section of the manifest file.

This is for some common parameters that are used by multiple plugins or components.

See [Valid Common User Input](#valid-common-user-input) for valid keys.

---
```c++
ChannelResponse onTimelineUpdated();
```
Inform the component manager that the timeline of actions has been updated. The component manager will call `getTimeline()` to get the updated timeline. This is commonly called during `addLink()` or `removeLink()` to adjust actions so that packages can be sent or received during them.

---
## The User Model Component Interface

This is the interface that the rest of the RACE application uses to communicate with the user model. The user model must implement this interface and return the implementing object in the `createUserModel()` call.

---
```c++
ComponentStatus onUserInputReceived(RaceHandle handle, bool answered, const std::string &response);
```
Receive a response to a previous `requestPluginUserInput()` or `requestCommonUserInput()` call. The handle will match the handle returned in the `SdkResponse` object of the call that this is responding to.

---
```c++
UserModelProperties getUserModelProperties();
```
The component manager will call this to get some properties of the user model. The fields of the UserModelProperties object returned controls how long the timeline is expected to be and how often it should be updated independent of `onTimelineUpdated()` calls. A usermodel which produces many actions per time period should have a shorter timeline period and one which is very sparse should have a longer one. The fetch period should be approximately half the timeline period.

---
```c++
ComponentStatus addLink(const LinkID &link, const LinkParameters &params);
```
This is called to inform the user model that a link has been created with the specified parameters. The link id will match a subsequent call to `removeLink()`. `params` contains a string with implementation defined meaning. It can be used to communicate information from the transport with regards to this link.

---
```c++
ComponentStatus removeLink(const LinkID &link);
```
This is called to inform the user model that a link has been removed. The link id will match the corresponding call to `addLink()`.

---
```c++
ActionTimeline getTimeline(Timestamp start, Timestamp end);
```
The component manager calls this periodically to get a list of actions for the transport to execute. Each action should have a timestamp between the start timestamp and end timestamp. The component manager will never go back in time. If it has asked for actions after some `start` timestamp, all future `getTimestamp()` calls will have `start` equal to or greater than that value.

---
```c++
ComponentStatus onTransportEvent(const Event &event);
```
This gets called in response to the transport calling `onEvent()`. Details about the event are transport specific.

---
```c++
ActionTimeline onSendPackage(const LinkID &linkId, int bytes);
```
An optional call to react to send package events. Returned events are added to the component manager's timeline. Actions with a timestamp of 0 are done immediately. This may be used to implement a usermodel which sends messages out as soon as it gets them, but be careful about transports and encodings with size limits. A common issue is that a single action is produced in response to a `sendPackage()` call, but that package takes more than one action to send. This can be worked around with the `bytes` parameter if the size limit is known. The `bytes` parameter does not include fragmentation overhead per action which can be 30 bytes or higher if fragments of several packages are packed into a single action.

---

# The manifest.json file
The manifest.json file is a json file that informs the core of properties of each plugin before it gets loaded.  This allows the core to know if it needs to be loaded and how to load it. There must be a manifest.json in each kit. The format is as follows:

```json
{
    "plugins": [
        {
            "file_path": "<kit name>",
            "plugin_type": "TA2",
            "file_type": "<shared_library|python>",
            "node_type": "any",
            "shared_library_path": "<shared library path>",
            "encodings": ["<encoding component name>", ...],
            "transports": ["<transport component name>", ...],
            "usermodels": ["<usermodel component name>", ...],
            "channels": [],
        },
    ],
    "compositions": [
        {
            "id": "<composition component name>",
            "transport": "<transport component name>",
            "usermodel": "<usermodel component name>",
            "encodings": ["<encoding component name>", ...]
        }
    ],
    "channel_properties": [
        <omitted>
    ],
    "channel_parameters": [
      {
          "key": "<key value>",
          "plugin": "<plugin name>",
          "required": true|false,
          "type": "<string|int|float>",
          "default": <value of type matching value of type field>
      },
      ...
    ]
}
```

The channel properties section is rather large and has been omitted for brevity. See the [channel properties documentation](ChannelAndLinkProperties.md) for details

The `compositions`, `channel_properties`, and `channel_parameters` sections are currently optional. There are plans for the `channel_properties` and `channel_parameters` sections to become required and it is suggested to implement them. The `compositions` section is necessary if the kit supplies a decomposed channel. 

---
## Plugins section
This section contains a list of plugin definitions. Each definition has the following entries.

```
file_path
```
The name of the kit containing this plugin

---
```
plugin_type
```
This should be "TA2" for a comm plugin

---
```
file_type
```
The allowed values are `shared_library` and `python`

---
```
node_type
```
The allowed values are `client`, `server`, and `any`. This should be `any` for a comm plugin.

---
```
shared_library_path
```
If `file_type` is `shared_library`, this should contain the path of the plugin's shared object. This path is relative to the kit directory. If the .so file is at the top level of the directory, this should just be the file name.

This key may be omitted if the file type is not `shared_library`

---
```
encodings
```
A list of encoding components contained by the plugin.

This may be omitted if this plugin does not contain any encoding components.

---
```
transports
```
A list of transport components contained by the plugin.

This may be omitted if this plugin does not contain any transport components.


---
```
usermodels
```
A list of user model components contained by the plugin.

This may be omitted if this plugin does not contain any user model components.


---
```
channels
```
A list of unified channels contained by the plugin.

This may be omitted if this plugin does not contain any unified channels.

---

## Compositions section
This section contains a list of compositions. Each definition has the following entries.

```
id
```
The id of the composition. This also acts as the channel id for the composition.

---
```
transport
```
The transport component to be used for the composition. This component may refer to a component supplied by a different kit instead of being supplied by one of the plugins in this kit.

If this component is from a different kit, the kit and where to get it should be listed in the README as a dependency. If creating or running a deployment, compositions are checked to make sure all components are available, but they are not automatically loaded.

---
```
usermodel
```
The usermodel component to be used for the composition. This component may refer to a component supplied by a different kit instead of being supplied by one of the plugins in this kit.

If this component is from a different kit, the kit and where to get it should be listed in the README as a dependency. If creating or running a deployment, compositions are checked to make sure all components are available, but they are not automatically loaded.

---
```
encodings
```
A list of encoding components to be used for the composition. These components may refer to a components supplied by a different kit instead of being supplied by the plugins in this kit.

If this component is from a different kit, the kit and where to get it should be listed in the README as a dependency. If creating or running a deployment, compositions are checked to make sure all components are available, but they are not automatically loaded.

---

## Channel Properties section

The channel properties contains a list of channel properties objects. This is very verbose, and so is omitted for brevity. Refer to the [channel property documentation](ChannelAndLinkProperties.md) for details.

## Channel Parameters section
This section contains a list of channel parameter entries. These should correspond to calls to `requestPluginUserInput()`. Each definition has the following entries.

```
key
```
The key that's used when calling `requestPluginUserInput()`.

---
```
plugin
```
What plugin calls `requestPluginUserInput()` with the specified key.

This is optional if there is only one plugin in the kit

---
```
required
```
Whether the plugin requires this key to run.

---
```
type
```
What is the type of the value associated with this key. This controls the user input screen. All values will be passed as strings.

This field is optional. The default is "string".

---
```
default
```
If this value is not supplied, this will be supplied instead. the type of this field should match the value of the type field.

This field is optional.

---

# Adding a dependency (via RUNPATH)

The RACE system bundles some dependencies along with the system. These dependencies are part of the racesdk image. If a comm plugin needs one of these dependencies then including it is as simple as linking against it.

If a comm plugin requires additional shared libraries that are not included by the race system, a different approach must be taken. Additional dependencies must be bundled and built with the plugin and must be part of the kit that gets installed. e.g. they should sit within the plugin directory of the kit in this tree view:

```
kit
├── artifacts
│   ├── android-arm64-v8a-client
│   │   └── PluginTa2TwoSixStubDecomposed
│   │       ├── libPluginTa2TwoSixStubEncoding.so
|   |       └── manifest.json
...
```

Linking may be done as normal, but the RPATH must be set on the plugin library in order to find the dependency at runtime.

```cmake
set_target_properties(PluginTa2TwoSixStubEncoding PROPERTIES
    BUILD_RPATH "\${ORIGIN}/lib"
    INSTALL_RPATH "\${ORIGIN}/lib"
)
```
This is an example of setting the RPATH of a target using CMAKE. ${ORIGIN} must be used to get an RPATH relative to the library the RPATH is set on. In this case, it would now be able to find anything in a `lib` directory next to the plugin shared library.

The equivalent gcc/clang argument is:
```
-Wl,-rpath,${ORIGIN}/lib
```

# Valid Common User Input

The key argument to `requestCommonUserInput()` must take one of a few possible values. Valid values are `hostname`, for the publicly available host name (or ip address) of the current node if it has now, and `env`, for the type of environment the node is running on. Valid responses for `env` include "any", "dmz", "home", "phone", "service", and "user".

# Running a custom plugin

Follow the directions for running a race deployment with [RIB](https://github.com/tst-race/race-in-the-box). Make sure to mount a directory that contains your local kit. To use a locally built kit, use the flags `--comms-kit core=<path to local kit>` and `--comms-channel=<channel id>` when creating a deployment. Here is an example deployment create command that works with the example unified plugin created before:

```
rib deployment local create \
    --name github-test3 \
    --comms-kit core=plugin-ta2-twosix-cpp \
    --comms-kit local=/code/example-decomposed-comm-plugin/kit/ \
    --comms-channel StubComposition \
    --comms-channel twoSixDirectCpp \
    --comms-channel twoSixIndirectCpp \
    --linux-client-count=3 --linux-server-count=3
```


# Adapting these instructions for other programming languages

Language shims are provided via SWIG for python and golang. Language shims for rust are built inside the rust exemplar. The apis are generally the same, adapted for the language types. View the exemplar code in [race-core](https://github.com/tst-race/race-core) for examples.

One area of difference is for python manifest.json files. As python is not compiled, a .so file is not created. Instead, a python interpreter is used and python objects created by it. For unified plugins, the `python_module` and `python_class` keys should be set for the unified plugin. For a decomposed plugin only the `python_module` key is necessary, however an additional `create(Encoding|Transport|UserModel)` function should be present. This `create*` function has the same arguments and serves the same purpose as the `create*` C function given in the example code above.

Here is an example manifest.json for a unified plugin:

```json
{
    "plugins": [
        {
            "file_path": "PluginTa2TwoSixPython",
            "plugin_type": "comms",
            "file_type": "python",
            "node_type": "any",
            "python_module": "PluginTa2TwoSixPython.PluginTA2TwoSixPython",
            "python_class": "PluginTA2TwoSixPython",
            "channels": ["twoSixDirectPython", "twoSixIndirectPython"]
        }
    ],
    "channel_properties": {
        <omitted>
    },
    "channel_parameters": [
    ]
}
```
