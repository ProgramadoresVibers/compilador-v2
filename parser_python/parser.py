"""
Analisador Sintático (Parser) LL(1) para a linguagem MiniC.
Utiliza a técnica LL(1) com tabela de consulta [Variável, Terminal],
gramática corrigida (sem conflitos, fatorada e com função main opcional),
construção de AST e integração direta com o scanner.py.
"""

import sys
import io
import json
from pathlib import Path

# Força o terminal a ler e escrever em UTF-8
if sys.stdout.encoding != 'utf-8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

# Para achar o scanner
raiz_projeto = str(Path(__file__).resolve().parent.parent)
if raiz_projeto not in sys.path:
    sys.path.insert(0, raiz_projeto)

from scanner_python.scanner import analisar_arquivo as scanner_analisar_arquivo

# ==============================================================================
# DEFINIÇÕES DE TOKENS E TIPOS
# ==============================================================================
TIPOS = {"INT", "FLOAT", "BOOL", "CHAR"}
TIPOS_RETORNO = {"INT", "FLOAT", "BOOL", "CHAR", "VOID"}

NOMES_TOKENS = {
    "IDENT": "identificador",
    "INT_LIT": "literal inteiro",
    "FLOAT_LIT": "literal real",
    "CHAR_LIT": "literal caractere",
    "STRING_LIT": "literal cadeia",
    "SEMICOLON": "';' (ponto e vírgula)",
    "COMMA": "',' (vírgula)",
    "LPAREN": "'(' (abre parêntese)",
    "RPAREN": "')' (fecha parêntese)",
    "LBRACE": "'{' (abre chave)",
    "RBRACE": "'}' (fecha chave)",
    "LBRACKET": "'[' (abre colchete)",
    "RBRACKET": "']' (fecha colchete)",
    "ASSIGN": "'=' (atribuição)",
    "EOF": "fim de arquivo (EOF)",
}

