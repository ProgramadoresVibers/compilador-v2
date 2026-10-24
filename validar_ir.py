"""Validação semântica da IR: VM independente e intérprete de AST."""

import argparse
import ast
import math
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from parser_python.parser import analisar_arquivo

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\S+')
BINARY = {"add", "sub", "mul", "div", "mod", "lt", "le", "gt", "ge", "eq", "ne"}
ARITY = {
    "func": 2, "endfunc": 0, "alloc": 3, "arg": 2, "mov": 2,
    "load": 3, "store": 3, "param": 1, "call": 3, "i2f": 2,
    "neg": 2, "not": 2, "label": 1, "jmp": 1,
    "brfalse": 2, "brtrue": 2, "print": 1, "read": 0,
    **{op: 3 for op in BINARY},
}


def atom(text):
    if text == "true":
        return True
    if text == "false":
        return False
    if text.startswith(('"', "'")):
        return ast.literal_eval(text)
    return float(text) if any(c in text for c in ".eE") else int(text)


def binary(op, left, right):
    if op == "add":
        return left + right
    if op == "sub":
        return left - right
    if op == "mul":
        return left * right
    if op in {"div", "mod"}:
        if isinstance(left, int) and isinstance(right, int):
            quotient = abs(left) // abs(right)
            if (left < 0) != (right < 0):
                quotient = -quotient
            return quotient if op == "div" else left - quotient * right
        return left / right
    if op == "lt":
        return left < right
    if op == "le":
        return left <= right
    if op == "gt":
        return left > right
    if op == "ge":
        return left >= right
    if op == "eq":
        return left == right
    if op == "ne":
        return left != right
    raise AssertionError(op)


class Machine:
    def __init__(self, text, inputs=()):
        self.functions = {}
        self.globals = {}
        self.output = []
        self.inputs = iter(inputs)
        self.steps = 0
        current = None
        for line in text.splitlines():
            tokens = TOKEN.findall(line)
            if not tokens:
                continue
            op, *args = tokens
            if op == "ret":
                assert len(args) <= 1, line
            else:
                assert op in ARITY and len(args) == ARITY[op], line
            if op == "func":
                assert current is None and args[0] not in self.functions, line
                current = []
                self.functions[args[0]] = current
            elif op == "endfunc":
                assert current is not None, line
                current = None
            else:
                assert current is not None, line
                current.append(tokens)
        assert current is None and "$init" in self.functions
        for code in self.functions.values():
            labels = [i[1] for i in code if i[0] == "label"]
            assert len(labels) == len(set(labels)), "rótulo duplicado"
            for instruction in code:
                if instruction[0] in {"jmp", "brfalse", "brtrue"}:
                    assert instruction[-1] in labels, "destino de desvio ausente"
                if instruction[0] == "call":
                    assert instruction[2] in self.functions, "função ausente"

    def call(self, name, arguments=(), depth=0):
        assert depth < 150, "limite de recursão na VM"
        code = self.functions[name]
        labels = {i[1]: n for n, i in enumerate(code) if i[0] == "label"}
        local = {}
        params = []
        formals = iter(arguments)

        def get(text):
            if text.startswith("@"):
                return self.globals[text]
            if text.startswith("%"):
                assert text in local, f"uso sem definição: {text}"
                return local[text]
            return atom(text)

        def put(text, value):
            (self.globals if text.startswith("@") else local)[text] = value

        pc = 0
        while pc < len(code):
            self.steps += 1
            assert self.steps < 200000, "limite de execução da VM"
            op, *args = code[pc]
            pc += 1
            if op == "alloc":
                dest, kind, size = args
                # A suíte não deve depender de valores locais não inicializados.
                if kind.endswith("[]"):
                    put(dest, [0] * int(size))
                elif dest.startswith("@"):
                    put(dest, 0.0 if kind == "float" else 0)
            elif op == "arg":
                put(args[0], next(formals))
            elif op == "mov":
                put(args[0], get(args[1]))
            elif op in BINARY:
                put(args[0], binary(op, get(args[1]), get(args[2])))
            elif op in {"neg", "not", "i2f"}:
                value = get(args[1])
                if op == "neg":
                    result = -value
                elif op == "not":
                    result = not value
                else:
                    result = float(value)
                put(args[0], result)
            elif op == "load":
                vector, index = get(args[1]), get(args[2])
                assert 0 <= index < len(vector), "índice fora do vetor"
                put(args[0], vector[index])
            elif op == "store":
                vector, index = get(args[0]), get(args[1])
                assert 0 <= index < len(vector), "índice fora do vetor"
                vector[index] = get(args[2])
            elif op == "param":
                params.append(get(args[0]))
            elif op == "call":
                assert len(params) == int(args[2]), "aridade inválida na IR"
                put(args[0], self.call(args[1], params, depth + 1))
                params = []
            elif op == "print":
                self.output.append(get(args[0]))
            elif op == "read":
                next(self.inputs)
            elif op == "ret":
                return get(args[0]) if args else None
            elif op == "jmp":
                pc = labels[args[0]]
            elif op in {"brfalse", "brtrue"}:
                if bool(get(args[0])) == (op == "brtrue"):
                    pc = labels[args[1]]
        return None

    def run(self):
        self.call("$init")
        result = self.call("main") if "main" in self.functions else None
        sentinel = object()
        assert next(self.inputs, sentinel) is sentinel, "leitura removida indevidamente"
        return self.output, result


