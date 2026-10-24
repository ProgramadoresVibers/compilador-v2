"""IR de três endereços do MiniC, construída sobre o front-end existente."""

import argparse
import contextlib
import io
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scanner_python.scanner import analisar_arquivo as scan
from parser_python.parser import analisar_tokens, ErroSintatico
from analisador_semantico_python.minic import SemanticAnalyzer


BINARY = {
    "+": "add", "-": "sub", "*": "mul", "/": "div", "%": "mod",
    "<": "lt", ">": "gt", "<=": "le", ">=": "ge", "==": "eq", "!=": "ne",
}
PURE = set(BINARY.values()) | {"mov", "neg", "not", "i2f"}


def number(text):
    if text == "true":
        return 1
    if text == "false":
        return 0
    try:
        return float(text) if any(c in text for c in ".eE") else int(text)
    except ValueError:
        return None


def numeric_text(value, kind):
    if kind == "bool":
        return "true" if value else "false"
    if kind == "float":
        text = format(float(value), ".17g")
        return text if any(c in text for c in ".eE") else text + ".0"
    return str(int(value))


class Generator:
    def __init__(self, tree, optimize=False):
        self.tree = tree
        self.optimize = optimize
        self.code = []
        self.scopes = [{}]
        self.constants = {}
        self.counter = 0
        self.labels = 0
        self.loops = []
        self.return_type = "void"
        self.functions = {
            n["nome"]: n for n in tree["itens"] if n["no"] == "Funcao"
        }

    def emit(self, op, *args):
        self.code.append((op, *args))

    def temporary(self):
        self.counter += 1
        return f"%t{self.counter}"

    def label(self):
        self.labels += 1
        return f"L{self.labels}"

    def bind(self, node):
        self.counter += 1
        prefix = "@g" if len(self.scopes) == 1 else "%v"
        name = f"{prefix}{self.counter}_{node['nome']}"
        kind = node["tipo"] + ("[]" if node.get("vetor") or node.get("tamanho") else "")
        self.scopes[-1][node["nome"]] = (name, kind)
        return name, kind

    def lookup(self, name):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        raise ValueError(f"Identificador sem armazenamento: {name}")

    def cast(self, value, kind):
        operand, source = value
        if kind == "float" and source == "int":
            if self.optimize and number(operand) is not None:
                return numeric_text(number(operand), "float"), kind
            dest = self.temporary()
            self.emit("i2f", dest, operand)
            return dest, kind
        return value

    def save(self, dest, value):
        self.emit("mov", dest, value)
        self.constants.pop(dest, None)
        if self.optimize and number(value) is not None:
            self.constants[dest] = value

    def forget_globals(self):
        self.constants = {k: v for k, v in self.constants.items() if not k.startswith("@")}

    def invalidate(self, node):
        """Kill loop-carried definitions before lowering the loop, including calls."""
        if not isinstance(node, dict):
            return
        if node["no"] == "Chamada":
            self.forget_globals()
        if node["no"] == "Atribuicao" and node["destino"]["no"] == "Identificador":
            try:
                name, _ = self.lookup(node["destino"]["nome"])
                self.constants.pop(name, None)
            except ValueError:
                pass
        for value in node.values():
            if isinstance(value, dict):
                self.invalidate(value)
            elif isinstance(value, list):
                for child in value:
                    self.invalidate(child)

    def expr(self, node):
        tag = node["no"]
        if tag == "Literal":
            kind = "float" if node["tipo"] == "real" else node["tipo"]
            text = node["valor"]
            if kind == "float":
                text = numeric_text(float(text.rstrip("fF")), kind)
            return text, kind
        if tag == "Identificador":
            name, kind = self.lookup(node["nome"])
            if kind.endswith("[]"):
                return name, kind
            if self.optimize and name in self.constants:
                return self.constants[name], kind
            dest = self.temporary()
            self.emit("mov", dest, name)
            return dest, kind
        if tag == "Indice":
            base, index, kind = self.address(node)
            dest = self.temporary()
            self.emit("load", dest, base, index)
            return dest, kind
        if tag == "Atribuicao":
            target = node["destino"]
            if target["no"] == "Indice":
                base, index, kind = self.address(target)
                value, _ = self.cast(self.expr(node["valor"]), kind)
                self.emit("store", base, index, value)
            else:
                dest, kind = self.lookup(target["nome"])
                value, _ = self.cast(self.expr(node["valor"]), kind)
                self.save(dest, value)
            return value, kind
        if tag == "Chamada":
            function = self.functions[node["funcao"]["nome"]]
            values = []
            for arg, formal in zip(node["argumentos"], function["parametros"]):
                value = self.cast(self.expr(arg), formal["tipo"])
                values.append(value[0])
            for value in values:
                self.emit("param", value)
            dest = self.temporary()
            self.emit("call", dest, function["nome"], str(len(values)))
            self.forget_globals()
            return dest, function["tipo"]
        if tag == "Unaria":
            value, kind = self.expr(node["valor"])
            op = "not" if node["operador"] == "!" else "neg"
            if self.optimize and number(value) is not None:
                result = not number(value) if op == "not" else -number(value)
                if kind == "float" or -(2**31) <= result <= 2**31 - 1:
                    return numeric_text(result, kind), kind
            dest = self.temporary()
            self.emit(op, dest, value)
            return dest, kind
        if tag == "Binaria":
            if node["operador"] in ("&&", "||"):
                return self.logical(node)
            left = self.expr(node["esquerda"])
            right = self.expr(node["direita"])
            common = "float" if "float" in (left[1], right[1]) else left[1]
            left = self.cast(left, common)
            right = self.cast(right, common)
            op = BINARY[node["operador"]]
            kind = "bool" if op in {"lt", "gt", "le", "ge", "eq", "ne"} else common
            a, b = left[0], right[0]
            x, y = number(a), number(b)
            if self.optimize and x is not None and y is not None:
                folded = self.fold(op, x, y, kind)
                if folded is not None:
                    return folded, kind
            if self.optimize and kind == "int":
                if (op in {"add", "sub"} and y == 0) or (op in {"mul", "div"} and y == 1):
                    return a, kind
                if (op == "add" and x == 0) or (op == "mul" and x == 1):
                    return b, kind
            dest = self.temporary()
            self.emit(op, dest, a, b)
            return dest, kind
        raise ValueError(f"Expressão não suportada: {tag}")

    @staticmethod
    def fold(op, x, y, kind):
        if op in {"div", "mod"} and (y == 0 or x == -(2**31) and y == -1):
            return None
        quotient = 0
        if op == "mod" or op == "div" and kind == "int":
            quotient = int(x / y)
        operations = {
            "add": lambda: x + y, "sub": lambda: x - y, "mul": lambda: x * y,
            "div": lambda: x / y if kind == "float" else quotient,
            "mod": lambda: x - quotient * y,
            "lt": lambda: x < y, "gt": lambda: x > y,
            "le": lambda: x <= y, "ge": lambda: x >= y,
            "eq": lambda: x == y, "ne": lambda: x != y,
        }
        value = operations[op]()
        if not math.isfinite(value) or kind == "int" and not -(2**31) <= value <= 2**31 - 1:
            return None
        return numeric_text(value, kind)

    def address(self, node):
        base, kind = self.expr(node["base"])
        index, _ = self.expr(node["indice"])
        return base, index, kind[:-2]

    def logical(self, node):
        left, _ = self.expr(node["esquerda"])
        is_and = node["operador"] == "&&"
        known = number(left)
        if self.optimize and known is not None:
            if bool(known) != is_and:
                return ("false" if is_and else "true"), "bool"
            return self.expr(node["direita"])
        dest, end = self.temporary(), self.label()
        self.emit("mov", dest, "false" if is_and else "true")
        self.emit("brfalse" if is_and else "brtrue", left, end)
        self.constants.clear()
        right, _ = self.expr(node["direita"])
        self.emit("mov", dest, right)
        self.emit("label", end)
        self.constants.clear()
        return dest, "bool"

    def statement(self, node):
        if node is None:
            return False
        tag = node["no"]
        if tag == "Bloco":
            self.scopes.append({})
            stopped = self.block(node)
            self.scopes.pop()
            return stopped
        if tag == "Declaracao":
            name, kind = self.bind(node)
            size = node.get("tamanho")
            self.emit("alloc", name, kind, size["valor"] if size else "1")
            if node.get("valor"):
                value, _ = self.cast(self.expr(node["valor"]), kind)
                self.save(name, value)
        elif tag == "ComandoExpressao":
            self.expr(node["valor"])
        elif tag == "Imprimir":
            for arg in node["argumentos"]:
                value, _ = self.expr(arg)
                self.emit("print", value)
        elif tag == "Ler":
            self.emit("read")
        elif tag == "Retorno":
            if node.get("valor"):
                value, _ = self.cast(self.expr(node["valor"]), self.return_type)
                self.emit("ret", value)
            else:
                self.emit("ret")
            return True
        elif tag in {"Break", "Continue"}:
            self.emit("jmp", self.loops[-1][0 if tag == "Break" else 1])
            return True
        elif tag == "Se":
            cond, _ = self.expr(node["condicao"])
            if self.optimize and number(cond) is not None:
                return self.statement(node["entao"] if number(cond) else node.get("senao"))
            other, end = self.label(), self.label()
            self.emit("brfalse", cond, other)
            self.constants.clear()
            first = self.statement(node["entao"])
            self.emit("jmp", end)
            self.emit("label", other)
            self.constants.clear()
            second = self.statement(node.get("senao"))
            self.emit("label", end)
            self.constants.clear()
            return first and second
        elif tag in {"Enquanto", "Para"}:
            if tag == "Para" and node.get("inicio"):
                self.expr(node["inicio"])
            self.invalidate(node)
            test, step, end = self.label(), self.label(), self.label()
            self.emit("label", test)
            if node.get("condicao"):
                cond, _ = self.expr(node["condicao"])
                self.emit("brfalse", cond, end)
            self.loops.append((end, step))
            self.statement(node["corpo"])
            self.loops.pop()
            self.emit("label", step)
            # Definitions in the body must not leak into the step or next iteration.
            self.invalidate(node)
            if tag == "Para" and node.get("passo"):
                self.expr(node["passo"])
            self.emit("jmp", test)
            self.emit("label", end)
            self.invalidate(node)
        return False

    def block(self, node):
        stopped = False
        for item in node["itens"]:
            if self.optimize and stopped:
                break
            stopped = self.statement(item) or stopped
        return stopped

    def generate(self):
        self.emit("func", "$init", "void")
        for item in self.tree["itens"]:
            if item["no"] != "Funcao":
                self.statement(item)
        self.emit("ret")
        self.emit("endfunc")
        for function in self.functions.values():
            self.constants.clear()
            self.return_type = function["tipo"]
            self.scopes.append({})
            self.emit("func", function["nome"], function["tipo"])
            for formal in function["parametros"]:
                name, kind = self.bind(formal)
                self.emit("arg", name, kind)
            self.block(function["corpo"])
            if function["tipo"] == "void":
                self.emit("ret")
            self.emit("endfunc")
            self.scopes.pop()
        if self.optimize:
            self.cleanup()
        return self.format_code()

    @staticmethod
    def read_start(op):
        return 1 if op in PURE | {"load", "call", "alloc", "arg"} else 0

    def simplify_snapshots(self):
        code = list(self.code)
        for index, instruction in enumerate(code):
            if instruction is None:
                continue
            op, *args = instruction
            if op != "mov" or not args[0].startswith("%t") or not args[1].startswith(("%v", "@g")):
                continue
            dest, source = args
            last_use = index
            for pos in range(index + 1, len(code)):
                if code[pos] is None:
                    continue
                next_op, *operands = code[pos]
                if next_op == "endfunc":
                    break
                if dest in operands[self.read_start(next_op):]:
                    last_use = pos
            safe = True
            for pos in range(index + 1, last_use + 1):
                entry = code[pos]
                if entry is None:
                    continue
                next_op, *operands = entry
                if next_op in {"call", "label", "jmp"} or (
                    next_op in {"brfalse", "brtrue", "ret"} and pos < last_use
                ):
                    safe = False
                    break
                if self.read_start(next_op) and operands[0] == source:
                    safe = False
                    break
            if not safe:
                continue
            code[index] = None
            for pos in range(index + 1, last_use + 1):
                if code[pos] is None:
                    continue
                next_op, *operands = code[pos]
                first = self.read_start(next_op)
                operands[first:] = [source if value == dest else value for value in operands[first:]]
                code[pos] = (next_op, *operands)
        return [entry for entry in code if entry is not None]

    def format_code(self):
        code = self.simplify_snapshots()
        globals_ = {
            args[0]: args[0].split("_", 1)[1]
            for op, *args in code if op == "alloc" and args[0].startswith("@")
        }
        symbols = {value: key for key, value in BINARY.items()}
        output = []
        index = 0
        while index < len(code):
            _, function, _ = code[index]
            end = index + 1
            while code[end][0] != "endfunc":
                end += 1
            body = code[index + 1:end]
            index = end + 1
            if function == "$init" and all(entry[0] == "ret" for entry in body):
                continue
            names = dict(globals_)
            used = set(names.values())
            for op, *args in body:
                if op not in {"alloc", "arg"} or args[0] in names:
                    continue
                base = args[0].split("_", 1)[1]
                if function == "$init" and args[0].startswith("%"):
                    base += "#init"
                name, suffix = base, 2
                while name in used:
                    name = f"{base}#{suffix}"
                    suffix += 1
                names[args[0]] = name
                used.add(name)
            counter = 0

            def display(value):
                nonlocal counter
                if value.startswith("%t") and value not in names:
                    counter += 1
                    while f"t{counter}" in used:
                        counter += 1
                    names[value] = f"t{counter}"
                    used.add(names[value])
                return names.get(value, value)

            formals = [display(args[0]) for op, *args in body if op == "arg"]
            header = f"func {function}"
            if formals:
                header += "(" + ", ".join(formals) + ")"
            output.append(header)
            for op, *args in body:
                values = [display(value) for value in args]
                if op == "arg":
                    continue
                if op == "alloc":
                    if not args[0].startswith("@") and not args[1].endswith("[]"):
                        continue
                    line = "alloc " + " ".join(values)
                elif op == "mov":
                    line = f"{values[0]} = {values[1]}"
                elif op in symbols:
                    line = f"{values[0]} = {values[1]} {symbols[op]} {values[2]}"
                elif op in {"neg", "not", "i2f"}:
                    operator = {"neg": "-", "not": "not", "i2f": "int_to_float"}[op]
                    line = f"{values[0]} = {operator} {values[1]}"
                elif op == "call":
                    line = f"{values[0]} = call {values[1]}, {values[2]}"
                elif op == "load":
                    line = f"{values[0]} = load {values[1]}[{values[2]}]"
                elif op == "store":
                    line = f"store {values[0]}[{values[1]}], {values[2]}"
                elif op in {"brfalse", "brtrue"}:
                    branch = "ifFalse" if op == "brfalse" else "ifTrue"
                    line = f"{branch} {values[0]} goto {values[1]}"
                elif op == "jmp":
                    line = f"goto {values[0]}"
                elif op == "label":
                    output.append(values[0] + ":")
                    continue
                else:
                    keyword = "return" if op == "ret" else op
                    line = " ".join([keyword, *values])
                output.append("  " + line)
            output.append("endfunc")
        return "\n".join(output) + "\n"

    def cleanup(self):
        # Conservative dead definitions: retain trapping operations, memory and calls.
        changed = True
        while changed:
            used = set()
            for op, *args in self.code:
                operands = args[1:] if op in PURE | {"load", "call", "alloc", "arg"} else args
                used.update(x for x in operands if x.startswith(("%", "@")))
            result = []
            reachable = True
            for instruction in self.code:
                op, *args = instruction
                if op in {"func", "label", "endfunc"}:
                    reachable = True
                if not reachable:
                    continue
                removable = op in PURE - {"div", "mod"}
                if removable and args[0].startswith("%") and args[0] not in used:
                    continue
                result.append(instruction)
                if op in {"jmp", "ret"}:
                    reachable = False
            changed = result != self.code
            self.code = result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ir", action="store_true", help="emitir IR (modo padrão)")
    level = parser.add_mutually_exclusive_group()
    level.add_argument("-O0", action="store_true", help="sem otimizações")
    level.add_argument("-O1", action="store_true", help="otimizações conservadoras")
    parser.add_argument("arquivo")
    args = parser.parse_args(argv)
    try:
        tokens, errors = scan(args.arquivo)
        if errors:
            print(f"Erro léxico: {errors[0]}", file=sys.stderr)
            return 2
        tree = analisar_tokens(tokens)
    except ErroSintatico as error:
        print(f"Erro sintático: {error}", file=sys.stderr)
        return 3
    except (OSError, UnicodeError) as error:
        print(f"Erro de entrada: {error}", file=sys.stderr)
        return 1
    diagnostics = io.StringIO()
    with contextlib.redirect_stdout(diagnostics):
        accepted = SemanticAnalyzer().analyze(tree)
    if not accepted:
        print(diagnostics.getvalue(), file=sys.stderr)
        return 4
    sys.stdout.write(Generator(tree, args.O1).generate())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