# ==============================================================================
# TABELA DE CONSULTA LL(1) [Variável (Não-Terminal), Terminal]
# ==============================================================================
# Mapeia pares (nao_terminal, terminal) para a regra de produção a ser aplicada.
# Conforme a gramática formal corrigida e desprovida de conflitos.
TABELA_LL1 = {
    # programa -> lista_elementos EOF
    ("programa", "INT"): 1, ("programa", "FLOAT"): 1, ("programa", "BOOL"): 1,
    ("programa", "CHAR"): 1, ("programa", "VOID"): 1, ("programa", "LBRACE"): 1,
    ("programa", "IF"): 1, ("programa", "WHILE"): 1, ("programa", "FOR"): 1,
    ("programa", "RETURN"): 1, ("programa", "BREAK"): 1, ("programa", "CONTINUE"): 1,
    ("programa", "PRINT"): 1, ("programa", "READ"): 1, ("programa", "SEMICOLON"): 1,
    ("programa", "IDENT"): 1, ("programa", "INT_LIT"): 1, ("programa", "FLOAT_LIT"): 1,
    ("programa", "TRUE"): 1, ("programa", "FALSE"): 1, ("programa", "CHAR_LIT"): 1,
    ("programa", "STRING_LIT"): 1, ("programa", "LPAREN"): 1, ("programa", "MINUS"): 1,
    ("programa", "NOT"): 1, ("programa", "EOF"): 1,

    # lista_elementos -> elemento lista_elementos | epsilon
    ("lista_elementos", "INT"): 2, ("lista_elementos", "FLOAT"): 2, ("lista_elementos", "BOOL"): 2,
    ("lista_elementos", "CHAR"): 2, ("lista_elementos", "VOID"): 2, ("lista_elementos", "LBRACE"): 2,
    ("lista_elementos", "IF"): 2, ("lista_elementos", "WHILE"): 2, ("lista_elementos", "FOR"): 2,
    ("lista_elementos", "RETURN"): 2, ("lista_elementos", "BREAK"): 2, ("lista_elementos", "CONTINUE"): 2,
    ("lista_elementos", "PRINT"): 2, ("lista_elementos", "READ"): 2, ("lista_elementos", "SEMICOLON"): 2,
    ("lista_elementos", "IDENT"): 2, ("lista_elementos", "INT_LIT"): 2, ("lista_elementos", "FLOAT_LIT"): 2,
    ("lista_elementos", "TRUE"): 2, ("lista_elementos", "FALSE"): 2, ("lista_elementos", "CHAR_LIT"): 2,
    ("lista_elementos", "STRING_LIT"): 2, ("lista_elementos", "LPAREN"): 2, ("lista_elementos", "MINUS"): 2,
    ("lista_elementos", "NOT"): 2, ("lista_elementos", "EOF"): 3,

    # elemento -> decl_ou_funcao | comando
    ("elemento", "INT"): 4, ("elemento", "FLOAT"): 4, ("elemento", "BOOL"): 4,
    ("elemento", "CHAR"): 4, ("elemento", "VOID"): 4,
    ("elemento", "LBRACE"): 5, ("elemento", "IF"): 5, ("elemento", "WHILE"): 5,
    ("elemento", "FOR"): 5, ("elemento", "RETURN"): 5, ("elemento", "BREAK"): 5,
    ("elemento", "CONTINUE"): 5, ("elemento", "PRINT"): 5, ("elemento", "READ"): 5,
    ("elemento", "SEMICOLON"): 5, ("elemento", "IDENT"): 5, ("elemento", "INT_LIT"): 5,
    ("elemento", "FLOAT_LIT"): 5, ("elemento", "TRUE"): 5, ("elemento", "FALSE"): 5,
    ("elemento", "CHAR_LIT"): 5, ("elemento", "STRING_LIT"): 5, ("elemento", "LPAREN"): 5,
    ("elemento", "MINUS"): 5, ("elemento", "NOT"): 5,

    # resto_decl_func -> ( resto_df | resto_d resto_dl
    ("resto_decl_func", "LPAREN"): 7,
    ("resto_decl_func", "ASSIGN"): 8, ("resto_decl_func", "LBRACKET"): 8,
    ("resto_decl_func", "COMMA"): 8, ("resto_decl_func", "SEMICOLON"): 8,

    # resto_df -> parametros ) bloco | ) bloco
    ("resto_df", "INT"): 9, ("resto_df", "FLOAT"): 9, ("resto_df", "BOOL"): 9, ("resto_df", "CHAR"): 9,
    ("resto_df", "RPAREN"): 10,

    # tipo_retorno -> tipo | VOID
    ("tipo_retorno", "INT"): 11, ("tipo_retorno", "FLOAT"): 11,
    ("tipo_retorno", "BOOL"): 11, ("tipo_retorno", "CHAR"): 11,
    ("tipo_retorno", "VOID"): 12,

    # tipo -> INT | FLOAT | BOOL | CHAR
    ("tipo", "INT"): 13, ("tipo", "FLOAT"): 14, ("tipo", "BOOL"): 15, ("tipo", "CHAR"): 16,

    # parametros -> parametro resto_parametros
    ("parametros", "INT"): 17, ("parametros", "FLOAT"): 17, ("parametros", "BOOL"): 17, ("parametros", "CHAR"): 17,

    # resto_parametros -> , parametro resto_parametros | epsilon
    ("resto_parametros", "COMMA"): 18, ("resto_parametros", "RPAREN"): 19,

    # resto_parametro -> [ ] | epsilon
    ("resto_parametro", "LBRACKET"): 21, ("resto_parametro", "COMMA"): 22, ("resto_parametro", "RPAREN"): 22,

    # bloco -> { itens_bloco }
    ("bloco", "LBRACE"): 23,

    # itens_bloco -> item_bloco itens_bloco | epsilon
    ("itens_bloco", "INT"): 24, ("itens_bloco", "FLOAT"): 24, ("itens_bloco", "BOOL"): 24,
    ("itens_bloco", "CHAR"): 24, ("itens_bloco", "LBRACE"): 24, ("itens_bloco", "IF"): 24,
    ("itens_bloco", "WHILE"): 24, ("itens_bloco", "FOR"): 24, ("itens_bloco", "RETURN"): 24,
    ("itens_bloco", "BREAK"): 24, ("itens_bloco", "CONTINUE"): 24, ("itens_bloco", "PRINT"): 24,
    ("itens_bloco", "READ"): 24, ("itens_bloco", "SEMICOLON"): 24, ("itens_bloco", "IDENT"): 24,
    ("itens_bloco", "INT_LIT"): 24, ("itens_bloco", "FLOAT_LIT"): 24, ("itens_bloco", "TRUE"): 24,
    ("itens_bloco", "FALSE"): 24, ("itens_bloco", "CHAR_LIT"): 24, ("itens_bloco", "STRING_LIT"): 24,
    ("itens_bloco", "LPAREN"): 24, ("itens_bloco", "MINUS"): 24, ("itens_bloco", "NOT"): 24,
    ("itens_bloco", "RBRACE"): 25,

    # item_bloco -> declaracao_local | comando
    ("item_bloco", "INT"): 26, ("item_bloco", "FLOAT"): 26,
    ("item_bloco", "BOOL"): 26, ("item_bloco", "CHAR"): 26,
    ("item_bloco", "LBRACE"): 27, ("item_bloco", "IF"): 27, ("item_bloco", "WHILE"): 27,
    ("item_bloco", "FOR"): 27, ("item_bloco", "RETURN"): 27, ("item_bloco", "BREAK"): 27,
    ("item_bloco", "CONTINUE"): 27, ("item_bloco", "PRINT"): 27, ("item_bloco", "READ"): 27,
    ("item_bloco", "SEMICOLON"): 27, ("item_bloco", "IDENT"): 27, ("item_bloco", "INT_LIT"): 27,
    ("item_bloco", "FLOAT_LIT"): 27, ("item_bloco", "TRUE"): 27, ("item_bloco", "FALSE"): 27,
    ("item_bloco", "CHAR_LIT"): 27, ("item_bloco", "STRING_LIT"): 27, ("item_bloco", "LPAREN"): 27,
    ("item_bloco", "MINUS"): 27, ("item_bloco", "NOT"): 27,

    # resto_dl -> , declarador resto_dl | ;
    ("resto_dl", "COMMA"): 29, ("resto_dl", "SEMICOLON"): 30,

    # resto_d -> inicializacao | [ tamanho ] | epsilon
    ("resto_d", "ASSIGN"): 32, ("resto_d", "LBRACKET"): 33,
    ("resto_d", "COMMA"): 34, ("resto_d", "SEMICOLON"): 34,

    # inicializacao -> = expressao
    ("inicializacao", "ASSIGN"): 36,

    # comando
    ("comando", "LBRACE"): 37,
    ("comando", "IF"): 38,
    ("comando", "WHILE"): 39,
    ("comando", "FOR"): 40,
    ("comando", "RETURN"): 41,
    ("comando", "BREAK"): 42,
    ("comando", "CONTINUE"): 43,
    ("comando", "PRINT"): 44,
    ("comando", "READ"): 45,
    ("comando", "SEMICOLON"): 46, ("comando", "IDENT"): 46, ("comando", "INT_LIT"): 46,
    ("comando", "FLOAT_LIT"): 46, ("comando", "TRUE"): 46, ("comando", "FALSE"): 46,
    ("comando", "CHAR_LIT"): 46, ("comando", "STRING_LIT"): 46, ("comando", "LPAREN"): 46,
    ("comando", "MINUS"): 46, ("comando", "NOT"): 46,

    # resto_cm_if -> ELSE comando | epsilon (dangling else resolvido em favor de ELSE)
    ("resto_cm_if", "ELSE"): 49,
    ("resto_cm_if", "INT"): 50, ("resto_cm_if", "FLOAT"): 50, ("resto_cm_if", "BOOL"): 50,
    ("resto_cm_if", "CHAR"): 50, ("resto_cm_if", "VOID"): 50, ("resto_cm_if", "LBRACE"): 50,
    ("resto_cm_if", "IF"): 50, ("resto_cm_if", "WHILE"): 50, ("resto_cm_if", "FOR"): 50,
    ("resto_cm_if", "RETURN"): 50, ("resto_cm_if", "BREAK"): 50, ("resto_cm_if", "CONTINUE"): 50,
    ("resto_cm_if", "PRINT"): 50, ("resto_cm_if", "READ"): 50, ("resto_cm_if", "SEMICOLON"): 50,
    ("resto_cm_if", "IDENT"): 50, ("resto_cm_if", "INT_LIT"): 50, ("resto_cm_if", "FLOAT_LIT"): 50,
    ("resto_cm_if", "TRUE"): 50, ("resto_cm_if", "FALSE"): 50, ("resto_cm_if", "CHAR_LIT"): 50,
    ("resto_cm_if", "STRING_LIT"): 50, ("resto_cm_if", "LPAREN"): 50, ("resto_cm_if", "MINUS"): 50,
    ("resto_cm_if", "NOT"): 50, ("resto_cm_if", "RBRACE"): 50, ("resto_cm_if", "EOF"): 50,

    # exp_opcional -> expressao | epsilon
    ("exp_opcional", "IDENT"): 60, ("exp_opcional", "INT_LIT"): 60, ("exp_opcional", "FLOAT_LIT"): 60,
    ("exp_opcional", "TRUE"): 60, ("exp_opcional", "FALSE"): 60, ("exp_opcional", "CHAR_LIT"): 60,
    ("exp_opcional", "STRING_LIT"): 60, ("exp_opcional", "LPAREN"): 60, ("exp_opcional", "MINUS"): 60,
    ("exp_opcional", "NOT"): 60,
    ("exp_opcional", "SEMICOLON"): 61, ("exp_opcional", "RPAREN"): 61,

    # expressao -> expressao_or resto_atribuicao
    ("expressao", "IDENT"): 62, ("expressao", "INT_LIT"): 62, ("expressao", "FLOAT_LIT"): 62,
    ("expressao", "TRUE"): 62, ("expressao", "FALSE"): 62, ("expressao", "CHAR_LIT"): 62,
    ("expressao", "STRING_LIT"): 62, ("expressao", "LPAREN"): 62, ("expressao", "MINUS"): 62,
    ("expressao", "NOT"): 62,

    # resto_atribuicao -> = expressao | epsilon
    ("resto_atribuicao", "ASSIGN"): 63,
    ("resto_atribuicao", "SEMICOLON"): 64, ("resto_atribuicao", "RPAREN"): 64,
    ("resto_atribuicao", "COMMA"): 64, ("resto_atribuicao", "RBRACKET"): 64,

    # aux_exp_or -> || expressao_and aux_exp_or | epsilon
    ("aux_exp_or", "OR"): 66,
    ("aux_exp_or", "ASSIGN"): 67, ("aux_exp_or", "SEMICOLON"): 67,
    ("aux_exp_or", "RPAREN"): 67, ("aux_exp_or", "COMMA"): 67, ("aux_exp_or", "RBRACKET"): 67,

    # aux_exp_and -> && expressao_igualdade aux_exp_and | epsilon
    ("aux_exp_and", "AND"): 69,
    ("aux_exp_and", "OR"): 70, ("aux_exp_and", "ASSIGN"): 70, ("aux_exp_and", "SEMICOLON"): 70,
    ("aux_exp_and", "RPAREN"): 70, ("aux_exp_and", "COMMA"): 70, ("aux_exp_and", "RBRACKET"): 70,

    # aux_exp_igualdade -> == expr | != expr | epsilon
    ("aux_exp_igualdade", "EQ"): 72, ("aux_exp_igualdade", "NE"): 73, ("aux_exp_igualdade", "NEQ"): 73,
    ("aux_exp_igualdade", "AND"): 74, ("aux_exp_igualdade", "OR"): 74, ("aux_exp_igualdade", "ASSIGN"): 74,
    ("aux_exp_igualdade", "SEMICOLON"): 74, ("aux_exp_igualdade", "RPAREN"): 74,
    ("aux_exp_igualdade", "COMMA"): 74, ("aux_exp_igualdade", "RBRACKET"): 74,

    # aux_exp_relacional -> op_rel expr | epsilon
    ("aux_exp_relacional", "LT"): 76, ("aux_exp_relacional", "GT"): 76,
    ("aux_exp_relacional", "LE"): 76, ("aux_exp_relacional", "GE"): 76,
    ("aux_exp_relacional", "EQ"): 77, ("aux_exp_relacional", "NE"): 77, ("aux_exp_relacional", "NEQ"): 77,
    ("aux_exp_relacional", "AND"): 77, ("aux_exp_relacional", "OR"): 77, ("aux_exp_relacional", "ASSIGN"): 77,
    ("aux_exp_relacional", "SEMICOLON"): 77, ("aux_exp_relacional", "RPAREN"): 77,
    ("aux_exp_relacional", "COMMA"): 77, ("aux_exp_relacional", "RBRACKET"): 77,

    # aux_exp_aditiva -> + expr | - expr | epsilon
    ("aux_exp_aditiva", "PLUS"): 83, ("aux_exp_aditiva", "MINUS"): 84,
    ("aux_exp_aditiva", "LT"): 85, ("aux_exp_aditiva", "GT"): 85,
    ("aux_exp_aditiva", "LE"): 85, ("aux_exp_aditiva", "GE"): 85,
    ("aux_exp_aditiva", "EQ"): 85, ("aux_exp_aditiva", "NE"): 85, ("aux_exp_aditiva", "NEQ"): 85,
    ("aux_exp_aditiva", "AND"): 85, ("aux_exp_aditiva", "OR"): 85, ("aux_exp_aditiva", "ASSIGN"): 85,
    ("aux_exp_aditiva", "SEMICOLON"): 85, ("aux_exp_aditiva", "RPAREN"): 85,
    ("aux_exp_aditiva", "COMMA"): 85, ("aux_exp_aditiva", "RBRACKET"): 85,

    # aux_exp_multiplicativa -> * expr | / expr | % expr | epsilon
    ("aux_exp_multiplicativa", "STAR"): 87, ("aux_exp_multiplicativa", "SLASH"): 88,
    ("aux_exp_multiplicativa", "PERCENT"): 89,
    ("aux_exp_multiplicativa", "PLUS"): 90, ("aux_exp_multiplicativa", "MINUS"): 90,
    ("aux_exp_multiplicativa", "LT"): 90, ("aux_exp_multiplicativa", "GT"): 90,
    ("aux_exp_multiplicativa", "LE"): 90, ("aux_exp_multiplicativa", "GE"): 90,
    ("aux_exp_multiplicativa", "EQ"): 90, ("aux_exp_multiplicativa", "NE"): 90, ("aux_exp_multiplicativa", "NEQ"): 90,
    ("aux_exp_multiplicativa", "AND"): 90, ("aux_exp_multiplicativa", "OR"): 90, ("aux_exp_multiplicativa", "ASSIGN"): 90,
    ("aux_exp_multiplicativa", "SEMICOLON"): 90, ("aux_exp_multiplicativa", "RPAREN"): 90,
    ("aux_exp_multiplicativa", "COMMA"): 90, ("aux_exp_multiplicativa", "RBRACKET"): 90,

    # expressao_unaria -> - expr | ! expr | expressao_posfixa
    ("expressao_unaria", "MINUS"): 91, ("expressao_unaria", "NOT"): 92,
    ("expressao_unaria", "IDENT"): 93, ("expressao_unaria", "INT_LIT"): 93,
    ("expressao_unaria", "FLOAT_LIT"): 93, ("expressao_unaria", "TRUE"): 93,
    ("expressao_unaria", "FALSE"): 93, ("expressao_unaria", "CHAR_LIT"): 93,
    ("expressao_unaria", "STRING_LIT"): 93, ("expressao_unaria", "LPAREN"): 93,

    # aux_exp_posfixa -> [ expr ] | ( resto_chamada | epsilon
    ("aux_exp_posfixa", "LBRACKET"): 95,
    ("aux_exp_posfixa", "LPAREN"): 96,
    ("aux_exp_posfixa", "STAR"): 99, ("aux_exp_posfixa", "SLASH"): 99,
    ("aux_exp_posfixa", "PERCENT"): 99, ("aux_exp_posfixa", "PLUS"): 99,
    ("aux_exp_posfixa", "MINUS"): 99, ("aux_exp_posfixa", "LT"): 99,
    ("aux_exp_posfixa", "GT"): 99, ("aux_exp_posfixa", "LE"): 99,
    ("aux_exp_posfixa", "GE"): 99, ("aux_exp_posfixa", "EQ"): 99,
    ("aux_exp_posfixa", "NE"): 99, ("aux_exp_posfixa", "NEQ"): 99,
    ("aux_exp_posfixa", "AND"): 99, ("aux_exp_posfixa", "OR"): 99,
    ("aux_exp_posfixa", "ASSIGN"): 99, ("aux_exp_posfixa", "SEMICOLON"): 99,
    ("aux_exp_posfixa", "RPAREN"): 99, ("aux_exp_posfixa", "COMMA"): 99,
    ("aux_exp_posfixa", "RBRACKET"): 99,

    # resto_chamada -> ) | argumentos )
    ("resto_chamada", "RPAREN"): 97,
    ("resto_chamada", "IDENT"): 98, ("resto_chamada", "INT_LIT"): 98,
    ("resto_chamada", "FLOAT_LIT"): 98, ("resto_chamada", "TRUE"): 98,
    ("resto_chamada", "FALSE"): 98, ("resto_chamada", "CHAR_LIT"): 98,
    ("resto_chamada", "STRING_LIT"): 98, ("resto_chamada", "LPAREN"): 98,
    ("resto_chamada", "MINUS"): 98, ("resto_chamada", "NOT"): 98,

    # resto_argumentos -> , expressao resto_argumentos | epsilon
    ("resto_argumentos", "COMMA"): 100, ("resto_argumentos", "RPAREN"): 101,

    # primario
    ("primario", "IDENT"): 102, ("primario", "INT_LIT"): 103,
    ("primario", "FLOAT_LIT"): 104, ("primario", "TRUE"): 105,
    ("primario", "FALSE"): 106, ("primario", "CHAR_LIT"): 107,
    ("primario", "STRING_LIT"): 108, ("primario", "LPAREN"): 109,
}


