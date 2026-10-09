#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define PARSER_NO_MAIN
#define SCANNER_NO_MAIN
#include "../parser_c/parser.c"

/* ==============================================================================
 * TABELA DE SÍMBOLOS E ESCOPOS
 * ============================================================================== */

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_PARAM
} CategoriaSimbolo;

typedef struct {
    char nome[64];
    char tipo[32];
    int is_array;
} ParametroInfo;

typedef struct Simbolo Simbolo;
struct Simbolo {
    char nome[64];
    char tipo[32];
    CategoriaSimbolo categoria;
    int linha;
    int coluna;
    int is_array;
    ParametroInfo* params;
    size_t qtd_params;
    Simbolo* proximo;
};

typedef struct TabelaSimbolos TabelaSimbolos;
struct TabelaSimbolos {
    Simbolo* simbolos;
    TabelaSimbolos* pai;
};

static TabelaSimbolos* tabela_criar(TabelaSimbolos* pai) {
    TabelaSimbolos* tab = calloc(1, sizeof(*tab));
    tab->pai = pai;
    return tab;
}

static void tabela_liberar(TabelaSimbolos* tab) {
    if (!tab) return;
    Simbolo* curr = tab->simbolos;
    while (curr) {
        Simbolo* prox = curr->proximo;
        free(curr->params);
        free(curr);
        curr = prox;
    }
    free(tab);
}

static int tabela_definir(TabelaSimbolos* tab, Simbolo* sym, const Simbolo** out_prev) {
    Simbolo* curr = tab->simbolos;
    while (curr) {
        if (strcmp(curr->nome, sym->nome) == 0) {
            if (out_prev) *out_prev = curr;
            return 0;
        }
        curr = curr->proximo;
    }
    sym->proximo = tab->simbolos;
    tab->simbolos = sym;
    return 1;
}

static Simbolo* tabela_buscar(const TabelaSimbolos* tab, const char* nome) {
    const TabelaSimbolos* t = tab;
    while (t) {
        Simbolo* curr = t->simbolos;
        while (curr) {
            if (strcmp(curr->nome, nome) == 0) return curr;
            curr = curr->proximo;
        }
        t = t->pai;
    }
    return NULL;
}

/* ==============================================================================
 * ESTRUTURA DO ANALISADOR SEMÂNTICO
 * ============================================================================== */

#define MAX_ERROS 256
static char erros_semanticos[MAX_ERROS][512];
static size_t qtd_erros_semanticos = 0;

static void registrar_erro(const char* fmt, ...) {
    if (qtd_erros_semanticos >= MAX_ERROS) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(erros_semanticos[qtd_erros_semanticos++], 512, fmt, args);
    va_end(args);
}

static void add_error_sem001(const char* name, int line, int col) {
    registrar_erro("SEM001 — linha %d, coluna %d: Identificador “%s” não declarado neste escopo.",
                   line, col, name);
}

static void add_error_sem002(const char* name, int line, int col, const Simbolo* prev) {
    char prev_type[64];
    snprintf(prev_type, sizeof(prev_type), "%s%s", prev->tipo, prev->is_array ? "[]" : "");
    registrar_erro("SEM002 — linha %d, coluna %d: “%s” já declarado neste escopo; declaração anterior na linha %d, coluna %d (tipo %s).",
                   line, col, name, prev->linha, prev->coluna, prev_type);
}

static void add_error_sem003(const char* target_name, const char* target_type,
                             const char* expr_type, const char* expr_text, int line, int col) {
    registrar_erro("SEM003 — linha %d, coluna %d: Não é possível atribuir %s a %s sem conversão permitida (destino “%s”; expressão “%s”).",
                   line, col, expr_type, target_type, target_name, expr_text);
}

static void add_error_sem005(const char* cmd_name, const char* received_type,
                             const char* expr_text, int line, int col) {
    registrar_erro("SEM005 — linha %d, coluna %d: Condição de %s deve ter tipo bool; recebeu %s (expressão “%s”).",
                   line, col, cmd_name, received_type, expr_text);
}

