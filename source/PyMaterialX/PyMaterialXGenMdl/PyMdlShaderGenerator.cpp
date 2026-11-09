//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include <PyMaterialX/PyMaterialX.h>

#include <MaterialXGenShader/ShaderGenerator.h>
#include <MaterialXGenMdl/MdlShaderGenerator.h>

namespace py = pybind11;
namespace mx = MaterialX;

namespace
{
    // Creator wrapper to avoid having to expose the TypeSystem class in python
    mx::ShaderGeneratorPtr MdlShaderGenerator_create()
    {
        return mx::MdlShaderGenerator::create();
    }
}

void bindPyMdlShaderGenerator(py::module& mod)
{
    py::class_<mx::GenMdlOptions, mx::GenUserData, mx::GenMdlOptionsPtr> options(mod, "GenMdlOptions");
    py::enum_<mx::GenMdlOptions::MdlVersion>(options, "MdlVersion")
        .value("MDL_1_6", mx::GenMdlOptions::MdlVersion::MDL_1_6)
        .value("MDL_1_7", mx::GenMdlOptions::MdlVersion::MDL_1_7)
        .value("MDL_1_8", mx::GenMdlOptions::MdlVersion::MDL_1_8)
        .value("MDL_1_9", mx::GenMdlOptions::MdlVersion::MDL_1_9)
        .value("MDL_1_10", mx::GenMdlOptions::MdlVersion::MDL_1_10)
        .value("MDL_1_11", mx::GenMdlOptions::MdlVersion::MDL_1_11)
        .value("MDL_1_12", mx::GenMdlOptions::MdlVersion::MDL_1_12)
        .value("MDL_LATEST", mx::GenMdlOptions::MdlVersion::MDL_LATEST);
    options.def(py::init<>())
        .def_readwrite("targetVersion", &mx::GenMdlOptions::targetVersion)
        .def_readwrite("enable_derivatives", &mx::GenMdlOptions::enable_derivatives)
        .def_readonly_static("GEN_CONTEXT_USER_DATA_KEY", &mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY);

    py::class_<mx::MdlShaderGenerator, mx::ShaderGenerator, mx::MdlShaderGeneratorPtr>(mod, "MdlShaderGenerator")
        .def_static("create", &MdlShaderGenerator_create)
        .def("getTarget", &mx::MdlShaderGenerator::getTarget);
}