# ==============================================================================
# CLASSE DE EXCEÇÃO SINTÁTICA
# ==============================================================================
class ErroSintatico(SyntaxError):
    """Exceção para erros de sintaxe encontrados durante a análise."""
    def __init__(self, mensagem, token):
        self.token = token
        self.linha = token.get("line", 1)
        self.coluna = token.get("column", 1)
        super().__init__(
            f"Erro sintático na linha {self.linha}, coluna {self.coluna}: "
            f"{mensagem}; encontrado {token.get('token')} ({token.get('lexeme')!r})."
        )


# ==============================================================================
# ESTRUTURA DO PARSER LL(1)
# ==============================================================================
class ParserLL1:
    def __init__(self, tokens):
        self.tokens = self._adaptar_tokens(tokens)
        self.posicao = 0

    @staticmethod
    def _adaptar_tokens(tokens_originais):
        """
        Normaliza os tokens gerados pelo scanner:
        1. Desmembra literais numéricos com sinal negativo (ex: -5 vira MINUS e 5),
           permitindo que a regra expressao_unaria -> '-' expressao_unaria crie o nó Unary(-, Lit).
        2. Normaliza NEQ como NE para unificar a regra de desigualdade.
        """
        adaptados = []
        for t in tokens_originais:
            tok_type = t.get("token")
            lexeme = str(t.get("lexeme", ""))
            line = t.get("line", 1)
            col = t.get("column", 1)

            if tok_type == "INT_LIT" and lexeme.startswith("-"):
                adaptados.append({
                    "token": "MINUS", "lexeme": "-", "attribute": None,
                    "line": line, "column": col
                })
                resto = lexeme[1:]
                adaptados.append({
                    "token": "INT_LIT", "lexeme": resto, "attribute": int(resto),
                    "line": line, "column": col + 1
                })
            elif tok_type == "FLOAT_LIT" and lexeme.startswith("-"):
                adaptados.append({
                    "token": "MINUS", "lexeme": "-", "attribute": None,
                    "line": line, "column": col
                })
                resto = lexeme[1:]
                adaptados.append({
                    "token": "FLOAT_LIT", "lexeme": resto, "attribute": float(resto),
                    "line": line, "column": col + 1
                })
            else:
                adaptados.append(t)
        return adaptados

    def atual(self):
        if self.posicao < len(self.tokens):
            return self.tokens[self.posicao]
        return {"token": "EOF", "lexeme": "", "line": 1, "column": 1}

    def verificar(self, *tipos):
        return self.atual()["token"] in tipos

    def falhar(self, mensagem):
        raise ErroSintatico(mensagem, self.atual())

    def consumir(self, tipo):
        if not self.verificar(tipo):
            nome_esperado = NOMES_TOKENS.get(tipo, tipo)
            self.falhar(f"esperado {nome_esperado}")
        tok = self.atual()
        self.posicao += 1
        return tok

    def aceitar(self, *tipos):
        if self.verificar(*tipos):
            tok = self.atual()
            self.posicao += 1
            return tok
        return None

    def consultar_tabela(self, variavel):
        """Consulta a tabela de análise sintática LL(1) M[Variável, Terminal]."""
        terminal = self.atual()["token"]
        regra = TABELA_LL1.get((variavel, terminal))
        return regra

    # --------------------------------------------------------------------------
    # Regras Gramaticais e Construção da AST
    # --------------------------------------------------------------------------
    def parse(self):
        """Ponto de entrada: programa -> lista_elementos EOF"""
        regra = self.consultar_tabela("programa")
        if regra is None:
            self.falhar("esperado início de programa")
        itens = []
        while not self.verificar("EOF"):
            itens.extend(self.elemento())
        self.consumir("EOF")
        return {"no": "Programa", "itens": itens}

    def elemento(self):
        """elemento -> decl_ou_funcao | comando"""
        if self.verificar(*(TIPOS | {"VOID"})):
            return self.decl_ou_funcao()
        else:
            cmd = self.comando()
            return [cmd]

    def ler_tipo(self, permite_void=False):
        permitidos = TIPOS | ({"VOID"} if permite_void else set())
        if not self.verificar(*permitidos):
            msg = "esperado tipo: int, float, bool ou char" + (" ou void" if permite_void else "")
            self.falhar(msg)
        return self.consumir(self.atual()["token"])["lexeme"]

    def decl_ou_funcao(self):
        """decl_ou_funcao -> tipo_retorno IDENT resto_decl_func"""
        tipo = self.ler_tipo(permite_void=True)
        if not self.verificar("IDENT"):
            self.falhar("esperado identificador")
        nome = self.consumir("IDENT")["lexeme"]

        # resto_decl_func: se for '(', é declaração de função; caso contrário, é variável
        if self.aceitar("LPAREN"):
            parametros = self.lista_parametros()
            self.consumir("RPAREN")
            corpo = self.bloco()
            return [{"no": "Funcao", "tipo": tipo, "nome": nome, "parametros": parametros, "corpo": corpo}]

        if tipo == "void":
            self.falhar("void exige uma função, com '(' após o nome")

        return self.resto_declaracao(tipo, nome)

    def declaracao_local(self):
        tipo = self.ler_tipo(permite_void=False)
        if not self.verificar("IDENT"):
            self.falhar("esperado identificador")
        nome = self.consumir("IDENT")["lexeme"]
        return self.resto_declaracao(tipo, nome)

    def resto_declaracao(self, tipo, nome):
        decls = [self.declarador(tipo, nome)]
        while self.aceitar("COMMA"):
            if not self.verificar("IDENT"):
                self.falhar("esperado identificador após vírgula")
            prox_nome = self.consumir("IDENT")["lexeme"]
            decls.append(self.declarador(tipo, prox_nome))
        self.consumir("SEMICOLON")
        return decls

    def declarador(self, tipo, nome):
        tamanho = None
        valor = None
        if self.aceitar("LBRACKET"):
            tok_tam = self.consumir("INT_LIT")
            tamanho = {"no": "Literal", "tipo": "int", "valor": tok_tam["lexeme"]}
            self.consumir("RBRACKET")
        elif self.aceitar("ASSIGN"):
            valor = self.expressao()
        return {"no": "Declaracao", "tipo": tipo, "nome": nome, "tamanho": tamanho, "valor": valor}

    def lista_parametros(self):
        parametros = []
        if self.verificar("RPAREN"):
            return parametros
        while True:
            tipo = self.ler_tipo(permite_void=False)
            if not self.verificar("IDENT"):
                self.falhar("esperado identificador do parâmetro")
            nome = self.consumir("IDENT")["lexeme"]
            vetor = bool(self.aceitar("LBRACKET"))
            if vetor:
                self.consumir("RBRACKET")
            parametros.append({"no": "Parametro", "tipo": tipo, "nome": nome, "vetor": vetor})
            if not self.aceitar("COMMA"):
                break
        return parametros

    def bloco(self):
        self.consumir("LBRACE")
        itens = []
        while not self.verificar("RBRACE"):
            if self.verificar("EOF"):
                self.falhar("esperado '}' para fechar o bloco")
            if self.verificar(*TIPOS):
                itens.extend(self.declaracao_local())
            else:
                itens.append(self.comando())
        self.consumir("RBRACE")
        return {"no": "Bloco", "itens": itens}

    def comando(self):
        tok_type = self.atual()["token"]
        if tok_type == "LBRACE":
            return self.bloco()
        if tok_type == "IF":
            return self.comando_if()
        if tok_type == "WHILE":
            return self.comando_while()
        if tok_type == "FOR":
            return self.comando_for()
        if tok_type == "RETURN":
            return self.comando_return()
        if tok_type == "PRINT":
            return self.comando_print()
        if tok_type == "READ":
            return self.comando_read()
        if tok_type in {"BREAK", "CONTINUE"}:
            self.consumir(tok_type)
            self.consumir("SEMICOLON")
            return {"no": "Break" if tok_type == "BREAK" else "Continue"}
        if self.aceitar("SEMICOLON"):
            return {"no": "Vazio"}
        if tok_type in {"RBRACE", "RPAREN", "RBRACKET", "ELSE", "EOF"}:
            self.falhar("esperado início de comando")

        valor = self.expressao()
        self.consumir("SEMICOLON")
        return {"no": "ComandoExpressao", "valor": valor}

    def comando_if(self):
        self.consumir("IF")
        self.consumir("LPAREN")
        condicao = self.expressao()
        self.consumir("RPAREN")
        entao = self.comando()
        senao = self.comando() if self.aceitar("ELSE") else None
        return {"no": "Se", "condicao": condicao, "entao": entao, "senao": senao}

    def comando_while(self):
        self.consumir("WHILE")
        self.consumir("LPAREN")
        condicao = self.expressao()
        self.consumir("RPAREN")
        corpo = self.comando()
        return {"no": "Enquanto", "condicao": condicao, "corpo": corpo}

    def expressao_opcional(self, delimitador_fim):
        if self.verificar(delimitador_fim):
            return None
        return self.expressao()

    def comando_for(self):
        self.consumir("FOR")
        self.consumir("LPAREN")
        inicio = self.expressao_opcional("SEMICOLON")
        self.consumir("SEMICOLON")
        condicao = self.expressao_opcional("SEMICOLON")
        self.consumir("SEMICOLON")
        passo = self.expressao_opcional("RPAREN")
        self.consumir("RPAREN")
        corpo = self.comando()
        return {"no": "Para", "inicio": inicio, "condicao": condicao, "passo": passo, "corpo": corpo}

    def comando_return(self):
        self.consumir("RETURN")
        valor = self.expressao_opcional("SEMICOLON")
        self.consumir("SEMICOLON")
        return {"no": "Retorno", "valor": valor}

    def comando_print(self):
        self.consumir("PRINT")
        self.consumir("LPAREN")
        argumentos = self.lista_argumentos()
        self.consumir("RPAREN")
        self.consumir("SEMICOLON")
        return {"no": "Imprimir", "argumentos": argumentos}

    def comando_read(self):
        self.consumir("READ")
        self.consumir("LPAREN")
        self.consumir("RPAREN")
        self.consumir("SEMICOLON")
        return {"no": "Ler"}

    # --------------------------------------------------------------------------
    # Expressões com Precedência e Associatividade
    # --------------------------------------------------------------------------
    def expressao(self):
        return self.atribuicao()

    @staticmethod
    def _destino_valido(no):
        if not isinstance(no, dict):
            return False
        tipo = no.get("no")
        if tipo == "Identificador":
            return True
        if tipo == "Indice":
            return ParserLL1._destino_valido(no.get("base"))
        return False

    def atribuicao(self):
        esquerda = self.expressao_ou()
        if self.verificar("ASSIGN"):
            if not self._destino_valido(esquerda):
                self.falhar("o destino da atribuição deve ser um identificador ou índice de vetor")
            self.consumir("ASSIGN")
            direita = self.atribuicao()
            return {"no": "Atribuicao", "destino": esquerda, "valor": direita}
        return esquerda

    def binaria(self, prox_nivel, operadores):
        esquerda = prox_nivel()
        while self.verificar(*operadores):
            op_tok = self.atual()
            self.consumir(op_tok["token"])
            operador = op_tok["lexeme"]
            direita = prox_nivel()
            esquerda = {"no": "Binaria", "operador": operador, "esquerda": esquerda, "direita": direita}
        return esquerda

    def expressao_ou(self):
        return self.binaria(self.expressao_e, {"OR"})

    def expressao_e(self):
        return self.binaria(self.expressao_igualdade, {"AND"})

    def expressao_igualdade(self):
        return self.binaria(self.expressao_relacional, {"EQ", "NE", "NEQ"})

    def expressao_relacional(self):
        return self.binaria(self.expressao_aditiva, {"LT", "GT", "LE", "GE"})

    def expressao_aditiva(self):
        return self.binaria(self.expressao_multiplicativa, {"PLUS", "MINUS"})

    def expressao_multiplicativa(self):
        return self.binaria(self.expressao_unaria, {"STAR", "SLASH", "PERCENT"})

    def expressao_unaria(self):
        if self.verificar("MINUS", "NOT"):
            tok = self.atual()
            self.consumir(tok["token"])
            return {"no": "Unaria", "operador": tok["lexeme"], "valor": self.expressao_unaria()}
        return self.expressao_posfixa()

    def expressao_posfixa(self):
        arvore = self.primario()
        while True:
            if self.aceitar("LBRACKET"):
                indice = self.expressao()
                self.consumir("RBRACKET")
                arvore = {"no": "Indice", "base": arvore, "indice": indice}
            elif self.aceitar("LPAREN"):
                argumentos = [] if self.verificar("RPAREN") else self.lista_argumentos()
                self.consumir("RPAREN")
                arvore = {"no": "Chamada", "funcao": arvore, "argumentos": argumentos}
            else:
                break
        return arvore

    def lista_argumentos(self):
        argumentos = [self.expressao()]
        while self.aceitar("COMMA"):
            argumentos.append(self.expressao())
        return argumentos

    def primario(self):
        tipo = self.atual()["token"]
        if tipo == "IDENT":
            tok = self.consumir("IDENT")
            return {"no": "Identificador", "nome": tok["lexeme"]}

        tipos_literais = {
            "INT_LIT": "int", "FLOAT_LIT": "real", "TRUE": "bool", "FALSE": "bool",
            "CHAR_LIT": "char", "STRING_LIT": "string",
        }
        if tipo in tipos_literais:
            tok = self.consumir(tipo)
            return {"no": "Literal", "tipo": tipos_literais[tipo], "valor": tok["lexeme"]}

        if self.aceitar("LPAREN"):
            arv = self.expressao()
            self.consumir("RPAREN")
            return arv

        self.falhar("esperado expressão: identificador, literal ou '('")