static void add_error_sem006(const char* array_name, const char* received_type,
                             const char* expr_text, int line, int col) {
    registrar_erro("SEM006 — linha %d, coluna %d: Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",
                   line, col, array_name, received_type, expr_text);
}

static void add_error_sem007(const char* func_name, size_t expected, size_t received, int line, int col) {
    registrar_erro("SEM007 — linha %d, coluna %d: “%s” espera %zu argumento%s, mas recebeu %zu.",
                   line, col, func_name, expected, expected != 1 ? "s" : "", received);
}

static void add_error_sem008(size_t arg_idx, const char* func_name, const char* expected_type,
                             const char* received_type, const char* expr_text, int line, int col) {
    registrar_erro("SEM008 — linha %d, coluna %d: Argumento %zu de “%s”: esperado %s, recebido %s (expressão “%s”).",
                   line, col, arg_idx, func_name, expected_type, received_type, expr_text);
}

static void add_error_sem009(const char* expr_type, const char* func_type,
                             const char* func_name, int line, int col) {
    registrar_erro("SEM009 — linha %d, coluna %d: Retorno %s incompatível com o tipo %s da função “%s”; conversão implícita de %s para %s não permitida.",
                   line, col, expr_type, func_type, func_name, expr_type, func_type);
}

static void add_error_sem011(const char* func_name, const char* return_type,
                             const char* var_name, int line, int col) {
    registrar_erro("SEM011 — linha %d, coluna %d: A função “%s” pode terminar sem retornar %s; o ramo em que “%s” é falso alcança o fim do corpo.",
                   line, col, func_name, return_type, var_name);
}

static void add_error_sem012(const char* func_name, int line, int col) {
    registrar_erro("SEM012 — linha %d, coluna %d: Função “%s” não produz valor (retorno void) e não pode ser usada como expressão de atribuição.",
                   line, col, func_name);
}

static void add_error_sem013(const char* expr_text, int line, int col) {
    registrar_erro("SEM013 — linha %d, coluna %d: Destino de atribuição não é atribuível; o literal inteiro “%s” não designa uma variável ou elemento de vetor.",
                   line, col, expr_text);
}

/* ==============================================================================
 * ANÁLISE SEMÂNTICA: CHECAGEM DE TIPOS E COMANDOS
 * ============================================================================== */

static TabelaSimbolos* escopo_global = NULL;
static TabelaSimbolos* escopo_atual = NULL;
static const NoAST* funcao_atual = NULL;

static void entrar_escopo(void) {
    escopo_atual = tabela_criar(escopo_atual);
}

static void sair_escopo(void) {
    if (escopo_atual->pai) {
        TabelaSimbolos* pai = escopo_atual->pai;
        tabela_liberar(escopo_atual);
        escopo_atual = pai;
    }
}

static int tipo_atribuivel(const char* destino, const char* origem) {
    if (!destino || !origem) return 0;
    if (strcmp(destino, origem) == 0) return 1;
    if (strcmp(destino, "float") == 0 && strcmp(origem, "int") == 0) return 1;
    return 0;
}

static const char* normalizar_tipo(const char* tipo) {
    if (!tipo) return "";
    if (strcmp(tipo, "real") == 0) return "float";
    return tipo;
}

static int checar_expressao(const NoAST* no, char* out_tipo, size_t tam_tipo,
                            char* out_texto, size_t tam_texto);
static int visitar_comando(const NoAST* stmt);
static int visitar_bloco(const NoAST* node, int is_func_body);