class Transfer(Exception):
    def __init__(self, kind, value=None):
        self.kind = kind
        self.value = value


class TypedVector(list):
    def __init__(self, size, kind):
        super().__init__([0] * size)
        self.kind = kind


class SourceMachine:
    """Oráculo de execução da AST; não usa o gerador nem suas otimizações."""
    def __init__(self, tree, inputs=()):
        self.tree = tree
        self.functions = {n["nome"]: n for n in tree["itens"] if n["no"] == "Funcao"}
        self.scopes = [{}]
        self.output = []
        self.inputs = iter(inputs)
        self.steps = 0
        self.return_type = None

    def scope(self, name):
        return next(s for s in reversed(self.scopes) if name in s)

    @staticmethod
    def convert(value, kind):
        return float(value) if kind == "float" else value

    def address(self, node):
        if node["no"] == "Identificador":
            scope = self.scope(node["nome"])
            value, kind = scope[node["nome"]]
            return scope, node["nome"], kind
        vector = self.expr(node["base"])
        index = self.expr(node["indice"])
        assert 0 <= index < len(vector)
        return vector, index, vector.kind

    def expr(self, n):
        kind = n["no"]
        if kind == "Literal":
            text = n["valor"].rstrip("fF") if n["tipo"] == "real" else n["valor"]
            return atom(text)
        if kind == "Identificador":
            return self.scope(n["nome"])[n["nome"]][0]
        if kind == "Indice":
            vector, index, _ = self.address(n)
            return vector[index]
        if kind == "Atribuicao":
            container, key, target_type = self.address(n["destino"])
            value = self.expr(n["valor"])
            if isinstance(container, list):
                value = self.convert(value, target_type)
                container[key] = value
            else:
                value = self.convert(value, target_type)
                container[key] = (value, target_type)
            return value
        if kind == "Chamada":
            values = [self.expr(arg) for arg in n["argumentos"]]
            return self.call(n["funcao"]["nome"], values)
        if kind == "Unaria":
            value = self.expr(n["valor"])
            return not value if n["operador"] == "!" else -value
        if kind == "Binaria":
            left = self.expr(n["esquerda"])
            op = n["operador"]
            if op == "&&":
                return bool(left and self.expr(n["direita"]))
            if op == "||":
                return bool(left or self.expr(n["direita"]))
            names = {
                "+": "add", "-": "sub", "*": "mul", "/": "div", "%": "mod",
                "<": "lt", "<=": "le", ">": "gt", ">=": "ge", "==": "eq", "!=": "ne",
            }
            return binary(names[op], left, self.expr(n["direita"]))
        raise AssertionError(kind)

    def call(self, name, values):
        function = self.functions[name]
        previous_scopes, previous_type = self.scopes, self.return_type
        self.scopes = [previous_scopes[0], {}]
        self.return_type = function["tipo"]
        for formal, value in zip(function["parametros"], values):
            converted = value if formal.get("vetor") else self.convert(value, formal["tipo"])
            self.scopes[-1][formal["nome"]] = (converted, formal["tipo"])
        try:
            for item in function["corpo"]["itens"]:
                self.statement(item)
        except Transfer as transfer:
            assert transfer.kind == "return"
            return transfer.value
        finally:
            self.scopes, self.return_type = previous_scopes, previous_type
        return None

    def statement(self, n):
        if n is None:
            return
        self.steps += 1
        assert self.steps < 100000, "limite de execução do oráculo"
        kind = n["no"]
        if kind == "Bloco":
            self.scopes.append({})
            try:
                for child in n["itens"]:
                    self.statement(child)
            finally:
                self.scopes.pop()
        elif kind == "Declaracao":
            scope = self.scopes[-1]
            scope[n["nome"]] = (0, n["tipo"])
            if n.get("tamanho"):
                value = TypedVector(int(n["tamanho"]["valor"]), n["tipo"])
            elif n.get("valor"):
                value = self.convert(self.expr(n["valor"]), n["tipo"])
            else:
                value = 0.0 if n["tipo"] == "float" else 0
            scope[n["nome"]] = (value, n["tipo"])
        elif kind == "ComandoExpressao":
            self.expr(n["valor"])
        elif kind == "Imprimir":
            for arg in n["argumentos"]:
                self.output.append(self.expr(arg))
        elif kind == "Ler":
            next(self.inputs)
        elif kind == "Retorno":
            value = self.convert(self.expr(n["valor"]), self.return_type) if n.get("valor") else None
            raise Transfer("return", value)
        elif kind in {"Break", "Continue"}:
            raise Transfer(kind.lower())
        elif kind == "Se":
            self.statement(n["entao"] if self.expr(n["condicao"]) else n.get("senao"))
        elif kind in {"Enquanto", "Para"}:
            if kind == "Para" and n.get("inicio"):
                self.expr(n["inicio"])
            while n.get("condicao") is None or self.expr(n["condicao"]):
                self.steps += 1
                assert self.steps < 100000
                try:
                    self.statement(n["corpo"])
                except Transfer as transfer:
                    if transfer.kind == "break":
                        break
                    if transfer.kind != "continue":
                        raise
                if kind == "Para" and n.get("passo"):
                    self.expr(n["passo"])

    def run(self):
        for item in self.tree["itens"]:
            if item["no"] != "Funcao":
                self.statement(item)
        result = self.call("main", []) if "main" in self.functions else None
        sentinel = object()
        assert next(self.inputs, sentinel) is sentinel, "leitura removida indevidamente"
        return self.output, result