# ==============================================================================
# FORMATAÇÃO DA ÁRVORE SINTÁTICA ABSTRATA (AST)
# ==============================================================================
def formatar_ast(arvore):
    """Gera a representação textual exata da AST em conformidade com o gabarito dos testes."""
    if arvore is None:
        return "NULL"
    nome = arvore.get("no")
    if nome in {"Programa", "Bloco"}:
        rotulo = "Program" if nome == "Programa" else "Block"
        filhos = ", ".join(formatar_ast(item) for item in arvore["itens"])
        return f"{rotulo}({filhos})"
    if nome == "Declaracao":
        conteudo = f"{arvore['tipo']} {arvore['nome']}"
        if arvore.get("tamanho") is not None:
            conteudo += f" size={formatar_ast(arvore['tamanho'])}"
        if arvore.get("valor") is not None:
            conteudo += f" = {formatar_ast(arvore['valor'])}"
        return f"VarDecl({conteudo})"
    if nome == "Funcao":
        parametros = ", ".join(
            f"{p['tipo']} {p['nome']}" + ("[]" if p.get("vetor") else "")
            for p in arvore["parametros"]
        )
        return f"Function({arvore['tipo']} {arvore['nome']}({parametros}) {formatar_ast(arvore['corpo'])})"
    if nome == "Identificador":
        return f"Id({arvore['nome']})"
    if nome == "Literal":
        return f"Lit({arvore['tipo']},{arvore['valor']})"
    if nome == "Binaria":
        return f"Binary({arvore['operador']}, {formatar_ast(arvore['esquerda'])}, {formatar_ast(arvore['direita'])})"
    if nome == "Unaria":
        return f"Unary({arvore['operador']}, {formatar_ast(arvore['valor'])})"
    if nome == "Atribuicao":
        return f"Assign({formatar_ast(arvore['destino'])}, {formatar_ast(arvore['valor'])})"
    if nome == "Indice":
        return f"Index({formatar_ast(arvore['base'])}, {formatar_ast(arvore['indice'])})"
    if nome == "Chamada":
        partes = [arvore["funcao"], *arvore["argumentos"]]
        return "Call(" + ", ".join(formatar_ast(p) for p in partes) + ")"
    if nome == "ComandoExpressao":
        return f"ExprStmt({formatar_ast(arvore['valor'])})"
    if nome == "Retorno":
        return f"Return({formatar_ast(arvore['valor'])})"
    if nome == "Se":
        return f"If({formatar_ast(arvore['condicao'])}, {formatar_ast(arvore['entao'])}, {formatar_ast(arvore['senao'])})"
    if nome == "Enquanto":
        return f"While({formatar_ast(arvore['condicao'])}, {formatar_ast(arvore['corpo'])})"
    if nome == "Para":
        partes = [arvore[chave] for chave in ("inicio", "condicao", "passo", "corpo")]
        return "For(" + ", ".join(formatar_ast(p) for p in partes) + ")"
    if nome == "Imprimir":
        return "Print(" + ", ".join(formatar_ast(p) for p in arvore["argumentos"]) + ")"
    if nome in {"Break", "Continue", "Ler", "Vazio"}:
        rotulo = {"Ler": "Read", "Vazio": "Empty"}.get(nome, nome)
        return rotulo + "()"
    raise ValueError(f"Nó de AST desconhecido: {nome}")



