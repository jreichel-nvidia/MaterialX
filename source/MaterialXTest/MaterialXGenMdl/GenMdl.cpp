//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include <MaterialXTest/External/Catch/catch.hpp>
#include <MaterialXTest/MaterialXGenMdl/GenMdl.h>

#include <MaterialXCore/Document.h>

#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXFormat/XmlIo.h>

#include <MaterialXGenMdl/MdlShaderGenerator.h>
#include <MaterialXGenMdl/MdlSyntax.h>

#include <MaterialXGenShader/DefaultColorManagementSystem.h>
#include <MaterialXGenShader/Exception.h>
#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/Util.h>


namespace mx = MaterialX;


MdlStringResolverPtr MdlStringResolver::create()
{
    return MdlStringResolverPtr(new MdlStringResolver());
}

void MdlStringResolver::initialize(
    mx::DocumentPtr document,
    std::ostream* logFile,
    std::initializer_list<mx::FilePath> additionalSearchpaths)
{
    mx::FileSearchPath searchPath = mx::getDefaultDataSearchPath();
    mx::FilePath rootPath = searchPath.isEmpty() ? mx::FilePath() : searchPath[0];
    mx::FilePath coreModulePath = rootPath / std::string(MATERIALX_INSTALL_MDL_MODULE_PATH) / "mdl";
    mx::FilePath coreModulePath2 = coreModulePath / mx::FilePath("materialx");

    // use the source search paths as base
    mx::FileSearchPath paths = mx::getSourceSearchPath(document);
    paths.append(mx::FilePath(document->getSourceUri()).getParentPath());

    // paths specified by the build system
    paths.append(mx::FilePath(MATERIALX_MDL_IMPL_MODULE_PATH));
    mx::StringVec extraModulePaths = mx::splitString(MATERIALX_MDL_MODULE_PATHS, ",");
    for (const std::string& extraPath : extraModulePaths)
    {
        paths.append(mx::FilePath(extraPath));
    }

    // add additional search paths for the tests
    paths.append(rootPath);
    paths.append(coreModulePath);
    paths.append(coreModulePath2);
    for (const auto& addSp : additionalSearchpaths)
    {
        paths.append(addSp);
    }

    // use a set to remove duplicates while keeping the order
    auto less = [](const mx::FilePath& lhs, const mx::FilePath& rhs)
        { return lhs.asString() < rhs.asString(); };
    std::set<mx::FilePath, decltype(less)> unique_paths(less);

    _mdl_searchPaths.clear();
    for (const auto& path : paths)
    {
        // normalize all search paths, as we need this later in `resolve`
        auto normalizedPath = path.getNormalized();
        if (normalizedPath.exists() && unique_paths.insert(normalizedPath).second)
        {
            _mdl_searchPaths.append(normalizedPath);
        }
    }

    _logFile = logFile;
}

std::string MdlStringResolver::resolve(const std::string& str, const std::string&) const
{
    mx::FilePath normalizedPath = mx::FilePath(str).getNormalized();

    // in case the path is absolute we need to find a proper search path to put the file in
    if (normalizedPath.isAbsolute())
    {
        // find the highest priority search path that is a prefix of the resource path
        for (const auto& sp : _mdl_searchPaths)
        {
            if (sp.size() > normalizedPath.size())
                continue;

            bool isParent = true;
            for (size_t i = 0; i < sp.size(); ++i)
            {
                if (sp[i] != normalizedPath[i])
                {
                    isParent = false;
                    break;
                }
            }

            if (!isParent)
                continue;

            // found a search path that is a prefix of the resource
            std::string resource_path = normalizedPath.asString().substr(sp.asString().size());
            if (resource_path[0] != '/')
                resource_path = "/" + resource_path;
            return resource_path;
        }
    }
    if (_logFile)
    {
        *_logFile << "Failed to resolve a MaterialX resource via the given search paths. "
                  << "Dropping the resource from the Material. Resource Path: "
                  << normalizedPath.asString().c_str() << std::endl;
    }
    // drop the resource by returning the empty string.
    // alternatively, the resource could be copied into an MDL search path,
    // maybe even only temporary.
    return "";
}


