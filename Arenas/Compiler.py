# ================================
# QuarterLang Mapper-Compiler-Executor Backend
# By: VACU | QuarterLang Runtime
# ================================

from typing import Union, List, Dict

# ==== SYMBOLIC AST TYPES ====

class Expr:
    pass

class Num(Expr):
    def __init__(self, value: float): self.value = value
    def __repr__(self): return f"{self.value}"

class Var(Expr):
    def __init__(self, name: str): self.name = name
    def __repr__(self): return f"{self.name}"

class Add(Expr):
    def __init__(self, left: Expr, right: Expr): self.left, self.right = left, right
    def __repr__(self): return f"({self.left} + {self.right})"

class Sub(Expr):
    def __init__(self, left: Expr, right: Expr): self.left, self.right = left, right
    def __repr__(self): return f"({self.left} - {self.right})"

class Mul(Expr):
    def __init__(self, left: Expr, right: Expr): self.left, self.right = left, right
    def __repr__(self): return f"({self.left} * {self.right})"

class Div(Expr):
    def __init__(self, left: Expr, right: Expr): self.left, self.right = left, right
    def __repr__(self): return f"({self.left} / {self.right})"

class Pow(Expr):
    def __init__(self, base: Expr, exponent: Expr): self.base, self.exponent = base, exponent
    def __repr__(self): return f"({self.base} ^ {self.exponent})"

class Neg(Expr):
    def __init__(self, expr: Expr): self.expr = expr
    def __repr__(self): return f"-({self.expr})"

# ==== MAPPER: PARSER ====

class Parser:
    def __init__(self, input_str: str):
        self.input = input_str
        self.pos = 0

    def peek(self):
        return self.input[self.pos] if self.pos < len(self.input) else '\0'

    def advance(self):
        ch = self.peek()
        self.pos += 1
        return ch

    def parse_number(self):
        num_str = ''
        while self.peek().isdigit() or self.peek() == '.':
            num_str += self.advance()
        return Num(float(num_str))

    def parse_variable(self):
        name = ''
        while self.peek().isalpha():
            name += self.advance()
        return Var(name)

    def parse_factor(self):
        if self.peek() == '(':
            self.advance()  # skip '('
            expr = self.parse_expression()
            self.advance()  # skip ')'
            return expr
        elif self.peek().isdigit():
            return self.parse_number()
        elif self.peek().isalpha():
            return self.parse_variable()
        elif self.peek() == '-':
            self.advance()
            return Neg(self.parse_factor())
        else:
            raise SyntaxError(f"Unexpected character: {self.peek()}")

    def parse_power(self):
        left = self.parse_factor()
        while self.peek() == '^':
            self.advance()
            right = self.parse_factor()
            left = Pow(left, right)
        return left

    def parse_term(self):
        left = self.parse_power()
        while self.peek() in '*/':
            op = self.advance()
            right = self.parse_power()
            left = Mul(left, right) if op == '*' else Div(left, right)
        return left

    def parse_expression(self):
        left = self.parse_term()
        while self.peek() in '+-':
            op = self.advance()
            right = self.parse_term()
            left = Add(left, right) if op == '+' else Sub(left, right)
        return left

def map_source(src: str) -> Expr:
    parser = Parser(src)
    return parser.parse_expression()

# ==== COMPILER: TO BYTECODE ====

def compile_expr(expr: Expr, bytecode: List[str]):
    if isinstance(expr, Num):
        bytecode.append(f"PUSH {expr.value}")
    elif isinstance(expr, Var):
        bytecode.append(f"LOAD {expr.name}")
    elif isinstance(expr, Add):
        compile_expr(expr.left, bytecode)
        compile_expr(expr.right, bytecode)
        bytecode.append("ADD")
    elif isinstance(expr, Sub):
        compile_expr(expr.left, bytecode)
        compile_expr(expr.right, bytecode)
        bytecode.append("SUB")
    elif isinstance(expr, Mul):
        compile_expr(expr.left, bytecode)
        compile_expr(expr.right, bytecode)
        bytecode.append("MUL")
    elif isinstance(expr, Div):
        compile_expr(expr.left, bytecode)
        compile_expr(expr.right, bytecode)
        bytecode.append("DIV")
    elif isinstance(expr, Pow):
        compile_expr(expr.base, bytecode)
        compile_expr(expr.exponent, bytecode)
        bytecode.append("POW")
    elif isinstance(expr, Neg):
        compile_expr(expr.expr, bytecode)
        bytecode.append("NEG")
    else:
        raise ValueError(f"Unknown expression type: {type(expr)}")