def compile_ir(command, path, mode):
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", PYTHONIOENCODING="utf-8")
    return subprocess.run(
        [*command, "--ir", mode, str(path)],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=15,
        env=env,
    )


def same(actual, expected):
    if isinstance(actual, (list, tuple)) and isinstance(expected, (list, tuple)):
        return len(actual) == len(expected) and all(same(a, b) for a, b in zip(actual, expected))
    if isinstance(actual, (float, int)) and isinstance(expected, (float, int)):
        return math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-12)
    return actual == expected


def validate(command, suite, peer=None):
    total = passed = 0
    for source in sorted(suite.rglob("*.c")):
        total += 1
        try:
            reference = source.with_suffix(".txt")
            assert reference.is_file(), f"gabarito ausente: {reference}"
            criteria = reference.read_text(encoding="utf-8")
            error = "REJEITAR" in criteria
            expected_code = 0
            if error:
                match = re.search(r"código de saída (\d+)", criteria)
                assert match, "categoria de erro sem código de saída no gabarito"
                expected_code = int(match.group(1))
            runs = [compile_ir(command, source, mode) for mode in ("-O0", "-O1")]
            for run in runs:
                assert run.returncode == expected_code, f"status {run.returncode}, esperado {expected_code}: {run.stderr}"
            if error:
                for run in runs:
                    assert not run.stdout.strip(), "IR emitida para programa inválido"
                    category = {2: "léxico", 3: "sintático", 4: "SEM"}[expected_code]
                    assert category in run.stderr, f"categoria ausente: {run.stderr}"
            else:
                expected = SourceMachine(analisar_arquivo(source)).run()
                for run in runs:
                    actual = Machine(run.stdout).run()
                    assert same(actual, expected), f"execução {actual}, esperado {expected}"
            if peer:
                for mode, run in zip(("-O0", "-O1"), runs):
                    other = compile_ir(peer, source, mode)
                    assert other.returncode == run.returncode
                    assert other.stdout == run.stdout, f"IR C/Python diferente ({mode})"
            print(f"[OK] {source.parent.name}")
            passed += 1
        except (AssertionError, ValueError, KeyError, ZeroDivisionError, StopIteration, OSError, subprocess.TimeoutExpired) as exc:
            print(f"[FALHA] {source}: {exc}")
    print(f"Resultado: {passed}/{total} aprovados")
    return 0 if total and passed == total else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--python", action="store_true")
    parser.add_argument("--peer", help="executável C para comparar com Python")
    args = parser.parse_args()
    command = [sys.executable, args.compiler] if args.python else [args.compiler]
    peer = [args.peer] if args.peer else None
    return validate(command, args.suite, peer)


if __name__ == "__main__":
    raise SystemExit(main())