TEST_CASE("GenShader: MDL Syntax", "[genmdl]")
{
    mx::TypeSystemPtr ts = mx::TypeSystem::create();
    mx::SyntaxPtr syntax = mx::MdlSyntax::create(ts);

    REQUIRE(syntax->getTypeName(mx::Type::FLOAT) == "float");
    REQUIRE(syntax->getTypeName(mx::Type::COLOR3) == "color");
    REQUIRE(syntax->getTypeName(mx::Type::VECTOR3) == "float3");
    REQUIRE(syntax->getTypeName(mx::Type::FLOATARRAY) == "float");
    REQUIRE(syntax->getTypeName(mx::Type::INTEGERARRAY) == "int");
    REQUIRE(mx::Type::FLOATARRAY.isArray());
    REQUIRE(mx::Type::INTEGERARRAY.isArray());

    REQUIRE(syntax->getTypeName(mx::Type::BSDF) == "material");
    REQUIRE(syntax->getOutputTypeName(mx::Type::BSDF) == "material");

    // Set fixed precision with one digit
    mx::ScopedFloatFormatting format(mx::Value::FloatFormatFixed, 1);

    std::string value;
    value = syntax->getDefaultValue(mx::Type::FLOAT);
    REQUIRE(value == "0.0");
    value = syntax->getDefaultValue(mx::Type::COLOR3);
    REQUIRE(value == "color(0.0)");
    value = syntax->getDefaultValue(mx::Type::COLOR3, true);
    REQUIRE(value == "color(0.0)");
    value = syntax->getDefaultValue(mx::Type::COLOR4);
    REQUIRE(value == "mk_color4(0.0)");
    value = syntax->getDefaultValue(mx::Type::COLOR4, true);
    REQUIRE(value == "mk_color4(0.0)");
    value = syntax->getDefaultValue(mx::Type::FLOATARRAY, true);
    REQUIRE(value.empty());
    value = syntax->getDefaultValue(mx::Type::INTEGERARRAY, true);
    REQUIRE(value.empty());

    mx::ValuePtr floatValue = mx::Value::createValue<float>(42.0f);
    value = syntax->getValue(mx::Type::FLOAT, *floatValue);
    REQUIRE(value == "42.0");
    value = syntax->getValue(mx::Type::FLOAT, *floatValue, true);
    REQUIRE(value == "42.0");

    mx::ValuePtr color3Value = mx::Value::createValue<mx::Color3>(mx::Color3(1.0f, 2.0f, 3.0f));
    value = syntax->getValue(mx::Type::COLOR3, *color3Value);
    REQUIRE(value == "color(1.0, 2.0, 3.0)");
    value = syntax->getValue(mx::Type::COLOR3, *color3Value, true);
    REQUIRE(value == "color(1.0, 2.0, 3.0)");

    mx::ValuePtr color4Value = mx::Value::createValue<mx::Color4>(mx::Color4(1.0f, 2.0f, 3.0f, 4.0f));
    value = syntax->getValue(mx::Type::COLOR4, *color4Value);
    REQUIRE(value == "mk_color4(1.0, 2.0, 3.0, 4.0)");
    value = syntax->getValue(mx::Type::COLOR4, *color4Value, true);
    REQUIRE(value == "mk_color4(1.0, 2.0, 3.0, 4.0)");

    std::vector<float> floatArray = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f };
    mx::ValuePtr floatArrayValue = mx::Value::createValue<std::vector<float>>(floatArray);
    value = syntax->getValue(mx::Type::FLOATARRAY, *floatArrayValue);
    REQUIRE(value == "float[](0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7)");

    std::vector<int> intArray = { 1, 2, 3, 4, 5, 6, 7 };
    mx::ValuePtr intArrayValue = mx::Value::createValue<std::vector<int>>(intArray);
    value = syntax->getValue(mx::Type::INTEGERARRAY, *intArrayValue);
    REQUIRE(value == "int[](1, 2, 3, 4, 5, 6, 7)");

    for (const char* invalidName : { "", "1shader", "_shader", "shader name", "material" })
    {
        std::string validName = invalidName;
        syntax->makeValidName(validName);
        REQUIRE(!validName.empty());
        REQUIRE((validName[0] >= 'a' && validName[0] <= 'z'));
        REQUIRE(validName != invalidName);
    }

    mx::GenContext context(mx::MdlShaderGenerator::create());
    REQUIRE_THROWS_AS(context.getShaderGenerator().generate("1shader", nullptr, context),
                      mx::ExceptionShaderGenError);
}


