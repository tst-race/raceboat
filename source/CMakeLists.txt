
# Copyright 2023 Two Six Technologies
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

#Add the source to the shared library
add_library(raceSdkCommon SHARED
    api-managers/ChannelManager.cpp
    api-managers/ApiManager.cpp
    common/ChannelProperties.cpp
    common/ChannelRole.cpp
    common/ChannelStatus.cpp
    common/ConnectionStatus.cpp
    common/ConnectionType.cpp
    common/EncPkg.cpp
    common/LinkProperties.cpp
    common/LinkPropertyPair.cpp
    common/LinkPropertySet.cpp
    common/LinkStatus.cpp
    common/LinkType.cpp
    common/PackageStatus.cpp
    common/PackageType.cpp
    common/PluginResponse.cpp
    common/RaceEnums.cpp
    common/RaceLog.cpp
    common/SendType.cpp
    common/TransmissionType.cpp
    decomposed/ComponentActionManager.cpp
    decomposed/ComponentConnectionManager.cpp
    decomposed/ComponentLifetimeManager.cpp
    decomposed/ComponentLinkManager.cpp
    decomposed/ComponentManager.cpp
    decomposed/ComponentManagerTypes.cpp
    decomposed/ComponentPackageManager.cpp
    decomposed/ComponentReceivePackageManager.cpp
    decomposed/ComponentTypes.cpp
    decomposed/ComponentWrappers.cpp
    decomposed/MimeTypes.cpp
    decomposed/SdkWrappers.cpp
    unified/SdkResponse.cpp
    plugin-loading/ComponentPlugin.cpp
    plugin-loading/CompositeWrapper.cpp
    plugin-loading/Config.cpp
    plugin-loading/DecomposedPluginLoader.cpp
    plugin-loading/DynamicLibrary.cpp
    plugin-loading/LoaderWrapper.cpp
    plugin-loading/PluginLoader.cpp
    plugin-loading/PythonHelper.cpp
    plugin-loading/PythonComponentPlugin.cpp
    plugin-loading/PythonLoaderWrapper.cpp
    state-machine/ApiContext.cpp
    state-machine/ConduitStateMachine.cpp
    state-machine/ConnectionStateMachine.cpp
    state-machine/DialStateMachine.cpp
    state-machine/BootstrapDialStateMachine.cpp
    state-machine/Events.cpp
    state-machine/ListenStateMachine.cpp
    state-machine/BootstrapListenStateMachine.cpp
    state-machine/PreConduitStateMachine.cpp
    state-machine/BootstrapPreConduitStateMachine.cpp
    state-machine/ReceiveStateMachine.cpp
    state-machine/SendStateMachine.cpp
    state-machine/SendReceiveStateMachine.cpp
    state-machine/StateMachine.cpp
    base64.cpp
    Composition.cpp
    Core.cpp
    FileSystem.cpp
    Handler.cpp
    helper.cpp
    PluginContainer.cpp
    PluginDef.cpp
    PluginWrapper.cpp
    Race.cpp
    SdkWrapper.cpp
    Storage.cpp
    UserInput.cpp
)

#Add the headers to the shared library
file(GLOB_RECURSE COMM_LIB_PUBLIC_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/*.h
)

set_target_properties(raceSdkCommon PROPERTIES
    PUBLIC_HEADER "${COMM_LIB_PUBLIC_HEADERS}"
)
target_include_directories(raceSdkCommon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

file(GLOB_RECURSE COMM_LIB_PRIVATE_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

set_target_properties(raceSdkCommon PROPERTIES
    PRIVATE_HEADER "${COMM_LIB_PRIVATE_HEADERS}"
)

target_compile_definitions(raceSdkCommon PUBLIC SWIG_TYPE_TABLE=RaceSdk)
find_package(PythonLibs 3 REQUIRED)
target_compile_definitions(raceSdkCommon PRIVATE PYTHON_PLUGIN_SUPPORT=1)

if(ANDROID)
    target_link_libraries(raceSdkCommon
        PRIVATE
            language-shims-headers
            ${PYTHON_LIBRARIES}
            ${CMAKE_DL_LIBS} # Contains dlopen and dlclose for dynamically loading plugins.
    )
    target_include_directories(raceSdkCommon SYSTEM PRIVATE ${PYTHON_INCLUDE_PATH})

else()
    # Default to Linux
    target_link_libraries(raceSdkCommon
        PRIVATE
            language-shims-headers
            ${PYTHON_LIBRARIES}
            ${CMAKE_DL_LIBS} # Contains dlopen and dlclose for dynamically loading plugins.
        PUBLIC
            stdc++fs
    )
    target_include_directories(raceSdkCommon SYSTEM PRIVATE ${PYTHON_INCLUDE_PATH})
endif()

# TODO enable performance-* and readability-* checks (needs fixes)
# setup_clang_tidy_for_target(TARGET raceSdkCommon)
# setup_cppcheck_for_target(
#     TARGET raceSdkCommon
#     CHECKS all
#     SUPPRESS unmatchedSuppression unusedFunction missingIncludeSystem useStlAlgorithm
# )
setup_clang_format_for_target(raceSdkCommon)
