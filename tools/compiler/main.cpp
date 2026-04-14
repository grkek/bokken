#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <vector>
#include <sstream>
#include "quickjs.h"

namespace fs = std::filesystem;

// Structure to pass around the base source directory to the loader
struct CompilerContext
{
    fs::path sourceDirectory;
};

static int StubLogModule(JSContext *ctx, JSModuleDef *m)
{
    JS_SetModuleExport(ctx, m, "default", JS_NewObject(ctx));
    return 0;
}

static int StubWindowModule(JSContext *ctx, JSModuleDef *m)
{
    JS_SetModuleExport(ctx, m, "default", JS_NewObject(ctx));
    return 0;
}

static int StubCanvasModule(JSContext *ctx, JSModuleDef *m)
{
    JSValue defaultExport = JS_NewObject(ctx);
    auto noop = [](JSContext *c, JSValueConst, int, JSValueConst *)
    { return JS_UNDEFINED; };

    // Methods on the default export
    JS_SetPropertyStr(ctx, defaultExport, "createElement", JS_NewCFunction(ctx, noop, "createElement", 3));
    JS_SetPropertyStr(ctx, defaultExport, "render", JS_NewCFunction(ctx, noop, "render", 1));
    JS_SetPropertyStr(ctx, defaultExport, "useState", JS_NewCFunction(ctx, noop, "useState", 1));
    JS_SetModuleExport(ctx, m, "default", defaultExport);

    // Named exports
    JS_SetModuleExport(ctx, m, "View", JS_NewString(ctx, "view"));
    JS_SetModuleExport(ctx, m, "Label", JS_NewString(ctx, "label"));

    return 0;
}

static JSModuleDef *StubModuleLoader(JSContext *ctx, const char *module_name, void *opaque)
{
    auto *configuration = static_cast<CompilerContext *>(opaque);

    // 1. Resolve Native Bokken Modules
    if (strcmp(module_name, "bokken/canvas") == 0)
    {
        JSModuleDef *m = JS_NewCModule(ctx, "bokken/canvas", StubCanvasModule);
        JS_AddModuleExport(ctx, m, "default");
        JS_AddModuleExport(ctx, m, "View");
        JS_AddModuleExport(ctx, m, "Label");
        return m;
    }

    if (strcmp(module_name, "bokken/log") == 0)
    {
        JSModuleDef *m = JS_NewCModule(ctx, "bokken/log", StubLogModule);
        JS_AddModuleExport(ctx, m, "default");
        return m;
    }

    if (strcmp(module_name, "bokken/window") == 0)
    {
        JSModuleDef *m = JS_NewCModule(ctx, "bokken/window", StubWindowModule);
        JS_AddModuleExport(ctx, m, "default");
        return m;
    }

    // 2. Resolve User Modules
    // We try to find the file relative to the sourceDirectory
    fs::path targetPath = configuration->sourceDirectory / module_name;

    // QuickJS often omits the extension in imports; try to find the .js file
    if (!fs::exists(targetPath) && fs::exists(targetPath.string() + ".js"))
    {
        targetPath += ".js";
    }

    std::ifstream file(targetPath);
    if (!file.is_open())
    {
        fprintf(stderr, "  [Loader] Could not find user module: %s\n", module_name);
        return nullptr;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    std::string code = buf.str();

    // Compile the module. We use the path as the "name" so recursion works.
    JSValue func = JS_Eval(ctx, code.c_str(), code.size(), module_name,
                           JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException(func))
        return nullptr;

    // DO NOT JS_FreeValue(func). We need the ModuleDef to stay alive for serialization.
    return (JSModuleDef *)JS_VALUE_GET_PTR(func);
}

int main(int argc, char *argv[])
{
    std::string sourceDirStr, outputDirStr;
    bool verbose = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc)
            sourceDirStr = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            outputDirStr = argv[++i];
        else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
            verbose = true;
    }

    if (sourceDirStr.empty() || outputDirStr.empty())
    {
        std::cerr << "Usage: " << argv[0] << " --source <dir> --output <dir>\n";
        return 1;
    }

    CompilerContext config{fs::absolute(sourceDirStr)};

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JS_SetModuleLoaderFunc(rt, nullptr, StubModuleLoader, &config);

    int compiledCount = 0, errorCount = 0;

    for (const auto &entry : fs::recursive_directory_iterator(config.sourceDirectory))
    {
        if (entry.path().extension() != ".js")
            continue;

        // Path logic
        std::string relPath = fs::relative(entry.path(), config.sourceDirectory).string();
        fs::path outPath = fs::path(outputDirStr) / relPath;
        outPath.replace_extension(".script");
        fs::create_directories(outPath.parent_path());

        std::ifstream ifs(entry.path());
        std::string source((std::istreambuf_iterator<char>(ifs)), {});

        if (verbose)
            std::cout << "Compiling: " << relPath << "\n";

        // Use relPath as the ID so bytecode references are clean
        JSValue compiled = JS_Eval(ctx, source.c_str(), source.size(),
                                   relPath.c_str(),
                                   JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

        if (JS_IsException(compiled))
        {
            JSValue ex = JS_GetException(ctx);
            const char *s = JS_ToCString(ctx, ex);
            std::cerr << "  Error in " << relPath << ": " << (s ? s : "unknown") << "\n";
            JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, ex);
            errorCount++;
            continue;
        }

        // Write Bytecode
        size_t bcLen = 0;
        uint8_t *bcData = JS_WriteObject(ctx, &bcLen, compiled, JS_WRITE_OBJ_BYTECODE);
        if (bcData)
        {
            std::ofstream ofs(outPath, std::ios::binary);
            ofs.write(reinterpret_cast<const char *>(bcData), bcLen);
            js_free(ctx, bcData);
            std::cout << "✓ " << relPath << " (" << bcLen << " bytes)\n";
            compiledCount++;
        }
        JS_FreeValue(ctx, compiled);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    std::cout << "\nCompilation complete: " << compiledCount << " succeeded, " << errorCount << " failed\n";
    return (errorCount > 0) ? 1 : 0;
}