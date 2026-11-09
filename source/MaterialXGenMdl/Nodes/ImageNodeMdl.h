//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#ifndef MATERIALX_IMAGENODEMDL_H
#define MATERIALX_IMAGENODEMDL_H

#include <MaterialXGenMdl/Export.h>

#include "SourceCodeNodeMdl.h"

MATERIALX_NAMESPACE_BEGIN

/// Image node implementation for MDL
class MX_GENMDL_API ImageNodeMdl : public SourceCodeNodeMdl
{
    using BASE = SourceCodeNodeMdl;

  public:
    static const string FLIP_V; ///< name of the additional parameter "flip_v"

    static ShaderNodeImplPtr create();

    void addInputs(ShaderNode& node, GenContext& context) const override;

    bool isEditable(const ShaderInput& input) const override;

    void emitFunctionCall(const ShaderNode& node, GenContext& context, ShaderStage& stage) const override;

    /// Mark an image node to use derivative-aware lookup when derivative generation is enabled.
    ///
    /// Called during upstream traversal from heighttonormal inputs, before emitting shader code.
    /// The mark applies to all uses of this node, including uses through shared graph instances.
    void requireDerivatives(const ShaderNode* node) const { _derivativeNodes.insert(node); }

private:
    // Nodes marked for derivative-aware lookup, tracked individually because an implementation
    // can be shared by multiple image nodes.
    //
    // Shared graph instances accumulate marks on the same implementation nodes. MdlShaderGenerator
    // clears the implementation cache before each generation, so marks from a previous shader are
    // not reused.
    mutable std::set<const ShaderNode*> _derivativeNodes;
};

MATERIALX_NAMESPACE_END

#endif
