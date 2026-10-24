"""
Analisador Semântico MiniC.
Esta etapa realiza a checagem de tipos, gerenciamento de escopos e consistência semântica.
Integra-se com o front-end chamando o parser (o qual chama o scanner).

Representação de tipos usada pelo analisador:
    "int", "float", "bool", "char", "string", "void"  -> tipos escalares / especiais
    "int[]", "float[]", ...                           -> vetores (variáveis ou parâmetros)
    None                                               -> tipo desconhecido (um erro já foi
                                                          reportado; evita erros em cascata)
"""

import sys
from pathlib import Path


if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(newline='\n', encoding='utf-8')

# Adiciona a raiz do projeto ao sys.path para importar o parser
raiz_projeto = str(Path(__file__).resolve().parent.parent)
if raiz_projeto not in sys.path:
    sys.path.insert(0, raiz_projeto)

from parser_python.parser import analisar_arquivo, ErroSintatico


# ==============================================================================
# TIPOS E UTILITÁRIOS
# ==============================================================================

TIPOS_NUMERICOS = ("int", "float")
OPS_ARITMETICOS = ("+", "-", "*", "/")
OPS_RELACIONAIS = ("<", ">", "<=", ">=")
OPS_IGUALDADE = ("==", "!=")
OPS_LOGICOS = ("&&", "||")

PRECEDENCIA = {
    "||": 1, "&&": 2,
    "==": 3, "!=": 3,
    "<": 4, ">": 4, "<=": 4, ">=": 4,
    "+": 5, "-": 5,
    "*": 6, "/": 6, "%": 6,
}

DESCRICAO_LITERAL = {
    "int": "o literal inteiro",
    "real": "o literal real",
    "float": "o literal real",
    "bool": "o literal booleano",
    "char": "o literal de caractere",
    "string": "o literal de cadeia",
}


def normalizar_tipo(t):
    return "float" if t == "real" else t


def eh_numerico(t):
    return t in TIPOS_NUMERICOS


def eh_vetor(t):
    return isinstance(t, str) and t.endswith("[]")


def eh_literal_verdadeiro(node):
    return (isinstance(node, dict) and node.get("no") == "Literal"
            and node.get("tipo") == "bool" and node.get("valor") == "true")


def texto_expr(node):
    """Representação textual (sem efeitos colaterais) de uma expressão para as mensagens."""
    if not isinstance(node, dict):
        return ""
    tipo_no = node.get("no")
    if tipo_no == "Literal":
        return str(node.get("valor", ""))
    if tipo_no == "Identificador":
        return node.get("nome", "")
    if tipo_no == "Indice":
        return f"{texto_expr(node.get('base'))}[...]"
    if tipo_no == "Chamada":
        return f"{texto_expr(node.get('funcao'))}(...)"
    if tipo_no == "Atribuicao":
        return f"{texto_expr(node.get('destino'))} = {texto_expr(node.get('valor'))}"
    if tipo_no == "Binaria":
        op = node.get("operador", "")
        prec = PRECEDENCIA.get(op, 0)
        esq, dir_ = node.get("esquerda"), node.get("direita")
        t_esq, t_dir = texto_expr(esq), texto_expr(dir_)
        if _precisa_parenteses(esq, prec, lado_direito=False):
            t_esq = f"({t_esq})"
        if _precisa_parenteses(dir_, prec, lado_direito=True):
            t_dir = f"({t_dir})"
        return f"{t_esq} {op} {t_dir}"
    if tipo_no == "Unaria":
        filho = node.get("valor")
        t = texto_expr(filho)
        if isinstance(filho, dict) and filho.get("no") in ("Binaria", "Atribuicao"):
            t = f"({t})"
        return f"{node.get('operador', '')}{t}"
    return ""


