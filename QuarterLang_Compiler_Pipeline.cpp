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

