#pragma once
#include "QuarterLang_Compiler_Pipeline.hpp"

class QTRCreativeCanvas {
public:
    void runSketch(const std::string& qtrSketchCode);
    void setOutputStream(std::ostream& os);
private:
    std::ostream* output = &std::cout;
};