// Check that the "enable_derivatives" option can only be set for MDL 1.13 or later.
TEST_CASE("GenShader: MDL Derivatives Option", "[genmdl]")
{
    mx::GenContext context(mx::MdlShaderGenerator::create());
    mx::FileSearchPath searchPath = mx::getDefaultDataSearchPath();
    context.registerSourceCodeSearchPath(searchPath);
    mx::DocumentPtr doc = mx::createDocument();
    mx::loadLibraries({ "libraries" }, searchPath, doc);
    mx::NodePtr node = doc->addNode("constant", "constant", "float");

    // Generation without MDL-specific options keeps derivatives disabled.
    REQUIRE_NOTHROW(context.getShaderGenerator().generate("test", node, context));

    mx::GenMdlOptionsPtr options = std::make_shared<mx::GenMdlOptions>();
    REQUIRE_FALSE(options->enable_derivatives);
    context.pushUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, options);

    for (mx::GenMdlOptions::MdlVersion version :
         { mx::GenMdlOptions::MdlVersion::MDL_1_6,
           mx::GenMdlOptions::MdlVersion::MDL_1_7,
           mx::GenMdlOptions::MdlVersion::MDL_1_8,
           mx::GenMdlOptions::MdlVersion::MDL_1_9,
           mx::GenMdlOptions::MdlVersion::MDL_1_10,
           mx::GenMdlOptions::MdlVersion::MDL_1_11,
           mx::GenMdlOptions::MdlVersion::MDL_1_12 })
    {
        options->targetVersion = version;
        options->enable_derivatives = false;
        REQUIRE_NOTHROW(context.getShaderGenerator().generate("test", node, context));

        options->enable_derivatives = true;
        // TODO update to MDL_1_13
        if (version < mx::GenMdlOptions::MdlVersion::MDL_1_12)
        {
            REQUIRE_THROWS_AS(context.getShaderGenerator().generate("test", node, context),
                              mx::ExceptionShaderGenError);
        }
        else
        {
            REQUIRE_NOTHROW(context.getShaderGenerator().generate("test", node, context));
        }
    }
}


// Check that the translation of an image node feeding into heighttonormal.in is translated
// according to the "enable_derivatives" option.
TEST_CASE("GenShader: MDL Derivatives Image Nodes", "[genmdl]")
{
    mx::DocumentPtr doc = mx::createDocument();
    mx::FileSearchPath paths = mx::getDefaultDataSearchPath();
    mx::loadLibraries({ "libraries" }, paths, doc);
    mx::GenContext context(mx::MdlShaderGenerator::create());
    context.registerSourceCodeSearchPath(paths);
    mx::GenMdlOptionsPtr options = std::make_shared<mx::GenMdlOptions>();
    context.pushUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, options);

    for (std::string type : { "float", "color3", "color4", "vector2", "vector3", "vector4" })
    {
        mx::NodePtr image = doc->addNode("image", "image_test", type);
        mx::NodePtr extract;
        if (type != "float")
        {
            extract = doc->addNode("extract", "extract_test", "float");
            extract->addInput("in", type)->setConnectedNode(image);
            extract->setInputValue("index", 0);
        }
        mx::NodePtr height = doc->addNode("heighttonormal", "height_test", "vector3");
        height->addInput("in", "float")->setConnectedNode(extract ? extract : image);

        for (bool enabled : { false, true, false })
        {
            options->enable_derivatives = enabled;
            mx::ShaderPtr shader = context.getShaderGenerator().generate("test", height, context);
            std::string code = shader->getSourceCode();
            // TODO change to 1.13
            CHECK((code.find("mdl 99.99;") != std::string::npos) == enabled);
            CHECK((code.find("mx_image_with_derivatives_" + type) != std::string::npos) == enabled);
            CHECK((code.find("mx_heighttonormal_with_derivatives(") != std::string::npos) == enabled);
        }

        doc->removeNode("height_test");
        if (extract)
            doc->removeNode("extract_test");
        doc->removeNode("image_test");
    }
}


