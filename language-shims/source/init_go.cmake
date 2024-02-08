
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


set (CORE_SHIM_PATH ${CMAKE_CURRENT_BINARY_DIR}/language-shims/source/include/src/core)
message("initializing go core shims for use in other projects in ${CORE_SHIM_PATH}")
execute_process(
    COMMAND go mod init corePluginBindingsGolang.go
    WORKING_DIRECTORY ${CORE_SHIM_PATH}
)

set (COMM_SHIM_PATH ${CMAKE_CURRENT_BINARY_DIR}/language-shims/source/include/src/shims)
message("initializing go comm shims for use in other projects in ${COMM_SHIM_PATH}")
execute_process(
    COMMAND go mod init commsPluginBindingsGolang.go
    WORKING_DIRECTORY ${COMM_SHIM_PATH}
)