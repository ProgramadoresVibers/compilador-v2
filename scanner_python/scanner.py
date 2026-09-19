import json
import re
import sys
from pathlib import Path

# --- CONSTANTES DE TOKENS E ERROS ---
RESERVED_WORDS = [
    "int", "float", "char", "double", "void",
    "if", "else", "for", "while", "return",
    "do", "switch", "case", "break", "continue",
    "default", "struct", "bool", "true", "false",
    "print", "read"
]

TOK_INT = "INT"
TOK_FLOAT = "FLOAT"
TOK_CHAR = "CHAR"
TOK_DOUBLE = "DOUBLE"
TOK_VOID = "VOID"
TOK_IF = "IF"
TOK_ELSE = "ELSE"
TOK_FOR = "FOR"
TOK_WHILE = "WHILE"
TOK_RETURN = "RETURN"
TOK_DO = "DO"
TOK_SWITCH = "SWITCH"
TOK_CASE = "CASE"
TOK_BREAK = "BREAK"
TOK_CONTINUE = "CONTINUE"
TOK_DEFAULT = "DEFAULT"
TOK_STRUCT = "STRUCT"
TOK_BOOL = "BOOL"
TOK_TRUE = "TRUE"
TOK_FALSE = "FALSE"
TOK_PRINT = "PRINT"
TOK_READ = "READ"

TOK_IDENT = "IDENT"
TOK_INT_LIT = "INT_LIT"
TOK_FLOAT_LIT = "FLOAT_LIT"
TOK_CHAR_LIT = "CHAR_LIT"
TOK_STRING_LIT = "STRING_LIT"

TOK_LPAREN = "LPAREN"
TOK_RPAREN = "RPAREN"
TOK_LBRACE = "LBRACE"
TOK_RBRACE = "RBRACE"
TOK_LBRACKET = "LBRACKET"
TOK_RBRACKET = "RBRACKET"
TOK_COMMA = "COMMA"
TOK_SEMICOLON = "SEMICOLON"
TOK_COLON = "COLON"

TOK_LT = "LT"
TOK_GT = "GT"
TOK_ASSIGN = "ASSIGN"
TOK_PLUS = "PLUS"
TOK_MINUS = "MINUS"
TOK_ASTERISK = "STAR"
TOK_SLASH = "SLASH"
TOK_DOT = "DOT"
TOK_NOT = "NOT"
TOK_AMPERSAND = "AMPERSAND"
TOK_PIPE = "PIPE"
TOK_MOD = "PERCENT"

TOK_NEQ = "NEQ"
TOK_LE = "LE"
TOK_GE = "GE"
TOK_EQ = "EQ"
TOK_PLUS_ASSIGN = "PLUS_ASSIGN"
TOK_MINUS_ASSIGN = "MINUS_ASSIGN"
TOK_DIV_ASSIGN = "DIV_ASSIGN"
TOK_MUL_ASSIGN = "MUL_ASSIGN"
TOK_MOD_ASSIGN = "MOD_ASSIGN"
TOK_INC = "INC"
TOK_DEC = "DEC"
TOK_AND = "AND"
TOK_OR = "OR"

TOK_EOF = "EOF"

ERR_UNKNOWN_SYMBOL = "UNKNOWN_SYMBOL"
ERR_UNTERMINATED_CHAR = "UNTERMINATED_CHAR_LITERAL"
ERR_CHAR_TOO_LONG = "CHAR_TOO_LONG"
ERR_EMPTY_CHAR = "EMPTY_CHAR_LITERAL"
ERR_UNTERMINATED_BLOCK_COMMENT = "UNTERMINATED_BLOCK_COMMENT"
ERR_UNTERMINATED_STRING = "UNTERMINATED_STRING_LITERAL"
ERR_MALFORMED_REAL = "MALFORMED_REAL_LITERAL"
ERR_INVALID_IDENT = "INVALID_IDENTIFIER"
ERR_INVALID_INT = "INVALID_INT_LITERAL"
ERR_LEADING_ZERO = "LEADING_ZERO"