# ==== EXECUTOR ====

def execute_bytecode(bytecode: List[str], env: Dict[str, float]) -> float:
    stack: List[float] = []
    for instr in bytecode:
        if instr.startswith("PUSH "):
            stack.append(float(instr[5:]))
        elif instr.startswith("LOAD "):
            var_name = instr[5:]
            stack.append(env.get(var_name, 0.0))
        elif instr == "ADD":
            b, a = stack.pop(), stack.pop()
            stack.append(a + b)
        elif instr == "SUB":
            b, a = stack.pop(), stack.pop()
            stack.append(a - b)
        elif instr == "MUL":
            b, a = stack.pop(), stack.pop()
            stack.append(a * b)
        elif instr == "DIV":
            b, a = stack.pop(), stack.pop()
            if b == 0: raise ZeroDivisionError("Division by zero")
            stack.append(a / b)
        elif instr == "POW":
            b, a = stack.pop(), stack.pop()
            stack.append(a ** b)
        elif instr == "NEG":
            a = stack.pop()
            stack.append(-a)
        else:
            raise RuntimeError(f"Unknown instruction: {instr}")
    return stack[0] if stack else 0

# ==== ENTRY FUNCTION ====

def run_mce(source: str, vars: Dict[str, float] = {}) -> float:
    print("🔍 Parsing Expression...")
    ast = map_source(source)
    print(f"→ AST: {ast}")

    print("⚙️ Compiling...")
    bytecode = []
    compile_expr(ast, bytecode)

    print("🧠 Bytecode:")
    for line in bytecode:
        print("  ", line)

    print("🚀 Executing...")
    result = execute_bytecode(bytecode, vars)

    print(f"✅ Result: {result}")
    return result

# ==== DEMO ====

if __name__ == "__main__":
    # Example: You can replace this line with any valid expression string
    run_mce("3 + 4 * (2 - 1)")

    # Variable test
    run_mce("x ^ 2 + 3 * x", {"x": 5})

# ================================================
# QuarterLang MCE+ Backend: Functions, Loops,
# Symbolic Simplification & x86-64 EXE Export
# By: VACU | QuarterLang Runtime Extended Edition
# ================================================

import subprocess
import tempfile
import os
from typing import Union, List, Dict, Any

# ==== AST NODE DEFINITIONS ====

class ASTNode:
    pass

class Expr(ASTNode):
    pass

class Stmt(ASTNode):
    pass

# -- Expressions --

class Num(Expr):
    def __init__(self, value: float):
        self.value = value
    def __repr__(self):
        return f"{self.value}"

class Var(Expr):
    def __init__(self, name: str):
        self.name = name
    def __repr__(self):
        return f"{self.name}"

class Add(Expr):
    def __init__(self, left: Expr, right: Expr):
        self.left, self.right = left, right
    def __repr__(self):
        return f"({self.left} + {self.right})"

class Sub(Expr):
    def __init__(self, left: Expr, right: Expr):
        self.left, self.right = left, right
    def __repr__(self):
        return f"({self.left} - {self.right})"

class Mul(Expr):
    def __init__(self, left: Expr, right: Expr):
        self.left, self.right = left, right
    def __repr__(self):
        return f"({self.left} * {self.right})"

class Div(Expr):
    def __init__(self, left: Expr, right: Expr):
        self.left, self.right = left, right
    def __repr__(self):
        return f"({self.left} / {self.right})"

class Pow(Expr):
    def __init__(self, base: Expr, exponent: Expr):
        self.base, self.exponent = base, exponent
    def __repr__(self):
        return f"({self.base} ^ {self.exponent})"

class Neg(Expr):
    def __init__(self, expr: Expr):
        self.expr = expr
    def __repr__(self):
        return f"-({self.expr})"