# ==============================================================================
# FUNÇÕES DE EXECUÇÃO PRINCIPAIS
# ==============================================================================
def analisar_tokens(tokens):
    """Executa o parser sobre uma lista de tokens já gerada."""
    parser = ParserLL1(tokens)
    return parser.parse()


def analisar_arquivo(caminho_arquivo):
    """
    Integração completa com o scanner:
    1. Executa o scanner no arquivo.
    2. Valida se ocorreram erros léxicos.
    3. Executa a análise sintática LL(1) sobre os tokens.
    """
    caminho = str(Path(caminho_arquivo).resolve())
    tokens, erros_lexicos = scanner_analisar_arquivo(caminho)
    if erros_lexicos:
        primeiro_erro = erros_lexicos[0]
        raise SyntaxError(
            f"Erro léxico no arquivo {caminho}: {primeiro_erro.get('error', 'erro desconhecido')} "
            f"na linha {primeiro_erro.get('line')}, coluna {primeiro_erro.get('column')}."
        )
    return analisar_tokens(tokens)


def main():
    """Ponto de entrada via terminal: python parser.py codigo.c"""
    if len(sys.argv) < 2:
        print("Uso: python parser.py <caminho_arquivo_c>", file=sys.stderr)
        sys.exit(1)

    caminho = sys.argv[1]
    try:
        arvore = analisar_arquivo(caminho)
        saida_ast = formatar_ast(arvore)
        print(saida_ast)
        sys.exit(0)
    except ErroSintatico as erro:
        print("NÃO HÁ AST: o parser deve rejeitar a entrada.")
        #print(erro, file=sys.stderr)
        sys.exit(1)
    except Exception as erro:
        print(f"Erro: {erro}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