static void obter_tipo_destino(const NoAST* dest, char* out_tipo, size_t tam_tipo,
                               char* out_nome, size_t tam_nome) {
    out_tipo[0] = '\0';
    snprintf(out_nome, tam_nome, "expressao");
    if (!dest) return;

    if (dest->no == NO_IDENTIFICADOR) {
        snprintf(out_nome, tam_nome, "%s", dest->texto);
        Simbolo* sym = tabela_buscar(escopo_atual, dest->texto);
        if (!sym) {
            add_error_sem001(dest->texto, dest->linha, dest->coluna);
            return;
        }
        snprintf(out_tipo, tam_tipo, "%s", sym->tipo);
    } else if (dest->no == NO_INDICE) {
        const NoAST* base = dest->filhos[0];
        const NoAST* indice = dest->filhos[1];
        if (base && base->no == NO_IDENTIFICADOR) {
            snprintf(out_nome, tam_nome, "%s", base->texto);
            Simbolo* sym = tabela_buscar(escopo_atual, base->texto);
            if (!sym) {
                add_error_sem001(base->texto, base->linha, base->coluna);
            } else {
                snprintf(out_tipo, tam_tipo, "%s", sym->tipo);
            }
        }
        char idx_t[32] = {0};
        char idx_txt[128] = {0};
        checar_expressao(indice, idx_t, sizeof(idx_t), idx_txt, sizeof(idx_txt));
        if (idx_t[0] && strcmp(idx_t, "int") != 0) {
            add_error_sem006(out_nome, idx_t, idx_txt, indice ? indice->linha : dest->linha,
                             indice ? indice->coluna : dest->coluna);
        }
    }
}

static void visitar_atribuicao(const NoAST* node) {
    const NoAST* dest = node->filhos[0];
    const NoAST* val = node->filhos[1];

    if (dest && dest->no == NO_LITERAL) {
        add_error_sem013(dest->texto ? dest->texto : "", dest->linha, dest->coluna);
        if (val) {
            char dummy_t[32], dummy_txt[128];
            checar_expressao(val, dummy_t, sizeof(dummy_t), dummy_txt, sizeof(dummy_txt));
        }
        return;
    }

    char target_type[32] = {0};
    char target_name[64] = {0};
    obter_tipo_destino(dest, target_type, sizeof(target_type), target_name, sizeof(target_name));

    char expr_type[32] = {0};
    char expr_text[128] = {0};
    checar_expressao(val, expr_type, sizeof(expr_type), expr_text, sizeof(expr_text));

    if (strcmp(expr_type, "void") == 0) {
        if (val && val->no == NO_CHAMADA) {
            const NoAST* func = val->filhos[0];
            const char* f_nome = func && func->texto ? func->texto : "";
            add_error_sem012(f_nome, val->linha, val->coluna);
        }
        return;
    }

    if (target_type[0] && expr_type[0]) {
        if (!tipo_atribuivel(target_type, expr_type)) {
            int v_line = val ? val->linha : node->linha;
            int v_col = val ? val->coluna : node->coluna;
            add_error_sem003(target_name, target_type, expr_type, expr_text, v_line, v_col);
        }
    }
}

