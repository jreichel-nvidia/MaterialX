//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include <MaterialXGenMdl/Nodes/ImageNodeMdl.h>
#include <MaterialXGenMdl/MdlShaderGenerator.h>
#include <MaterialXGenMdl/Nodes/ConvolutionNode.h>
#include <MaterialXGenShader/ShaderGenerator.h>
#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/GenContext.h>

MATERIALX_NAMESPACE_BEGIN

const string ImageNodeMdl::FLIP_V = "flip_v";

ShaderNodeImplPtr ImageNodeMdl::create()
{
    return std::make_shared<ImageNodeMdl>();
}

void ImageNodeMdl::addInputs(ShaderNode& node, GenContext& context) const
{
    BASE::addInputs(node, context);
    node.addInput(ImageNodeMdl::FLIP_V, Type::BOOLEAN)->setUniform();
}

bool ImageNodeMdl::isEditable(const ShaderInput& input) const
{
    if (input.getName() == ImageNodeMdl::FLIP_V)
    {
        return false;
    }
    return BASE::isEditable(input);
}

void ImageNodeMdl::emitFunctionCall(const ShaderNode& _node, GenContext& context, ShaderStage& stage) const
{
    DEFINE_SHADER_STAGE(stage, Stage::PIXEL)
    {
        ShaderNode& node = const_cast<ShaderNode&>(_node);

        GenMdlOptionsPtr options = context.getUserData<GenMdlOptions>(GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY);
        bool derivatives = options && options->enable_derivatives;
        ShaderOutput* output = node.getOutput();
        string outputSuffix;
        context.getOutputSuffix(output, outputSuffix);
        if (!derivatives && outputSuffix.empty() && output && !output->getConnections().empty())
        {
            bool sampledOnly = true;
            for (const ShaderInput* connection : output->getConnections())
            {
                const ShaderNode* downstreamNode = connection->getNode();
                if (!downstreamNode || downstreamNode->isAGraph())
                {
                    sampledOnly = false;
                    break;
                }
                const ConvolutionNode* impl = dynamic_cast<const ConvolutionNode*>(&downstreamNode->getImplementation());
                if (!impl || !impl->isInputSampled(*connection))
                {
                    sampledOnly = false;
                    break;
                }
            }
            if (sampledOnly)
            {
                return;
            }
        }

        ShaderInput* flipUInput = node.getInput(ImageNodeMdl::FLIP_V);
        ValuePtr value = TypedValue<bool>::createValue(context.getOptions().fileTextureVerticalFlip);
        if (flipUInput)
        {
            flipUInput->setValue(value);
        }
        if (derivatives && _derivativeNodes.count(&node))
        {
            string source = _functionSource;
            string prefix = "materialx::stdlib_{{MDL_VERSION_SUFFIX}}::mx_image_";
            size_t pos = source.find(prefix);
            // TODO replace 1_13 with {{MDL_VERSION_SUFFIX}}
            if (pos != string::npos)
                source.replace(pos, prefix.size(), "materialx::stdlib_1_13::mx_image_with_derivatives_");
            BASE::emitFunctionCall(_node, context, stage, source);
            return;
        }
        BASE::emitFunctionCall(_node, context, stage);
    }
}

MATERIALX_NAMESPACE_END