STATE_INITIAL = "ESTADO_INICIAL"
STATE_IDENTIFIER = "ESTADO_IDENTIFICADOR"
STATE_INT = "ESTADO_NUMERO_INTEIRO"
STATE_REAL = "ESTADO_NUMERO_REAL"
STATE_FLOAT_SUFFIX = "ESTADO_SUFIXO_FLOAT"
STATE_SYMBOL = "ESTADO_SIMBOLO"
STATE_STRING = "ESTADO_CADEIA"
STATE_CHAR = "ESTADO_CARACTERE"
STATE_ESCAPE_STRING = "ESTADO_ESCAPE_CADEIA"
STATE_ESCAPE_CHAR = "ESTADO_ESCAPE_CARACTERE"
STATE_CHAR_WITH_ESCAPE = "ESTADO_CARACTERE_COM_ESCAPE"
STATE_LINE_COMMENT = "ESTADO_COMENTARIO_LINHA"
STATE_BLOCK_COMMENT = "ESTADO_COMENTARIO_BLOCO"
STATE_BLOCK_COMMENT_END = "ESTADO_COMENTARIO_BLOCO_QUASE_FIM"


def recuperar_tokens_do_lexema(lexema, linha, coluna, tokens):
    """
    Recupera símbolos estruturais encontrados no final de um
    lexema inválido.

    Exemplo:

        "texto sem fim);

    recupera:

        RPAREN
        SEMICOLON
    """

    mapa_tokens = {
        "(": TOK_LPAREN,
        ")": TOK_RPAREN,
        "{": TOK_LBRACE,
        "}": TOK_RBRACE,
        "[": TOK_LBRACKET,
        "]": TOK_RBRACKET,
        ",": TOK_COMMA,
        ";": TOK_SEMICOLON,
        ":": TOK_COLON,
        "<": TOK_LT,
        ">": TOK_GT,
        "=": TOK_ASSIGN,
        "+": TOK_PLUS,
        "-": TOK_MINUS,
        "*": TOK_ASTERISK,
        "/": TOK_SLASH,
        ".": TOK_DOT,
        "!": TOK_NOT,
        "&": TOK_AMPERSAND,
        "|": TOK_PIPE,
        "%": TOK_MOD
    }

    encontrados = []

    pos = len(lexema) - 1

    # ------------------------------------------------------------
    # Ignora espaços no final.
    # ------------------------------------------------------------

    while pos >= 0 and lexema[pos] in " \t\r":
        pos -= 1

    # ------------------------------------------------------------
    # Recupera a sequência de símbolos que estiver no final.
    #
    # Exemplo:
    #
    # "texto sem fim);
    #
    #                ^^
    #
    # recupera:
    #
    # )
    # ;
    # ------------------------------------------------------------

    while pos >= 0 and lexema[pos] in mapa_tokens:
        encontrados.append(
            (
                mapa_tokens[lexema[pos]],
                lexema[pos],
                pos
            )
        )

        pos -= 1

    # A busca foi feita de trás para frente.
    encontrados.reverse()

    # ------------------------------------------------------------
    # Gera os tokens na ordem correta.
    # ------------------------------------------------------------

    for token, lexema_token, posicao in encontrados:
        tokens.append({
            "token": token,
            "lexeme": lexema_token,
            "attribute": None,
            "line": linha,
            "column": coluna + posicao
        })


def registrar_erro(erros, nome_arquivo, linha, coluna, tipo_erro, lexema):
    erros.append({
        "error": tipo_erro,
        "lexeme": lexema,
        "line": linha,
        "column": coluna
    })


def adicionar_identificador(string_temp, linha, coluna, tokens, reserved_words):
    if string_temp in reserved_words:
        tokens.append(
            {"token": string_temp.upper(), "lexeme": string_temp, "attribute": None, "line": linha, "column": coluna})
    else:
        tokens.append(
            {"token": TOK_IDENT, "lexeme": string_temp, "attribute": string_temp, "line": linha, "column": coluna})


def mapear_simbolo_simples(simbolo):
    match simbolo:
        case "<":
            return TOK_LT
        case ">":
            return TOK_GT
        case "=":
            return TOK_ASSIGN
        case "+":
            return TOK_PLUS
        case "-":
            return TOK_MINUS
        case "*":
            return TOK_ASTERISK
        case "/":
            return TOK_SLASH
        case ".":
            return TOK_DOT
        case "!":
            return TOK_NOT
        case "&":
            return TOK_AMPERSAND
        case "|":
            return TOK_PIPE
        case "%":
            return TOK_MOD
    return None