static int checar_expressao(const NoAST* no, char* out_tipo, size_t tam_tipo,
                            char* out_texto, size_t tam_texto) {
    out_tipo[0] = '\0';
    out_texto[0] = '\0';
    if (!no) return 0;

    switch (no->no) {
        case NO_LITERAL: {
            const char* t = normalizar_tipo(no->tipo);
            snprintf(out_tipo, tam_tipo, "%s", t);
            snprintf(out_texto, tam_texto, "%s", no->texto ? no->texto : "");
            return 1;
        }

        case NO_IDENTIFICADOR: {
            snprintf(out_texto, tam_texto, "%s", no->texto);
            Simbolo* sym = tabela_buscar(escopo_atual, no->texto);
            if (!sym) {
                add_error_sem001(no->texto, no->linha, no->coluna);
                return 0;
            }
            if (sym->is_array) {
                snprintf(out_tipo, tam_tipo, "%s[]", sym->tipo);
            } else {
                snprintf(out_tipo, tam_tipo, "%s", sym->tipo);
            }
            return 1;
        }

        case NO_INDICE: {
            const NoAST* base = no->filhos[0];
            const NoAST* indice = no->filhos[1];
            Simbolo* sym = NULL;
            if (base && base->no == NO_IDENTIFICADOR) {
                snprintf(out_texto, tam_texto, "%s[...]", base->texto);
                sym = tabela_buscar(escopo_atual, base->texto);
                if (!sym) {
                    add_error_sem001(base->texto, base->linha, base->coluna);
                }
            } else {
                snprintf(out_texto, tam_texto, "vetor[...]");
            }
            char idx_t[32] = {0};
            char idx_txt[128] = {0};
            checar_expressao(indice, idx_t, sizeof(idx_t), idx_txt, sizeof(idx_txt));
            if (idx_t[0] && strcmp(idx_t, "int") != 0) {
                const char* b_nome = (base && base->texto) ? base->texto : "vetor";
                add_error_sem006(b_nome, idx_t, idx_txt, indice ? indice->linha : no->linha,
                                 indice ? indice->coluna : no->coluna);
            }
            if (sym) {
                snprintf(out_tipo, tam_tipo, "%s", sym->tipo);
            }
            return 1;
        }

        case NO_CHAMADA: {
            const NoAST* func = no->filhos[0];
            const char* func_name = (func && func->texto) ? func->texto : "";
            int f_line = func ? func->linha : no->linha;
            int f_col = func ? func->coluna : no->coluna;

            Simbolo* sym = tabela_buscar(escopo_global, func_name);
            if (!sym || sym->categoria != SYM_FUNC) {
                add_error_sem001(func_name, f_line, f_col);
                snprintf(out_texto, tam_texto, "%s()", func_name);
                return 0;
            }

            size_t expected_params = sym->qtd_params;
            size_t args_count = no->quantidade;
            if (args_count != expected_params) {
                add_error_sem007(func_name, expected_params, args_count, f_line, f_col);
            }

            size_t limit = args_count < expected_params ? args_count : expected_params;
            for (size_t i = 0; i < limit; i++) {
                const NoAST* arg = no->itens[i];
                char a_type[32] = {0};
                char a_text[128] = {0};
                checar_expressao(arg, a_type, sizeof(a_type), a_text, sizeof(a_text));

                ParametroInfo* p = &sym->params[i];
                char expected_str[64];
                snprintf(expected_str, sizeof(expected_str), "%s%s", p->tipo, p->is_array ? "[]" : "");

                if (a_type[0]) {
                    int match = 0;
                    if (p->is_array && strcmp(a_type, expected_str) == 0) {
                        match = 1;
                    } else if (!p->is_array && tipo_atribuivel(p->tipo, a_type)) {
                        match = 1;
                    }
                    if (!match) {
                        int a_line = arg ? arg->linha : f_line;
                        int a_col = arg ? arg->coluna : f_col;
                        add_error_sem008(i + 1, func_name, expected_str, a_type, a_text, a_line, a_col);
                    }
                }
            }

            snprintf(out_tipo, tam_tipo, "%s", sym->tipo);
            snprintf(out_texto, tam_texto, "%s(...)", func_name);
            return 1;
        }

        case NO_ATRIBUICAO: {
            visitar_atribuicao(no);
            char target_name[64];
            obter_tipo_destino(no->filhos[0], out_tipo, tam_tipo, target_name, sizeof(target_name));
            snprintf(out_texto, tam_texto, "atribuicao");
            return 1;
        }

        case NO_BINARIA: {
            char l_t[32] = {0}, r_t[32] = {0};
            char l_txt[128] = {0}, r_txt[128] = {0};
            checar_expressao(no->filhos[0], l_t, sizeof(l_t), l_txt, sizeof(l_txt));
            checar_expressao(no->filhos[1], r_t, sizeof(r_t), r_txt, sizeof(r_txt));

            const char* op = no->texto ? no->texto : "";
            snprintf(out_texto, tam_texto, "%s %s %s", l_txt, op, r_txt);

            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
                if (strcmp(l_t, "float") == 0 || strcmp(r_t, "float") == 0) {
                    snprintf(out_tipo, tam_tipo, "float");
                } else {
                    snprintf(out_tipo, tam_tipo, "int");
                }
            } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                       strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
                       strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
                       strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                snprintf(out_tipo, tam_tipo, "bool");
            }
            return 1;
        }

        case NO_UNARIA: {
            char e_t[32] = {0};
            char e_txt[128] = {0};
            checar_expressao(no->filhos[0], e_t, sizeof(e_t), e_txt, sizeof(e_txt));
            const char* op = no->texto ? no->texto : "";
            if (strcmp(op, "!") == 0) {
                snprintf(out_tipo, tam_tipo, "bool");
                snprintf(out_texto, tam_texto, "!%s", e_txt);
            } else {
                snprintf(out_tipo, tam_tipo, "%s", e_t);
                snprintf(out_texto, tam_texto, "%s%s", op, e_txt);
            }
            return 1;
        }

        default:
            return 0;
    }
}

