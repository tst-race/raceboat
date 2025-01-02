
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

#ifdef PYTHON_PLUGIN_SUPPORT
#include "PythonHelper.h"

#include "helper.h"
namespace Raceboat {

bool initPython(std::string pythonHome, std::string pluginPath,
                std::string shimsPath) {
  TRACE_FUNCTION(pythonHome, pluginPath, shimsPath);
  static bool called = false;
  if (!called) {
    called = true;
    if (!pythonHome.empty()) {
      helper::logInfo(logPrefix + "Setting python home to " + pythonHome);
      std::wstring widePythonHome(pythonHome.begin(), pythonHome.end());
      #pragma clang diagnostic ignored "-Wdeprecated-declarations"
      Py_SetPythonHome(widePythonHome.c_str());

      auto pythonPath = pythonHome + ":" + pythonHome + "/lib-dynload/";
      helper::logInfo(logPrefix + "Setting python path to " + pythonPath);
      std::wstring widePythonPath(pythonPath.begin(), pythonPath.end());
      Py_SetPath(widePythonPath.c_str());
    }
    Py_Initialize();

    PyObject *sys = PyImport_ImportModule("sys");
    PyObject *path = PyObject_GetAttrString(sys, "path");
    PyList_Append(path, PyUnicode_FromString(pluginPath.c_str()));
    PyList_Append(path, PyUnicode_FromString(shimsPath.c_str()));

    helper::logInfo(logPrefix +
                    "Python version: " + std::string(Py_GetVersion()));
  }
  return true;
}

std::string logPyErr(PyObject *value) {
  TRACE_FUNCTION();
  helper::logError(logPrefix + "an error occurred while loading the plugin.");
  PyObject *valueStr = PyObject_Str(value);
  if (valueStr == nullptr) {
    const std::string errorMessage =
        "logPyErr failed to get error description: an error occurred while "
        "trying to "
        "retrieve "
        "the Python error [valueStr].";
    helper::logError(logPrefix + errorMessage);
    return errorMessage;
  }
  PyObject *encodedValueStr =
      PyUnicode_AsEncodedString(valueStr, "utf-8", "~E~");
  if (encodedValueStr == nullptr) {
    const std::string errorMessage =
        "logPyErr failed to get error description: an error occurred while "
        "trying to "
        "retrieve "
        "the Python error [encodedValueStr].";
    helper::logError(logPrefix + errorMessage);
    return errorMessage;
  }
  const char *bytes = PyBytes_AS_STRING(encodedValueStr);
  if (bytes == nullptr) {
    const std::string errorMessage =
        "logPyErr failed to get error description: an error occurred while "
        "trying to "
        "retrieve "
        "the Python error [bytes].";
    helper::logError(logPrefix + errorMessage);
    return errorMessage;
  }
  const std::string errorDescription{bytes};
  helper::logError(logPrefix + errorDescription);
  return errorDescription;
}

void checkForPythonError() {
  PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
  PyErr_Fetch(&type, &value, &traceback);
  if (value != nullptr) {
    const std::string errorMessage = logPyErr(value);
    throw std::runtime_error(errorMessage);
  }
}

void savePythonThread() {
  TRACE_FUNCTION();
  // WARNING: calling PyEval_SaveThread() twice will cause the program to crash.
  static bool PyEval_SaveThread_wasCalled = false;
  if (!PyEval_SaveThread_wasCalled) {
    PyEval_SaveThread_wasCalled = true;
    // Release the GIL and reset the thread state to NULL. Note that the Python
    // plugin will deadlock if this API is not called.
    // https://docs.python.org/3.7/c-api/init.html#c.PyEval_SaveThread
    // TODO: we should be storing the result of this call and passing it to
    // PyEval_RestoreThread on shutdown.
    helper::logDebug(logPrefix + "calling PyEval_SaveThread...");
    /*PyThreadState *save = */ PyEval_SaveThread();
    helper::logDebug(logPrefix + "PyEval_SaveThread returned");
  }
}

void destroyPythonPlugin(void **obj, const std::string &pluginType) {
  Py_Initialize();
  PyGILState_STATE gstate = PyGILState_Ensure();

  swig_type_info *pTypeInfo = SWIG_TypeQuery(pluginType.c_str());
  PyObject *new_pObj = SWIG_NewPointerObj(*obj, pTypeInfo, SWIG_POINTER_OWN);
  Py_DECREF(new_pObj);

  PyGILState_Release(gstate);
}
} // namespace Raceboat
#endif