// Check that a derivative requirement applies to the shared implementation of a compound node.
//
// Two instances use the same graph containing an image: one feeds heighttonormal.in, the other
// feeds an ordinary value consumer. The shared image must use derivative-aware lookup for both
// instances, with no separate ordinary lookup emitted.
TEST_CASE("GenShader: MDL Derivatives Compound Instances", "[genmdl]")
{
    mx::DocumentPtr doc = mx::createDocument();
    mx::FileSearchPath paths = mx::getDefaultDataSearchPath();
    mx::loadLibraries({ "libraries" }, paths, doc);
    mx::readFromXmlString(doc, R"(<materialx version="1.39">
      <nodedef name="ND_test_texture" node="test_texture"><output name="out" type="float"/></nodedef>
      <nodegraph name="NG_test_texture" nodedef="ND_test_texture">
        <image name="tex" type="float"/>
        <output name="out" type="float" nodename="tex"/>
      </nodegraph>
      <test_texture name="height_source" type="float"/>
      <test_texture name="ordinary_source" type="float"/>
      <heighttonormal name="normal" type="vector3"><input name="in" type="float" nodename="height_source"/></heighttonormal>
      <convert name="ordinary_vector" type="vector3"><input name="in" type="float" nodename="ordinary_source"/></convert>
      <add name="result" type="vector3">
        <input name="in1" type="vector3" nodename="normal"/>
        <input name="in2" type="vector3" nodename="ordinary_vector"/>
      </add>
    </materialx>)");
    mx::GenContext context(mx::MdlShaderGenerator::create());
    context.registerSourceCodeSearchPath(paths);
    mx::GenMdlOptionsPtr options = std::make_shared<mx::GenMdlOptions>();
    options->enable_derivatives = true;
    context.pushUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, options);

    mx::ShaderPtr shader = context.getShaderGenerator().generate("test", doc->getNode("result"), context);
    std::string code = shader->getSourceCode();
    CHECK(code.find("::mx_image_with_derivatives_float(") != std::string::npos);
    CHECK(code.find("::mx_image_float(") == std::string::npos);
}


// Check an image node shared directly by heighttonormal and an ordinary value consumer.
//
// With derivatives enabled, connecting the image to heighttonormal.in must emit exactly one
// derivative-aware lookup shared by both consumers. Connecting it only to heighttonormal.scale
// must not request derivatives. Toggle the option in the same context to check repeated generation.
TEST_CASE("GenShader: MDL Derivatives Shared Image Nodes", "[genmdl]")
{
    mx::DocumentPtr doc = mx::createDocument();
    mx::FileSearchPath paths = mx::getDefaultDataSearchPath();
    mx::loadLibraries({ "libraries" }, paths, doc);

    mx::NodePtr image = doc->addNode("image", "shared_image", "float");
    mx::NodePtr convert = doc->addNode("convert", "ordinary", "vector3");
    convert->addInput("in", "float")->setConnectedNode(image);
    mx::NodePtr height = doc->addNode("heighttonormal", "normal", "vector3");
    mx::NodePtr result = doc->addNode("add", "result", "vector3");
    result->addInput("in1", "vector3")->setConnectedNode(height);
    result->addInput("in2", "vector3")->setConnectedNode(convert);

    mx::GenContext context(mx::MdlShaderGenerator::create());
    context.registerSourceCodeSearchPath(paths);
    mx::GenMdlOptionsPtr options = std::make_shared<mx::GenMdlOptions>();
    context.pushUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, options);

    for (std::string input : { "in", "scale" })
    {
        height->addInput(input, "float")->setConnectedNode(image);
        for (bool enabled : { true, false, true })
        {
            options->enable_derivatives = enabled;
            mx::ShaderPtr shader = context.getShaderGenerator().generate("test", result, context);
            std::string code = shader->getSourceCode();
            std::string call = "::mx_image_with_derivatives_float(";
            size_t pos = code.find(call);
            bool expected = enabled && input == "in";
            CHECK((pos != std::string::npos) == expected);
            if (expected)
            {
                CHECK(code.find(call, pos + call.size()) == std::string::npos);
                CHECK(code.find("::mx_image_float(") == std::string::npos);
            }
        }
        height->removeInput(input);
    }
}


