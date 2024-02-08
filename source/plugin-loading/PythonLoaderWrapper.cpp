
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

#include "PythonLoaderWrapper.h"

#ifdef PYTHON_PLUGIN_SUPPORT
#include <Python.h>

#include <algorithm> // std::find
#include <fstream>
#include <memory> // std::shared_ptr
#include <stdexcept>
#include <string>
#include <vector>

#include "PythonHelper.h"
#include "helper.h"

namespace Raceboat {

/**
 * @brief Create a Python plugin.
 *
 * @param sdk Pointer to the RaceSdk instance associated with the plugin.
 * @return void* Pointer to the plugin, or nullptr on failure.
 */
static void *createPythonPlugin(IRaceSdkComms *sdk,
                                const std::string &pythonModule,
                                const std::string &pythonClass) {
  TRACE_FUNCTION(pythonModule, pythonClass);
  if (sdk == nullptr) {
    helper::logError(logPrefix + "RaceSdk pointer is nullptr");
    return nullptr;
  }

  // Ensure that the current thread is ready to call the Python C API regardless
  // of the current state of Python, or of the global interpreter lock. This may
  // be called as many times as desired by a thread as long as each call is
  // matched with a call to PyGILState_Release().
  // https://docs.python.org/3.7/c-api/init.html#c.PyGILState_Ensure
  PyGILState_STATE gstate = PyGILState_Ensure();

  // Import the Python plugin module;
  // https://docs.python.org/3.7/c-api/import.html#c.PyImport_ImportModule
  PyObject *new_pModule = PyImport_ImportModule(pythonModule.c_str());
  checkForPythonError();

  // Get the Python plugin class from the module.
  // https://docs.python.org/3.7/c-api/object.html#c.PyObject_GetAttrString
  PyObject *new_plugin =
      PyObject_GetAttrString(new_pModule, pythonClass.c_str());
  Py_DECREF(new_pModule);
  if (!new_plugin || !PyCallable_Check(new_plugin)) {
    helper::logError(logPrefix + "Cannot find plugin.");
    checkForPythonError();
    PyGILState_Release(gstate);
    return nullptr;
  }

  // Set the Python wrapped SDK object as a Python argument.
  PyObject *new_pArgs = PyTuple_New(1);

  // Convert/wrap the SDK object for python using SWIG bindings.
  PyTuple_SetItem(new_pArgs, 0,
                  SWIG_NewPointerObj(sdk, SWIG_TypeQuery("IRaceSdkComms*"), 0));

  // Construct the Python plugin object.
  // https://docs.python.org/3.7/c-api/object.html#c.PyObject_CallObject
  PyObject *new_instance = PyObject_CallObject(new_plugin, new_pArgs);
  Py_DECREF(new_plugin);
  Py_DECREF(new_pArgs);
  if (new_instance == nullptr) {
    helper::logError(logPrefix + "PyObject_CallObject returned nullptr");
    checkForPythonError();
    PyGILState_Release(gstate);
    return nullptr;
  }

  // Convert/wrap the Python plugin for C++ using SWIG bindings.
  void *pythonPluginCpp = nullptr;
  swig_type_info *pTypeInfo = SWIG_TypeQuery("IRacePluginComms*");

  const int res = SWIG_ConvertPtr(new_instance, &pythonPluginCpp, pTypeInfo,
                                  SWIG_POINTER_DISOWN); // steal reference
  if (!SWIG_IsOK(res)) {
    helper::logError(
        logPrefix +
        "Failed to convert pointer to IRacePluginComms*. ptypeinfo = " +
        std::string{pTypeInfo->name});
    Py_DECREF(new_instance);
    PyGILState_Release(gstate);
    return nullptr;
  }

  // Release any acquired resources now that we are done calling Python APIs.
  // This is the matching call to the call to PyGILState_Ensure() above.
  // https://docs.python.org/3.7/c-api/init.html#c.PyGILState_Release
  PyGILState_Release(gstate);

  savePythonThread();

  helper::logInfo(logPrefix + "returning");
  return pythonPluginCpp;
}

static void destroyPythonCommsPlugin(void *obj) {
  destroyPythonPlugin(&obj, "IRacePluginComms*");
}

std::shared_ptr<IRacePluginComms>
createPythonPluginSharedPtr(IRaceSdkComms *sdk, const PluginDef &pluginDef) {
  return std::shared_ptr<IRacePluginComms>(
      static_cast<IRacePluginComms *>(createPythonPlugin(
          sdk, pluginDef.pythonModule, pluginDef.pythonClass)),
      destroyPythonCommsPlugin);
}

PythonLoaderWrapper::PythonLoaderWrapper(PluginContainer &container, Core &core,
                                         const PluginDef &pluginDef)
    : PluginWrapper(container) {
  // TODO: fix python path on android?
  static bool pythonInitialized =
      initPython("", core.getFS().makePluginInstallBasePath().string(),
                 core.getFS().makeShimsPath("python").string());
  TRACE_METHOD(pythonInitialized);
  this->mPlugin = createPythonPluginSharedPtr(this->getSdk(), pluginDef);
  if (!this->mPlugin) {
    const std::string errorMessage = logPrefix + "failed to create plugin";
    helper::logError(errorMessage);
    throw std::runtime_error(errorMessage);
  }
  this->mDescription = pluginDef.filePath;
}

PythonLoaderWrapper::~PythonLoaderWrapper() {
  TRACE_METHOD();
  this->mPlugin.reset();
}

} // namespace Raceboat
#endif