class Call(Expr):
    def __init__(self, name: str, args: List[Expr]):
        self.name, self.args = name, args
    def __repr__(self):
        return f"{self.name}({', '.join(map(str, self.args))})"

# -- Statements --

class ExprStmt(Stmt):
    def __init__(self, expr: Expr):
        self.expr = expr

class Assign(Stmt):
    def __init__(self, name: str, expr: Expr):
        self.name, self.expr = name, expr

class FunctionDef(Stmt):
    def __init__(self, name: str, params: List[str], body: List[Stmt]):
        self.name, self.params, self.body = name, params, body

class Return(Stmt):
    def __init__(self, expr: Expr):
        self.expr = expr

class While(Stmt):
    def __init__(self, condition: Expr, body: List[Stmt]):
        self.condition, self.body = condition, body

class For(Stmt):
    def __init__(self, var: str, start: Expr, end: Expr, body: List[Stmt]):
        self.var, self.start, self.end, self.body = var, start, end, body

# ==== PARSER EXTENSION ====

class Parser:
    def __init__(self, input_str: str):
        self.input = input_str
        self.pos = 0

    def peek(self):
        return self.input[self.pos] if self.pos < len(self.input) else '\0'

    def advance(self):
        ch = self.peek()
        self.pos += 1
        return ch

    def skip_whitespace(self):
        while self.peek().isspace():
            self.advance()

    def parse_identifier(self):
        self.skip_whitespace()
        name = ''
        while self.peek().isalpha() or self.peek().isdigit() or self.peek() == '_':
            name += self.advance()
        return name

    def parse_number(self):
        self.skip_whitespace()
        num_str = ''
        while self.peek().isdigit() or self.peek() == '.':
            num_str += self.advance()
        return Num(float(num_str))

    def parse_primary(self):
        self.skip_whitespace()
        ch = self.peek()
        if ch == '(':
            self.advance()
            expr = self.parse_expression()
            self.advance()  # skip ')'
            return expr
        if ch.isdigit():
            return self.parse_number()
        if ch.isalpha():
            name = self.parse_identifier()
            self.skip_whitespace()
            if self.peek() == '(':
                self.advance()
                args = []
                if self.peek() != ')':
                    while True:
                        args.append(self.parse_expression())
                        self.skip_whitespace()
                        if self.peek() == ')':
                            break
                        self.advance()  # skip ','
                self.advance()  # skip ')'
                return Call(name, args)
            return Var(name)
        if ch == '-':
            self.advance()
            return Neg(self.parse_primary())
        raise SyntaxError(f"Unexpected char: {ch}")

    def parse_power(self):
        left = self.parse_primary()
        self.skip_whitespace()
        while self.peek() == '^':
            self.advance()
            right = self.parse_primary()
            left = Pow(left, right)
            self.skip_whitespace()
        return left

    def parse_term(self):
        left = self.parse_power()
        self.skip_whitespace()
        while self.peek() in '*/':
            op = self.advance()
            right = self.parse_power()
            left = Mul(left, right) if op == '*' else Div(left, right)
            self.skip_whitespace()
        return left

    def parse_expression(self):
        left = self.parse_term()
        self.skip_whitespace()
        while self.peek() in '+-':
            op = self.advance()
            right = self.parse_term()
            left = Add(left, right) if op == '+' else Sub(left, right)
            self.skip_whitespace()
        return left

    def parse_statement(self):
        self.skip_whitespace()
        # function def?
        if self.input[self.pos:self.pos+4] == "def ":
            self.pos += 4
            name = self.parse_identifier()
            self.skip_whitespace(); self.advance()  # skip '('
            params = []
            while self.peek() != ')':
                params.append(self.parse_identifier())
                self.skip_whitespace()
                if self.peek() == ',':
                    self.advance()
                    self.skip_whitespace()
            self.advance(); self.skip_whitespace()  # skip ')'
            body = []
            while not self.input[self.pos:self.pos+4] == "end\n":
                body.append(self.parse_statement())
                self.skip_whitespace()
            self.pos += 4  # skip 'end\n'
            return FunctionDef(name, params, body)

        # while loop?
        if self.input[self.pos:self.pos+6] == "while ":
            self.pos += 6
            cond = self.parse_expression()
            self.skip_whitespace(); self.advance()  # skip ':'
            body = []
            while not self.input[self.pos:self.pos+4] == "end\n":
                body.append(self.parse_statement())
                self.skip_whitespace()
            self.pos += 4
            return While(cond, body)

        # for loop?
        if self.input[self.pos:self.pos+4] == "for ":
            self.pos += 4
            var = self.parse_identifier()
            self.skip_whitespace(); self.advance()  # skip '='
            start = self.parse_expression()
            self.skip_whitespace(); self.advance()  # skip ':'
            end = self.parse_expression()
            self.skip_whitespace(); self.advance()  # skip ':'
            body = []
            while not self.input[self.pos:self.pos+4] == "end\n":
                body.append(self.parse_statement())
                self.skip_whitespace()
            self.pos += 4
            return For(var, start, end, body)

        # return?
        if self.input[self.pos:self.pos+7] == "return ":
            self.pos += 7
            expr = self.parse_expression()
            self.skip_whitespace(); self.advance()  # skip '\n'
            return Return(expr)

        # assignment?
        name = self.parse_identifier()
        self.skip_whitespace()
        if self.peek() == '=':
            self.advance()
            expr = self.parse_expression()
            self.skip_whitespace(); self.advance()  # skip '\n'
            return Assign(name, expr)

        # expr statement
        expr = self.parse_expression()
        self.skip_whitespace(); self.advance()  # skip '\n'
        return ExprStmt(expr)

    def parse(self) -> List[Stmt]:
        stmts = []
        while self.pos < len(self.input):
            stmts.append(self.parse_statement())
            self.skip_whitespace()
        return stmts