// Check that traversal resolves graph interface inputs through each calling instance.
//
// Two instances share a graph with two outputs, each depending on a different interface input.
// The callers connect four distinct images in total. Test heighttonormal both outside the graph
// (after summing its outputs) and inside it (on each output). All four images must use
// derivative-aware lookup: visiting an implementation through one instance must not prevent
// visiting it through the other instance and following that caller's connections.
TEST_CASE("GenShader: MDL Derivatives Traversal Interfaces", "[genmdl]")
{
    for (bool internalHeightToNormal : { false, true })
    {
        mx::DocumentPtr doc = mx::createDocument();
        mx::FileSearchPath paths = mx::getDefaultDataSearchPath();
        mx::loadLibraries({ "libraries" }, paths, doc);
        mx::readFromXmlString(doc, R"(<materialx version="1.39">
          <nodedef name="ND_test_pair" node="test_pair">
            <input name="a" type="float" value="0"/>
            <input name="b" type="float" value="0"/>
            <output name="outa" type="float"/>
            <output name="outb" type="float"/>
          </nodedef>
          <nodegraph name="NG_test_pair" nodedef="ND_test_pair">
            <sin name="sa" type="float"><input name="in" type="float" interfacename="a"/></sin>
            <cos name="cb" type="float"><input name="in" type="float" interfacename="b"/></cos>
            <output name="outa" type="float" nodename="sa"/>
            <output name="outb" type="float" nodename="cb"/>
          </nodegraph>
        </materialx>)");

        mx::NodeGraphPtr graph = doc->getNodeGraph("NG_test_pair");
        std::string outputType = internalHeightToNormal ? "vector3" : "float";

        // Add internal heighttonormal node.
        if (internalHeightToNormal)
        {
            for (std::string name : { "outa", "outb" })
            {
                mx::OutputPtr output = graph->getOutput(name);
                mx::NodePtr height = graph->addNode("heighttonormal", "normal_" + name, "vector3");
                height->addInput("in", "float")->setConnectedNode(output->getConnectedNode());
                output->setConnectedNode(height);
                output->setType(outputType);
                doc->getNodeDef("ND_test_pair")->getOutput(name)->setType(outputType);
            }
        }
        mx::NodePtr result = doc->addNode("add", "result", "vector3");

        for (int i = 0; i < 2; ++i)
        {
            // Add inputs.
            std::string suffix = std::to_string(i);
            mx::NodePtr pair = doc->addNode("test_pair", "pair" + suffix, "multioutput");
            for (std::string name : { "a", "b" })
            {
                mx::NodePtr image = doc->addNode("image", "image_" + name + suffix, "float");
                pair->addInput(name, "float")->setConnectedNode(image);
            }

            // Add sum node.
            mx::NodePtr sum = doc->addNode("add", "sum" + suffix, outputType);
            for (int j = 0; j < 2; ++j)
            {
                mx::InputPtr input = sum->addInput("in" + std::to_string(j + 1), outputType);
                input->setConnectedNode(pair);
                input->setOutputString(j == 0 ? "outa" : "outb");
            }

            // Add external heighttonormal node.
            mx::NodePtr normal = sum;
            if (!internalHeightToNormal)
            {
                normal = doc->addNode("heighttonormal", "normal" + suffix, "vector3");
                normal->addInput("in", "float")->setConnectedNode(sum);
            }
            result->addInput("in" + std::to_string(i + 1), "vector3")->setConnectedNode(normal);
        }

        mx::GenContext context(mx::MdlShaderGenerator::create());
        context.registerSourceCodeSearchPath(paths);
        mx::GenMdlOptionsPtr options = std::make_shared<mx::GenMdlOptions>();
        options->enable_derivatives = true;
        context.pushUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, options);

        // Check that there are four calls to mx_image_with_derivatives_float and none for
        // mx_image_float.
        mx::ShaderPtr shader = context.getShaderGenerator().generate("test", result, context);
        std::string code = shader->getSourceCode();
        std::string call = "::mx_image_with_derivatives_float(";
        size_t count = 0;
        for (size_t pos = code.find(call); pos != std::string::npos; pos = code.find(call, pos + call.size()))
            ++count;
        CHECK(count == 4);
        CHECK(code.find("::mx_image_float(") == std::string::npos);
    }
}


