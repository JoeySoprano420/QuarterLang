/*
 * QuarterLang Compiler Pipeline
 * ------------------------------
 * Professional C++ implementation mapping:
 *   QuarterLang source -> AST -> CFG(IR) -> x86-64 machine code
 *
 * Stages:
 *   1) AST Definitions
 *   2) Parser
 *   3) IR (CFG) Definitions
 *   4) AST→IR Lowering
 *   5) x86-64 CodeGen
 *   6) Mapping Table
 *   7) Utilities
 *   8) Driver
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include "StandardLib.h"
#include <iostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <unordered_map>
#include <stdexcept>
#include <cassert>
#include <cctype>
#include <iostream>
#include "QTRScriptEngine.hpp"
#include "QTRCreativeCanvas.hpp"
#include "DGEngine.hpp"
#pragma once
#include "QuarterLang_Compiler_Pipeline.hpp"
#pragma once
#include "QuarterLang_Compiler_Pipeline.hpp"
#pragma once
#include "QuarterLang_Compiler_Pipeline.hpp"

//==========================================================
// 1. AST DEFINITIONS
//==========================================================
struct ASTExpr { virtual ~ASTExpr() = default; };

struct ASTLiteral : ASTExpr {
    std::string value;
    ASTLiteral(const std::string& v) : value(v) {}
};

struct ASTBinary : ASTExpr {
    std::string lhs, op, rhs;
    ASTBinary(const std::string& l, const std::string& o, const std::string& r)
        : lhs(l), op(o), rhs(r) {}
};

struct ASTValDecl : ASTExpr {
    std::string name, type;
    std::unique_ptr<ASTExpr> init;
};

struct ASTLoop : ASTExpr {
    std::unique_ptr<ASTExpr> start, end;
    std::vector<std::unique_ptr<ASTExpr>> body;
};

struct ASTProgram : ASTExpr {
    std::vector<std::unique_ptr<ASTExpr>> statements;
};

//==========================================================
// 2. PARSER (Lexer + Recursive-Descent Parsing)
//==========================================================
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in) {
    std::string lhs, op, rhs;
    in >> lhs;
    if (!(in >> op)) return std::make_unique<ASTLiteral>(lhs);
    if (!(in >> rhs)) return std::make_unique<ASTLiteral>(lhs);
    return std::make_unique<ASTBinary>(lhs, op, rhs);
}

std::unique_ptr<ASTProgram> parse(const std::string& src) {
    auto program = std::make_unique<ASTProgram>();
    std::istringstream in(src);
    std::string token;

    while (in >> token) {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); // skip ':'
            in >> decl->type; in.ignore(1); // skip '='
            decl->init = parseExpr(in);
            program->statements.push_back(std::move(decl));
        } else if (token == "loop") {
            auto loop = std::make_unique<ASTLoop>();
            loop->start = parseExpr(in);
            std::string to;
            in >> to; // expect "to"
            loop->end = parseExpr(in);
            std::string brace;
            in >> brace; // expect '{'
            while (in >> token && token != "}") {
                if (token == "val") {
                    auto decl = std::make_unique<ASTValDecl>();
                    in >> decl->name; in.ignore(1);
                    in >> decl->type; in.ignore(1);
                    decl->init = parseExpr(in);
                    loop->body.push_back(std::move(decl));
                }
            }
            program->statements.push_back(std::move(loop));
        }
    }
    return program;
}

//==========================================================
// 3. IR (CFG) DEFINITIONS
//==========================================================
enum class IROp { Alloc, Store, Load, Add, Sub, Mul, Div,
                  Jump, CondJump, Call, Ret, DgAdd, DgToDec, DecToDg };

struct IRInstr {
    IROp op;
    std::vector<std::string> args;
};

struct BasicBlock {
    std::string name;
    std::vector<IRInstr> instrs;
    BasicBlock* trueBranch = nullptr;
    BasicBlock* falseBranch = nullptr;
};

struct IRFunction {
    std::string name;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
};

//==========================================================
// 4. AST -> IR LOWERING
//==========================================================
IRFunction lower(ASTProgram& program) {
    IRFunction fn{"_main"};
    auto entry = std::make_unique<BasicBlock>();
    entry->name = "entry";

    for (auto& stmt : program.statements) {
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            if (val->type != "int") {
                std::cerr << "Type error: Only 'int' is supported.\n";
                exit(1);
            }
            entry->instrs.push_back({IROp::Alloc, {val->name}});
            if (auto bin = dynamic_cast<ASTBinary*>(val->init.get())) {
                entry->instrs.push_back({IROp::Store, {val->name, bin->lhs}});
                IROp op = (bin->op == "+") ? IROp::Add : (bin->op == "-") ? IROp::Sub : IROp::Add;
                entry->instrs.push_back({op, {val->name, bin->rhs}});
            } else {
                entry->instrs.push_back({IROp::Store, {val->name, static_cast<ASTLiteral*>(val->init.get())->value}});
            }
        } else if (auto loop = dynamic_cast<ASTLoop*>(stmt.get())) {
            entry->instrs.push_back({IROp::Alloc, {"i"}});
            entry->instrs.push_back({IROp::Store, {"i", static_cast<ASTLiteral*>(loop->start.get())->value}});

            auto loopCond = std::make_unique<BasicBlock>(); loopCond->name = "loop_cond";
            auto loopBody = std::make_unique<BasicBlock>(); loopBody->name = "loop_body";
            auto loopEnd  = std::make_unique<BasicBlock>(); loopEnd->name  = "loop_end";

            entry->instrs.push_back({IROp::Jump, {loopCond->name}});
            loopCond->instrs.push_back({IROp::CondJump, {"i", static_cast<ASTLiteral*>(loop->end.get())->value, loopEnd->name}});
            loopCond->instrs.push_back({IROp::Jump, {loopBody->name}});

            for (auto& inner : loop->body) {
                if (auto innerVal = dynamic_cast<ASTValDecl*>(inner.get())) {
                    loopBody->instrs.push_back({IROp::Alloc, {innerVal->name}});
                    loopBody->instrs.push_back({IROp::Store, {innerVal->name, static_cast<ASTLiteral*>(innerVal->init.get())->value}});
                }
            }
            loopBody->instrs.push_back({IROp::Add, {"i", "1"}});
            loopBody->instrs.push_back({IROp::Jump, {loopCond->name}});

            fn.blocks.push_back(std::move(loopCond));
            fn.blocks.push_back(std::move(loopBody));
            fn.blocks.push_back(std::move(loopEnd));
        }
    }
    fn.blocks.push_back(std::move(entry));
    return fn;
}

//==========================================================
// 5. X86-64 CODE GENERATOR
//==========================================================
class X86_64CodeGen {
public:
    void generate(IRFunction& fn) {
        emitPrologue();
        for (auto& block : fn.blocks) {
            emitLabel(block->name);
            for (auto& instr : block->instrs)
                emitInstr(instr);
        }
        emitEpilogue();
    }
private:
    void emitPrologue() {
        std::cout << "push rbp\nmov rbp, rsp\n";
    }
    void emitEpilogue() {
        std::cout << "mov rsp, rbp\npop rbp\nret\n";
    }
    void emitLabel(const std::string& name) {
        std::cout << name << ":\n";
    }
    void emitInstr(const IRInstr& instr) {
        if (instr.op == IROp::Add)
            std::cout << "  add " << instr.args[0] << ", " << instr.args[1] << "\n";
        else if (instr.op == IROp::Sub)
            std::cout << "  sub " << instr.args[0] << ", " << instr.args[1] << "\n";
        else if (instr.op == IROp::Mul)
            std::cout << "  imul " << instr.args[0] << ", " << instr.args[1] << "\n";
        else if (instr.op == IROp::Div)
            std::cout << "  xor rdx, rdx\n  mov rax, " << instr.args[0] << "\n  div " << instr.args[1] << "\n";
        else if (instr.op == IROp::Alloc)
            std::cout << "  ; alloc " << instr.args[0] << "\n";
        else if (instr.op == IROp::Store)
            std::cout << "  mov [" << instr.args[0] << "], " << instr.args[1] << "\n";
        else if (instr.op == IROp::Load)
            std::cout << "  mov rax, [" << instr.args[0] << "]\n";
        else if (instr.op == IROp::Jump)
            std::cout << "  jmp " << instr.args[0] << "\n";
        else if (instr.op == IROp::CondJump)
            std::cout << "  cmp " << instr.args[0] << ", " << instr.args[1] << "\n  jne " << instr.args[2] << "\n";
        else if (instr.op == IROp::Call)
            std::cout << "  call " << instr.args[0] << "\n";
        else if (instr.op == IROp::Ret)
            std::cout << "  ret\n";
        else if (instr.op == IROp::DgToDec)
            std::cout << "  call to_dg\n";
        else if (instr.op == IROp::DecToDg)
            std::cout << "  call from_dg\n";
        else if (instr.op == IROp::DgAdd)
            std::cout << "  call dg_add\n";
        else
            std::cout << "  ; unimplemented IR op\n";
    }
};

//==========================================================
// 6. MAPPING TABLE: QuarterLang <-> IR <-> x86
//==========================================================
static const struct MappingEntry {
    const char* quarter;
    const char* pseudocode;
    IROp    irOp;
    const char* x86;
} mappingTable[] = {
    {"val",    "ValDecl",   IROp::Alloc,    "alloc [rsp-idx]"},
    {"derive", "Derive",    IROp::Add,      "add reg, imm"},
    {"dg_add", "DgAdd",     IROp::DgAdd,    "call dg_add"},
    {"from_dg","DgToDec",   IROp::DgToDec,  "call from_dg"},
    {"to_dg",  "DecToDg",   IROp::DecToDg,  "call to_dg"},
    {"loop",   "Loop",      IROp::Jump,     "jmp label"},
    {"when",   "CondJump",  IROp::CondJump, "cmp/jl label"},
    {"say",    "CallPrint", IROp::Call,     "call print_func"},
    // ... more entries ...
};

//==========================================================
// 7. UTILITIES
//==========================================================
std::string loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open source file");
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

//==========================================================
// 8. DRIVER (main)
//==========================================================
int main(int argc, char** argv) {
    try {
        std::string source = loadFile(argc > 1 ? argv[1] : "program.qtr");
        auto ast = parse(source);
        IRFunction ir = lower(*ast);
        X86_64CodeGen cg;
        cg.generate(ir);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

/*
 * QuarterLang Compiler Pipeline — Functions + Standard Library
 * -------------------------------------------------------------
 * Stages:
 *   1) AST Definitions
 *   2) Parser (with func/call support)
 *   3) IR (CFG) Definitions
 *   4) AST→IR Lowering
 *   5) x86-64 CodeGen
 *   6) Mapping Table (Stdlib)
 *   7) Utilities
 *   8) Driver
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>

//==========================================================
// 1. AST DEFINITIONS (now with FunctionDef/Call)
//==========================================================
struct ASTExpr { virtual ~ASTExpr() = default; };

struct ASTLiteral : ASTExpr {
    std::string value;
    ASTLiteral(const std::string& v) : value(v) {}
};

struct ASTBinary : ASTExpr {
    std::string lhs, op, rhs;
    ASTBinary(const std::string& l, const std::string& o, const std::string& r)
        : lhs(l), op(o), rhs(r) {}
};

struct ASTValDecl : ASTExpr {
    std::string name, type;
    std::unique_ptr<ASTExpr> init;
};

struct ASTLoop : ASTExpr {
    std::unique_ptr<ASTExpr> start, end;
    std::vector<std::unique_ptr<ASTExpr>> body;
};

struct ASTCall : ASTExpr {
    std::string funcName;
    std::vector<std::unique_ptr<ASTExpr>> args;
    ASTCall(const std::string& name) : funcName(name) {}
};

struct ASTFunction : ASTExpr {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<ASTExpr>> body;
};

struct ASTProgram : ASTExpr {
    std::vector<std::unique_ptr<ASTExpr>> statements;
    std::unordered_map<std::string, ASTFunction*> functions;
};

//==========================================================
// 2. PARSER (Lexer + Recursive-Descent Parsing)
//==========================================================
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in);

std::unique_ptr<ASTCall> parseCall(const std::string& funcName, std::istringstream& in) {
    auto call = std::make_unique<ASTCall>(funcName);
    std::string arg;
    char ch;
    in >> ch; // '('
    while (in.peek() != ')' && in >> std::ws) {
        call->args.push_back(parseExpr(in));
        if (in.peek() == ',') in.ignore(1);
    }
    in.ignore(1); // ')'
    return call;
}

std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in) {
    std::string token;
    in >> token;

    // Handle literal or variable
    if (isdigit(token[0]) || isalpha(token[0])) {
        if (in.peek() == '(') { // function call
            return parseCall(token, in);
        }
        std::string op, rhs;
        std::streampos oldpos = in.tellg();
        if (in >> op) {
            if (op == "+" || op == "-" || op == "*" || op == "/") {
                in >> rhs;
                return std::make_unique<ASTBinary>(token, op, rhs);
            } else {
                in.seekg(oldpos);
            }
        }
        return std::make_unique<ASTLiteral>(token);
    }
    return std::make_unique<ASTLiteral>(token);
}

std::unique_ptr<ASTFunction> parseFunction(std::istringstream& in) {
    auto func = std::make_unique<ASTFunction>();
    std::string name, param;
    in >> name;      // function name
    func->name = name;
    in >> param;     // '('
    while (in.peek() != ')' && in >> std::ws) {
        in >> param;
        if (param.back() == ',') param.pop_back();
        func->params.push_back(param);
    }
    in.ignore(1); // ')'
    std::string brace;
    in >> brace; // '{'
    std::string token;
    while (in >> token && token != "}") {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); // skip ':'
            in >> decl->type; in.ignore(1); // skip '='
            decl->init = parseExpr(in);
            func->body.push_back(std::move(decl));
        } else if (token == "loop") {
            auto loop = std::make_unique<ASTLoop>();
            loop->start = parseExpr(in);
            std::string to;
            in >> to; // expect "to"
            loop->end = parseExpr(in);
            std::string b;
            in >> b; // expect '{'
            while (in >> token && token != "}") {
                if (token == "val") {
                    auto decl = std::make_unique<ASTValDecl>();
                    in >> decl->name; in.ignore(1);
                    in >> decl->type; in.ignore(1);
                    decl->init = parseExpr(in);
                    loop->body.push_back(std::move(decl));
                }
            }
            func->body.push_back(std::move(loop));
        } else if (token == "call") {
            std::string fname;
            in >> fname;
            func->body.push_back(parseCall(fname, in));
        }
    }
    return func;
}

std::unique_ptr<ASTProgram> parse(const std::string& src) {
    auto program = std::make_unique<ASTProgram>();
    std::istringstream in(src);
    std::string token;

    while (in >> token) {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); // skip ':'
            in >> decl->type; in.ignore(1); // skip '='
            decl->init = parseExpr(in);
            program->statements.push_back(std::move(decl));
        } else if (token == "loop") {
            auto loop = std::make_unique<ASTLoop>();
            loop->start = parseExpr(in);
            std::string to;
            in >> to; // expect "to"
            loop->end = parseExpr(in);
            std::string brace;
            in >> brace; // expect '{'
            while (in >> token && token != "}") {
                if (token == "val") {
                    auto decl = std::make_unique<ASTValDecl>();
                    in >> decl->name; in.ignore(1);
                    in >> decl->type; in.ignore(1);
                    decl->init = parseExpr(in);
                    loop->body.push_back(std::move(decl));
                }
            }
            program->statements.push_back(std::move(loop));
        } else if (token == "func") {
            auto func = parseFunction(in);
            program->functions[func->name] = func.get();
            program->statements.push_back(std::move(func));
        } else if (token == "call") {
            std::string fname;
            in >> fname;
            program->statements.push_back(parseCall(fname, in));
        }
    }
    return program;
}

//==========================================================
// 3. IR (CFG) DEFINITIONS (now with functions)
//==========================================================
enum class IROp { Alloc, Store, Load, Add, Sub, Mul, Div,
                  Jump, CondJump, Call, Ret, DgAdd, DgToDec, DecToDg };

struct IRInstr {
    IROp op;
    std::vector<std::string> args;
};

struct BasicBlock {
    std::string name;
    std::vector<IRInstr> instrs;
    BasicBlock* trueBranch = nullptr;
    BasicBlock* falseBranch = nullptr;
};

struct IRFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
};

//==========================================================
// 4. AST -> IR LOWERING
//==========================================================
struct IRProgram {
    std::unordered_map<std::string, IRFunction> functions;
};

void lowerFunction(ASTFunction* astFunc, IRProgram& irprog) {
    IRFunction irFunc;
    irFunc.name = astFunc->name;
    irFunc.params = astFunc->params;
    auto entry = std::make_unique<BasicBlock>();
    entry->name = irFunc.name + "_entry";
    for (auto& stmt : astFunc->body) {
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            if (val->type != "int") {
                std::cerr << "Type error: Only 'int' supported.\n";
                exit(1);
            }
            entry->instrs.push_back({IROp::Alloc, {val->name}});
            entry->instrs.push_back({IROp::Store, {val->name, static_cast<ASTLiteral*>(val->init.get())->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get()))
                    args.push_back(lit->value);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
        // Expand for loops/calls as desired...
    }
    irFunc.blocks.push_back(std::move(entry));
    irprog.functions[irFunc.name] = std::move(irFunc);
}

IRProgram lower(ASTProgram& program) {
    IRProgram irprog;
    for (auto& stmt : program.statements) {
        if (auto func = dynamic_cast<ASTFunction*>(stmt.get()))
            lowerFunction(func, irprog);
        // Lower global val/call/loops into main?
    }
    // Add main (for top-level statements)
    IRFunction mainFn;
    mainFn.name = "_main";
    auto entry = std::make_unique<BasicBlock>();
    entry->name = "entry";
    for (auto& stmt : program.statements) {
        if (dynamic_cast<ASTFunction*>(stmt.get())) continue;
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            if (val->type != "int") {
                std::cerr << "Type error: Only 'int' is supported.\n";
                exit(1);
            }
            entry->instrs.push_back({IROp::Alloc, {val->name}});
            entry->instrs.push_back({IROp::Store, {val->name, static_cast<ASTLiteral*>(val->init.get())->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get()))
                    args.push_back(lit->value);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    mainFn.blocks.push_back(std::move(entry));
    irprog.functions[mainFn.name] = std::move(mainFn);
    return irprog;
}

//==========================================================
// 5. X86-64 CODE GENERATOR (now with function codegen)
//==========================================================
class X86_64CodeGen {
public:
    void generate(const IRProgram& prog) {
        for (const auto& [name, fn] : prog.functions) {
            std::cout << name << ":\n";
            emitPrologue();
            for (const auto& block : fn.blocks)
                for (const auto& instr : block->instrs)
                    emitInstr(instr);
            emitEpilogue();
        }
    }
private:
    void emitPrologue() { std::cout << "push rbp\nmov rbp, rsp\n"; }
    void emitEpilogue() { std::cout << "mov rsp, rbp\npop rbp\nret\n"; }
    void emitInstr(const IRInstr& instr) {
        if (instr.op == IROp::Add)
            std::cout << "  add " << instr.args[0] << ", " << instr.args[1] << "\n";
        else if (instr.op == IROp::Sub)
            std::cout << "  sub " << instr.args[0] << ", " << instr.args[1] << "\n";
        else if (instr.op == IROp::Call)
            std::cout << "  call " << instr.args[0] << "\n";
        else if (instr.op == IROp::Alloc)
            std::cout << "  ; alloc " << instr.args[0] << "\n";
        else if (instr.op == IROp::Store)
            std::cout << "  mov [" << instr.args[0] << "], " << instr.args[1] << "\n";
        // ... rest as before ...
        else
            std::cout << "  ; unimplemented IR op\n";
    }
};

//==========================================================
// 6. MAPPING TABLE: QuarterLang <-> IR <-> x86 + STANDARD LIBRARY
//==========================================================
static const struct MappingEntry {
    const char* quarter;
    const char* pseudocode;
    IROp    irOp;
    const char* x86;
} mappingTable[] = {
    {"val",    "ValDecl",   IROp::Alloc,    "alloc [rsp-idx]"},
    {"derive", "Derive",    IROp::Add,      "add reg, imm"},
    {"func",   "FuncDef",   IROp::Call,     "call label"},
    {"call",   "Call",      IROp::Call,     "call label"},
    {"say",    "CallPrint", IROp::Call,     "call print_func"},
    {"max",    "CallMax",   IROp::Call,     "call max_func"},
    {"min",    "CallMin",   IROp::Call,     "call min_func"},
    {"input",  "CallInput", IROp::Call,     "call input_func"},
    // ... more stdlib as needed ...
};

//==========================================================
// 7. UTILITIES
//==========================================================
std::string loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open source file");
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

//==========================================================
// 8. DRIVER (main)
//==========================================================
int main(int argc, char** argv) {
    try {
        std::string source = loadFile(argc > 1 ? argv[1] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        X86_64CodeGen cg;
        cg.generate(irprog);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

/*
 * QuarterLang Compiler Pipeline - Full Features
 * ---------------------------------------------------
 * Features:
 *  🧠 Functions (def/call)
 *  📦 Standard Library (say, max, min, input)
 *  🧪 Unit Tests (IR/Lowering)
 *  🖥️ REPL (interactive)
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cassert>

//==========================================================
// 1. AST DEFINITIONS (with FunctionDef/Call)
//==========================================================
struct ASTExpr { virtual ~ASTExpr() = default; };

struct ASTLiteral : ASTExpr {
    std::string value;
    ASTLiteral(const std::string& v) : value(v) {}
};

struct ASTBinary : ASTExpr {
    std::string lhs, op, rhs;
    ASTBinary(const std::string& l, const std::string& o, const std::string& r)
        : lhs(l), op(o), rhs(r) {}
};

struct ASTValDecl : ASTExpr {
    std::string name, type;
    std::unique_ptr<ASTExpr> init;
};

struct ASTLoop : ASTExpr {
    std::unique_ptr<ASTExpr> start, end;
    std::vector<std::unique_ptr<ASTExpr>> body;
};

struct ASTCall : ASTExpr {
    std::string funcName;
    std::vector<std::unique_ptr<ASTExpr>> args;
    ASTCall(const std::string& name) : funcName(name) {}
};

struct ASTFunction : ASTExpr {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<ASTExpr>> body;
};

struct ASTProgram : ASTExpr {
    std::vector<std::unique_ptr<ASTExpr>> statements;
    std::unordered_map<std::string, ASTFunction*> functions;
};

//==========================================================
// 2. PARSER (Supports functions/calls)
//==========================================================
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in);

std::unique_ptr<ASTCall> parseCall(const std::string& funcName, std::istringstream& in) {
    auto call = std::make_unique<ASTCall>(funcName);
    char ch;
    in >> ch; // '('
    while (in.peek() != ')' && in >> std::ws) {
        call->args.push_back(parseExpr(in));
        if (in.peek() == ',') in.ignore(1);
    }
    in.ignore(1); // ')'
    return call;
}

std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in) {
    std::string token;
    in >> token;
    if (isdigit(token[0]) || isalpha(token[0])) {
        if (in.peek() == '(') { // function call
            return parseCall(token, in);
        }
        std::string op, rhs;
        std::streampos oldpos = in.tellg();
        if (in >> op) {
            if (op == "+" || op == "-" || op == "*" || op == "/") {
                in >> rhs;
                return std::make_unique<ASTBinary>(token, op, rhs);
            } else {
                in.seekg(oldpos);
            }
        }
        return std::make_unique<ASTLiteral>(token);
    }
    return std::make_unique<ASTLiteral>(token);
}

std::unique_ptr<ASTFunction> parseFunction(std::istringstream& in) {
    auto func = std::make_unique<ASTFunction>();
    std::string name, param;
    in >> name;
    func->name = name;
    in >> param; // '('
    while (in.peek() != ')' && in >> std::ws) {
        in >> param;
        if (param.back() == ',') param.pop_back();
        func->params.push_back(param);
    }
    in.ignore(1); // ')'
    std::string brace;
    in >> brace; // '{'
    std::string token;
    while (in >> token && token != "}") {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); // skip ':'
            in >> decl->type; in.ignore(1); // skip '='
            decl->init = parseExpr(in);
            func->body.push_back(std::move(decl));
        } else if (token == "loop") {
            auto loop = std::make_unique<ASTLoop>();
            loop->start = parseExpr(in);
            std::string to;
            in >> to;
            loop->end = parseExpr(in);
            std::string b;
            in >> b;
            while (in >> token && token != "}") {
                if (token == "val") {
                    auto decl = std::make_unique<ASTValDecl>();
                    in >> decl->name; in.ignore(1);
                    in >> decl->type; in.ignore(1);
                    decl->init = parseExpr(in);
                    loop->body.push_back(std::move(decl));
                }
            }
            func->body.push_back(std::move(loop));
        } else if (token == "call") {
            std::string fname;
            in >> fname;
            func->body.push_back(parseCall(fname, in));
        }
    }
    return func;
}

std::unique_ptr<ASTProgram> parse(const std::string& src) {
    auto program = std::make_unique<ASTProgram>();
    std::istringstream in(src);
    std::string token;

    while (in >> token) {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1);
            in >> decl->type; in.ignore(1);
            decl->init = parseExpr(in);
            program->statements.push_back(std::move(decl));
        } else if (token == "loop") {
            auto loop = std::make_unique<ASTLoop>();
            loop->start = parseExpr(in);
            std::string to;
            in >> to;
            loop->end = parseExpr(in);
            std::string brace;
            in >> brace;
            while (in >> token && token != "}") {
                if (token == "val") {
                    auto decl = std::make_unique<ASTValDecl>();
                    in >> decl->name; in.ignore(1);
                    in >> decl->type; in.ignore(1);
                    decl->init = parseExpr(in);
                    loop->body.push_back(std::move(decl));
                }
            }
            program->statements.push_back(std::move(loop));
        } else if (token == "func") {
            auto func = parseFunction(in);
            program->functions[func->name] = func.get();
            program->statements.push_back(std::move(func));
        } else if (token == "call") {
            std::string fname;
            in >> fname;
            program->statements.push_back(parseCall(fname, in));
        }
    }
    return program;
}

//==========================================================
// 3. IR (CFG) DEFINITIONS (with functions)
//==========================================================
enum class IROp { Alloc, Store, Load, Add, Sub, Mul, Div,
                  Jump, CondJump, Call, Ret, DgAdd, DgToDec, DecToDg };

struct IRInstr {
    IROp op;
    std::vector<std::string> args;
};

struct BasicBlock {
    std::string name;
    std::vector<IRInstr> instrs;
};

struct IRFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
};

struct IRProgram {
    std::unordered_map<std::string, IRFunction> functions;
};

//==========================================================
// 4. AST -> IR LOWERING
//==========================================================
void lowerFunction(ASTFunction* astFunc, IRProgram& irprog) {
    IRFunction irFunc;
    irFunc.name = astFunc->name;
    irFunc.params = astFunc->params;
    auto entry = std::make_unique<BasicBlock>();
    entry->name = irFunc.name + "_entry";
    for (auto& stmt : astFunc->body) {
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            entry->instrs.push_back({IROp::Alloc, {val->name}});
            entry->instrs.push_back({IROp::Store, {val->name, static_cast<ASTLiteral*>(val->init.get())->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get()))
                    args.push_back(lit->value);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    irFunc.blocks.push_back(std::move(entry));
    irprog.functions[irFunc.name] = std::move(irFunc);
}

IRProgram lower(ASTProgram& program) {
    IRProgram irprog;
    for (auto& stmt : program.statements) {
        if (auto func = dynamic_cast<ASTFunction*>(stmt.get()))
            lowerFunction(func, irprog);
    }
    IRFunction mainFn;
    mainFn.name = "_main";
    auto entry = std::make_unique<BasicBlock>();
    entry->name = "entry";
    for (auto& stmt : program.statements) {
        if (dynamic_cast<ASTFunction*>(stmt.get())) continue;
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            entry->instrs.push_back({IROp::Alloc, {val->name}});
            entry->instrs.push_back({IROp::Store, {val->name, static_cast<ASTLiteral*>(val->init.get())->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get()))
                    args.push_back(lit->value);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    mainFn.blocks.push_back(std::move(entry));
    irprog.functions[mainFn.name] = std::move(mainFn);
    return irprog;
}

//==========================================================
// 5. X86-64 CODE GENERATOR (with functions/stdlib)
//==========================================================
class X86_64CodeGen {
public:
    void generate(const IRProgram& prog) {
        for (const auto& [name, fn] : prog.functions) {
            std::cout << name << ":\n";
            emitPrologue();
            for (const auto& block : fn.blocks)
                for (const auto& instr : block->instrs)
                    emitInstr(instr);
            emitEpilogue();
        }
        emitStdlib();
    }
private:
    void emitPrologue() { std::cout << "push rbp\nmov rbp, rsp\n"; }
    void emitEpilogue() { std::cout << "mov rsp, rbp\npop rbp\nret\n"; }
    void emitInstr(const IRInstr& instr) {
        if (instr.op == IROp::Add)
            std::cout << "  add " << instr.args[0] << ", " << instr.args[1] << "\n";
        else if (instr.op == IROp::Call)
            std::cout << "  call " << instr.args[0] << "\n";
        else if (instr.op == IROp::Alloc)
            std::cout << "  ; alloc " << instr.args[0] << "\n";
        else if (instr.op == IROp::Store)
            std::cout << "  mov [" << instr.args[0] << "], " << instr.args[1] << "\n";
        else
            std::cout << "  ; unimplemented IR op\n";
    }
    void emitStdlib() {
        // "say" is a call to print
        std::cout << "say:\n";
        std::cout << "  ; stdlib print\n";
        std::cout << "  ret\n";
        std::cout << "max:\n";
        std::cout << "  ; stdlib max (mock)\n";
        std::cout << "  ret\n";
        std::cout << "min:\n";
        std::cout << "  ; stdlib min (mock)\n";
        std::cout << "  ret\n";
        std::cout << "input:\n";
        std::cout << "  ; stdlib input (mock)\n";
        std::cout << "  ret\n";
    }
};

//==========================================================
// 6. MAPPING TABLE (standard library symbols)
//==========================================================
static const struct MappingEntry {
    const char* quarter;
    const char* pseudocode;
    IROp    irOp;
    const char* x86;
} mappingTable[] = {
    {"val",    "ValDecl",   IROp::Alloc,    "alloc [rsp-idx]"},
    {"derive", "Derive",    IROp::Add,      "add reg, imm"},
    {"func",   "FuncDef",   IROp::Call,     "call label"},
    {"call",   "Call",      IROp::Call,     "call label"},
    {"say",    "CallPrint", IROp::Call,     "call say"},
    {"max",    "CallMax",   IROp::Call,     "call max"},
    {"min",    "CallMin",   IROp::Call,     "call min"},
    {"input",  "CallInput", IROp::Call,     "call input"},
    // ... add more stdlib symbols here ...
};

//==========================================================
// 7. UTILITIES
//==========================================================
std::string loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open source file");
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

//==========================================================
// 8. 🧪 UNIT TESTS
//==========================================================
void test_ir_lowering() {
    // Function test
    ASTProgram program;
    auto func = std::make_unique<ASTFunction>();
    func->name = "f";
    func->params = {"x"};
    auto decl = std::make_unique<ASTValDecl>();
    decl->name = "y"; decl->type = "int";
    decl->init = std::make_unique<ASTLiteral>("42");
    func->body.push_back(std::move(decl));
    program.statements.push_back(std::move(func));
    auto irprog = lower(program);
    assert(irprog.functions.count("f"));
    assert(irprog.functions["f"].blocks[0]->instrs[0].op == IROp::Alloc);
    // Call test
    ASTProgram program2;
    program2.statements.push_back(std::make_unique<ASTCall>("say"));
    auto irprog2 = lower(program2);
    assert(irprog2.functions["_main"].blocks[0]->instrs[0].op == IROp::Call);
    // Stdlib test
    ASTProgram program3;
    program3.statements.push_back(std::make_unique<ASTCall>("max"));
    auto irprog3 = lower(program3);
    assert(irprog3.functions["_main"].blocks[0]->instrs[0].args[0] == "max");
    std::cout << "All unit tests passed.\n";
}

//==========================================================
// 9. 🖥️ REPL
//==========================================================
void repl() {
    std::cout << "QuarterLang REPL. Type 'exit' to quit.\n";
    std::string line, src;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        if (line.empty()) continue;
        src = line;
        try {
            auto ast = parse(src);
            auto irprog = lower(*ast);
            X86_64CodeGen cg;
            cg.generate(irprog);
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }
}

//==========================================================
// 10. DRIVER (main)
//==========================================================
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--test") {
        test_ir_lowering();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--repl") {
        repl();
        return 0;
    }
    try {
        std::string source = loadFile(argc > 1 ? argv[1] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        X86_64CodeGen cg;
        cg.generate(irprog);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

/*
 * QuarterLang Compiler/Interpreter Pipeline
 * - Functions, Calls, Stack Frames, REPL & Debugger
 *
 * Key Features:
 *   - Function definitions and calls
 *   - Stack-based memory layout & offsets
 *   - Step debugger & REPL
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <map>
#include <stdexcept>
#include <cassert>

//-----------------------------------------------------
// AST DEFINITIONS
//-----------------------------------------------------
struct ASTExpr { virtual ~ASTExpr() = default; };
struct ASTLiteral : ASTExpr { std::string value; ASTLiteral(const std::string& v) : value(v) {} };
struct ASTVar : ASTExpr { std::string name; ASTVar(const std::string& n): name(n) {} };
struct ASTBinary : ASTExpr { std::string lhs, op, rhs; ASTBinary(const std::string& l, const std::string& o, const std::string& r) : lhs(l), op(o), rhs(r) {} };
struct ASTValDecl : ASTExpr { std::string name, type; std::unique_ptr<ASTExpr> init; };
struct ASTLoop : ASTExpr { std::unique_ptr<ASTExpr> start, end; std::vector<std::unique_ptr<ASTExpr>> body; };
struct ASTCall : ASTExpr { std::string funcName; std::vector<std::unique_ptr<ASTExpr>> args; ASTCall(const std::string& name): funcName(name) {} };
struct ASTFunction : ASTExpr { std::string name; std::vector<std::string> params; std::vector<std::unique_ptr<ASTExpr>> body; };
struct ASTProgram : ASTExpr {
    std::vector<std::unique_ptr<ASTExpr>> statements;
    std::unordered_map<std::string, ASTFunction*> functions;
};

//-----------------------------------------------------
// PARSER (mini recursive-descent, supports functions/calls)
//-----------------------------------------------------
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in);

std::unique_ptr<ASTCall> parseCall(const std::string& funcName, std::istringstream& in) {
    auto call = std::make_unique<ASTCall>(funcName);
    char ch;
    in >> ch; // '('
    while (in.peek() != ')' && in >> std::ws) {
        call->args.push_back(parseExpr(in));
        if (in.peek() == ',') in.ignore(1);
    }
    in.ignore(1); // ')'
    return call;
}
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in) {
    std::string token;
    in >> token;
    if (isdigit(token[0])) return std::make_unique<ASTLiteral>(token);
    if (isalpha(token[0])) {
        if (in.peek() == '(') { return parseCall(token, in); }
        std::string op, rhs;
        std::streampos oldpos = in.tellg();
        if (in >> op) {
            if (op == "+" || op == "-" || op == "*" || op == "/") {
                in >> rhs;
                return std::make_unique<ASTBinary>(token, op, rhs);
            } else { in.seekg(oldpos); }
        }
        return std::make_unique<ASTVar>(token);
    }
    return std::make_unique<ASTLiteral>(token);
}
std::unique_ptr<ASTFunction> parseFunction(std::istringstream& in) {
    auto func = std::make_unique<ASTFunction>();
    std::string name, param;
    in >> name; func->name = name;
    in >> param; // '('
    while (in.peek() != ')' && in >> std::ws) {
        in >> param; if (param.back() == ',') param.pop_back();
        func->params.push_back(param);
    }
    in.ignore(1); // ')'
    std::string brace; in >> brace; // '{'
    std::string token;
    while (in >> token && token != "}") {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            func->body.push_back(std::move(decl));
        } else if (token == "call") {
            std::string fname; in >> fname; func->body.push_back(parseCall(fname, in));
        }
    }
    return func;
}
std::unique_ptr<ASTProgram> parse(const std::string& src) {
    auto program = std::make_unique<ASTProgram>();
    std::istringstream in(src);
    std::string token;
    while (in >> token) {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            program->statements.push_back(std::move(decl));
        } else if (token == "func") {
            auto func = parseFunction(in);
            program->functions[func->name] = func.get();
            program->statements.push_back(std::move(func));
        } else if (token == "call") {
            std::string fname; in >> fname; program->statements.push_back(parseCall(fname, in));
        }
    }
    return program;
}

//-----------------------------------------------------
// IR (now with memory offsets, functions, and calls)
//-----------------------------------------------------
enum class IROp { Alloc, Store, Load, Add, Call, Ret, Print };
struct IRInstr { IROp op; std::vector<std::string> args; };
struct BasicBlock { std::string name; std::vector<IRInstr> instrs; };
struct IRFunction {
    std::string name; std::vector<std::string> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    std::map<std::string, int> varOffsets; // name -> stack offset
    int stackSize = 0;
};
struct IRProgram { std::unordered_map<std::string, IRFunction> functions; };

//-----------------------------------------------------
// Lowering: AST → IR (layout with stack offsets)
//-----------------------------------------------------
int allocateVar(IRFunction& fn, const std::string& name) {
    int ofs = fn.stackSize++;
    fn.varOffsets[name] = ofs;
    return ofs;
}
void lowerFunction(ASTFunction* astFunc, IRProgram& irprog) {
    IRFunction irFunc; irFunc.name = astFunc->name; irFunc.params = astFunc->params;
    int argIdx = 0;
    for (const auto& p : irFunc.params) irFunc.varOffsets[p] = argIdx++;
    irFunc.stackSize = argIdx;
    auto entry = std::make_unique<BasicBlock>();
    entry->name = irFunc.name + "_entry";
    for (auto& stmt : astFunc->body) {
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(irFunc, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
            else if (auto var = dynamic_cast<ASTVar*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, var->name}});
            else if (auto bin = dynamic_cast<ASTBinary*>(val->init.get()))
                entry->instrs.push_back({IROp::Add, {val->name, bin->lhs, bin->rhs}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args; for (auto& a : call->args) {
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            }
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    irFunc.blocks.push_back(std::move(entry));
    irprog.functions[irFunc.name] = std::move(irFunc);
}
IRProgram lower(ASTProgram& program) {
    IRProgram irprog;
    for (auto& stmt : program.statements)
        if (auto func = dynamic_cast<ASTFunction*>(stmt.get()))
            lowerFunction(func, irprog);
    IRFunction mainFn;
    mainFn.name = "_main";
    auto entry = std::make_unique<BasicBlock>();
    entry->name = "entry";
    for (auto& stmt : program.statements) {
        if (dynamic_cast<ASTFunction*>(stmt.get())) continue;
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(mainFn, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    mainFn.blocks.push_back(std::move(entry));
    irprog.functions[mainFn.name] = std::move(mainFn);
    return irprog;
}

//-----------------------------------------------------
// INTERPRETER (executes IR, manages call stack/locals)
//-----------------------------------------------------
struct StackFrame {
    std::map<std::string, int> vars; // var name -> value
    int retAddr = -1; // not used in this mockup
};
class IRInterpreter {
public:
    IRInterpreter(const IRProgram& prog) : prog(prog) {}
    int call(const std::string& name, const std::vector<int>& args) {
        auto it = prog.functions.find(name);
        if (it == prog.functions.end()) throw std::runtime_error("No such function: " + name);
        StackFrame frame;
        int idx = 0;
        for (const auto& p : it->second.params) frame.vars[p] = args[idx++];
        callStack.push_back(frame);
        int result = execFunc(it->second);
        callStack.pop_back();
        return result;
    }
    void execMain() { call("_main", {}); }
    void debugMain();
private:
    const IRProgram& prog;
    std::vector<StackFrame> callStack;
    int execFunc(const IRFunction& fn) {
        for (const auto& block : fn.blocks)
            for (const auto& instr : block->instrs)
                step(instr);
        return 0;
    }
    void step(const IRInstr& instr) {
        if (instr.op == IROp::Alloc) callStack.back().vars[instr.args[0]] = 0;
        else if (instr.op == IROp::Store) callStack.back().vars[instr.args[0]] = std::stoi(instr.args[1]);
        else if (instr.op == IROp::Add)
            callStack.back().vars[instr.args[0]] =
                valueOf(instr.args[1]) + valueOf(instr.args[2]);
        else if (instr.op == IROp::Call) {
            std::vector<int> args;
            for (size_t i = 1; i < instr.args.size(); ++i)
                args.push_back(valueOf(instr.args[i]));
            if (instr.args[0] == "say") {
                for (int v : args) std::cout << v << std::endl;
            } else {
                call(instr.args[0], args);
            }
        }
    }
    int valueOf(const std::string& name) {
        for (auto it = callStack.rbegin(); it != callStack.rend(); ++it)
            if (it->vars.count(name)) return it->vars.at(name);
        return std::stoi(name); // literal fallback
    }
};
void IRInterpreter::debugMain() {
    std::cout << "Debugger (step=Enter/print p/vars v/quit q):\n";
    auto it = prog.functions.find("_main");
    if (it == prog.functions.end()) throw std::runtime_error("No _main");
    auto& fn = it->second;
    StackFrame frame;
    callStack.push_back(frame);
    for (const auto& block : fn.blocks)
        for (const auto& instr : block->instrs) {
            std::cout << "[step] " << (int)instr.op << ": ";
            for (const auto& a : instr.args) std::cout << a << " ";
            std::cout << "\n> ";
            std::string cmd; std::getline(std::cin, cmd);
            if (cmd == "q") return;
            if (cmd == "p") { for (const auto& v : callStack.back().vars) std::cout << v.first << "=" << v.second << "\n"; }
            else step(instr);
        }
    callStack.pop_back();
}

//-----------------------------------------------------
// REPL
//-----------------------------------------------------
void repl() {
    std::cout << "QuarterLang REPL. Type 'exit' to quit.\n";
    std::string line, src;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        src = line;
        try {
            auto ast = parse(src);
            IRProgram irprog = lower(*ast);
            IRInterpreter interp(irprog);
            interp.execMain();
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }
}

//-----------------------------------------------------
// DRIVER (main)
//-----------------------------------------------------
std::string loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open source file");
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--repl") { repl(); return 0; }
    if (argc > 1 && std::string(argv[1]) == "--debug") {
        std::string source = loadFile(argc > 2 ? argv[2] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.debugMain();
        return 0;
    }
    try {
        std::string source = loadFile(argc > 1 ? argv[1] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.execMain();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

/*
 * QuarterLang Compiler/Interpreter
 *  - Functions & Calls
 *  - Stack Frame Memory Layout (with Offsets)
 *  - REPL with Built-in Debugger
 *  - Standard Library in QuarterLang
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <stdexcept>
#include <cassert>

//======================================================
// AST DEFINITIONS
//======================================================
struct ASTExpr { virtual ~ASTExpr() = default; };
struct ASTLiteral : ASTExpr { std::string value; ASTLiteral(const std::string& v): value(v) {} };
struct ASTVar : ASTExpr { std::string name; ASTVar(const std::string& n): name(n) {} };
struct ASTBinary : ASTExpr { std::string lhs, op, rhs; ASTBinary(const std::string& l, const std::string& o, const std::string& r): lhs(l), op(o), rhs(r) {} };
struct ASTValDecl : ASTExpr { std::string name, type; std::unique_ptr<ASTExpr> init; };
struct ASTCall : ASTExpr { std::string funcName; std::vector<std::unique_ptr<ASTExpr>> args; ASTCall(const std::string& f): funcName(f) {} };
struct ASTFunction : ASTExpr { std::string name; std::vector<std::string> params; std::vector<std::unique_ptr<ASTExpr>> body; };
struct ASTProgram : ASTExpr {
    std::vector<std::unique_ptr<ASTExpr>> statements;
    std::unordered_map<std::string, ASTFunction*> functions;
};

//======================================================
// PARSER (Minimal, handles functions, calls, expressions)
//======================================================
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in);
std::unique_ptr<ASTCall> parseCall(const std::string& funcName, std::istringstream& in) {
    auto call = std::make_unique<ASTCall>(funcName);
    char ch;
    in >> ch; // '('
    while (in.peek() != ')' && in >> std::ws) {
        call->args.push_back(parseExpr(in));
        if (in.peek() == ',') in.ignore(1);
    }
    in.ignore(1); // ')'
    return call;
}
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in) {
    std::string token;
    in >> token;
    if (isdigit(token[0])) return std::make_unique<ASTLiteral>(token);
    if (isalpha(token[0])) {
        if (in.peek() == '(') return parseCall(token, in);
        std::string op, rhs;
        std::streampos oldpos = in.tellg();
        if (in >> op) {
            if (op == "+" || op == "-" || op == "*" || op == "/") {
                in >> rhs;
                return std::make_unique<ASTBinary>(token, op, rhs);
            } else in.seekg(oldpos);
        }
        return std::make_unique<ASTVar>(token);
    }
    return std::make_unique<ASTLiteral>(token);
}
std::unique_ptr<ASTFunction> parseFunction(std::istringstream& in) {
    auto func = std::make_unique<ASTFunction>();
    std::string name, param;
    in >> name; func->name = name;
    in >> param; // '('
    while (in.peek() != ')' && in >> std::ws) {
        in >> param; if (param.back() == ',') param.pop_back();
        func->params.push_back(param);
    }
    in.ignore(1); // ')'
    std::string brace; in >> brace; // '{'
    std::string token;
    while (in >> token && token != "}") {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            func->body.push_back(std::move(decl));
        } else if (token == "call") {
            std::string fname; in >> fname; func->body.push_back(parseCall(fname, in));
        }
    }
    return func;
}
std::unique_ptr<ASTProgram> parse(const std::string& src) {
    auto program = std::make_unique<ASTProgram>();
    std::istringstream in(src);
    std::string token;
    while (in >> token) {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            program->statements.push_back(std::move(decl));
        } else if (token == "func") {
            auto func = parseFunction(in);
            program->functions[func->name] = func.get();
            program->statements.push_back(std::move(func));
        } else if (token == "call") {
            std::string fname; in >> fname; program->statements.push_back(parseCall(fname, in));
        }
    }
    return program;
}

//======================================================
// IR + Stack Frame Layout + Function Call/Return
//======================================================
enum class IROp { Alloc, Store, Load, Add, Call, Ret, Print };
struct IRInstr { IROp op; std::vector<std::string> args; };
struct BasicBlock { std::string name; std::vector<IRInstr> instrs; };
struct IRFunction {
    std::string name; std::vector<std::string> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    std::map<std::string, int> varOffsets; // name → stack offset
    int stackSize = 0;
};
struct IRProgram { std::unordered_map<std::string, IRFunction> functions; };

// Allocation assigns stack offset (incremented for each new var)
int allocateVar(IRFunction& fn, const std::string& name) {
    int ofs = fn.stackSize++;
    fn.varOffsets[name] = ofs;
    return ofs;
}
void lowerFunction(ASTFunction* astFunc, IRProgram& irprog) {
    IRFunction irFunc; irFunc.name = astFunc->name; irFunc.params = astFunc->params;
    int argIdx = 0;
    for (const auto& p : irFunc.params) irFunc.varOffsets[p] = argIdx++;
    irFunc.stackSize = argIdx;
    auto entry = std::make_unique<BasicBlock>();
    entry->name = irFunc.name + "_entry";
    for (auto& stmt : astFunc->body) {
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(irFunc, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
            else if (auto var = dynamic_cast<ASTVar*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, var->name}});
            else if (auto bin = dynamic_cast<ASTBinary*>(val->init.get()))
                entry->instrs.push_back({IROp::Add, {val->name, bin->lhs, bin->rhs}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args; for (auto& a : call->args) {
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            }
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    irFunc.blocks.push_back(std::move(entry));
    irprog.functions[irFunc.name] = std::move(irFunc);
}
IRProgram lower(ASTProgram& program) {
    IRProgram irprog;
    for (auto& stmt : program.statements)
        if (auto func = dynamic_cast<ASTFunction*>(stmt.get()))
            lowerFunction(func, irprog);
    IRFunction mainFn;
    mainFn.name = "_main";
    auto entry = std::make_unique<BasicBlock>();
    entry->name = "entry";
    for (auto& stmt : program.statements) {
        if (dynamic_cast<ASTFunction*>(stmt.get())) continue;
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(mainFn, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    mainFn.blocks.push_back(std::move(entry));
    irprog.functions[mainFn.name] = std::move(mainFn);
    return irprog;
}

//======================================================
// INTERPRETER (Stack-based, with debugging & print)
//======================================================
struct StackFrame {
    std::map<std::string, int> vars; // var name → value
    int retAddr = -1;
};
class IRInterpreter {
public:
    IRInterpreter(const IRProgram& prog) : prog(prog) {}
    int call(const std::string& name, const std::vector<int>& args) {
        auto it = prog.functions.find(name);
        if (it == prog.functions.end()) throw std::runtime_error("No such function: " + name);
        StackFrame frame;
        int idx = 0;
        for (const auto& p : it->second.params) frame.vars[p] = args[idx++];
        callStack.push_back(frame);
        int result = execFunc(it->second);
        callStack.pop_back();
        return result;
    }
    void execMain() { call("_main", {}); }
    void debugMain();
private:
    const IRProgram& prog;
    std::vector<StackFrame> callStack;
    int execFunc(const IRFunction& fn) {
        for (const auto& block : fn.blocks)
            for (const auto& instr : block->instrs)
                step(instr);
        return 0;
    }
    void step(const IRInstr& instr) {
        if (instr.op == IROp::Alloc) callStack.back().vars[instr.args[0]] = 0;
        else if (instr.op == IROp::Store) callStack.back().vars[instr.args[0]] = std::stoi(instr.args[1]);
        else if (instr.op == IROp::Add)
            callStack.back().vars[instr.args[0]] =
                valueOf(instr.args[1]) + valueOf(instr.args[2]);
        else if (instr.op == IROp::Call) {
            std::vector<int> args;
            for (size_t i = 1; i < instr.args.size(); ++i)
                args.push_back(valueOf(instr.args[i]));
            if (instr.args[0] == "say") {
                for (int v : args) std::cout << v << std::endl;
            } else {
                call(instr.args[0], args);
            }
        }
    }
    int valueOf(const std::string& name) {
        for (auto it = callStack.rbegin(); it != callStack.rend(); ++it)
            if (it->vars.count(name)) return it->vars.at(name);
        return std::stoi(name); // literal fallback
    }
};
void IRInterpreter::debugMain() {
    std::cout << "Debugger (Enter=step, v=vars, q=quit):\n";
    auto it = prog.functions.find("_main");
    if (it == prog.functions.end()) throw std::runtime_error("No _main");
    auto& fn = it->second;
    StackFrame frame;
    callStack.push_back(frame);
    for (const auto& block : fn.blocks)
        for (const auto& instr : block->instrs) {
            std::cout << "[step] " << (int)instr.op << ": ";
            for (const auto& a : instr.args) std::cout << a << " ";
            std::cout << "\n> ";
            std::string cmd; std::getline(std::cin, cmd);
            if (cmd == "q") return;
            if (cmd == "v") { for (const auto& v : callStack.back().vars) std::cout << v.first << "=" << v.second << "\n"; }
            else step(instr);
        }
    callStack.pop_back();
}

//======================================================
// REPL (Interactive)
//======================================================
void repl() {
    std::cout << "QuarterLang REPL. Type 'exit' to quit.\n";
    std::string line, src;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        src = line;
        try {
            auto ast = parse(src);
            IRProgram irprog = lower(*ast);
            IRInterpreter interp(irprog);
            interp.execMain();
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }
}

//======================================================
// FILE LOADER & DRIVER (main)
//======================================================
std::string loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open source file");
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--repl") { repl(); return 0; }
    if (argc > 1 && std::string(argv[1]) == "--debug") {
        std::string source = loadFile(argc > 2 ? argv[2] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.debugMain();
        return 0;
    }
    try {
        std::string source = loadFile(argc > 1 ? argv[1] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.execMain();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

/*
 * QuarterLang Compiler/Interpreter
 * - Functions & Calls, Stack Frame & Offsets, REPL, Debugger
 * - Standard Library ready
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <stdexcept>
#include <cassert>

//======================================================
// AST DEFINITIONS
//======================================================
struct ASTExpr { virtual ~ASTExpr() = default; };
struct ASTLiteral : ASTExpr { std::string value; ASTLiteral(const std::string& v): value(v) {} };
struct ASTVar : ASTExpr { std::string name; ASTVar(const std::string& n): name(n) {} };
struct ASTBinary : ASTExpr { std::string lhs, op, rhs; ASTBinary(const std::string& l, const std::string& o, const std::string& r): lhs(l), op(o), rhs(r) {} };
struct ASTValDecl : ASTExpr { std::string name, type; std::unique_ptr<ASTExpr> init; };
struct ASTCall : ASTExpr { std::string funcName; std::vector<std::unique_ptr<ASTExpr>> args; ASTCall(const std::string& f): funcName(f) {} };
struct ASTFunction : ASTExpr { std::string name; std::vector<std::string> params; std::vector<std::unique_ptr<ASTExpr>> body; };
struct ASTProgram : ASTExpr {
    std::vector<std::unique_ptr<ASTExpr>> statements;
    std::unordered_map<std::string, ASTFunction*> functions;
};

//======================================================
// PARSER (handles functions, calls, expressions)
//======================================================
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in);
std::unique_ptr<ASTCall> parseCall(const std::string& funcName, std::istringstream& in) {
    auto call = std::make_unique<ASTCall>(funcName);
    char ch; in >> ch; // '('
    while (in.peek() != ')' && in >> std::ws) {
        call->args.push_back(parseExpr(in));
        if (in.peek() == ',') in.ignore(1);
    }
    in.ignore(1); // ')'
    return call;
}
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in) {
    std::string token; in >> token;
    if (isdigit(token[0])) return std::make_unique<ASTLiteral>(token);
    if (isalpha(token[0])) {
        if (in.peek() == '(') return parseCall(token, in);
        std::string op, rhs; std::streampos oldpos = in.tellg();
        if (in >> op) {
            if (op == "+" || op == "-" || op == "*" || op == "/") {
                in >> rhs;
                return std::make_unique<ASTBinary>(token, op, rhs);
            } else in.seekg(oldpos);
        }
        return std::make_unique<ASTVar>(token);
    }
    return std::make_unique<ASTLiteral>(token);
}
std::unique_ptr<ASTFunction> parseFunction(std::istringstream& in) {
    auto func = std::make_unique<ASTFunction>();
    std::string name, param; in >> name; func->name = name;
    in >> param; // '('
    while (in.peek() != ')' && in >> std::ws) {
        in >> param; if (param.back() == ',') param.pop_back();
        func->params.push_back(param);
    }
    in.ignore(1); // ')'
    std::string brace; in >> brace; // '{'
    std::string token;
    while (in >> token && token != "}") {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            func->body.push_back(std::move(decl));
        } else if (token == "call") {
            std::string fname; in >> fname; func->body.push_back(parseCall(fname, in));
        }
    }
    return func;
}
std::unique_ptr<ASTProgram> parse(const std::string& src) {
    auto program = std::make_unique<ASTProgram>();
    std::istringstream in(src);
    std::string token;
    while (in >> token) {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            program->statements.push_back(std::move(decl));
        } else if (token == "func") {
            auto func = parseFunction(in);
            program->functions[func->name] = func.get();
            program->statements.push_back(std::move(func));
        } else if (token == "call") {
            std::string fname; in >> fname; program->statements.push_back(parseCall(fname, in));
        }
    }
    return program;
}

//======================================================
// IR + Stack Frame Layout + Function Call/Return
//======================================================
enum class IROp { Alloc, Store, Load, Add, Call, Ret, Print };
struct IRInstr { IROp op; std::vector<std::string> args; };
struct BasicBlock { std::string name; std::vector<IRInstr> instrs; };
struct IRFunction {
    std::string name; std::vector<std::string> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    std::map<std::string, int> varOffsets; // name → stack offset
    int stackSize = 0;
};
struct IRProgram { std::unordered_map<std::string, IRFunction> functions; };

int allocateVar(IRFunction& fn, const std::string& name) {
    int ofs = fn.stackSize++;
    fn.varOffsets[name] = ofs;
    return ofs;
}
void lowerFunction(ASTFunction* astFunc, IRProgram& irprog) {
    IRFunction irFunc; irFunc.name = astFunc->name; irFunc.params = astFunc->params;
    int argIdx = 0;
    for (const auto& p : irFunc.params) irFunc.varOffsets[p] = argIdx++;
    irFunc.stackSize = argIdx;
    auto entry = std::make_unique<BasicBlock>();
    entry->name = irFunc.name + "_entry";
    for (auto& stmt : astFunc->body) {
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(irFunc, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
            else if (auto var = dynamic_cast<ASTVar*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, var->name}});
            else if (auto bin = dynamic_cast<ASTBinary*>(val->init.get()))
                entry->instrs.push_back({IROp::Add, {val->name, bin->lhs, bin->rhs}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args; for (auto& a : call->args) {
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            }
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    irFunc.blocks.push_back(std::move(entry));
    irprog.functions[irFunc.name] = std::move(irFunc);
}
IRProgram lower(ASTProgram& program) {
    IRProgram irprog;
    for (auto& stmt : program.statements)
        if (auto func = dynamic_cast<ASTFunction*>(stmt.get()))
            lowerFunction(func, irprog);
    IRFunction mainFn;
    mainFn.name = "_main";
    auto entry = std::make_unique<BasicBlock>();
    entry->name = "entry";
    for (auto& stmt : program.statements) {
        if (dynamic_cast<ASTFunction*>(stmt.get())) continue;
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(mainFn, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    mainFn.blocks.push_back(std::move(entry));
    irprog.functions[mainFn.name] = std::move(mainFn);
    return irprog;
}

//======================================================
// INTERPRETER (Stack-based, with debugging & print)
//======================================================
struct StackFrame {
    std::map<std::string, int> vars; // var name → value
    int retAddr = -1;
};
class IRInterpreter {
public:
    IRInterpreter(const IRProgram& prog) : prog(prog) {}
    int call(const std::string& name, const std::vector<int>& args) {
        auto it = prog.functions.find(name);
        if (it == prog.functions.end()) throw std::runtime_error("No such function: " + name);
        StackFrame frame;
        int idx = 0;
        for (const auto& p : it->second.params) frame.vars[p] = args[idx++];
        callStack.push_back(frame);
        int result = execFunc(it->second);
        callStack.pop_back();
        return result;
    }
    void execMain() { call("_main", {}); }
    void debugMain();
private:
    const IRProgram& prog;
    std::vector<StackFrame> callStack;
    int execFunc(const IRFunction& fn) {
        for (const auto& block : fn.blocks)
            for (const auto& instr : block->instrs)
                step(instr);
        return 0;
    }
    void step(const IRInstr& instr) {
        if (instr.op == IROp::Alloc) callStack.back().vars[instr.args[0]] = 0;
        else if (instr.op == IROp::Store) callStack.back().vars[instr.args[0]] = std::stoi(instr.args[1]);
        else if (instr.op == IROp::Add)
            callStack.back().vars[instr.args[0]] =
                valueOf(instr.args[1]) + valueOf(instr.args[2]);
        else if (instr.op == IROp::Call) {
            std::vector<int> args;
            for (size_t i = 1; i < instr.args.size(); ++i)
                args.push_back(valueOf(instr.args[i]));
            if (instr.args[0] == "say") {
                for (int v : args) std::cout << v << std::endl;
            } else {
                call(instr.args[0], args);
            }
        }
    }
    int valueOf(const std::string& name) {
        for (auto it = callStack.rbegin(); it != callStack.rend(); ++it)
            if (it->vars.count(name)) return it->vars.at(name);
        return std::stoi(name); // literal fallback
    }
};
void IRInterpreter::debugMain() {
    std::cout << "Debugger (Enter=step, v=vars, q=quit):\n";
    auto it = prog.functions.find("_main");
    if (it == prog.functions.end()) throw std::runtime_error("No _main");
    auto& fn = it->second;
    StackFrame frame;
    callStack.push_back(frame);
    for (const auto& block : fn.blocks)
        for (const auto& instr : block->instrs) {
            std::cout << "[step] " << (int)instr.op << ": ";
            for (const auto& a : instr.args) std::cout << a << " ";
            std::cout << "\n> ";
            std::string cmd; std::getline(std::cin, cmd);
            if (cmd == "q") return;
            if (cmd == "v") { for (const auto& v : callStack.back().vars) std::cout << v.first << "=" << v.second << "\n"; }
            else step(instr);
        }
    callStack.pop_back();
}

//======================================================
// REPL (Interactive)
//======================================================
void repl() {
    std::cout << "QuarterLang REPL. Type 'exit' to quit.\n";
    std::string line, src;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
        src = line;
        try {
            auto ast = parse(src);
            IRProgram irprog = lower(*ast);
            IRInterpreter interp(irprog);
            interp.execMain();
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }
}

//======================================================
// FILE LOADER & DRIVER (main)
//======================================================
std::string loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open source file");
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--repl") { repl(); return 0; }
    if (argc > 1 && std::string(argv[1]) == "--debug") {
        std::string source = loadFile(argc > 2 ? argv[2] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.debugMain();
        return 0;
    }
    try {
        std::string source = loadFile(argc > 1 ? argv[1] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.execMain();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

/*
 * quarterc.cpp - QuarterLang Compiler/Interpreter
 * --------------------------------------------------
 * Supports: full module pipeline, user functions/calls,
 * stack memory layout, IR/CFG lowering, REPL/debugger,
 * and a mapping for all core/stdlib sections.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <stdexcept>

// ===============================
// AST & Module Stubs (22 sections)
// ===============================

// -- 1. MemoryHandler, RangeAdjuster, ErrorHandler, etc. --
namespace MemoryHandler { /* ... */ }
namespace RangeAdjuster { /* ... */ }
namespace ErrorHandler {
    void error(int code, const std::string& msg) { throw std::runtime_error("[Error " + std::to_string(code) + "]: " + msg); }
    void warn(const std::string& msg) { std::cerr << "[Warning]: " << msg << std::endl; }
    void info(const std::string& msg) { std::cerr << "[Info]: " << msg << std::endl; }
}
// ... etc., up to 22 sections (stubs here for structure) ...

