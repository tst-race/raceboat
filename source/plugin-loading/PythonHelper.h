
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

#pragma once

#ifdef PYTHON_PLUGIN_SUPPORT
#include <Python.h>

#include <string>

#include "swigpyrun.h"

namespace RaceLib {

/**
 * @brief Initialize the python interpreter. Loads the python interpreter from the location
 * specified by pythonHome. Adds plugin module path and shims path to the python path so plugins
 * and shims can be loaded. No-op if called multiple times.
 *
 * @param pythonHome The location to load the python interpreter from
 * @param pluginModulePath The location where plugins are located
 * @param shimsPath The location where python language shims are located
 * @return true
 */
bool initPython(std::string pythonHome, std::string pluginModulePath, std::string shimsPath);

/**
 * @brief Log a Python error. This function should be passed the resulting `value` from
 *        `PyErr_Fetch`.
 *
 * @param value Pointer to the Python exception object.
 */
std::string logPyErr(PyObject *value);

/**
 * @brief Check if a Python error occurred. If it did, throw an exception describing the error.
 *
 */
void checkForPythonError();

void savePythonThread();

/**
 * @brief Destroy the plugin with the given type
 *
 */
void destroyPythonPlugin(void **obj, const std::string &pluginType);

}  // namespace RaceLib

#endif