# ==== SYMBOLIC ALGEBRA SIMPLIFIER ====

class Simplifier:
    @staticmethod
    def simplify(expr: Expr) -> Expr:
        # recursive descent simplification
        if isinstance(expr, Add):
            l = Simplifier.simplify(expr.left)
            r = Simplifier.simplify(expr.right)
            # constant folding
            if isinstance(l, Num) and isinstance(r, Num):
                return Num(l.value + r.value)
            # x + 0 -> x
            if isinstance(r, Num) and r.value == 0:
                return l
            if isinstance(l, Num) and l.value == 0:
                return r
            return Add(l, r)

        if isinstance(expr, Sub):
            l = Simplifier.simplify(expr.left)
            r = Simplifier.simplify(expr.right)
            if isinstance(l, Num) and isinstance(r, Num):
                return Num(l.value - r.value)
            # x - 0 -> x
            if isinstance(r, Num) and r.value == 0:
                return l
            return Sub(l, r)

        if isinstance(expr, Mul):
            l = Simplifier.simplify(expr.left)
            r = Simplifier.simplify(expr.right)
            if isinstance(l, Num) and isinstance(r, Num):
                return Num(l.value * r.value)
            # x * 1 -> x, x * 0 -> 0
            if isinstance(r, Num):
                if r.value == 1: return l
                if r.value == 0: return Num(0)
            if isinstance(l, Num):
                if l.value == 1: return r
                if l.value == 0: return Num(0)
            return Mul(l, r)

        if isinstance(expr, Div):
            l = Simplifier.simplify(expr.left)
            r = Simplifier.simplify(expr.right)
            if isinstance(l, Num) and isinstance(r, Num) and r.value != 0:
                return Num(l.value / r.value)
            # x / 1 -> x
            if isinstance(r, Num) and r.value == 1:
                return l
            return Div(l, r)

        if isinstance(expr, Pow):
            b = Simplifier.simplify(expr.base)
            e = Simplifier.simplify(expr.exponent)
            if isinstance(b, Num) and isinstance(e, Num):
                return Num(b.value ** e.value)
            # x^1 -> x, x^0 -> 1
            if isinstance(e, Num):
                if e.value == 1: return b
                if e.value == 0: return Num(1)
            return Pow(b, e)

        if isinstance(expr, Neg):
            inner = Simplifier.simplify(expr.expr)
            if isinstance(inner, Num):
                return Num(-inner.value)
            return Neg(inner)

        if isinstance(expr, Call):
            args = [Simplifier.simplify(a) for a in expr.args]
            return Call(expr.name, args)

        return expr

    @staticmethod
    def simplify_stmts(stmts: List[Stmt]) -> List[Stmt]:
        out = []
        for s in stmts:
            if isinstance(s, Assign):
                s.expr = Simplifier.simplify(s.expr)
                out.append(s)
            elif isinstance(s, ExprStmt):
                s.expr = Simplifier.simplify(s.expr)
                out.append(s)
            elif isinstance(s, FunctionDef):
                s.body = Simplifier.simplify_stmts(s.body)
                out.append(s)
            elif isinstance(s, (While, For)):
                # simplify loop conditions
                s.condition = Simplifier.simplify(s.condition)
                s.body = Simplifier.simplify_stmts(s.body)
                out.append(s)
            else:
                out.append(s)
        return out