// --- Core AST nodes (expression hierarchy) ---
struct ASTExpr { virtual ~ASTExpr() = default; };
struct ASTLiteral : ASTExpr { std::string value; ASTLiteral(const std::string& v) : value(v) {} };
struct ASTVar : ASTExpr { std::string name; ASTVar(const std::string& n): name(n) {} };
struct ASTBinary : ASTExpr { std::string lhs, op, rhs; ASTBinary(const std::string& l, const std::string& o, const std::string& r): lhs(l), op(o), rhs(r) {} };
struct ASTValDecl : ASTExpr { std::string name, type; std::unique_ptr<ASTExpr> init; };
struct ASTCall : ASTExpr { std::string funcName; std::vector<std::unique_ptr<ASTExpr>> args; ASTCall(const std::string& f): funcName(f) {} };
struct ASTFunction : ASTExpr { std::string name; std::vector<std::string> params; std::vector<std::unique_ptr<ASTExpr>> body; };
struct ASTProgram : ASTExpr {
    std::vector<std::unique_ptr<ASTExpr>> statements;
    std::unordered_map<std::string, ASTFunction*> functions;
};

// --- IR (CFG) Model ---
enum class IROp { Alloc, Store, Load, Add, Call, Ret, Print };
struct IRInstr { IROp op; std::vector<std::string> args; };
struct BasicBlock { std::string name; std::vector<IRInstr> instrs; };
struct IRFunction {
    std::string name; std::vector<std::string> params;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    std::map<std::string, int> varOffsets; // name → stack offset
    int stackSize = 0;
};
struct IRProgram { std::unordered_map<std::string, IRFunction> functions; };

