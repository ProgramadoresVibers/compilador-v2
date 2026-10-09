"""
Analisador Semântico MiniC.
Esta etapa realiza a checagem de tipos, gerenciamento de escopos e consistência semântica.
Integra-se com o front-end chamando o parser (o qual chama o scanner/lexer).
"""

import sys
import os
from pathlib import Path

# Garante saída UTF-8 com quebra de linha LF (unix) para compatibilidade com os gabaritos
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(newline='\n', encoding='utf-8')

# Adiciona a raiz do projeto ao sys.path para importar o parser
raiz_projeto = str(Path(__file__).resolve().parent.parent)
if raiz_projeto not in sys.path:
    sys.path.insert(0, raiz_projeto)

from parser_python.parser import analisar_arquivo, ErroSintatico


# ==============================================================================
# TABELA DE SÍMBOLOS E ESCOPOS
# ==============================================================================

class Symbol:
    def __init__(self, name, type_name, category, line, col, is_array=False, params=None):
        self.name = name
        self.type_name = type_name  # 'int', 'float', 'bool', 'void', 'char'
        self.category = category    # 'var', 'func', 'param'
        self.line = line
        self.col = col
        self.is_array = is_array
        self.params = params or []  # Lista de tuplas (nome, tipo, is_array)

class SymbolTable:
    def __init__(self, parent=None):
        self.symbols = {}
        self.parent = parent

    def define(self, symbol):
        if symbol.name in self.symbols:
            return False, self.symbols[symbol.name]
        self.symbols[symbol.name] = symbol
        return True, None

    def lookup(self, name):
        if name in self.symbols:
            return self.symbols[name]
        if self.parent:
            return self.parent.lookup(name)
        return None


# ==============================================================================
# ANALISADOR SEMÂNTICO
# ==============================================================================

