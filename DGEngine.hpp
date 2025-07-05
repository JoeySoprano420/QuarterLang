#pragma once
#include "QuarterLang_Compiler_Pipeline.hpp"

class DGEngine {
public:
    std::string toDG(int decimal);
    int fromDG(const std::string& dg);
    std::string addDG(const std::string& a, const std::string& b);
};