static void visitar_declaracao_variavel(const NoAST* node) {
    Simbolo* sym = calloc(1, sizeof(*sym));
    snprintf(sym->nome, sizeof(sym->nome), "%s", node->texto);
    snprintf(sym->tipo, sizeof(sym->tipo), "%s", normalizar_tipo(node->tipo));
    sym->categoria = SYM_VAR;
    sym->linha = node->linha;
    sym->coluna = node->coluna;
    sym->is_array = (node->filhos[0] != NULL);

    const Simbolo* prev = NULL;
    if (!tabela_definir(escopo_atual, sym, &prev)) {
        add_error_sem002(node->texto, node->linha, node->coluna, prev);
        free(sym);
    }

    if (node->filhos[1]) {
        char val_type[32] = {0};
        char val_text[128] = {0};
        checar_expressao(node->filhos[1], val_type, sizeof(val_type), val_text, sizeof(val_text));
        if (val_type[0] && !tipo_atribuivel(node->tipo, val_type)) {
            int v_line = node->filhos[1]->linha;
            int v_col = node->filhos[1]->coluna;
            add_error_sem003(node->texto, node->tipo, val_type, val_text, v_line, v_col);
        }
    }
}

static int visitar_bloco(const NoAST* node, int is_func_body) {
    if (!node) return 0;
    if (!is_func_body) entrar_escopo();

    int guaranteed_return = 0;
    for (size_t i = 0; i < node->quantidade; i++) {
        if (visitar_comando(node->itens[i])) {
            guaranteed_return = 1;
        }
    }

    if (!is_func_body) sair_escopo();
    return guaranteed_return;
}

