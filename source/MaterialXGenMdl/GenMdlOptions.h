//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#ifndef MATERIALX_GENMDLOPTIONS_H
#define MATERIALX_GENMDLOPTIONS_H

/// @file
/// MDL shader generation options

#include <MaterialXGenMdl/Export.h>

#include <MaterialXGenShader/GenUserData.h>

MATERIALX_NAMESPACE_BEGIN

/// Generator context data class for MDL options.
class MX_GENMDL_API GenMdlOptions : public GenUserData
{
  public:
    /// MDL Versions supported by the Code Generator
    enum class MdlVersion
    {
        MDL_1_6,
        MDL_1_7,
        MDL_1_8,
        MDL_1_9,
        MDL_1_10,
        MDL_1_11,
        MDL_1_12,
        MDL_LATEST = MDL_1_12
    };

    /// Create MDL code generator options with default values.
    GenMdlOptions() :
        targetVersion(MdlVersion::MDL_LATEST),
        enable_derivatives(false) { }

    /// Unique identifier for the MDL options on the GenContext object.
    static const string GEN_CONTEXT_USER_DATA_KEY;

    /// The MDL version number the generated module will have.
    /// Allows to generate MDL for older applications by limiting support according
    /// to the corresponding specification. By default this option is MDL_LATEST.
    MdlVersion targetVersion;

    /// Enable derivative support.
    ///
    /// Disabled by default and requires MDL 1.13.
    ///
    /// When enabled, image nodes upstream of a heighttonormal "in" input are mapped to
    /// mx_image_with_derivatives instead of mx_image.
    bool enable_derivatives;
};

/// Shared pointer to GenMdlOptions
using GenMdlOptionsPtr = shared_ptr<class GenMdlOptions>;

MATERIALX_NAMESPACE_END

#endif