// ========== PARSER ==========

std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in);
std::unique_ptr<ASTCall> parseCall(const std::string& funcName, std::istringstream& in) {
    auto call = std::make_unique<ASTCall>(funcName); char ch; in >> ch; // '('
    while (in.peek() != ')' && in >> std::ws) {
        call->args.push_back(parseExpr(in)); if (in.peek() == ',') in.ignore(1);
    }
    in.ignore(1); // ')'
    return call;
}
std::unique_ptr<ASTExpr> parseExpr(std::istringstream& in) {
    std::string token; in >> token;
    if (isdigit(token[0])) return std::make_unique<ASTLiteral>(token);
    if (isalpha(token[0])) {
        if (in.peek() == '(') return parseCall(token, in);
        std::string op, rhs; std::streampos oldpos = in.tellg();
        if (in >> op) {
            if (op == "+" || op == "-" || op == "*" || op == "/") { in >> rhs; return std::make_unique<ASTBinary>(token, op, rhs); }
            else in.seekg(oldpos);
        }
        return std::make_unique<ASTVar>(token);
    }
    return std::make_unique<ASTLiteral>(token);
}
std::unique_ptr<ASTFunction> parseFunction(std::istringstream& in) {
    auto func = std::make_unique<ASTFunction>();
    std::string name, param; in >> name; func->name = name;
    in >> param; // '('
    while (in.peek() != ')' && in >> std::ws) { in >> param; if (param.back() == ',') param.pop_back(); func->params.push_back(param); }
    in.ignore(1); // ')'
    std::string brace; in >> brace; // '{'
    std::string token;
    while (in >> token && token != "}") {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>(); in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in); func->body.push_back(std::move(decl));
        } else if (token == "call") {
            std::string fname; in >> fname; func->body.push_back(parseCall(fname, in));
        }
    }
    return func;
}
std::unique_ptr<ASTProgram> parse(const std::string& src) {
    auto program = std::make_unique<ASTProgram>(); std::istringstream in(src); std::string token;
    while (in >> token) {
        if (token == "val") {
            auto decl = std::make_unique<ASTValDecl>(); in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in); program->statements.push_back(std::move(decl));
        } else if (token == "func") {
            auto func = parseFunction(in); program->functions[func->name] = func.get(); program->statements.push_back(std::move(func));
        } else if (token == "call") {
            std::string fname; in >> fname; program->statements.push_back(parseCall(fname, in));
        }
    }
    return program;
}