def _precisa_parenteses(filho, prec_pai, lado_direito):
    if not isinstance(filho, dict):
        return False
    if filho.get("no") == "Atribuicao":
        return True
    if filho.get("no") != "Binaria":
        return False
    prec = PRECEDENCIA.get(filho.get("operador"), 0)
    return prec < prec_pai or (lado_direito and prec == prec_pai)


def sempre_retorna(stmt):
    """True se o comando nunca termina normalmente (todo caminho executa um return
    ou fica preso em um laço infinito sem break)."""
    if not isinstance(stmt, dict):
        return False
    tipo_no = stmt.get("no")
    if tipo_no == "Retorno":
        return True
    if tipo_no == "Bloco":
        return any(sempre_retorna(s) for s in stmt.get("itens", []))
    if tipo_no == "Se":
        return (stmt.get("senao") is not None
                and sempre_retorna(stmt.get("entao"))
                and sempre_retorna(stmt.get("senao")))
    if tipo_no == "Enquanto":
        return eh_literal_verdadeiro(stmt.get("condicao")) and not contem_break(stmt.get("corpo"))
    if tipo_no == "Para":
        cond = stmt.get("condicao")
        return (cond is None or eh_literal_verdadeiro(cond)) and not contem_break(stmt.get("corpo"))
    return False


