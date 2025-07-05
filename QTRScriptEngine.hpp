#pragma once
#include "QuarterLang_Compiler_Pipeline.hpp"

class QTRScriptEngine {
public:
    void loadScript(const std::string& qtrSource);
    void execute();
    void bindFunction(const std::string& name, std::function<void()> fn);
private:
    std::string compiledAssembly;
    std::unordered_map<std::string, std::function<void()>> nativeBindings;
};