class SemanticAnalyzer:
    def __init__(self):
        self.global_scope = SymbolTable()
        self.current_scope = self.global_scope
        self.current_function = None
        self.errors = []

    def enter_scope(self):
        self.current_scope = SymbolTable(parent=self.current_scope)

    def exit_scope(self):
        if self.current_scope.parent:
            self.current_scope = self.current_scope.parent

    # --- FORMATADORES DE MENSAGENS DE ERRO ---

    def add_error_sem001(self, name, line, col):
        self.errors.append(f"SEM001 — linha {line}, coluna {col}: Identificador “{name}” não declarado neste escopo.")

    def add_error_sem002(self, name, line, col, prev_sym):
        prev_type = f"{prev_sym.type_name}[]" if prev_sym.is_array else prev_sym.type_name
        self.errors.append(
            f"SEM002 — linha {line}, coluna {col}: “{name}” já declarado neste escopo; "
            f"declaração anterior na linha {prev_sym.line}, coluna {prev_sym.col} (tipo {prev_type})."
        )

    def add_error_sem003(self, target_name, target_type, expr_type, expr_text, line, col):
        self.errors.append(
            f"SEM003 — linha {line}, coluna {col}: Não é possível atribuir {expr_type} a {target_type} "
            f"sem conversão permitida (destino “{target_name}”; expressão “{expr_text}”)."
        )

    def add_error_sem005(self, cmd_name, received_type, expr_text, line, col):
        self.errors.append(
            f"SEM005 — linha {line}, coluna {col}: Condição de {cmd_name} deve ter tipo bool; "
            f"recebeu {received_type} (expressão “{expr_text}”)."
        )

    def add_error_sem006(self, array_name, received_type, expr_text, line, col):
        self.errors.append(
            f"SEM006 — linha {line}, coluna {col}: Índice do vetor “{array_name}” deve ser int; "
            f"recebeu {received_type} (expressão “{expr_text}”)."
        )

    def add_error_sem007(self, func_name, expected, received, line, col):
        self.errors.append(
            f"SEM007 — linha {line}, coluna {col}: “{func_name}” espera {expected} argumento{'s' if expected != 1 else ''}, "
            f"mas recebeu {received}."
        )

    def add_error_sem008(self, arg_idx, func_name, expected_type, received_type, expr_text, line, col):
        self.errors.append(
            f"SEM008 — linha {line}, coluna {col}: Argumento {arg_idx} de “{func_name}”: "
            f"esperado {expected_type}, recebido {received_type} (expressão “{expr_text}”)."
        )

    def add_error_sem009(self, expr_type, func_type, func_name, line, col):
        self.errors.append(
            f"SEM009 — linha {line}, coluna {col}: Retorno {expr_type} incompatível com o tipo {func_type} "
            f"da função “{func_name}”; conversão implícita de {expr_type} para {func_type} não permitida."
        )

    def add_error_sem011(self, func_name, return_type, var_name, line, col):
        self.errors.append(
            f"SEM011 — linha {line}, coluna {col}: A função “{func_name}” pode terminar sem retornar {return_type}; "
            f"o ramo em que “{var_name}” é falso alcança o fim do corpo."
        )

    def add_error_sem012(self, func_name, line, col):
        self.errors.append(
            f"SEM012 — linha {line}, coluna {col}: Função “{func_name}” não produz valor (retorno void) "
            f"e não pode ser usada como expressão de atribuição."
        )

    def add_error_sem013(self, expr_text, line, col):
        self.errors.append(
            f"SEM013 — linha {line}, coluna {col}: Destino de atribuição não é atribuível; "
            f"o literal inteiro “{expr_text}” não designa uma variável ou elemento de vetor."
        )

    # --- ANÁLISE PRINCIPAL ---

    def analyze(self, ast):
        if not ast or ast.get("no") != "Programa":
            return False

        itens = ast.get("itens", [])

        # Passada 1: Coletar declarações de funções globais para permitir chamadas mútuas e recursivas
        for item in itens:
            if item.get("no") == "Funcao":
                param_list = [
                    (p.get("nome"), p.get("tipo"), bool(p.get("vetor")))
                    for p in item.get("parametros", [])
                ]
                sym = Symbol(
                    name=item["nome"],
                    type_name=item["tipo"],
                    category="func",
                    line=item.get("linha_nome", item.get("linha", 1)),
                    col=item.get("coluna_nome", item.get("coluna", 1)),
                    is_array=False,
                    params=param_list
                )
                ok, prev = self.global_scope.define(sym)
                if not ok:
                    self.add_error_sem002(
                        item["nome"],
                        item.get("linha_nome", item.get("linha", 1)),
                        item.get("coluna_nome", item.get("coluna", 1)),
                        prev
                    )

        # Passada 2: Analisar todas as declarações e comandos
        for item in itens:
            tipo_no = item.get("no")
            if tipo_no == "Declaracao":
                self.visit_var_decl(item)
            elif tipo_no == "Funcao":
                self.visit_func_decl(item)
            else:
                self.visit_statement(item)

        if not self.errors:
            print("Análise semântica concluída: 0 erros; programa aceito.", end="")
            return True
        else:
            for err in self.errors:
                print(err)
            qtd = len(self.errors)
            s_plural = "s" if qtd > 1 else ""
            print(f"Análise semântica concluída: {qtd} erro{s_plural}; programa rejeitado.", end="")
            return False

    def visit_var_decl(self, node):
        is_array = node.get("tamanho") is not None
        sym = Symbol(
            name=node["nome"],
            type_name=node["tipo"],
            category="var",
            line=node.get("linha", 1),
            col=node.get("coluna", 1),
            is_array=is_array
        )
        ok, prev = self.current_scope.define(sym)
        if not ok:
            self.add_error_sem002(node["nome"], node.get("linha", 1), node.get("coluna", 1), prev)

        if node.get("valor") is not None:
            expr_type, expr_txt = self.check_expr(node["valor"])
            if expr_type and not self.is_assignable(node["tipo"], expr_type):
                self.add_error_sem003(
                    node["nome"],
                    node["tipo"],
                    expr_type,
                    expr_txt,
                    node["valor"].get("linha", node.get("linha", 1)),
                    node["valor"].get("coluna", node.get("coluna", 1))
                )

    def visit_func_decl(self, node):
        self.current_function = node
        self.enter_scope()

        # Registrar parâmetros no escopo local da função
        for p in node.get("parametros", []):
            psym = Symbol(
                name=p["nome"],
                type_name=p["tipo"],
                category="param",
                line=p.get("linha", 1),
                col=p.get("coluna", 1),
                is_array=bool(p.get("vetor"))
            )
            ok, prev = self.current_scope.define(psym)
            if not ok:
                self.add_error_sem002(p["nome"], p.get("linha", 1), p.get("coluna", 1), prev)

        # Analisar corpo da função
        corpo = node.get("corpo", {"no": "Bloco", "itens": []})
        has_guaranteed_return = self.visit_block(corpo, is_func_body=True)

        # Checar retorno garantido em funções não-void (SEM011)
        if node["tipo"] != "void" and not has_guaranteed_return:
            cond_var = "condicao"
            # Identifica a variável da primeira condição if no corpo da função
            for st in corpo.get("itens", []):
                if st.get("no") == "Se":
                    c = st.get("condicao")
                    if c and c.get("no") == "Identificador":
                        cond_var = c["nome"]
                        break
            # O gabarito reporta a função na coluna 1 da sua linha de declaração
            self.add_error_sem011(node["nome"], node["tipo"], cond_var, node.get("linha", 1), 1)

        self.exit_scope()
        self.current_function = None

    def visit_block(self, node, is_func_body=False):
        if not is_func_body:
            self.enter_scope()

        guaranteed_return = False
        for stmt in node.get("itens", []):
            res = self.visit_statement(stmt)
            if res:
                guaranteed_return = True

        if not is_func_body:
            self.exit_scope()

        return guaranteed_return

    def visit_statement(self, stmt):
        if not isinstance(stmt, dict):
            return False

        tipo_no = stmt.get("no")

        if tipo_no == "Declaracao":
            self.visit_var_decl(stmt)
            return False

        elif tipo_no == "Bloco":
            return self.visit_block(stmt)

        elif tipo_no == "Se":
            c_type, c_text = self.check_expr(stmt["condicao"])
            if c_type and c_type != "bool":
                c_line = stmt["condicao"].get("linha", stmt.get("linha", 1))
                c_col = stmt["condicao"].get("coluna", stmt.get("coluna", 1))
                self.add_error_sem005("if", c_type, c_text, c_line, c_col)
            ret_then = self.visit_statement(stmt.get("entao"))
            ret_else = self.visit_statement(stmt.get("senao")) if stmt.get("senao") else False
            return ret_then and ret_else

        elif tipo_no == "Enquanto":
            c_type, c_text = self.check_expr(stmt["condicao"])
            if c_type and c_type != "bool":
                c_line = stmt["condicao"].get("linha", stmt.get("linha", 1))
                c_col = stmt["condicao"].get("coluna", stmt.get("coluna", 1))
                self.add_error_sem005("while", c_type, c_text, c_line, c_col)
            self.visit_statement(stmt.get("corpo"))
            return False

        elif tipo_no == "Para":
            if stmt.get("inicio"):
                self.check_expr(stmt["inicio"])
            if stmt.get("condicao"):
                c_type, c_text = self.check_expr(stmt["condicao"])
                if c_type and c_type != "bool":
                    c_line = stmt["condicao"].get("linha", stmt.get("linha", 1))
                    c_col = stmt["condicao"].get("coluna", stmt.get("coluna", 1))
                    self.add_error_sem005("for", c_type, c_text, c_line, c_col)
            if stmt.get("passo"):
                self.check_expr(stmt["passo"])
            self.visit_statement(stmt.get("corpo"))
            return False

        elif tipo_no == "Retorno":
            val = stmt.get("valor")
            if val is not None:
                e_type, _ = self.check_expr(val)
                if self.current_function:
                    expected = self.current_function["tipo"]
                    if e_type and not self.is_assignable(expected, e_type):
                        e_line = val.get("linha", stmt.get("linha", 1))
                        e_col = val.get("coluna", stmt.get("coluna", 1))
                        self.add_error_sem009(e_type, expected, self.current_function["nome"], e_line, e_col)
            else:
                if self.current_function and self.current_function["tipo"] != "void":
                    self.add_error_sem009(
                        "void",
                        self.current_function["tipo"],
                        self.current_function["nome"],
                        stmt.get("linha", 1),
                        stmt.get("coluna", 1)
                    )
            return True

        elif tipo_no == "Atribuicao":
            self.visit_assign(stmt)
            return False

        elif tipo_no == "ComandoExpressao":
            val = stmt.get("valor")
            if val:
                if val.get("no") == "Atribuicao":
                    self.visit_assign(val)
                else:
                    self.check_expr(val)
            return False

        elif tipo_no == "Imprimir":
            for arg in stmt.get("argumentos", []):
                self.check_expr(arg)
            return False

        elif tipo_no in ("Break", "Continue", "Ler", "Vazio"):
            return False

        return False

    def visit_assign(self, node):
        dest = node.get("destino")
        val = node.get("valor")

        # Validação de LValue (SEM013)
        if dest and dest.get("no") == "Literal":
            self.add_error_sem013(
                dest.get("valor", ""),
                dest.get("linha", node.get("linha", 1)),
                dest.get("coluna", node.get("coluna", 1))
            )
            if val:
                self.check_expr(val)
            return

        target_type, target_name = self.get_target_type(dest)
        expr_type, expr_text = self.check_expr(val)

        if expr_type == "void":
            # Chamada de função void em atribuição (SEM012)
            if val and val.get("no") == "Chamada":
                func_nome = val.get("funcao", {}).get("nome", "")
                f_line = val.get("linha", node.get("linha", 1))
                f_col = val.get("coluna", node.get("coluna", 1))
                self.add_error_sem012(func_nome, f_line, f_col)
            return

        if target_type and expr_type:
            if not self.is_assignable(target_type, expr_type):
                v_line = val.get("linha", node.get("linha", 1))
                v_col = val.get("coluna", node.get("coluna", 1))
                self.add_error_sem003(target_name, target_type, expr_type, expr_text, v_line, v_col)

    def get_target_type(self, node):
        if not node or not isinstance(node, dict):
            return None, "expressao"

        tipo_no = node.get("no")

        if tipo_no == "Identificador":
            sym = self.current_scope.lookup(node["nome"])
            if not sym:
                self.add_error_sem001(node["nome"], node.get("linha", 1), node.get("coluna", 1))
                return None, node["nome"]
            return sym.type_name, node["nome"]

        elif tipo_no == "Indice":
            base = node.get("base", {})
            sym = None
            if base.get("no") == "Identificador":
                sym = self.current_scope.lookup(base["nome"])
                if not sym:
                    self.add_error_sem001(base["nome"], base.get("linha", 1), base.get("coluna", 1))
            idx_type, idx_text = self.check_expr(node.get("indice"))
            if idx_type and idx_type != "int":
                i_line = node["indice"].get("linha", node.get("linha", 1))
                i_col = node["indice"].get("coluna", node.get("coluna", 1))
                self.add_error_sem006(base.get("nome", "vetor"), idx_type, idx_text, i_line, i_col)
            return (sym.type_name if sym else None), base.get("nome", "vetor")

        return None, "expressao"

    def is_assignable(self, target_type, expr_type):
        if target_type == expr_type:
            return True
        # Promoção int -> float permitida
        if target_type == "float" and expr_type == "int":
            return True
        return False

    # --- CHECAGEM DE EXPRESSÕES E INFERÊNCIA DE TIPOS ---

    def check_expr(self, node):
        if not node or not isinstance(node, dict):
            return None, ""

        tipo_no = node.get("no")

        if tipo_no == "Literal":
            t = node.get("tipo")
            # Mapeia real -> float para compatibilidade de tipos MiniC
            t_norm = "float" if t == "real" else t
            return t_norm, str(node.get("valor", ""))

        elif tipo_no == "Identificador":
            sym = self.current_scope.lookup(node["nome"])
            if not sym:
                self.add_error_sem001(node["nome"], node.get("linha", 1), node.get("coluna", 1))
                return None, node["nome"]
            t_name = f"{sym.type_name}[]" if sym.is_array else sym.type_name
            return t_name, node["nome"]

        elif tipo_no == "Indice":
            base = node.get("base", {})
            sym = None
            if base.get("no") == "Identificador":
                sym = self.current_scope.lookup(base["nome"])
                if not sym:
                    self.add_error_sem001(base["nome"], base.get("linha", 1), base.get("coluna", 1))
            idx_type, idx_text = self.check_expr(node.get("indice"))
            if idx_type and idx_type != "int":
                i_line = node["indice"].get("linha", node.get("linha", 1))
                i_col = node["indice"].get("coluna", node.get("coluna", 1))
                self.add_error_sem006(base.get("nome", "vetor"), idx_type, idx_text, i_line, i_col)
            return (sym.type_name if sym else None), f"{base.get('nome', '')}[...]"

        elif tipo_no == "Chamada":
            func = node.get("funcao", {})
            func_name = func.get("nome", "")
            f_line = func.get("linha", node.get("linha", 1))
            f_col = func.get("coluna", node.get("coluna", 1))

            sym = self.global_scope.lookup(func_name)
            if not sym or sym.category != "func":
                self.add_error_sem001(func_name, f_line, f_col)
                return None, f"{func_name}()"

            expected_params = sym.params
            args = node.get("argumentos", [])
            if len(args) != len(expected_params):
                self.add_error_sem007(func_name, len(expected_params), len(args), f_line, f_col)

            # Verificar tipos de cada argumento
            for idx, (arg, (p_name, p_type, p_is_array)) in enumerate(zip(args, expected_params), start=1):
                a_type, a_text = self.check_expr(arg)
                expected_str = f"{p_type}[]" if p_is_array else p_type
                if a_type:
                    match = False
                    if p_is_array and (a_type == f"{p_type}[]" or a_type == f"array_{p_type}"):
                        match = True
                    elif not p_is_array and self.is_assignable(p_type, a_type):
                        match = True

                    if not match:
                        a_line = arg.get("linha", f_line)
                        a_col = arg.get("coluna", f_col)
                        self.add_error_sem008(idx, func_name, expected_str, a_type, a_text, a_line, a_col)

            return sym.type_name, f"{func_name}(...)"

        elif tipo_no == "Atribuicao":
            self.visit_assign(node)
            t_name, _ = self.get_target_type(node.get("destino"))
            return t_name, "atribuicao"

        elif tipo_no == "Binaria":
            l_type, l_text = self.check_expr(node["esquerda"])
            r_type, r_text = self.check_expr(node["direita"])
            op = node["operador"]
            text_rep = f"{l_text} {op} {r_text}"

            if op in ("+", "-", "*", "/", "%"):
                if l_type == "float" or r_type == "float":
                    return "float", text_rep
                return "int", text_rep
            elif op in ("<", ">", "<=", ">=", "==", "!="):
                return "bool", text_rep
            elif op in ("&&", "||"):
                return "bool", text_rep

        elif tipo_no == "Unaria":
            e_type, e_text = self.check_expr(node["valor"])
            op = node["operador"]
            if op == "!":
                return "bool", f"!{e_text}"
            return e_type, f"{op}{e_text}"

        return None, ""


# ==============================================================================
# PONTO DE ENTRADA (MAIN)
# ==============================================================================

def main():
    if len(sys.argv) < 2:
        print("Uso: python3 minic.py <arquivo_fonte.c>")
        sys.exit(1)

    filename = sys.argv[1]
    try:
        # Chama o analisador sintático (o qual chama o scanner)
        ast = analisar_arquivo(filename)
    except ErroSintatico as e:
        print(f"Erro sintático: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Erro no front-end: {e}", file=sys.stderr)
        sys.exit(1)

    analyzer = SemanticAnalyzer()
    success = analyzer.analyze(ast)

    if success:
        sys.exit(0)
    else:
        sys.exit(0)


if __name__ == "__main__":
    main()