def contem_break(stmt):
    """True se o comando contém um break que sai do laço corrente (não desce em laços internos)."""
    if not isinstance(stmt, dict):
        return False
    tipo_no = stmt.get("no")
    if tipo_no == "Break":
        return True
    if tipo_no == "Bloco":
        return any(contem_break(s) for s in stmt.get("itens", []))
    if tipo_no == "Se":
        return contem_break(stmt.get("entao")) or contem_break(stmt.get("senao"))
    return False


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

    @property
    def full_type(self):
        return f"{self.type_name}[]" if self.is_array else self.type_name


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
        self.loop_depth = 0
        self.errors = []

    def enter_scope(self):
        self.current_scope = SymbolTable(parent=self.current_scope)

    def exit_scope(self):
        if self.current_scope.parent:
            self.current_scope = self.current_scope.parent

    def _err(self, code, line, col, msg):
        self.errors.append(f"{code} — linha {line}, coluna {col}: {msg}")

    @staticmethod
    def _pos(node, fallback=None):
        fb = fallback or {}
        if not isinstance(node, dict):
            node = {}
        return node.get("linha", fb.get("linha", 1)), node.get("coluna", fb.get("coluna", 1))

    # --- FORMATADORES DE MENSAGENS DE ERRO ---

    def add_error_sem001(self, name, line, col):
        self._err("SEM001", line, col, f"Identificador “{name}” não declarado neste escopo.")

    def add_error_sem002(self, name, line, col, prev_sym):
        self._err("SEM002", line, col,
                  f"“{name}” já declarado neste escopo; "
                  f"declaração anterior na linha {prev_sym.line}, coluna {prev_sym.col} (tipo {prev_sym.full_type}).")

    def add_error_sem003(self, target_name, target_type, expr_type, expr_text, line, col):
        self._err("SEM003", line, col,
                  f"Não é possível atribuir {expr_type} a {target_type} "
                  f"sem conversão permitida (destino “{target_name}”; expressão “{expr_text}”).")

    def add_error_sem005(self, cmd_name, received_type, expr_text, line, col):
        self._err("SEM005", line, col,
                  f"Condição de {cmd_name} deve ter tipo bool; "
                  f"recebeu {received_type} (expressão “{expr_text}”).")

    def add_error_sem006(self, array_name, received_type, expr_text, line, col):
        self._err("SEM006", line, col,
                  f"Índice do vetor “{array_name}” deve ser int; "
                  f"recebeu {received_type} (expressão “{expr_text}”).")

    def add_error_sem007(self, func_name, expected, received, line, col):
        self._err("SEM007", line, col,
                  f"“{func_name}” espera {expected} argumento{'s' if expected != 1 else ''}, "
                  f"mas recebeu {received}.")

    def add_error_sem008(self, arg_idx, func_name, expected_type, received_type, expr_text, line, col):
        self._err("SEM008", line, col,
                  f"Argumento {arg_idx} de “{func_name}”: "
                  f"esperado {expected_type}, recebido {received_type} (expressão “{expr_text}”).")

    def add_error_sem009(self, expr_type, func_type, func_name, line, col):
        self._err("SEM009", line, col,
                  f"Retorno {expr_type} incompatível com o tipo {func_type} "
                  f"da função “{func_name}”; conversão implícita de {expr_type} para {func_type} não permitida.")

    def add_error_sem009_void(self, func_name, expr_text, line, col):
        self._err("SEM009", line, col,
                  f"A função “{func_name}” tem tipo void e não pode retornar valor (expressão “{expr_text}”).")

    def add_error_sem011(self, func_name, return_type, motivo, line, col):
        self._err("SEM011", line, col,
                  f"A função “{func_name}” pode terminar sem retornar {return_type}; {motivo}.")

    def add_error_sem012(self, func_name, contexto, line, col):
        self._err("SEM012", line, col,
                  f"Função “{func_name}” não produz valor (retorno void) "
                  f"e não pode ser usada {contexto}.")

    def add_error_sem013(self, motivo, line, col):
        self._err("SEM013", line, col, f"Destino de atribuição não é atribuível; {motivo}.")

    def add_error_sem014_bin(self, op, l_type, r_type, expr_text, line, col):
        self._err("SEM014", line, col,
                  f"Operador “{op}” não se aplica aos tipos {l_type} e {r_type} (expressão “{expr_text}”).")

    def add_error_sem014_un(self, op, e_type, expr_text, line, col):
        self._err("SEM014", line, col,
                  f"Operador “{op}” não se aplica ao tipo {e_type} (expressão “{expr_text}”).")

    def add_error_sem015(self, name, type_desc, line, col):
        self._err("SEM015", line, col,
                  f"“{name}” não é vetor e não pode ser indexado (tipo {type_desc}).")

    def add_error_sem016(self, name, line, col):
        self._err("SEM016", line, col,
                  f"“{name}” é uma função e não pode ser usada como valor sem ser chamada.")

    def add_error_sem017(self, name, type_desc, line, col):
        self._err("SEM017", line, col,
                  f"“{name}” não é uma função e não pode ser chamado (tipo {type_desc}).")

    def add_error_sem018(self, cmd, line, col):
        self._err("SEM018", line, col, f"Comando {cmd} fora de um laço (while ou for).")

    def add_error_sem019(self, line, col):
        self._err("SEM019", line, col, "Comando return fora de uma função.")

    def add_error_sem020(self, name, line, col):
        self._err("SEM020", line, col, f"Tamanho do vetor “{name}” deve ser maior que zero.")

    # --- ANÁLISE PRINCIPAL ---

    def analyze(self, ast):
        if not ast or ast.get("no") != "Programa":
            return False

        itens = ast.get("itens", [])

        # Passada 1: coletar assinaturas das funções globais (permite recursão e chamadas
        # para funções definidas mais adiante). Duplicatas são reportadas na passada 2,
        # para que os diagnósticos saiam na ordem do código-fonte.
        duplicadas = {}
        for item in itens:
            if item.get("no") == "Funcao":
                param_list = [
                    (p.get("nome"), p.get("tipo"), bool(p.get("vetor")))
                    for p in item.get("parametros", [])
                ]
                sym = Symbol(
                    name=item["nome"],
                    type_name=normalizar_tipo(item["tipo"]),
                    category="func",
                    line=item.get("linha_nome", item.get("linha", 1)),
                    col=item.get("coluna_nome", item.get("coluna", 1)),
                    params=param_list
                )
                ok, prev = self.global_scope.define(sym)
                if not ok:
                    duplicadas[id(item)] = prev

        # Passada 2: analisar todas as declarações e comandos, em ordem
        for item in itens:
            tipo_no = item.get("no")
            if tipo_no == "Declaracao":
                self.visit_var_decl(item)
            elif tipo_no == "Funcao":
                if id(item) in duplicadas:
                    self.add_error_sem002(
                        item["nome"],
                        item.get("linha_nome", item.get("linha", 1)),
                        item.get("coluna_nome", item.get("coluna", 1)),
                        duplicadas[id(item)]
                    )
                self.visit_func_decl(item)
            else:
                self.visit_statement(item)

        if not self.errors:
            print("Análise semântica concluída: 0 erros; programa aceito.", end="")
            return True

        for err in self.errors:
            print(err)
        qtd = len(self.errors)
        s_plural = "s" if qtd > 1 else ""
        print(f"Análise semântica concluída: {qtd} erro{s_plural}; programa rejeitado.", end="")
        return False

    # --- DECLARAÇÕES ---

    def visit_var_decl(self, node):
        tamanho = node.get("tamanho")
        is_array = tamanho is not None
        sym = Symbol(
            name=node["nome"],
            type_name=normalizar_tipo(node["tipo"]),
            category="var",
            line=node.get("linha", 1),
            col=node.get("coluna", 1),
            is_array=is_array
        )
        ok, prev = self.current_scope.define(sym)
        if not ok:
            self.add_error_sem002(node["nome"], node.get("linha", 1), node.get("coluna", 1), prev)

        if is_array:
            try:
                n = int(str(tamanho.get("valor", "1")), 10)
            except ValueError:
                n = 1
            if n <= 0:
                self.add_error_sem020(node["nome"], *self._pos(tamanho, node))

        valor = node.get("valor")
        if valor is not None:
            expr_type = self.value_of(valor, "como expressão de atribuição")
            if expr_type and not self.is_assignable(sym.type_name, expr_type):
                self.add_error_sem003(node["nome"], sym.type_name, expr_type, texto_expr(valor),
                                      *self._pos(valor, node))

    def visit_func_decl(self, node):
        self.current_function = node
        self.loop_depth = 0
        self.enter_scope()

        # Registrar parâmetros no escopo local da função
        for p in node.get("parametros", []):
            psym = Symbol(
                name=p["nome"],
                type_name=normalizar_tipo(p["tipo"]),
                category="param",
                line=p.get("linha", 1),
                col=p.get("coluna", 1),
                is_array=bool(p.get("vetor"))
            )
            ok, prev = self.current_scope.define(psym)
            if not ok:
                self.add_error_sem002(p["nome"], p.get("linha", 1), p.get("coluna", 1), prev)

        # O corpo compartilha o escopo dos parâmetros
        corpo = node.get("corpo", {"no": "Bloco", "itens": []})
        for stmt in corpo.get("itens", []):
            self.visit_statement(stmt)

        # Checar retorno garantido em funções não-void (SEM011)
        tipo_ret = normalizar_tipo(node["tipo"])
        if tipo_ret != "void" and not sempre_retorna(corpo):
            self.add_error_sem011(node["nome"], tipo_ret, self.motivo_queda(corpo),
                                  node.get("linha", 1), node.get("coluna", 1))

        self.exit_scope()
        self.current_function = None

    @staticmethod
    def motivo_queda(corpo):
        """Descreve, para o SEM011, por qual caminho o fim do corpo é alcançado."""
        for st in reversed(corpo.get("itens", [])):
            if not isinstance(st, dict) or st.get("no") != "Se":
                continue
            ret_entao = sempre_retorna(st.get("entao"))
            ret_senao = sempre_retorna(st.get("senao"))
            cond = texto_expr(st.get("condicao"))
            if ret_entao and not ret_senao:
                return f"o ramo em que “{cond}” é falso alcança o fim do corpo"
            if ret_senao and not ret_entao:
                return f"o ramo em que “{cond}” é verdadeiro alcança o fim do corpo"
        return "o fim do corpo pode ser alcançado sem um comando return"

    # --- COMANDOS ---

    def visit_block(self, node):
        self.enter_scope()
        for stmt in node.get("itens", []):
            self.visit_statement(stmt)
        self.exit_scope()

    def check_condition(self, cond, cmd_name, fallback):
        c_type = self.value_of(cond, f"como condição de {cmd_name}")
        if c_type and c_type != "bool":
            self.add_error_sem005(cmd_name, c_type, texto_expr(cond), *self._pos(cond, fallback))

    def visit_loop_body(self, body):
        self.loop_depth += 1
        self.visit_statement(body)
        self.loop_depth -= 1

    def visit_statement(self, stmt):
        if not isinstance(stmt, dict):
            return

        tipo_no = stmt.get("no")

        if tipo_no == "Declaracao":
            self.visit_var_decl(stmt)

        elif tipo_no == "Bloco":
            self.visit_block(stmt)

        elif tipo_no == "Se":
            self.check_condition(stmt["condicao"], "if", stmt)
            self.visit_statement(stmt.get("entao"))
            if stmt.get("senao"):
                self.visit_statement(stmt.get("senao"))

        elif tipo_no == "Enquanto":
            self.check_condition(stmt["condicao"], "while", stmt)
            self.visit_loop_body(stmt.get("corpo"))

        elif tipo_no == "Para":
            if stmt.get("inicio"):
                self.check_expr(stmt["inicio"])
            if stmt.get("condicao"):
                self.check_condition(stmt["condicao"], "for", stmt)
            if stmt.get("passo"):
                self.check_expr(stmt["passo"])
            self.visit_loop_body(stmt.get("corpo"))

        elif tipo_no == "Retorno":
            self.visit_return(stmt)

        elif tipo_no == "Atribuicao":
            self.visit_assign(stmt)

        elif tipo_no == "ComandoExpressao":
            # Em posição de comando, uma chamada void é permitida
            if stmt.get("valor"):
                self.check_expr(stmt["valor"])

        elif tipo_no == "Imprimir":
            for arg in stmt.get("argumentos", []):
                self.value_of(arg, "como argumento de print")

        elif tipo_no in ("Break", "Continue"):
            if self.loop_depth == 0:
                self.add_error_sem018("break" if tipo_no == "Break" else "continue", *self._pos(stmt))

        # "Ler" e "Vazio" não têm restrições semânticas

    def visit_return(self, stmt):
        val = stmt.get("valor")
        func = self.current_function
        if func is None:
            self.add_error_sem019(*self._pos(stmt))
            if val is not None:
                self.check_expr(val)
            return

        expected = normalizar_tipo(func["tipo"])
        if expected == "void":
            if val is not None:
                self.check_expr(val)
                self.add_error_sem009_void(func["nome"], texto_expr(val), *self._pos(val, stmt))
            return

        if val is None:
            self.add_error_sem009("void", expected, func["nome"], *self._pos(stmt))
            return

        e_type = self.value_of(val, "como valor de retorno")
        if e_type and not self.is_assignable(expected, e_type):
            self.add_error_sem009(e_type, expected, func["nome"], *self._pos(val, stmt))

    # --- ATRIBUIÇÃO ---

    def visit_assign(self, node):
        """Analisa uma atribuição e devolve o tipo do destino (tipo da expressão)."""
        dest = node.get("destino")
        val = node.get("valor")

        target_type = self.check_target(dest, node)
        expr_type = self.value_of(val, "como expressão de atribuição")

        if target_type and expr_type and not self.is_assignable(target_type, expr_type):
            if isinstance(dest, dict) and dest.get("no") == "Indice":
                target_name = texto_expr(dest.get("base"))
            else:
                target_name = texto_expr(dest)
            self.add_error_sem003(target_name, target_type, expr_type, texto_expr(val),
                                  *self._pos(val, node))
        return target_type

    def check_target(self, dest, node):
        """Valida o destino de uma atribuição (valor-L) e devolve seu tipo escalar."""
        if not isinstance(dest, dict):
            return None
        line, col = self._pos(dest, node)
        tipo_no = dest.get("no")

        if tipo_no == "Identificador":
            sym = self.current_scope.lookup(dest["nome"])
            if not sym:
                self.add_error_sem001(dest["nome"], line, col)
                return None
            if sym.category == "func":
                self.add_error_sem013(
                    f"“{dest['nome']}” é uma função e não designa uma variável ou elemento de vetor", line, col)
                return None
            if sym.is_array:
                self.add_error_sem013(
                    f"o vetor “{dest['nome']}” não pode ser atribuído como um todo; atribua a um elemento",
                    line, col)
                return None
            return sym.type_name

        if tipo_no == "Indice":
            return self.check_expr(dest)

        if tipo_no == "Literal":
            desc = DESCRICAO_LITERAL.get(dest.get("tipo"), "o literal")
            self.add_error_sem013(
                f"{desc} “{dest.get('valor', '')}” não designa uma variável ou elemento de vetor", line, col)
            return None

        # Qualquer outra expressão (aritmética, chamada, atribuição...) não é atribuível
        self.check_expr(dest)
        self.add_error_sem013(
            f"a expressão “{texto_expr(dest)}” não designa uma variável ou elemento de vetor", line, col)
        return None

    @staticmethod
    def is_assignable(target_type, expr_type):
        if target_type == expr_type:
            return True
        # Promoção int -> float permitida
        return target_type == "float" and expr_type == "int"

    # --- CHECAGEM DE EXPRESSÕES E INFERÊNCIA DE TIPOS ---

    def value_of(self, node, contexto):
        """Checa uma expressão usada como valor: chamadas void não são permitidas (SEM012)."""
        t = self.check_expr(node)
        if t == "void":
            func = node.get("funcao", {}) if isinstance(node, dict) else {}
            self.add_error_sem012(texto_expr(func), contexto, *self._pos(node))
            return None
        return t

    def check_expr(self, node):
        if not isinstance(node, dict):
            return None

        tipo_no = node.get("no")
        line, col = self._pos(node)

        if tipo_no == "Literal":
            return normalizar_tipo(node.get("tipo"))

        if tipo_no == "Identificador":
            sym = self.current_scope.lookup(node["nome"])
            if not sym:
                self.add_error_sem001(node["nome"], line, col)
                return None
            if sym.category == "func":
                self.add_error_sem016(node["nome"], line, col)
                return None
            return sym.full_type

        if tipo_no == "Indice":
            return self.check_index(node)

        if tipo_no == "Chamada":
            return self.check_call(node)

        if tipo_no == "Atribuicao":
            return self.visit_assign(node)

        if tipo_no == "Binaria":
            return self.check_binary(node)

        if tipo_no == "Unaria":
            op = node["operador"]
            e_type = self.value_of(node["valor"], f"como operando de “{op}”")
            if op == "!":
                if e_type and e_type != "bool":
                    self.add_error_sem014_un(op, e_type, texto_expr(node), line, col)
                return "bool"
            # menos unário
            if e_type and not eh_numerico(e_type):
                self.add_error_sem014_un(op, e_type, texto_expr(node), line, col)
                return None
            return e_type

        return None

    def check_index(self, node):
        base = node.get("base", {})
        indice = node.get("indice")
        b_line, b_col = self._pos(base, node)
        base_name = texto_expr(base)
        elem_type = None

        if isinstance(base, dict) and base.get("no") == "Identificador":
            sym = self.current_scope.lookup(base["nome"])
            if not sym:
                self.add_error_sem001(base["nome"], b_line, b_col)
            elif sym.category == "func":
                self.add_error_sem015(base_name, "função", b_line, b_col)
            elif not sym.is_array:
                self.add_error_sem015(base_name, sym.type_name, b_line, b_col)
            else:
                elem_type = sym.type_name
        else:
            b_type = self.value_of(base, "como vetor")
            if b_type and not eh_vetor(b_type):
                self.add_error_sem015(base_name, b_type, b_line, b_col)
            elif b_type:
                elem_type = b_type[:-2]

        idx_type = self.value_of(indice, "como índice de vetor")
        if idx_type and idx_type != "int":
            self.add_error_sem006(base_name, idx_type, texto_expr(indice), *self._pos(indice, node))
        return elem_type

    def check_call(self, node):
        func = node.get("funcao", {})
        args = node.get("argumentos", [])
        f_line, f_col = self._pos(func, node)
        func_name = texto_expr(func)

        sym = None
        if isinstance(func, dict) and func.get("no") == "Identificador":
            # Respeita o escopo léxico: uma variável local pode sombrear uma função global
            sym = self.current_scope.lookup(func_name)
            if not sym:
                self.add_error_sem001(func_name, f_line, f_col)
            elif sym.category != "func":
                self.add_error_sem017(func_name, sym.full_type, f_line, f_col)
                sym = None
        else:
            f_type = self.check_expr(func)
            if f_type:
                self.add_error_sem017(func_name, f_type, f_line, f_col)

        if sym is None:
            # Ainda analisa os argumentos para reportar erros contidos neles
            for arg in args:
                self.check_expr(arg)
            return None

        expected_params = sym.params
        if len(args) != len(expected_params):
            self.add_error_sem007(func_name, len(expected_params), len(args), f_line, f_col)

        for idx, arg in enumerate(args, start=1):
            a_type = self.value_of(arg, f"como argumento de “{func_name}”")
            if idx > len(expected_params) or not a_type:
                continue
            _, p_type, p_is_array = expected_params[idx - 1]
            p_type = normalizar_tipo(p_type)
            expected_str = f"{p_type}[]" if p_is_array else p_type
            if p_is_array:
                match = a_type == expected_str
            else:
                match = self.is_assignable(p_type, a_type)
            if not match:
                self.add_error_sem008(idx, func_name, expected_str, a_type, texto_expr(arg),
                                      *self._pos(arg, func))

        return sym.type_name

    def check_binary(self, node):
        op = node["operador"]
        contexto = f"como operando de “{op}”"
        l_type = self.value_of(node["esquerda"], contexto)
        r_type = self.value_of(node["direita"], contexto)
        line, col = self._pos(node)
        ambos = l_type is not None and r_type is not None

        def erro():
            self.add_error_sem014_bin(op, l_type, r_type, texto_expr(node), line, col)

        if op in OPS_ARITMETICOS:
            if ambos:
                if not (eh_numerico(l_type) and eh_numerico(r_type)):
                    erro()
                    return None
                return "float" if "float" in (l_type, r_type) else "int"
            conhecido = l_type or r_type
            return conhecido if eh_numerico(conhecido) else None

        if op == "%":
            if ambos and not (l_type == "int" and r_type == "int"):
                erro()
                return None
            return "int"

        if op in OPS_RELACIONAIS:
            if ambos and not ((eh_numerico(l_type) and eh_numerico(r_type))
                              or (l_type == r_type == "char")):
                erro()
            return "bool"

        if op in OPS_IGUALDADE:
            if ambos and not ((eh_numerico(l_type) and eh_numerico(r_type))
                              or (l_type == r_type and l_type in ("bool", "char"))):
                erro()
            return "bool"

        if op in OPS_LOGICOS:
            if ambos and not (l_type == "bool" and r_type == "bool"):
                erro()
            return "bool"

        return None


# ==============================================================================
# PONTO DE ENTRADA (MAIN)
# ==============================================================================

def main():
    if any(arg in {"--ir", "-O0", "-O1"} for arg in sys.argv[1:]):
        from representacao_intermediaria_python.representacao_intermediaria import main as ir_main
        sys.exit(ir_main())

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

    # Código de saída 0 = programa aceito; 1 = programa rejeitado
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()