static int visitar_comando(const NoAST* stmt) {
    if (!stmt) return 0;

    switch (stmt->no) {
        case NO_DECLARACAO:
            visitar_declaracao_variavel(stmt);
            return 0;

        case NO_BLOCO:
            return visitar_bloco(stmt, 0);

        case NO_SE: {
            const NoAST* cond = stmt->filhos[0];
            char c_type[32] = {0}, c_text[128] = {0};
            checar_expressao(cond, c_type, sizeof(c_type), c_text, sizeof(c_text));
            if (c_type[0] && strcmp(c_type, "bool") != 0) {
                int c_line = cond ? cond->linha : stmt->linha;
                int c_col = cond ? cond->coluna : stmt->coluna;
                add_error_sem005("if", c_type, c_text, c_line, c_col);
            }
            int ret_then = visitar_comando(stmt->filhos[1]);
            int ret_else = stmt->filhos[2] ? visitar_comando(stmt->filhos[2]) : 0;
            return (ret_then && ret_else);
        }

        case NO_ENQUANTO: {
            const NoAST* cond = stmt->filhos[0];
            char c_type[32] = {0}, c_text[128] = {0};
            checar_expressao(cond, c_type, sizeof(c_type), c_text, sizeof(c_text));
            if (c_type[0] && strcmp(c_type, "bool") != 0) {
                int c_line = cond ? cond->linha : stmt->linha;
                int c_col = cond ? cond->coluna : stmt->coluna;
                add_error_sem005("while", c_type, c_text, c_line, c_col);
            }
            visitar_comando(stmt->filhos[1]);
            return 0;
        }

        case NO_PARA: {
            char d_t[32], d_txt[128];
            if (stmt->filhos[0]) checar_expressao(stmt->filhos[0], d_t, sizeof(d_t), d_txt, sizeof(d_txt));
            if (stmt->filhos[1]) {
                checar_expressao(stmt->filhos[1], d_t, sizeof(d_t), d_txt, sizeof(d_txt));
                if (d_t[0] && strcmp(d_t, "bool") != 0) {
                    add_error_sem005("for", d_t, d_txt, stmt->filhos[1]->linha, stmt->filhos[1]->coluna);
                }
            }
            if (stmt->filhos[2]) checar_expressao(stmt->filhos[2], d_t, sizeof(d_t), d_txt, sizeof(d_txt));
            visitar_comando(stmt->filhos[3]);
            return 0;
        }

        case NO_RETORNO: {
            const NoAST* val = stmt->filhos[0];
            if (val) {
                char e_type[32] = {0}, e_text[128] = {0};
                checar_expressao(val, e_type, sizeof(e_type), e_text, sizeof(e_text));
                if (funcao_atual) {
                    const char* expected = normalizar_tipo(funcao_atual->tipo);
                    if (e_type[0] && !tipo_atribuivel(expected, e_type)) {
                        int e_line = val->linha ? val->linha : stmt->linha;
                        int e_col = val->coluna ? val->coluna : stmt->coluna;
                        add_error_sem009(e_type, expected, funcao_atual->texto, e_line, e_col);
                    }
                }
            } else {
                if (funcao_atual && strcmp(funcao_atual->tipo, "void") != 0) {
                    add_error_sem009("void", funcao_atual->tipo, funcao_atual->texto,
                                     stmt->linha, stmt->coluna);
                }
            }
            return 1;
        }

        case NO_ATRIBUICAO:
            visitar_atribuicao(stmt);
            return 0;

        case NO_COMANDO_EXPRESSAO: {
            const NoAST* val = stmt->filhos[0];
            if (val) {
                if (val->no == NO_ATRIBUICAO) {
                    visitar_atribuicao(val);
                } else {
                    char d_t[32], d_txt[128];
                    checar_expressao(val, d_t, sizeof(d_t), d_txt, sizeof(d_txt));
                }
            }
            return 0;
        }

        case NO_IMPRIMIR: {
            char d_t[32], d_txt[128];
            for (size_t i = 0; i < stmt->quantidade; i++) {
                checar_expressao(stmt->itens[i], d_t, sizeof(d_t), d_txt, sizeof(d_txt));
            }
            return 0;
        }

        default:
            return 0;
    }
}

static void visitar_declaracao_funcao(const NoAST* node) {
    funcao_atual = node;
    entrar_escopo();

    // Registrar parâmetros no escopo local da função
    for (size_t i = 0; i < node->quantidade; i++) {
        const NoAST* p = node->itens[i];
        Simbolo* psym = calloc(1, sizeof(*psym));
        snprintf(psym->nome, sizeof(psym->nome), "%s", p->texto);
        snprintf(psym->tipo, sizeof(psym->tipo), "%s", normalizar_tipo(p->tipo));
        psym->categoria = SYM_PARAM;
        psym->linha = p->linha;
        psym->coluna = p->coluna;
        psym->is_array = p->vetor;

        const Simbolo* prev = NULL;
        if (!tabela_definir(escopo_atual, psym, &prev)) {
            add_error_sem002(p->texto, p->linha, p->coluna, prev);
            free(psym);
        }
    }

    const NoAST* corpo = node->filhos[0];
    int has_guaranteed_return = visitar_bloco(corpo, 1);

    // Validação de retorno obrigatório em funções não-void (SEM011)
    if (strcmp(node->tipo, "void") != 0 && !has_guaranteed_return) {
        const char* cond_var = "condicao";
        if (corpo) {
            for (size_t i = 0; i < corpo->quantidade; i++) {
                const NoAST* st = corpo->itens[i];
                if (st && st->no == NO_SE) {
                    const NoAST* c = st->filhos[0];
                    if (c && c->no == NO_IDENTIFICADOR && c->texto) {
                        cond_var = c->texto;
                        break;
                    }
                }
            }
        }

        add_error_sem011(node->texto, node->tipo, cond_var, node->linha, 1);
    }

    sair_escopo();
    funcao_atual = NULL;
}