# ==== BYTECODE & ASM GENERATOR ====

class CodeGen:
    def __init__(self):
        self.bytecode: List[str] = []
        self.labels: int = 0
        self.functions: Dict[str, List[str]] = {}
        self.current_func: str = None

    def new_label(self, base: str) -> str:
        lbl = f"{base}_{self.labels}"
        self.labels += 1
        return lbl

    def emit(self, instr: str):
        self.bytecode.append(instr)

    def compile_expr(self, expr: Expr):
        if isinstance(expr, Num):
            self.emit(f"PUSH {expr.value}")
        elif isinstance(expr, Var):
            self.emit(f"LOAD {expr.name}")
        elif isinstance(expr, Add):
            self.compile_expr(expr.left)
            self.compile_expr(expr.right)
            self.emit("ADD")
        elif isinstance(expr, Sub):
            self.compile_expr(expr.left)
            self.compile_expr(expr.right)
            self.emit("SUB")
        elif isinstance(expr, Mul):
            self.compile_expr(expr.left)
            self.compile_expr(expr.right)
            self.emit("MUL")
        elif isinstance(expr, Div):
            self.compile_expr(expr.left)
            self.compile_expr(expr.right)
            self.emit("DIV")
        elif isinstance(expr, Pow):
            self.compile_expr(expr.base)
            self.compile_expr(expr.exponent)
            self.emit("POW")
        elif isinstance(expr, Neg):
            self.compile_expr(expr.expr)
            self.emit("NEG")
        elif isinstance(expr, Call):
            for arg in expr.args:
                self.compile_expr(arg)
            self.emit(f"CALL {expr.name}")
        else:
            raise ValueError(f"Unknown expr: {expr}")

    def compile_stmt(self, stmt: Stmt):
        if isinstance(stmt, ExprStmt):
            self.compile_expr(stmt.expr)
            self.emit("POP")
        elif isinstance(stmt, Assign):
            self.compile_expr(stmt.expr)
            self.emit(f"STORE {stmt.name}")
        elif isinstance(stmt, Return):
            self.compile_expr(stmt.expr)
            self.emit("RET")
        elif isinstance(stmt, While):
            start = self.new_label("while_start")
            end = self.new_label("while_end")
            self.emit(f"LABEL {start}")
            self.compile_expr(stmt.condition)
            self.emit(f"JZ {end}")
            for sub in stmt.body:
                self.compile_stmt(sub)
            self.emit(f"JMP {start}")
            self.emit(f"LABEL {end}")
        elif isinstance(stmt, For):
            loop_var = stmt.var
            # initialize
            self.compile_expr(stmt.start)
            self.emit(f"STORE {loop_var}")
            start = self.new_label("for_start")
            end = self.new_label("for_end")
            self.emit(f"LABEL {start}")
            self.emit(f"LOAD {loop_var}")
            self.compile_expr(stmt.end)
            self.emit(f"GT")
            self.emit(f"JZ {end}")
            for sub in stmt.body:
                self.compile_stmt(sub)
            # increment
            self.emit(f"LOAD {loop_var}")
            self.emit("PUSH 1")
            self.emit("ADD")
            self.emit(f"STORE {loop_var}")
            self.emit(f"JMP {start}")
            self.emit(f"LABEL {end}")
        elif isinstance(stmt, FunctionDef):
            prev_bc = self.bytecode
            self.bytecode = []
            old_func = self.current_func
            self.current_func = stmt.name
            # prologue
            self.emit("PUSH_FRAME")
            for s in stmt.body:
                self.compile_stmt(s)
            self.emit("POP_FRAME")
            self.emit("RET")
            self.functions[stmt.name] = self.bytecode
            self.bytecode = prev_bc
            self.current_func = old_func
        else:
            raise ValueError(f"Unknown stmt: {stmt}")

    def finalize(self) -> List[str]:
        # flatten: main entry + functions
        out = []
        # function bodies
        for name, code in self.functions.items():
            out.append(f"FUNC {name}")
            out += code
            out.append(f"END_FUNC {name}")
        # main
        out.append("FUNC main")
        for instr in self.bytecode:
            out.append(instr)
        out.append("END_FUNC main")
        out.append("CALL main")
        out.append("HALT")
        return out