// Check output selection and interface resolution through two levels of shared graph instances.
//
// Each of two outer instances calls the same inner two-output graph and forwards its interface
// inputs. Four distinct images supply the outer callers. Only one output per caller feeds
// heighttonormal.in; the other feeds an ordinary consumer. Check each image's lookup separately:
// only the selected pair must use derivatives, even though all four images share an implementation.
// The inner instance is shared, so traversal must retain the full path to distinguish outer callers.
TEST_CASE("GenShader: MDL Derivatives Nested Output Selection", "[genmdl]")
{
    mx::DocumentPtr doc = mx::createDocument();
    mx::FileSearchPath paths = mx::getDefaultDataSearchPath();
    mx::loadLibraries({ "libraries" }, paths, doc);
    mx::readFromXmlString(doc, R"(<materialx version="1.39">
      <nodedef name="ND_pair" node="test_pair">
        <input name="a" type="float" value="0"/>
        <input name="b" type="float" value="0"/>
        <output name="outa" type="float"/>
        <output name="outb" type="float"/>
      </nodedef>
      <nodegraph name="NG_pair" nodedef="ND_pair">
        <sin name="a" type="float"><input name="in" type="float" interfacename="a"/></sin>
        <cos name="b" type="float"><input name="in" type="float" interfacename="b"/></cos>
        <output name="outa" type="float" nodename="a"/>
        <output name="outb" type="float" nodename="b"/>
      </nodegraph>
      <nodedef name="ND_outer" node="test_outer">
        <input name="a" type="float" value="0"/>
        <input name="b" type="float" value="0"/>
        <output name="outa" type="float"/>
        <output name="outb" type="float"/>
      </nodedef>
      <nodegraph name="NG_outer" nodedef="ND_outer">
        <test_pair name="inner" type="multioutput">
          <input name="a" type="float" interfacename="a"/>
          <input name="b" type="float" interfacename="b"/>
        </test_pair>
        <output name="outa" type="float" nodename="inner" output="outa"/>
        <output name="outb" type="float" nodename="inner" output="outb"/>
      </nodegraph>
    </materialx>)");

    mx::NodePtr result = doc->addNode("add", "result", "vector3");
    for (int i = 0; i < 2; ++i)
    {
        // Add outer instances and their inputs.
        std::string suffix = std::to_string(i);
        mx::NodePtr outer = doc->addNode("test_outer", "outer" + suffix, "multioutput");
        for (std::string name : { "a", "b" })
        {
            mx::NodePtr image = doc->addNode("image", "image_" + name + suffix, "float");
            outer->addInput(name, "float")->setConnectedNode(image);
        }

        // Connect heighttonormal consumer to outa.
        mx::NodePtr heightToNormal = doc->addNode("heighttonormal", "normal" + suffix, "vector3");
        mx::InputPtr heightInput = heightToNormal->addInput("in", "float");
        heightInput->setConnectedNode(outer);
        heightInput->setOutputString("outa");

        // Connect ordinary consumer to outb.
        mx::NodePtr ordinary = doc->addNode("convert", "ordinary" + suffix, "vector3");
        mx::InputPtr ordinaryInput = ordinary->addInput("in", "float");
        ordinaryInput->setConnectedNode(outer);
        ordinaryInput->setOutputString("outb");

        // Add both consumers and connect to one input of result.
        mx::NodePtr sum = doc->addNode("add", "sum" + suffix, "vector3");
        sum->addInput("in1", "vector3")->setConnectedNode(heightToNormal);
        sum->addInput("in2", "vector3")->setConnectedNode(ordinary);
        result->addInput("in" + std::to_string(i + 1), "vector3")->setConnectedNode(sum);
    }

    mx::GenContext context(mx::MdlShaderGenerator::create());
    context.registerSourceCodeSearchPath(paths);
    mx::GenMdlOptionsPtr options = std::make_shared<mx::GenMdlOptions>();
    options->enable_derivatives = true;
    context.pushUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, options);

    // Exercise both selections so always following the first graph output cannot pass.
    for (std::string selected : { "a", "b" })
    {
        CAPTURE(selected);

        // Connect heighttonormal consumer to selected output, ordinary consumer to other output.
        for (int i = 0; i < 2; ++i)
        {
            std::string suffix = std::to_string(i);
            doc->getNode("normal" + suffix)->getInput("in")->setOutputString("out" + selected);
            doc->getNode("ordinary" + suffix)->getInput("in")->setOutputString(
                selected == "a" ? "outb" : "outa");
        }

        mx::ShaderPtr shader = context.getShaderGenerator().generate("test", result, context);
        std::string code = shader->getSourceCode();
        for (int i = 0; i < 2; ++i)
        {
            for (std::string name : { "a", "b" })
            {
                std::string image = "image_" + name + std::to_string(i);
                CAPTURE(image);
                size_t pos = code.find(image + "_out = ");
                REQUIRE(pos != std::string::npos);
                std::string assignment = code.substr(pos, code.find('\n', pos) - pos);
                bool derivatives = name == selected;
                CHECK((assignment.find("::mx_image_with_derivatives_float(") != std::string::npos) == derivatives);
                CHECK((assignment.find("::mx_image_float(") != std::string::npos) == !derivatives);
            }
        }
    }
}