def analisar_arquivo(nome_arquivo):
    tokens = []
    erros = []
    i = j = 1

    with open(nome_arquivo, "r", encoding="utf-8") as arquivo:
        conteudo = arquivo.read()

    comprimento = len(conteudo)
    idx_cursor = 0

    string_temp = ""
    lexema_bruto = ""
    estado_atual = STATE_INITIAL
    linha_inicio_token = 1
    coluna_inicio_token = 1

    while idx_cursor < comprimento:
        caractere = conteudo[idx_cursor]
        reprocessar = False

        if estado_atual == STATE_INITIAL:
            linha_inicio_token = i
            coluna_inicio_token = j

            if caractere == " ":
                pass
            elif caractere == "\n":
                i += 1
                j = 0
            elif caractere == "\r":
                pass
            elif re.match(r"[A-Za-z_]", caractere):
                string_temp += caractere
                estado_atual = STATE_IDENTIFIER
            elif caractere.isdigit():
                string_temp += caractere
                estado_atual = STATE_INT
            elif re.match(r"[(){},;:\[\]]", caractere):
                match caractere:
                    case "(":
                        t_text = TOK_LPAREN
                    case ")":
                        t_text = TOK_RPAREN
                    case "{":
                        t_text = TOK_LBRACE
                    case "}":
                        t_text = TOK_RBRACE
                    case "[":
                        t_text = TOK_LBRACKET
                    case "]":
                        t_text = TOK_RBRACKET
                    case ",":
                        t_text = TOK_COMMA
                    case ";":
                        t_text = TOK_SEMICOLON
                    case ":":
                        t_text = TOK_COLON
                tokens.append({"token": t_text, "lexeme": caractere, "attribute": None, "line": linha_inicio_token,
                               "column": coluna_inicio_token})
            elif re.match(r"[!=\-+/*><.&|%]", caractere):
                string_temp += caractere
                estado_atual = STATE_SYMBOL
            elif caractere == "'":
                lexema_bruto += caractere
                estado_atual = STATE_CHAR
            elif caractere == '"':
                lexema_bruto += caractere
                estado_atual = STATE_STRING
            else:
                registrar_erro(erros, nome_arquivo, linha_inicio_token, coluna_inicio_token, ERR_UNKNOWN_SYMBOL,
                               caractere)

        elif estado_atual == STATE_IDENTIFIER:
            if re.match(r"[A-Za-z0-9_]", caractere):
                string_temp += caractere
            else:
                adicionar_identificador(string_temp, linha_inicio_token, coluna_inicio_token, tokens, RESERVED_WORDS)
                string_temp = ""
                estado_atual = STATE_INITIAL
                reprocessar = True

        elif estado_atual == STATE_INT:

            if (
                    string_temp[0] == '0'
                    and len(string_temp) == 1
                    and caractere.isdigit()
            ):
                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_LEADING_ZERO,
                    string_temp + caractere
                )

            if caractere.isdigit():

                string_temp += caractere

            elif caractere == ".":

                # ============================================================
                # TRATAMENTO DO PONTO APÓS UM INTEIRO
                # ============================================================
                #
                # 12.34
                # -> FLOAT_LIT
                #
                # 12.
                # -> ERRO
                # -> INT_LIT 12
                # -> DOT .
                #
                # 12.abc
                # -> ERRO
                # -> INT_LIT 12
                # -> DOT .
                # -> IDENT abc
                #
                # O ponto só entra no FLOAT_LIT se houver um dígito
                # imediatamente depois dele.
                # ============================================================

                if (
                        idx_cursor + 1 < comprimento
                        and conteudo[idx_cursor + 1].isdigit()
                ):

                    # É realmente um número real.
                    string_temp += caractere
                    estado_atual = STATE_REAL

                else:

                    # --------------------------------------------------------
                    # O número termina antes do ponto.
                    # Porém, "12." é considerado um REAL MALFORMADO.
                    # --------------------------------------------------------

                    registrar_erro(
                        erros,
                        nome_arquivo,
                        linha_inicio_token,
                        coluna_inicio_token,
                        ERR_MALFORMED_REAL,
                        string_temp + caractere
                    )

                    # --------------------------------------------------------
                    # Gera o INT_LIT com o número antes do ponto.
                    # --------------------------------------------------------

                    tokens.append({
                        "token": TOK_INT_LIT,
                        "lexeme": string_temp,
                        "attribute": int(string_temp),
                        "line": linha_inicio_token,
                        "column": coluna_inicio_token
                    })

                    # --------------------------------------------------------
                    # Limpa o número.
                    # --------------------------------------------------------

                    string_temp = ""

                    # --------------------------------------------------------
                    # Volta ao estado inicial.
                    # --------------------------------------------------------

                    estado_atual = STATE_INITIAL

                    # --------------------------------------------------------
                    # MUITO IMPORTANTE:
                    #
                    # O "." ainda não foi consumido.
                    #
                    # Portanto, o mesmo "." será processado novamente pelo
                    # STATE_INITIAL.
                    # --------------------------------------------------------

                    reprocessar = True

            elif re.match(
                    r"[ \n\r+\-/*=!<>,;:(){}\[\]&|%]",
                    caractere
            ):

                tokens.append({
                    "token": TOK_INT_LIT,
                    "lexeme": string_temp,
                    "attribute": int(string_temp),
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

                string_temp = ""
                estado_atual = STATE_INITIAL

                # O delimitador ainda precisa ser processado.
                reprocessar = True

            else:

                # ============================================================
                # NÚMERO SEGUIDO DE IDENTIFICADOR
                # ============================================================
                #
                # Exemplo:
                #
                # 123abc
                #
                # Gera:
                #
                # INT_LIT  -> 123
                # IDENT    -> abc
                #
                # E também:
                #
                # INVALID_IDENTIFIER -> 123abc
                # ============================================================

                lexema_invalido = string_temp + caractere

                pos = idx_cursor + 1

                while (
                        pos < comprimento
                        and re.match(r"[A-Za-z0-9_]", conteudo[pos])
                ):
                    lexema_invalido += conteudo[pos]
                    pos += 1

                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_INVALID_IDENT,
                    lexema_invalido
                )

                # Gera o número normalmente.
                tokens.append({
                    "token": TOK_INT_LIT,
                    "lexeme": string_temp,
                    "attribute": int(string_temp),
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

                string_temp = ""

                # A letra atual ainda não foi consumida.
                estado_atual = STATE_IDENTIFIER

                # O identificador começa na posição da letra.
                linha_inicio_token = i
                coluna_inicio_token = j

                # Processa novamente o caractere atual.
                reprocessar = True

        elif estado_atual == STATE_REAL:
            if caractere == ".":
                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_MALFORMED_REAL,
                    string_temp + caractere
                )

                string_temp = ""
                estado_atual = STATE_INITIAL
                reprocessar = True

            elif re.match(r"[0-9]", caractere):
                string_temp += caractere

            elif caractere in "Ff":
                lexema_bruto = string_temp + caractere
                estado_atual = STATE_FLOAT_SUFFIX

            elif re.match(r"[A-Za-z_]", caractere):
                # ----------------------------------------------------
                # Número real seguido de identificador.
                #
                # Exemplo:
                #     12.3abc
                #
                # Deve gerar:
                #     FLOAT_LIT -> 12.3
                #     IDENT     -> abc
                #
                # E também:
                #     MALFORMED_REAL_LITERAL -> 12.3abc
                # ----------------------------------------------------

                lexema_invalido = string_temp + caractere

                # Apenas percorre os próximos caracteres para descobrir
                # o lexema completo do erro, sem consumir os caracteres.
                pos = idx_cursor + 1

                while (
                        pos < comprimento
                        and re.match(r"[A-Za-z0-9_]", conteudo[pos])
                ):
                    lexema_invalido += conteudo[pos]
                    pos += 1

                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_MALFORMED_REAL,
                    lexema_invalido
                )

                # Produz o FLOAT_LIT que já foi reconhecido.
                tokens.append({
                    "token": TOK_FLOAT_LIT,
                    "lexeme": string_temp,
                    "attribute": float(string_temp),
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

                string_temp = ""

                # A letra atual ainda não foi consumida.
                estado_atual = STATE_IDENTIFIER

                # O identificador começa na posição da letra.
                linha_inicio_token = i
                coluna_inicio_token = j

                # Reprocessa a letra atual.
                reprocessar = True

            elif re.match(
                    r"[ \n\r+\-/*=!<>,;:(){}\[\]&|%]",
                    caractere
            ):
                tokens.append({
                    "token": TOK_FLOAT_LIT,
                    "lexeme": string_temp,
                    "attribute": float(string_temp),
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

                string_temp = ""
                estado_atual = STATE_INITIAL
                reprocessar = True

            else:
                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_MALFORMED_REAL,
                    string_temp
                )

                string_temp = ""
                estado_atual = STATE_INITIAL


        elif estado_atual == STATE_FLOAT_SUFFIX:

            if re.match(
                    r"[A-Za-z_]",
                    caractere
            ):
                # ----------------------------------------------------
                # Número real com sufixo F/f seguido de identificador.
                #
                # Exemplo:
                #     12.3Fabc
                #
                # Deve gerar:
                #     FLOAT_LIT -> 12.3F
                #     IDENT     -> abc
                #
                # E também registrar:
                #     MALFORMED_REAL_LITERAL -> 12.3Fabc
                # ----------------------------------------------------

                lexema_invalido = lexema_bruto + caractere

                pos = idx_cursor + 1

                while (
                        pos < comprimento
                        and re.match(r"[A-Za-z0-9_]", conteudo[pos])
                ):
                    lexema_invalido += conteudo[pos]
                    pos += 1

                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_MALFORMED_REAL,
                    lexema_invalido
                )

                # Produz o FLOAT_LIT válido até o F/f.
                valor_float = lexema_bruto[:-1]

                tokens.append({
                    "token": TOK_FLOAT_LIT,
                    "lexeme": lexema_bruto,
                    "attribute": float(valor_float),
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

                string_temp = ""
                lexema_bruto = ""

                # A letra ainda não foi consumida.
                estado_atual = STATE_IDENTIFIER

                linha_inicio_token = i
                coluna_inicio_token = j

                reprocessar = True

            elif re.match(
                    r"[ \n\r+\-/*=!<>,;:(){}\[\]&|%]",
                    caractere
            ):
                valor_float = lexema_bruto[:-1]

                tokens.append({
                    "token": TOK_FLOAT_LIT,
                    "lexeme": lexema_bruto,
                    "attribute": float(valor_float),
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

                string_temp = ""
                lexema_bruto = ""
                estado_atual = STATE_INITIAL
                reprocessar = True

            else:
                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_MALFORMED_REAL,
                    lexema_bruto
                )

                string_temp = ""
                lexema_bruto = ""
                estado_atual = STATE_INITIAL

        elif estado_atual == STATE_SYMBOL:
            simbolo_inicial = string_temp[0]

            if simbolo_inicial == "-" and caractere.isdigit() and (
                    not tokens or tokens[-1]["token"] not in [TOK_INT_LIT, TOK_FLOAT_LIT, TOK_IDENT, TOK_RPAREN,
                                                              TOK_CHAR_LIT, TOK_STRING_LIT]):
                string_temp += caractere
                estado_atual = STATE_INT
            elif caractere == "=" and simbolo_inicial not in "&|.":
                match simbolo_inicial:
                    case "!":
                        t_text = TOK_NEQ
                    case "<":
                        t_text = TOK_LE
                    case ">":
                        t_text = TOK_GE
                    case "=":
                        t_text = TOK_EQ
                    case "+":
                        t_text = TOK_PLUS_ASSIGN
                    case "-":
                        t_text = TOK_MINUS_ASSIGN
                    case "/":
                        t_text = TOK_DIV_ASSIGN
                    case "*":
                        t_text = TOK_MUL_ASSIGN
                    case "%":
                        t_text = TOK_MOD_ASSIGN
                tokens.append(
                    {"token": t_text, "lexeme": string_temp + caractere, "attribute": None, "line": linha_inicio_token,
                     "column": coluna_inicio_token})
                string_temp = ""
                estado_atual = STATE_INITIAL
            elif simbolo_inicial == "+" and caractere == "+":
                tokens.append(
                    {"token": TOK_INC, "lexeme": string_temp + caractere, "attribute": None, "line": linha_inicio_token,
                     "column": coluna_inicio_token})
                string_temp = ""
                estado_atual = STATE_INITIAL
            elif simbolo_inicial == "-" and caractere == "-":
                tokens.append(
                    {"token": TOK_DEC, "lexeme": string_temp + caractere, "attribute": None, "line": linha_inicio_token,
                     "column": coluna_inicio_token})
                string_temp = ""
                estado_atual = STATE_INITIAL
            elif simbolo_inicial == "&" and caractere == "&":
                tokens.append(
                    {"token": TOK_AND, "lexeme": string_temp + caractere, "attribute": None, "line": linha_inicio_token,
                     "column": coluna_inicio_token})
                string_temp = ""
                estado_atual = STATE_INITIAL
            elif simbolo_inicial == "|" and caractere == "|":
                tokens.append(
                    {"token": TOK_OR, "lexeme": string_temp + caractere, "attribute": None, "line": linha_inicio_token,
                     "column": coluna_inicio_token})
                string_temp = ""
                estado_atual = STATE_INITIAL
            elif simbolo_inicial == "/" and caractere in "/*":
                string_temp += caractere
                if caractere == "/":
                    estado_atual = STATE_LINE_COMMENT
                else:
                    estado_atual = STATE_BLOCK_COMMENT
            else:
                t_text = mapear_simbolo_simples(simbolo_inicial)
                tokens.append({"token": t_text, "lexeme": string_temp, "attribute": None, "line": linha_inicio_token,
                               "column": coluna_inicio_token})
                string_temp = ""
                estado_atual = STATE_INITIAL
                reprocessar = True

        elif estado_atual == STATE_STRING:
            lexema_bruto += caractere

            if caractere == "\n":

                # ====================================================
                # CADEIA NÃO TERMINADA
                # ====================================================

                lexema_erro = lexema_bruto.rstrip("\n")

                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_UNTERMINATED_STRING,
                    lexema_erro
                )

                # ----------------------------------------------------
                # Recupera os símbolos estruturais que estavam no final
                # do lexema inválido.
                #
                # Exemplo:
                #
                # "texto sem fim);
                #
                # recupera:
                #
                # )
                # ;
                # ----------------------------------------------------

                recuperar_tokens_do_lexema(
                    lexema_erro,
                    linha_inicio_token,
                    coluna_inicio_token,
                    tokens
                )

                string_temp = ""
                lexema_bruto = ""

                i += 1
                j = 0

                estado_atual = STATE_INITIAL

            elif caractere == "\\":
                estado_atual = STATE_ESCAPE_STRING

            elif caractere != '"':
                string_temp += caractere

            else:
                tokens.append({
                    "token": TOK_STRING_LIT,
                    "lexeme": lexema_bruto,
                    "attribute": string_temp,
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

                string_temp = ""
                lexema_bruto = ""
                estado_atual = STATE_INITIAL

        elif estado_atual == STATE_CHAR:

            if caractere == "\n":

                # ====================================================
                # CARACTERE NÃO TERMINADO
                # ====================================================

                lexema_erro = lexema_bruto

                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_UNTERMINATED_CHAR,
                    lexema_erro
                )

                # ----------------------------------------------------
                # Recupera símbolos estruturais que eventualmente
                # tenham sido consumidos dentro do lexema inválido.
                # ----------------------------------------------------

                recuperar_tokens_do_lexema(
                    lexema_erro,
                    linha_inicio_token,
                    coluna_inicio_token,
                    tokens
                )

                string_temp = ""
                lexema_bruto = ""

                i += 1
                j = 0

                estado_atual = STATE_INITIAL

            elif caractere == "'":

                lexema_bruto += caractere

                if len(string_temp) == 0:

                    registrar_erro(
                        erros,
                        nome_arquivo,
                        linha_inicio_token,
                        coluna_inicio_token,
                        ERR_EMPTY_CHAR,
                        lexema_bruto
                    )

                else:

                    tokens.append({
                        "token": TOK_CHAR_LIT,
                        "lexeme": lexema_bruto,
                        "attribute": string_temp,
                        "line": linha_inicio_token,
                        "column": coluna_inicio_token
                    })

                string_temp = ""
                lexema_bruto = ""
                estado_atual = STATE_INITIAL

            elif caractere in "; \t\r" or re.match(
                    r"[+\-*/=!<>,(){}\[\]]",
                    caractere
            ):

                # ====================================================
                # CARACTERE NÃO TERMINADO
                #
                # Exemplo:
                #
                # char c = 'a;
                #
                # O ; é considerado parte da recuperação do erro.
                # ====================================================

                lexema_bruto += caractere

                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_UNTERMINATED_CHAR,
                    lexema_bruto
                )

                # ----------------------------------------------------
                # Recupera os símbolos do final do lexema.
                # ----------------------------------------------------

                recuperar_tokens_do_lexema(
                    lexema_bruto,
                    linha_inicio_token,
                    coluna_inicio_token,
                    tokens
                )

                string_temp = ""
                lexema_bruto = ""

                estado_atual = STATE_INITIAL

                # ----------------------------------------------------
                # NÃO reprocessa o caractere.
                #
                # Ele já foi tratado pela recuperação.
                # ----------------------------------------------------

                reprocessar = False

            else:

                lexema_bruto += caractere

                if caractere == "\\":
                    estado_atual = STATE_ESCAPE_CHAR

                else:

                    if len(string_temp) >= 1:

                        # ------------------------------------------------
                        # Caractere com mais de um conteúdo.
                        #
                        # Continua coletando até encontrar a aspa
                        # simples ou uma quebra de linha.
                        # ------------------------------------------------

                        while (
                                idx_cursor + 1 < comprimento
                                and conteudo[idx_cursor + 1] != "'"
                                and conteudo[idx_cursor + 1] != "\n"
                        ):
                            lexema_bruto += conteudo[idx_cursor + 1]

                            idx_cursor += 1
                            j += 1

                        if (
                                idx_cursor < comprimento
                                and conteudo[idx_cursor] != "\n"
                        ):
                            lexema_bruto += "'"

                            idx_cursor += 1
                            j += 1

                        registrar_erro(
                            erros,
                            nome_arquivo,
                            linha_inicio_token,
                            coluna_inicio_token,
                            ERR_CHAR_TOO_LONG,
                            lexema_bruto
                        )

                        # --------------------------------------------
                        # Recupera os símbolos que estiverem no final
                        # do lexema inválido.
                        # --------------------------------------------

                        recuperar_tokens_do_lexema(
                            lexema_bruto,
                            linha_inicio_token,
                            coluna_inicio_token,
                            tokens
                        )

                        string_temp = ""
                        lexema_bruto = ""
                        estado_atual = STATE_INITIAL

                        idx_cursor -= 1
                        j -= 1

                    else:
                        string_temp += caractere

        elif estado_atual in (STATE_ESCAPE_STRING, STATE_ESCAPE_CHAR):
            lexema_bruto += caractere
            match caractere:
                case "n":
                    string_temp += "\n"
                case "t":
                    string_temp += "\t"
                case "0":
                    string_temp += "\0"
                case "\\":
                    string_temp += "\\"
                case "'":
                    string_temp += "'"
                case '"':
                    string_temp += '"'
                case _:
                    string_temp += caractere

            if estado_atual == STATE_ESCAPE_STRING:
                estado_atual = STATE_STRING
            else:
                estado_atual = STATE_CHAR_WITH_ESCAPE

        elif estado_atual == STATE_CHAR_WITH_ESCAPE:

            lexema_bruto += caractere

            if caractere != "'":

                # ====================================================
                # CARACTERE COM ESCAPE E CONTEÚDO EXTRA
                # ====================================================

                registrar_erro(
                    erros,
                    nome_arquivo,
                    linha_inicio_token,
                    coluna_inicio_token,
                    ERR_CHAR_TOO_LONG,
                    lexema_bruto
                )

                recuperar_tokens_do_lexema(
                    lexema_bruto,
                    linha_inicio_token,
                    coluna_inicio_token,
                    tokens
                )

            else:

                tokens.append({
                    "token": TOK_CHAR_LIT,
                    "lexeme": lexema_bruto,
                    "attribute": string_temp,
                    "line": linha_inicio_token,
                    "column": coluna_inicio_token
                })

            string_temp = ""
            lexema_bruto = ""
            estado_atual = STATE_INITIAL

        elif estado_atual == STATE_LINE_COMMENT:
            if caractere == "\n":
                string_temp = ""
                estado_atual = STATE_INITIAL
                i += 1
                j = 0

        elif estado_atual == STATE_BLOCK_COMMENT:
            string_temp += caractere
            if caractere == "*":
                estado_atual = STATE_BLOCK_COMMENT_END
            elif caractere == "\n":
                i += 1
                j = 0

        elif estado_atual == STATE_BLOCK_COMMENT_END:
            string_temp += caractere
            if caractere == "/":
                string_temp = ""
                estado_atual = STATE_INITIAL
            elif caractere != "*":
                if caractere == "\n":
                    i += 1
                    j = 0
                estado_atual = STATE_BLOCK_COMMENT

        if not reprocessar:
            idx_cursor += 1
            j += 1

    # Fim do arquivo - Tratamento de estados pendentes
    if estado_atual == STATE_IDENTIFIER:
        adicionar_identificador(string_temp, linha_inicio_token, coluna_inicio_token, tokens, RESERVED_WORDS)
    elif estado_atual == STATE_INT:
        tokens.append(
            {"token": TOK_INT_LIT, "lexeme": string_temp, "attribute": int(string_temp), "line": linha_inicio_token,
             "column": coluna_inicio_token})
    elif estado_atual in (STATE_REAL, STATE_FLOAT_SUFFIX):
        val_proc = string_temp if string_temp[-1] != "." else string_temp + "0"
        tokens.append({"token": TOK_FLOAT_LIT, "lexeme": lexema_bruto or string_temp, "attribute": float(val_proc),
                       "line": linha_inicio_token, "column": coluna_inicio_token})
    elif estado_atual == STATE_SYMBOL:
        t_text = mapear_simbolo_simples(string_temp)
        tokens.append({"token": t_text, "lexeme": string_temp, "attribute": None, "line": linha_inicio_token,
                       "column": coluna_inicio_token})
    elif estado_atual in (STATE_STRING, STATE_ESCAPE_STRING):
        registrar_erro(erros, nome_arquivo, linha_inicio_token, coluna_inicio_token, ERR_UNTERMINATED_STRING,
                       lexema_bruto)
    elif estado_atual in (STATE_CHAR, STATE_ESCAPE_CHAR, STATE_CHAR_WITH_ESCAPE):
        registrar_erro(erros, nome_arquivo, linha_inicio_token, coluna_inicio_token, ERR_UNTERMINATED_CHAR,
                       lexema_bruto)
    elif estado_atual in (STATE_BLOCK_COMMENT, STATE_BLOCK_COMMENT_END):
        registrar_erro(erros, nome_arquivo, linha_inicio_token, coluna_inicio_token, ERR_UNTERMINATED_BLOCK_COMMENT,
                       string_temp)

    tokens.append({"token": TOK_EOF, "lexeme": "", "attribute": None, "line": i, "column": j if j > 0 else 1})

    return tokens, erros


if __name__ == "__main__":
    # Verifica se o caminho do arquivo foi passado como argumento
    if len(sys.argv) < 2:
        print("Erro: Nenhum arquivo de entrada fornecido.", file=sys.stderr)
        sys.exit(1)

    # Modo somente tokens/léxico se solicitado explicitamente
    if sys.argv[1] in ("--tokens", "-t"):
        if len(sys.argv) < 3:
            print("Erro: Nenhum arquivo de entrada fornecido para o scanner.", file=sys.stderr)
            sys.exit(1)
        arquivo_path = sys.argv[2]
        tokens, erros = analisar_arquivo(arquivo_path)
        for token in tokens:
            print(json.dumps(token, ensure_ascii=False))

        if erros:
            pasta_erros = Path("errors")
            pasta_erros.mkdir(parents=True, exist_ok=True)
            nome_base = Path(arquivo_path).stem
            caminho_erros = pasta_erros / f"{nome_base}.errors.jsonl"
            with open(caminho_erros, "w", encoding="utf-8") as f:
                for erro in erros:
                    f.write(json.dumps(erro, ensure_ascii=False) + "\n")
    else:
        # Para achar o parser
        raiz_projeto = str(Path(__file__).resolve().parent.parent)
        if raiz_projeto not in sys.path:
            sys.path.insert(0, raiz_projeto)
            
        # Execução integrada do parser via scanner: python scanner.py codigo.c
        try:
            from parser_python.parser import main as parser_main
        except ImportError:
            import parser
            parser_main = parser.main
        parser_main()