static int executar_analise_semantica(const NoAST* arvore) {
    if (!arvore || arvore->no != NO_PROGRAMA) return 0;

    escopo_global = tabela_criar(NULL);
    escopo_atual = escopo_global;

    // Passada 1: Coletar declarações de funções globais para permitir recursão e chamadas mútuas
    for (size_t i = 0; i < arvore->quantidade; i++) {
        const NoAST* item = arvore->itens[i];
        if (item && item->no == NO_FUNCAO) {
            Simbolo* fsym = calloc(1, sizeof(*fsym));
            snprintf(fsym->nome, sizeof(fsym->nome), "%s", item->texto);
            snprintf(fsym->tipo, sizeof(fsym->tipo), "%s", normalizar_tipo(item->tipo));
            fsym->categoria = SYM_FUNC;
            fsym->linha = item->linha_nome ? item->linha_nome : item->linha;
            fsym->coluna = item->coluna_nome ? item->coluna_nome : item->coluna;
            fsym->qtd_params = item->quantidade;
            if (fsym->qtd_params > 0) {
                fsym->params = calloc(fsym->qtd_params, sizeof(ParametroInfo));
                for (size_t j = 0; j < item->quantidade; j++) {
                    const NoAST* p = item->itens[j];
                    snprintf(fsym->params[j].nome, sizeof(fsym->params[j].nome), "%s", p->texto ? p->texto : "");
                    snprintf(fsym->params[j].tipo, sizeof(fsym->params[j].tipo), "%s", normalizar_tipo(p->tipo));
                    fsym->params[j].is_array = p->vetor;
                }
            }

            const Simbolo* prev = NULL;
            if (!tabela_definir(escopo_global, fsym, &prev)) {
                add_error_sem002(item->texto, fsym->linha, fsym->coluna, prev);
                free(fsym->params);
                free(fsym);
            }
        }
    }

    // Passada 2: Analisar declarações globais e corpos de funções
    for (size_t i = 0; i < arvore->quantidade; i++) {
        const NoAST* item = arvore->itens[i];
        if (!item) continue;
        if (item->no == NO_DECLARACAO) {
            visitar_declaracao_variavel(item);
        } else if (item->no == NO_FUNCAO) {
            visitar_declaracao_funcao(item);
        } else {
            visitar_comando(item);
        }
    }

    // Emissão de diagnósticos
    if (qtd_erros_semanticos == 0) {
        printf("Análise semântica concluída: 0 erros; programa aceito.");
    } else {
        for (size_t i = 0; i < qtd_erros_semanticos; i++) {
            printf("%s\n", erros_semanticos[i]);
        }
        printf("Análise semântica concluída: %zu erro%s; programa rejeitado.",
               qtd_erros_semanticos, qtd_erros_semanticos > 1 ? "s" : "");
    }

    tabela_liberar(escopo_global);
    escopo_global = NULL;
    escopo_atual = NULL;

    return (qtd_erros_semanticos == 0);
}

/* ==============================================================================
 * PONTO DE ENTRADA (MAIN)
 * ============================================================================== */

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <caminho_arquivo_c>\n", argv[0]);
        return 1;
    }

    free_scanner();
    analyze_file(argv[1]);
    if (errors_count) {
        const Error* erro = &errors[0];
        fprintf(stderr, "Erro: Erro léxico no arquivo %s: %s na linha %d, coluna %d.\n",
                argv[1], erro->error, erro->line, erro->column);
        free_scanner();
        return 1;
    }

    ParserLL1* p = calloc(1, sizeof(*p));
    if (!p) {
        fprintf(stderr, "Erro: Memória insuficiente para executar o parser.\n");
        free_scanner();
        return 1;
    }

    switch (setjmp(p->erro)) {
        case 0:
            break;
        case 1:
            fprintf(stderr, "NÃO HÁ AST: o parser deve rejeitar a entrada.\n");
            liberar_parser(p);
            free_scanner();
            return 1;
        default:
            fprintf(stderr, "Erro: Memória insuficiente para executar o parser.\n");
            liberar_parser(p);
            free_scanner();
            return 1;
    }

    NoAST* arvore = analisar_tokens(p, tokens, tokens_count);
    executar_analise_semantica(arvore);

    liberar_parser(p);
    free_scanner();
    return 0;
}