// Check that the graph traversal is not linear in the number of paths (but linear in the number of
// nodes and edges).
TEST_CASE("GenShader: MDL Derivatives Traversal Complexity", "[genmdl]")
{
    mx::DocumentPtr doc = mx::createDocument();
    mx::FileSearchPath paths = mx::getDefaultDataSearchPath();
    mx::loadLibraries({ "libraries" }, paths, doc);

    mx::NodePtr upstream = doc->addNode("image", "image", "float");
    for (int i = 0; i < 24; ++i)
    {
        mx::NodePtr node = doc->addNode("add", "branch" + std::to_string(i), "float");
        node->addInput("in1", "float")->setConnectedNode(upstream);
        node->addInput("in2", "float")->setConnectedNode(upstream);
        upstream = node;
    }
    mx::NodePtr height = doc->addNode("heighttonormal", "normal", "vector3");
    height->addInput("in", "float")->setConnectedNode(upstream);

    mx::GenContext context(mx::MdlShaderGenerator::create());
    context.registerSourceCodeSearchPath(paths);
    mx::GenMdlOptionsPtr options = std::make_shared<mx::GenMdlOptions>();
    options->enable_derivatives = true;
    context.pushUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, options);

    mx::ShaderPtr shader = context.getShaderGenerator().generate("test", height, context);
    std::string code = shader->getSourceCode();
    CHECK(code.find("::mx_image_with_derivatives_float(") != std::string::npos);
    CHECK(code.find("::mx_image_float(") == std::string::npos);
}


TEST_CASE("GenShader: MDL Implementation Check", "[genmdl]")
{
    mx::GenContext context(mx::MdlShaderGenerator::create());

    mx::StringSet generatorSkipNodeTypes;
    generatorSkipNodeTypes.insert("light");

    mx::StringSet generatorSkipNodeDefs;
    // add nodes to be skipped here
    // ex)
    // generatorSkipNodeDefs.insert("ND_hextiledimage_color3");

    GenShaderUtil::checkImplementations(context, generatorSkipNodeTypes, generatorSkipNodeDefs);
}


void MdlShaderGeneratorTester::preprocessDocument(mx::DocumentPtr doc)
{
    if (!_mdlCustomResolver)
        _mdlCustomResolver = MdlStringResolver::create();

    _mdlCustomResolver->initialize(doc, &_logFile, { _searchPath.asString() });
    mx::flattenFilenames(doc, _mdlCustomResolver->getMdlSearchPaths(), _mdlCustomResolver);
}

void MdlShaderGeneratorTester::compileSource(const std::vector<mx::FilePath>& sourceCodePaths)
{
    if (sourceCodePaths.empty() || sourceCodePaths[0].isEmpty())
        return;

    // only compile of the compiler binary is available
    std::string mdlcCommand(MATERIALX_MDL_BINARY_MDLC);
    if (mdlcCommand.empty())
    {
        return;
    }

    auto options = std::dynamic_pointer_cast<mx::GenMdlOptions>(_userData[mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY]);
    if (options && options->enable_derivatives)
        mdlcCommand += " --experimental-features";

    // use the same paths as the resolver
    for (const auto& sp : _mdlCustomResolver->getMdlSearchPaths())
    {
        mdlcCommand += " -p \"" + sp.asString() + "\"";
    }

    // additionally the generated module needs to found in a search path too
    mx::FilePath moduleToTestPath = sourceCodePaths[0].getParentPath();
    mdlcCommand += " -p \"" + moduleToTestPath.asString() + "\"";
    mdlcCommand += " -p \"" + moduleToTestPath.getParentPath().asString() + "\"";

    // avoid warning "C183: unused parameter '...'"
    mdlcCommand += " -W \"183=off\"";

    // avoid warning "C350: unused let temporary '...'"
    mdlcCommand += " -W \"350=off\"";

    // avoid warning "C181: unused variable '...'"
    mdlcCommand += " -W \"181=off\"";

    // but treat all other warnings as errors
    mdlcCommand += " -W err";

    // put the qualified module name to load as positional argument
    std::string moduleToTest = sourceCodePaths[0].getBaseName();
    moduleToTest = moduleToTest.substr(0, moduleToTest.size() - 4); // drop '.mdl'
    mdlcCommand += " ::" + moduleToTest;

    // redirect output
    mx::FilePath errorFile = moduleToTestPath / (moduleToTest + "_log.txt");
    mdlcCommand += " > " + errorFile.asString() + " 2>&1";

    // execute the compiler and evaluate return code and the output stream
    int returnValue = std::system(mdlcCommand.c_str());
    std::ifstream errorStream(errorFile);
    mx::StringVec result;
    std::string line;
    bool writeErrorCode = false;
    while (std::getline(errorStream, line))
    {
        if (!writeErrorCode)
        {
            _logFile << mdlcCommand << std::endl;
            _logFile << "\tReturn code: " << std::to_string(returnValue) << std::endl;
            writeErrorCode = true;
        }
        if (line.find(": Warning ") != std::string::npos)
        {
            _logFile << "\tWarning: " << line << std::endl;
        }
        else
        {
            _logFile << "\tError: " << line << std::endl;
        }
    }

    // return 0 indicates success, everything else a failure
    INFO("MDL compiler command: " << mdlcCommand);
    INFO("MDL compiler log: " << errorFile.asString());
    CHECK(returnValue == 0);
}

