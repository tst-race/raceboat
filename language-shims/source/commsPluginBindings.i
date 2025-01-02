%module(directors="1", threads="1") commsPluginBindings

// We need to include CommsPlugin.h in the SWIG generated C++ file
%{
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>

#include "constants.h"

#include "ChannelProperties.h"
#include "ChannelRole.h"
#include "ChannelStatus.h"
#include "ConnectionStatus.h"
#include "ConnectionType.h"
#include "EncPkg.h"
#include "LinkProperties.h"
#include "LinkPropertyPair.h"
#include "LinkPropertySet.h"
#include "LinkStatus.h"
#include "LinkType.h"
#include "PackageStatus.h"
#include "PackageType.h"
#include "PackageStatus.h"
#include "PluginConfig.h"
#include "PluginResponse.h"
#include "RaceEnums.h"
#include "RaceHandle.h"
#include "RaceLog.h"
#include "SendType.h"
#include "TransmissionType.h"

#include "race/decomposed/ComponentTypes.h"
#include "race/decomposed/IComponentBase.h"
#include "race/decomposed/IComponentSdkBase.h"
#include "race/decomposed/IEncodingComponent.h"
#include "race/decomposed/IEncodingSdk.h"
#include "race/decomposed/ITransportComponent.h"
#include "race/decomposed/ITransportSdk.h"
#include "race/decomposed/IUserModelComponent.h"
#include "race/decomposed/IUserModelSdk.h"

#include "race/unified/IRacePluginComms.h"
#include "race/unified/IRaceSdkCommon.h"
#include "race/unified/IRaceSdkComms.h"
#include "race/unified/SdkResponse.h"
%}

// Enable cross-language polymorphism in the SWIG wrapper.
// It's pretty slow so not enable by default
%feature("director");

// Catch any unhandled Python error and throw a C++ exception with the error message
%feature("director:except") {
    if ($error != NULL) {
        PyObject *ptype, *pvalue, *ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);

        // $symname will get the name of the current Python method
        std::string what = "$symname: ";

        // Add the error type name
        PyObject *ptypename = PyObject_GetAttrString(ptype, "__name__");
        what += PyUnicode_AsUTF8(ptypename);
        Py_XDECREF(ptypename);

        // Add the error message (__repr__ of the error), if the error object exists
        if (pvalue != NULL) {
            PyObject *pvalstr = PyObject_Str(pvalue);
            what += ": ";
            what += PyUnicode_AsUTF8(pvalstr);
            Py_XDECREF(pvalstr);
        }

        // Give object references back to Python interpreter
        PyErr_Restore(ptype, pvalue, ptraceback);
        throw std::runtime_error(what);
    }
}

// Tell swig to wrap everything in CommsPlugin.h
%include "std_string.i"
%include "stdint.i"
%include "std_vector.i"
%include "std_unordered_map.i"

%ignore ::createPluginComms;
%ignore ::destroyPluginComms;
%ignore ::createEncoding;
%ignore ::destroyEncoding;
%ignore ::createTransport;
%ignore ::destroyTransport;
%ignore ::createUserModel;
%ignore ::destroyUserModel;

%template(RaceHandleVector) std::vector<unsigned long>;
%template(RoleVector) std::vector<ChannelRole>;
%template(StringVector) std::vector<std::string>;
%template(ByteVector) std::vector<uint8_t>;
%template(EncodingParamVector) std::vector<EncodingParameters>;
%template(ActionVector) std::vector<Action>;

%typemap(in) std::vector<std::string> * (std::vector<std::string> temp) {
    PyObject *iterator = PyObject_GetIter($input);
    PyObject *item;

    while ((item = PyIter_Next(iterator))) {
        const char * eType = PyUnicode_AsUTF8(item);
        temp.push_back(eType);
        Py_DECREF(item);
    }

    Py_DECREF(iterator);
    $1 = &temp;
}

%typemap(in) std::unordered_map<std::string, std::vector<EncodingType>> * (std::unordered_map<std::string, std::vector<EncodingType>> temp) {
    PyObject *keys = PyDict_Keys($input);
    PyObject *k, *v;
    Py_ssize_t pos = 0;

    while (PyDict_Next($input, &pos, &k, &v)) {
        if (!PyUnicode_Check(k)) {
            continue;
        }
        const char *key = PyUnicode_AsUTF8(k);
        int val_size = PyList_Size(v);
        std::vector<EncodingType> vec;
        for (int j = 0; j < val_size; j++) {
            const char * eType = PyUnicode_AsUTF8(PyList_GetItem(v, j));
            vec.push_back(eType);
        }
        temp[key] = vec;
    }
    $1 = &temp;
}

%include "race/common/constants.h"

%include "race/common/ChannelProperties.h"
%include "race/common/ChannelRole.h"
%include "race/common/ChannelStatus.h"
%include "race/common/ConnectionStatus.h"
%include "race/common/ConnectionType.h"
%include "race/common/EncPkg.h"
%include "race/common/LinkProperties.h"
%include "race/common/LinkPropertyPair.h"
%include "race/common/LinkPropertySet.h"
%include "race/common/LinkStatus.h"
%include "race/common/LinkType.h"
%include "race/common/PackageStatus.h"
%include "race/common/PackageType.h"
%include "race/common/PackageStatus.h"
%include "race/common/PluginConfig.h"
%include "race/common/PluginResponse.h"
%include "race/common/RaceEnums.h"
%include "race/common/RaceExport.h"
%include "race/common/RaceHandle.h"
%include "race/common/RaceLog.h"
%include "race/common/SendType.h"
%include "race/common/TransmissionType.h"

%include "race/decomposed/ComponentTypes.h"
%include "race/decomposed/IComponentBase.h"
%include "race/decomposed/IComponentSdkBase.h"
%include "race/decomposed/IEncodingComponent.h"
%include "race/decomposed/IEncodingSdk.h"
%include "race/decomposed/ITransportComponent.h"
%include "race/decomposed/ITransportSdk.h"
%include "race/decomposed/IUserModelComponent.h"
%include "race/decomposed/IUserModelSdk.h"

%include "race/unified/IRacePluginComms.h"
%include "race/unified/IRaceSdkCommon.h"
%include "race/unified/IRaceSdkComms.h"
%include "race/unified/SdkResponse.h"