// ========== AST → IR Lowering ==========

int allocateVar(IRFunction& fn, const std::string& name) { int ofs = fn.stackSize++; fn.varOffsets[name] = ofs; return ofs; }
void lowerFunction(ASTFunction* astFunc, IRProgram& irprog) {
    IRFunction irFunc; irFunc.name = astFunc->name; irFunc.params = astFunc->params;
    int argIdx = 0; for (const auto& p : irFunc.params) irFunc.varOffsets[p] = argIdx++; irFunc.stackSize = argIdx;
    auto entry = std::make_unique<BasicBlock>(); entry->name = irFunc.name + "_entry";
    for (auto& stmt : astFunc->body) {
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(irFunc, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
            else if (auto var = dynamic_cast<ASTVar*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, var->name}});
            else if (auto bin = dynamic_cast<ASTBinary*>(val->init.get()))
                entry->instrs.push_back({IROp::Add, {val->name, bin->lhs, bin->rhs}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args; for (auto& a : call->args) {
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            }
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    irFunc.blocks.push_back(std::move(entry));
    irprog.functions[irFunc.name] = std::move(irFunc);
}
IRProgram lower(ASTProgram& program) {
    IRProgram irprog;
    for (auto& stmt : program.statements)
        if (auto func = dynamic_cast<ASTFunction*>(stmt.get()))
            lowerFunction(func, irprog);
    IRFunction mainFn; mainFn.name = "_main";
    auto entry = std::make_unique<BasicBlock>(); entry->name = "entry";
    for (auto& stmt : program.statements) {
        if (dynamic_cast<ASTFunction*>(stmt.get())) continue;
        if (auto val = dynamic_cast<ASTValDecl*>(stmt.get())) {
            int ofs = allocateVar(mainFn, val->name);
            entry->instrs.push_back({IROp::Alloc, {val->name, std::to_string(ofs)}});
            if (auto lit = dynamic_cast<ASTLiteral*>(val->init.get()))
                entry->instrs.push_back({IROp::Store, {val->name, lit->value}});
        } else if (auto call = dynamic_cast<ASTCall*>(stmt.get())) {
            std::vector<std::string> args;
            for (auto& a : call->args)
                if (auto lit = dynamic_cast<ASTLiteral*>(a.get())) args.push_back(lit->value);
                else if (auto var = dynamic_cast<ASTVar*>(a.get())) args.push_back(var->name);
            args.insert(args.begin(), call->funcName);
            entry->instrs.push_back({IROp::Call, args});
        }
    }
    mainFn.blocks.push_back(std::move(entry));
    irprog.functions[mainFn.name] = std::move(mainFn);
    return irprog;
}

// ========== INTERPRETER & DEBUGGER ==========

struct StackFrame { std::map<std::string, int> vars; int retAddr = -1; };
class IRInterpreter {
public:
    IRInterpreter(const IRProgram& prog) : prog(prog) {}
    int call(const std::string& name, const std::vector<int>& args) {
        auto it = prog.functions.find(name); if (it == prog.functions.end()) throw std::runtime_error("No such function: " + name);
        StackFrame frame; int idx = 0; for (const auto& p : it->second.params) frame.vars[p] = args[idx++]; callStack.push_back(frame);
        int result = execFunc(it->second); callStack.pop_back(); return result;
    }
    void execMain() { call("_main", {}); }
    void debugMain();
private:
    const IRProgram& prog; std::vector<StackFrame> callStack;
    int execFunc(const IRFunction& fn) {
        for (const auto& block : fn.blocks)
            for (const auto& instr : block->instrs) step(instr);
        return 0;
    }
    void step(const IRInstr& instr) {
        if (instr.op == IROp::Alloc) callStack.back().vars[instr.args[0]] = 0;
        else if (instr.op == IROp::Store) callStack.back().vars[instr.args[0]] = std::stoi(instr.args[1]);
        else if (instr.op == IROp::Add)
            callStack.back().vars[instr.args[0]] = valueOf(instr.args[1]) + valueOf(instr.args[2]);
        else if (instr.op == IROp::Call) {
            std::vector<int> args;
            for (size_t i = 1; i < instr.args.size(); ++i) args.push_back(valueOf(instr.args[i]));
            if (instr.args[0] == "say") { for (int v : args) std::cout << v << std::endl; }
            else { call(instr.args[0], args); }
        }
    }
    int valueOf(const std::string& name) {
        for (auto it = callStack.rbegin(); it != callStack.rend(); ++it)
            if (it->vars.count(name)) return it->vars.at(name);
        return std::stoi(name);
    }
};
void IRInterpreter::debugMain() {
    std::cout << "Debugger (Enter=step, v=vars, q=quit):\n";
    auto it = prog.functions.find("_main"); if (it == prog.functions.end()) throw std::runtime_error("No _main");
    auto& fn = it->second; StackFrame frame; callStack.push_back(frame);
    for (const auto& block : fn.blocks)
        for (const auto& instr : block->instrs) {
            std::cout << "[step] " << (int)instr.op << ": ";
            for (const auto& a : instr.args) std::cout << a << " "; std::cout << "\n> ";
            std::string cmd; std::getline(std::cin, cmd);
            if (cmd == "q") return;
            if (cmd == "v") { for (const auto& v : callStack.back().vars) std::cout << v.first << "=" << v.second << "\n"; }
            else step(instr);
        }
    callStack.pop_back();
}

// ========== REPL ==========
void repl() {
    std::cout << "QuarterLang REPL. Type 'exit' to quit.\n"; std::string line, src;
    while (true) {
        std::cout << ">> "; if (!std::getline(std::cin, line)) break;
        if (line == "exit") break; src = line;
        try { auto ast = parse(src); IRProgram irprog = lower(*ast); IRInterpreter interp(irprog); interp.execMain(); }
        catch (const std::exception& ex) { std::cerr << "Error: " << ex.what() << std::endl; }
    }
}

// ========== DRIVER ==========
std::string loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Could not open source file");
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--repl") { repl(); return 0; }
    if (argc > 1 && std::string(argv[1]) == "--debug") {
        std::string source = loadFile(argc > 2 ? argv[2] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.debugMain();
        return 0;
    }
    try {
        std::string source = loadFile(argc > 1 ? argv[1] : "program.qtr");
        auto ast = parse(source);
        IRProgram irprog = lower(*ast);
        IRInterpreter interp(irprog);
        interp.execMain();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

else if (token == "return") {
    func->returnExpr = parseExpr(in);
}

#pragma once
// Memory allocation and GC management for QuarterLang.

#include <cstddef>

class MemoryHandler {
public:
    static void* allocate(std::size_t size);
    static void free(void* ptr);
    static void gc_collect();
};

#include "MemoryHandler.h"
#include <cstdlib>

void* MemoryHandler::allocate(std::size_t size) { return std::malloc(size); }
void MemoryHandler::free(void* ptr) { std::free(ptr); }
void MemoryHandler::gc_collect() { /* GC logic here */ }

#pragma once
// Numeric range and normalization utilities.

class RangeAdjuster {
public:
    static int clamp(int value, int min, int max);
    static double normalize(double value, double in_min, double in_max, double out_min, double out_max);
};

#include "RangeAdjuster.h"
int RangeAdjuster::clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
double RangeAdjuster::normalize(double value, double in_min, double in_max, double out_min, double out_max) {
    double ratio = (value - in_min) / (in_max - in_min);
    return out_min + ratio * (out_max - out_min);
}

#pragma once
// Centralized error, warning, and info reporting.

#include <string>

class ErrorHandler {
public:
    static void error(int code, const std::string& message);
    static void warn(const std::string& message);
    static void info(const std::string& message);
};

#include "ErrorHandler.h"
#include <iostream>
#include <stdexcept>

void ErrorHandler::error(int code, const std::string& message) {
    throw std::runtime_error("[Error " + std::to_string(code) + "]: " + message);
}
void ErrorHandler::warn(const std::string& message) {
    std::cerr << "[Warning]: " << message << std::endl;
}
void ErrorHandler::info(const std::string& message) {
    std::cout << "[Info]: " << message << std::endl;
}

#pragma once
// Symbol indexing for ASTs.

#include <unordered_map>
#include <string>
#include "AST.h"

class Indexter {
public:
    static std::unordered_map<std::string, int> indexSymbols(const AST& ast);
};

#include "Indexter.h"
// TODO: Implement symbol indexing.
std::unordered_map<std::string, int> Indexter::indexSymbols(const AST&) {
    return {};
}

#pragma once
// File and console I/O utilities.

#include <string>

class IO {
public:
    static std::string readFile(const std::string& path);
    static void writeFile(const std::string& path, const std::string& data);
    static void print(const std::string& text);
    static void println(const std::string& text);
};

#include "IO.h"
#include <fstream>
#include <iostream>

std::string IO::readFile(const std::string& path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
void IO::writeFile(const std::string& path, const std::string& data) {
    std::ofstream out(path);
    out << data;
}
void IO::print(const std::string& text) { std::cout << text; }
void IO::println(const std::string& text) { std::cout << text << std::endl; }

#pragma once
// Module/package manager.

#include <string>

class Filer {
public:
    static std::string loadModule(const std::string& name);
};

#include "Filer.h"
#include "IO.h"

std::string Filer::loadModule(const std::string& name) {
    return IO::readFile(name + ".qtr");
}

#pragma once
// Built-in library management.

#include <string>
#include <unordered_map>

class LibrarySystem {
public:
    static void registerLibrary(const std::string& name, void* module);
    static void* getLibrary(const std::string& name);
private:
    static std::unordered_map<std::string, void*>& libs();
};

#include "LibrarySystem.h"
std::unordered_map<std::string, void*>& LibrarySystem::libs() {
    static std::unordered_map<std::string, void*> instance;
    return instance;
}
void LibrarySystem::registerLibrary(const std::string& name, void* module) {
    libs()[name] = module;
}
void* LibrarySystem::getLibrary(const std::string& name) {
    auto it = libs().find(name);
    if (it == libs().end()) throw std::runtime_error("Library not found: " + name);
    return it->second;
}

#pragma once
// Tokenizer for QuarterLang source.

#include <vector>
#include <string>

struct Token {
    std::string type;
    std::string value;
};

class Lexer {
public:
    static std::vector<Token> lex(const std::string& input);
};

#include "Lexer.h"
// TODO: Implement real lexing logic.
std::vector<Token> Lexer::lex(const std::string&) { return {}; }

#pragma once
// Syntax coloring for token streams.

#include <vector>
#include <string>

struct StyledToken {
    Token token;
    std::string style;
};

class SyntaxHighlighter {
public:
    static std::vector<StyledToken> highlight(const std::vector<Token>& tokens);
};

#include "SyntaxHighlighter.h"
// TODO: Implement syntax highlighting.
std::vector<StyledToken> SyntaxHighlighter::highlight(const std::vector<Token>& tokens) {
    std::vector<StyledToken> styled;
    for (const auto& t : tokens) styled.push_back({t, "plain"});
    return styled;
}

#pragma once
// Pretty-printer for AST/code.

#include <string>
#include "AST.h"

class Formatter {
public:
    static std::string format(const AST& ast);
};

#include "Formatter.h"
// TODO: Implement formatting logic.
std::string Formatter::format(const AST&) { return ""; }

#pragma once
// Recursive descent parser.

#include <string>
#include <memory>
#include "AST.h"

class Parser {
public:
    static std::unique_ptr<AST> parse(const std::string& src);
};

#include "Parser.h"
// TODO: Implement parsing.
std::unique_ptr<AST> Parser::parse(const std::string&) { return nullptr; }

#pragma once
// AST node types for QuarterLang.

#include <vector>
#include <memory>
#include <string>

class ASTNode { public: virtual ~ASTNode() = default; };
class AST : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> nodes;
};

#include "AST.h"
// Implement as needed.

#pragma once
// Intermediate Representation generator.

#include "AST.h"
#include <vector>
#include <string>

struct IR {
    std::string opcode;
    std::vector<std::string> args;
};

class IRBytecode {
public:
    static std::vector<IR> generate(const AST& ast);
};

#include "IRBytecode.h"
// TODO: Implement IR generation.
std::vector<IR> IRBytecode::generate(const AST&) { return {}; }

#pragma once
// Module encapsulation and wrapping.

#include <string>

class Encapsulation {
public:
    static std::string wrap(const std::string& moduleStr);
};

#include "Encapsulation.h"
std::string Encapsulation::wrap(const std::string& moduleStr) {
    return "<module>" + moduleStr + "</module>";
}

#pragma once
// Lexical scope resolver.

#include "AST.h"

class Scoper {
public:
    static void resolveScopes(AST& ast);
};

#include "Scoper.h"
// TODO: Implement scope resolution.
void Scoper::resolveScopes(AST&) {}

#pragma once
// Symbol binding and environment linking.

#include "AST.h"

class Binder {
public:
    static void bind(AST& ast);
};

#include "Binder.h"
// TODO: Implement binding logic.
void Binder::bind(AST&) {}

#pragma once
// AST ↔ IR adapter.

#include "AST.h"
#include "IRBytecode.h"
#include <vector>

class Adapter {
public:
    static std::vector<IR> adaptToIR(const AST& ast);
};

#include "Adapter.h"
std::vector<IR> Adapter::adaptToIR(const AST& ast) {
    return IRBytecode::generate(ast);
}

#pragma once
// IR composer for multiple modules.

#include <vector>
#include "IRBytecode.h"

class Composer {
public:
    static std::vector<IR> compose(const std::vector<std::vector<IR>>& listOfIRs);
};

#include "Composer.h"
std::vector<IR> Composer::compose(const std::vector<std::vector<IR>>& listOfIRs) {
    std::vector<IR> out;
    for (const auto& irs : listOfIRs) out.insert(out.end(), irs.begin(), irs.end());
    return out;
}

#pragma once
// Seeds the stdlib and core modules.

#include <vector>
#include <string>

class Seeder {
public:
    static std::vector<std::string> seedStdlib();
};

#include "Seeder.h"
std::vector<std::string> Seeder::seedStdlib() {
    return { "core", "math" };
}

#pragma once
// NASM/x64 code generator.

#include "IRBytecode.h"
#include <string>
#include <vector>

class CodeGenerator {
public:
    static std::string generateNASM(const std::vector<IR>& irs);
};

#include "CodeGenerator.h"
std::string CodeGenerator::generateNASM(const std::vector<IR>& irs) {
    std::string out = "section .text\n";
    for (const auto& ir : irs)
        out += ir.opcode + " ; ...\n";
    return out;
}

#pragma once
// Assembles NASM to binary.

#include <string>

class BinaryEmitter {
public:
    static std::string emit(const std::string& bin);
};

#include "BinaryEmitter.h"
std::string BinaryEmitter::emit(const std::string&) {
    // TODO: Call assembler, return binary blob or filename
    return "out.bin";
}

#pragma once
// QuarterLang standard library (C++ bindings).

class StandardLib {
public:
    static void registerBuiltins();
    static int print(int x);
    static int add(int x, int y);
    static int mul(int x, int y);
    static int identity(int x);
    static int compare(int x, int y);
    static int sub(int x, int y);
    static int div(int x, int y);
    static int mod(int x, int y);
    static int min(int x, int y);
    static int max(int x, int y);
    static int pow(int x, int y);
    static int abs(int x);
    static int clamp(int x, int min, int max);
    static int is_even(int x);
    static int factorial(int x);
    // ... more as needed
};

void StandardLib::registerBuiltins() { /* Bind to runtime */ }
int StandardLib::print(int x) { std::cout << x << std::endl; return x; }
int StandardLib::add(int x, int y) { return x + y; }
int StandardLib::mul(int x, int y) { return x * y; }
int StandardLib::identity(int x) { return x; }
int StandardLib::compare(int x, int y) { std::cout << x << " " << y << std::endl; return 0; }
int StandardLib::sub(int x, int y) { return x - y; }
int StandardLib::div(int x, int y) { return x / y; }
int StandardLib::mod(int x, int y) { return x % y; }
int StandardLib::min(int x, int y) { return (x < y) ? x : y; }
int StandardLib::max(int x, int y) { return (x > y) ? x : y; }
int StandardLib::pow(int x, int y) { int r = 1; for (int i = 0; i < y; ++i) r *= x; return r; }
int StandardLib::abs(int x) { return (x < 0) ? -x : x; }
int StandardLib::clamp(int x, int min, int max) { if (x < min) return min; if (x > max) return max; return x; }
int StandardLib::is_even(int x) { return x % 2 == 0 ? 1 : 0; }
int StandardLib::factorial(int x) { return (x <= 1) ? 1 : x * factorial(x - 1); }

// === Token ===
struct Token {
    std::string type, value;
    Token(std::string t, std::string v) : type(std::move(t)), value(std::move(v)) {}
};

// === 1. MemoryHandler ===
class MemoryHandler {
public:
    static void* allocate(std::size_t size) { return std::malloc(size); }
    static void free(void* ptr) { std::free(ptr); }
    static void gc_collect() { /* TODO: mark-and-sweep */ }
};

// === 2. RangeAdjuster ===
class RangeAdjuster {
public:
    static int clamp(int v, int mn, int mx) { return (v < mn) ? mn : (v > mx) ? mx : v; }
    static double normalize(double v, double in_min, double in_max, double out_min, double out_max) {
        double ratio = (v - in_min) / (in_max - in_min);
        return out_min + ratio * (out_max - out_min);
    }
};

// === 3. ErrorHandler ===
class ErrorHandler {
public:
    static void error(int code, const std::string& msg) { throw std::runtime_error("[Error " + std::to_string(code) + "]: " + msg); }
    static void warn(const std::string& msg) { std::cerr << "[Warning]: " << msg << std::endl; }
    static void info(const std::string& msg) { std::cout << "[Info]: " << msg << std::endl; }
};

// === 4. Indexter ===
struct AST;
class Indexter {
public:
    static std::unordered_map<std::string, int> indexSymbols(const AST&) { return {}; }
};

// === 5. IO ===
class IO {
public:
    static std::string readFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) ErrorHandler::error(2, "File not found: " + path);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    static void writeFile(const std::string& path, const std::string& data) {
        std::ofstream out(path); out << data;
    }
    static void print(const std::string& t) { std::cout << t; }
    static void println(const std::string& t) { std::cout << t << std::endl; }
};

// === 6. Filer ===
class Filer {
public:
    static std::string loadModule(const std::string& name) {
        return IO::readFile(name + ".qtr");
    }
};

// === 7. LibrarySystem ===
class LibrarySystem {
    static std::unordered_map<std::string, void*>& libs() {
        static std::unordered_map<std::string, void*> l;
        return l;
    }
public:
    static void registerLibrary(const std::string& name, void* module) { libs()[name] = module; }
    static void* getLibrary(const std::string& name) {
        auto it = libs().find(name);
        if (it == libs().end()) ErrorHandler::error(404, "Library not found: " + name);
        return it->second;
    }
};

// === 8. Lexer ===
class Lexer {
public:
    static std::vector<Token> lex(const std::string& input) {
        std::vector<Token> tokens;
        for (size_t i = 0; i < input.size(); ) {
            while (i < input.size() && std::isspace(input[i])) ++i;
            if (i == input.size()) break;
            if (std::isalpha(input[i])) {
                size_t j = i;
                while (j < input.size() && (std::isalnum(input[j]) || input[j]=='_')) ++j;
                tokens.emplace_back("IDENT", input.substr(i, j-i));
                i = j;
            } else if (std::isdigit(input[i])) {
                size_t j = i;
                while (j < input.size() && std::isdigit(input[j])) ++j;
                tokens.emplace_back("NUMBER", input.substr(i, j-i));
                i = j;
            } else {
                tokens.emplace_back("SYM", std::string(1, input[i++]));
            }
        }
        return tokens;
    }
};

// === 9. SyntaxHighlighter ===
struct StyledToken { Token token; std::string style; };
class SyntaxHighlighter {
public:
    static std::vector<StyledToken> highlight(const std::vector<Token>& tokens) {
        std::vector<StyledToken> out;
        for (const auto& t : tokens) out.push_back({t, "plain"});
        return out;
    }
};

// === 10. Formatter ===
struct ASTNode;
struct AST;
class Formatter {
public:
    static std::string format(const AST&); // See below
};

// === 11. Parser (with AST) ===
struct ASTNode { virtual ~ASTNode() = default; };
struct ASTLiteral : ASTNode { std::string value; ASTLiteral(std::string v): value(std::move(v)) {} };
struct ASTVar : ASTNode { std::string name; ASTVar(std::string n): name(std::move(n)) {} };
struct ASTBinary : ASTNode { std::string lhs, op, rhs; ASTBinary(std::string l, std::string o, std::string r): lhs(std::move(l)), op(std::move(o)), rhs(std::move(r)) {} };
struct ASTValDecl : ASTNode { std::string name, type; std::unique_ptr<ASTNode> init; };
struct ASTCall : ASTNode { std::string funcName; std::vector<std::unique_ptr<ASTNode>> args; ASTCall(std::string f): funcName(std::move(f)) {} };
struct ASTFunction : ASTNode { std::string name; std::vector<std::string> params; std::vector<std::unique_ptr<ASTNode>> body; };
struct AST : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> nodes;
    std::unordered_map<std::string, ASTFunction*> functions;
};
std::unique_ptr<ASTNode> parseExpr(std::istringstream&);
std::unique_ptr<ASTCall> parseCall(const std::string& funcName, std::istringstream& in) {
    auto call = std::make_unique<ASTCall>(funcName); char ch; in >> ch; // (
    while (in.peek() != ')' && in >> std::ws) {
        call->args.push_back(parseExpr(in));
        if (in.peek() == ',') in.ignore(1);
    }
    in.ignore(1); // )
    return call;
}
std::unique_ptr<ASTNode> parseExpr(std::istringstream& in) {
    std::string token; in >> token;
    if (std::isdigit(token[0])) return std::make_unique<ASTLiteral>(token);
    if (std::isalpha(token[0])) {
        if (in.peek() == '(') return parseCall(token, in);
        std::string op, rhs; std::streampos oldpos = in.tellg();
        if (in >> op) {
            if (op == "+" || op == "-" || op == "*" || op == "/") { in >> rhs;
                return std::make_unique<ASTBinary>(token, op, rhs);
            } else in.seekg(oldpos);
        }
        return std::make_unique<ASTVar>(token);
    }
    return std::make_unique<ASTLiteral>(token);
}
std::unique_ptr<ASTFunction> parseFunction(std::istringstream& in) {
    auto func = std::make_unique<ASTFunction>();
    std::string name, param; in >> name; func->name = name; in >> param; // (
    while (in.peek() != ')' && in >> std::ws) { in >> param; if (param.back()==',') param.pop_back(); func->params.push_back(param); }
    in.ignore(1); std::string brace; in >> brace; std::string tok;
    while (in >> tok && tok != "}") {
        if (tok == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            func->body.push_back(std::move(decl));
        } else if (tok == "call") {
            std::string fname; in >> fname; func->body.push_back(parseCall(fname, in));
        }
    }
    return func;
}
std::unique_ptr<AST> parse(const std::string& src) {
    auto ast = std::make_unique<AST>(); std::istringstream in(src); std::string tok;
    while (in >> tok) {
        if (tok == "val") {
            auto decl = std::make_unique<ASTValDecl>();
            in >> decl->name; in.ignore(1); in >> decl->type; in.ignore(1); decl->init = parseExpr(in);
            ast->nodes.push_back(std::move(decl));
        } else if (tok == "func") {
            auto func = parseFunction(in);
            ast->functions[func->name] = func.get();
            ast->nodes.push_back(std::move(func));
        } else if (tok == "call") {
            std::string fname; in >> fname; ast->nodes.push_back(parseCall(fname, in));
        }
    }
    return ast;
}

// === 12. Formatter (AST Pretty-Printer) ===
std::string Formatter::format(const AST& ast) {
    std::ostringstream out;
    for (const auto& node : ast.nodes) {
        if (auto decl = dynamic_cast<ASTValDecl*>(node.get()))
            out << "val " << decl->name << " : " << decl->type << " = ...\n";
        else if (auto call = dynamic_cast<ASTCall*>(node.get()))
            out << "call " << call->funcName << "(...)\n";
        else if (auto func = dynamic_cast<ASTFunction*>(node.get()))
            out << "func " << func->name << "(...)\n";
    }
    return out.str();
}

// === 13. IRBytecode ===
struct IR { std::string opcode; std::vector<std::string> args; };
class IRBytecode {
public:
    static std::vector<IR> generate(const AST&) { return {}; }
};

// === 14. Encapsulation ===
class Encapsulation {
public:
    static std::string wrap(const std::string& m) { return "<module>" + m + "</module>"; }
};

// === 15. Scoper ===
class Scoper {
public:
    static void resolveScopes(AST&) {}
};

// === 16. Binder ===
class Binder {
public:
    static void bind(AST&) {}
};

// === 17. Adapter ===
class Adapter {
public:
    static std::vector<IR> adaptToIR(const AST& ast) { return IRBytecode::generate(ast); }
};

// === 18. Composer ===
class Composer {
public:
    static std::vector<IR> compose(const std::vector<std::vector<IR>>& irs) {
        std::vector<IR> out;
        for (const auto& v : irs) out.insert(out.end(), v.begin(), v.end());
        return out;
    }
};

// === 19. Seeder ===
class Seeder {
public:
    static std::vector<std::string> seedStdlib() { return {"core", "math"}; }
};

// === 20. CodeGenerator ===
class CodeGenerator {
public:
    static std::string generateNASM(const std::vector<IR>& irs) {
        std::string out = "section .text\n";
        for (const auto& ir : irs) out += ir.opcode + " ...\n";
        return out;
    }
};

// === 21. BinaryEmitter ===
class BinaryEmitter {
public:
    static std::string emit(const std::string&) { return "out.bin"; }
};

// === 22. StandardLib ===
class StandardLib {
public:
    static void registerBuiltins() {}
    static int print(int x) { std::cout << x << std::endl; return x; }
    static int add(int x, int y) { return x + y; }
    static int mul(int x, int y) { return x * y; }
    static int identity(int x) { return x; }
    static int compare(int x, int y) { std::cout << x << " " << y << std::endl; return 0; }
    static int sub(int x, int y) { return x - y; }
    static int div(int x, int y) { return x / y; }
    static int mod(int x, int y) { return x % y; }
    static int min(int x, int y) { return (x < y) ? x : y; }
    static int max(int x, int y) { return (x > y) ? x : y; }
    static int pow(int x, int y) { int r = 1; for (int i=0; i<y; ++i) r *= x; return r; }
    static int abs(int x) { return (x < 0) ? -x : x; }
    static int clamp(int x, int mn, int mx) { return (x < mn) ? mn : (x > mx) ? mx : x; }
    static int is_even(int x) { return x % 2 == 0 ? 1 : 0; }
    static int factorial(int x) { return (x <= 1) ? 1 : x * factorial(x - 1); }
};

// ==== MAIN (demo parse/run workflow) ====
int main(int argc, char** argv) {
    std::string src = argc > 1 ? IO::readFile(argv[1]) : "func print(x) { call say(x) } call print(123)";
    try {
        auto ast = parse(src);
        std::cout << Formatter::format(*ast) << std::endl;
        // ... Further pipeline: IR, codegen, etc.
        StandardLib::print(42);
    } catch (const std::exception& ex) {
        ErrorHandler::error(900, ex.what());
    }
}

int main() {
    std::cout << "🔧 QuarterLang Unified Runtime Demo\n";

    // 🕹️ 1. Embedded/Game Scripting
    QTRScriptEngine engine;
    engine.bindFunction("movePlayer", []() {
        std::cout << "🚀 Player moved!\n";
    });
    engine.loadScript(R"(
        star
        say "Game script running..."
        movePlayer()
        end
    )");
    engine.execute();

    // 🎨 2. Creative Coding Canvas
    QTRCreativeCanvas canvas;
    canvas.setOutputStream(std::cout);
    canvas.runSketch(R"(
        star
        say "🎨 Drawing a circle..."
        draw_circle 100 100 50
        end
    )");

    // 🔟 3. DG Math Engine
    DGEngine dg;
    std::string dgVal = dg.toDG(1234);
    std::cout << "🔢 Decimal 1234 → DG: " << dgVal << "\n";
    std::cout << "🔁 DG " << dgVal << " → Decimal: " << dg.fromDG(dgVal) << "\n";
    std::cout << "➕ DG Add (9A1 + 1B): " << dg.addDG("9A1", "1B") << "\n";

    return 0;
}

class QTRScriptEngine {
public:
    void loadScript(const std::string& qtrSource);
    void execute();
    void bindFunction(const std::string& name, std::function<void()> fn);
private:
    std::string compiledAssembly;
    std::unordered_map<std::string, std::function<void()>> nativeBindings;
};

class QTRCreativeCanvas {
public:
    void runSketch(const std::string& qtrSketchCode);
    void setOutputStream(std::ostream& os);
private:
    std::ostream* output = &std::cout;
};

class DGEngine {
public:
    std::string toDG(int decimal);
    int fromDG(const std::string& dg);
    std::string addDG(const std::string& a, const std::string& b);
};

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "test") {
        test_ir_lowering();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "repl") {
        repl();
        return 0;
    }
    // ... normal compilation flow ...
}