TEST_CASE("GenShader: MDL Shader Generation", "[genmdl]")
{
    mx::FileSearchPath searchPath = mx::getDefaultDataSearchPath();

    mx::FilePathVec testRootPaths;
    testRootPaths.push_back(searchPath.find("resources/Materials/TestSuite"));
    testRootPaths.push_back(searchPath.find("resources/Materials/Examples"));

    const mx::FilePath logPath("genmdl_mdl_generate_test.txt");

    // Write shaders and try to compile only if mdlc exe specified.
    std::string mdlcExec(MATERIALX_MDL_BINARY_MDLC);
    bool writeShadersToDisk = !mdlcExec.empty();
    MdlShaderGeneratorTester tester(mx::MdlShaderGenerator::create(), testRootPaths, searchPath, logPath, writeShadersToDisk);
    tester.addSkipLibraryFiles();

    mx::GenOptions genOptions;
    genOptions.targetColorSpaceOverride = "lin_rec709_scene";

    // No texture coordinate flips are required for MDL, since MDL and MaterialX
    // define texture space equally, with the origin at the lower left.

    mx::FilePath optionsFilePath = searchPath.find("resources/Materials/TestSuite/_options.mtlx");

    // Specify the MDL target version to be the latest which is also the default.
    mx::GenMdlOptionsPtr genMdlOptions = std::make_shared<mx::GenMdlOptions>();
    genMdlOptions->targetVersion = mx::GenMdlOptions::MdlVersion::MDL_LATEST;
    tester.addUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, genMdlOptions);

    tester.validate(genOptions, optionsFilePath);
}

// Compile a fixed regression subset in both modes, independent of external testing options.
TEST_CASE("GenShader: MDL Shader Generation Derivatives Subset", "[genmdl]")
{
    bool derivatives = false;
    SECTION("Disabled") { derivatives = false; }
    SECTION("Enabled") { derivatives = true; }
    CAPTURE(derivatives);

    mx::FileSearchPath searchPath = mx::getDefaultDataSearchPath();
    mx::FilePath root = searchPath.find("resources/Materials/TestSuite");
    GenShaderUtil::TestSuiteOptions options {};
    options.targets = { "genmdl" };
    options.checkImplCount = false;
    // Keep artifacts with the resolved test resources, as for ordinary shader generation.
    options.outputDirectory = root / "genmdl_derivative_subset" /
        (derivatives ? "enabled" : "disabled");
    for (std::string file : {
             "stdlib/convolution/heighttonormal.mtlx",
             "stdlib/convolution/heighttonormal2.mtlx",
             "stdlib/color_management/color3_vec3_cm_test.mtlx",
             "stdlib/texture/image_channels.mtlx",
             "pbrlib/multioutput/mixedoutput.mtlx",
             "pbrlib/multioutput/mixedoutput2.mtlx" })
    {
        INFO("Required corpus file: " << file);
        REQUIRE((root / file).exists());
        options.overrideFiles.push_back(mx::FilePath(file).getBaseName());
    }

    bool compile = !std::string(MATERIALX_MDL_BINARY_MDLC).empty();
    if (!compile)
        WARN("mdlc is not configured: running generation only; compilation is skipped.");
    MdlShaderGeneratorTester tester(mx::MdlShaderGenerator::create(), { root }, searchPath,
                                   "compile.log", compile);
    tester.addSkipLibraryFiles();
    mx::GenMdlOptionsPtr mdlOptions = std::make_shared<mx::GenMdlOptions>();
    mdlOptions->enable_derivatives = derivatives;
    tester.addUserData(mx::GenMdlOptions::GEN_CONTEXT_USER_DATA_KEY, mdlOptions);
    mx::GenOptions genOptions;
    genOptions.targetColorSpaceOverride = "lin_rec709_scene";
    tester.validate(genOptions, options);
}