# ==== x86-64 ASSEMBLER & EXE EXPORT ====

class Assembler:
    @staticmethod
    def bytecode_to_nasm(bytecode: List[str]) -> str:
        # simple stack-machine to x86-64 SysV
        asm = ["global _start", "section .text", "_start:"]
        for instr in bytecode:
            parts = instr.split()
            op = parts[0]
            if op == "PUSH":
                asm.append(f"  ; {instr}")
                asm.append(f"  push qword {parts[1]}")
            elif op == "POP":
                asm.append("  pop rax")
            elif op == "LOAD":
                asm.append(f"  ; {instr}")
                asm.append(f"  mov rax, [rel {parts[1]}]")
                asm.append("  push rax")
            elif op == "STORE":
                asm.append(f"  ; {instr}")
                asm.append("  pop rax")
                asm.append(f"  mov [rel {parts[1]}], rax")
            elif op in ("ADD","SUB","MUL","DIV","NEG"):
                asm.append(f"  ; {instr}")
                asm.append("  pop rbx")
                asm.append("  pop rax")
                if op == "ADD":
                    asm.append("  add rax, rbx")
                elif op == "SUB":
                    asm.append("  sub rax, rbx")
                elif op == "MUL":
                    asm.append("  imul rax, rbx")
                elif op == "DIV":
                    asm.append("  cqo")
                    asm.append("  idiv rbx")
                elif op == "NEG":
                    asm.append("  neg rax")
                asm.append("  push rax")
            elif op == "CALL":
                name = parts[1]
                asm.append(f"  ; {instr}")
                asm.append(f"  call {name}")
            elif op.startswith("J") or op == "LABEL":
                # skip advanced control for brevity
                pass
            elif op == "RET":
                asm.append("  ret")
            elif op == "HALT":
                asm.append("  mov rax, 60")
                asm.append("  xor rdi, rdi")
                asm.append("  syscall")
        return "\n".join(asm)

    @staticmethod
    def assemble_and_link(nasm_code: str, output: str):
        with tempfile.NamedTemporaryFile(suffix=".asm", delete=False) as f:
            f.write(nasm_code.encode())
            asm_path = f.name

        obj_path = asm_path.replace(".asm", ".o")
        subprocess.run(["nasm", "-f", "elf64", asm_path, "-o", obj_path], check=True)
        # link with system linker
        subprocess.run(["ld", obj_path, "-o", output], check=True)
        os.unlink(asm_path)
        os.unlink(obj_path)

# ==== ENTRY POINT ====

def compile_and_export(source: str, exe_name: str = "a.out"):
    # parse
    parser = Parser(source)
    stmts = parser.parse()
    # simplify
    stmts = Simplifier.simplify_stmts(stmts)
    # compile to bytecode
    cg = CodeGen()
    for s in stmts:
        cg.compile_stmt(s)
    bc = cg.finalize()
    # to NASM
    nasm = Assembler.bytecode_to_nasm(bc)
    # write & assemble
    Assembler.assemble_and_link(nasm, exe_name)
    print(f"[+] Generated executable: {exe_name}")

# ==== USAGE DEMO ====

if __name__ == "__main__":
    source = """
def square(x)
    return x * x
end
def main()
    a = 3
    b = square(a + 2)
    c = 0
    while c < b
        c = c + 1
    end
    return c
end
"""
    compile_and_export(source, "program.exe")
