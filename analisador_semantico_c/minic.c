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
 * TIPOS
 *
 * Os tipos são representados por ponteiros para cadeias estáticas:
 *   "int", "float", "bool", "char", "string", "void"  -> escalares / especiais
 *   "int[]", "float[]", "bool[]", "char[]"            -> vetores
 *   NULL                                               -> tipo desconhecido (um erro já
 *                                                         foi reportado; evita cascata)
 * ============================================================================== */

typedef const char* Tipo;

static const char* const TIPOS_BASE[] = {"int", "float", "bool", "char", "string", "void"};
static const char* const TIPOS_VETOR[] = {"int[]", "float[]", "bool[]", "char[]", "string[]", "void[]"};
#define QTD_TIPOS (sizeof(TIPOS_BASE) / sizeof(TIPOS_BASE[0]))

static int tipo_igual(Tipo a, Tipo b) {
    return a && b && strcmp(a, b) == 0;
}

/* Converte o nome de tipo do parser para a cadeia canônica (real -> float). */
static Tipo tipo_canonico(const char* t) {
    if (!t) return NULL;
    if (strcmp(t, "real") == 0) return "float";
    for (size_t i = 0; i < QTD_TIPOS; i++) {
        if (strcmp(t, TIPOS_BASE[i]) == 0) return TIPOS_BASE[i];
        if (strcmp(t, TIPOS_VETOR[i]) == 0) return TIPOS_VETOR[i];
    }
    return t;
}

static Tipo tipo_vetor_de(Tipo base) {
    for (size_t i = 0; i < QTD_TIPOS; i++) {
        if (tipo_igual(base, TIPOS_BASE[i])) return TIPOS_VETOR[i];
    }
    return NULL;
}

static int eh_vetor(Tipo t) {
    if (!t) return 0;
    size_t n = strlen(t);
    return n > 2 && strcmp(t + n - 2, "[]") == 0;
}

static Tipo tipo_elemento(Tipo vetor) {
    for (size_t i = 0; i < QTD_TIPOS; i++) {
        if (tipo_igual(vetor, TIPOS_VETOR[i])) return TIPOS_BASE[i];
    }
    return NULL;
}

static int eh_numerico(Tipo t) {
    return tipo_igual(t, "int") || tipo_igual(t, "float");
}

static int tipo_atribuivel(Tipo destino, Tipo origem) {
    if (!destino || !origem) return 0;
    if (strcmp(destino, origem) == 0) return 1;
    /* Promoção int -> float permitida */
    return strcmp(destino, "float") == 0 && strcmp(origem, "int") == 0;
}

static int eh_literal_verdadeiro(const NoAST* no) {
    return no && no->no == NO_LITERAL && no->tipo && strcmp(no->tipo, "bool") == 0 &&
           no->texto && strcmp(no->texto, "true") == 0;
}

/* ==============================================================================
 * TEXTO DINÂMICO (para mensagens e representação de expressões)
 * ============================================================================== */

typedef struct {
    char* dados;
    size_t tamanho;
    size_t capacidade;
} Texto;

static void texto_add(Texto* t, const char* s) {
    if (!s) return;
    size_t n = strlen(s);
    if (t->tamanho + n + 1 > t->capacidade) {
        size_t cap = t->capacidade ? t->capacidade : 64;
        while (t->tamanho + n + 1 > cap) cap *= 2;
        char* novo = realloc(t->dados, cap);
        if (!novo) {
            fprintf(stderr, "Erro: Memória insuficiente.\n");
            exit(1);
        }
        t->dados = novo;
        t->capacidade = cap;
    }
    memcpy(t->dados + t->tamanho, s, n + 1);
    t->tamanho += n;
}

static char* formatar(const char* fmt, ...) {
    va_list args, copia;
    va_start(args, fmt);
    va_copy(copia, args);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char* buf = malloc((size_t)(n < 0 ? 0 : n) + 1);
    if (!buf) {
        fprintf(stderr, "Erro: Memória insuficiente.\n");
        exit(1);
    }
    vsnprintf(buf, (size_t)(n < 0 ? 0 : n) + 1, fmt, copia);
    va_end(copia);
    return buf;
}

static int precedencia(const char* op) {
    if (!op) return 0;
    if (strcmp(op, "||") == 0) return 1;
    if (strcmp(op, "&&") == 0) return 2;
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return 3;
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) return 4;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 5;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) return 6;
    return 0;
}

static int precisa_parenteses(const NoAST* filho, int prec_pai, int lado_direito) {
    if (!filho) return 0;
    if (filho->no == NO_ATRIBUICAO) return 1;
    if (filho->no != NO_BINARIA) return 0;
    int prec = precedencia(filho->texto);
    return prec < prec_pai || (lado_direito && prec == prec_pai);
}

static void texto_expr_rec(const NoAST* no, Texto* t) {
    if (!no) return;
    switch (no->no) {
        case NO_LITERAL:
        case NO_IDENTIFICADOR:
            texto_add(t, no->texto ? no->texto : "");
            break;
        case NO_INDICE:
            texto_expr_rec(no->filhos[0], t);
            texto_add(t, "[...]");
            break;
        case NO_CHAMADA:
            texto_expr_rec(no->filhos[0], t);
            texto_add(t, "(...)");
            break;
        case NO_ATRIBUICAO:
            texto_expr_rec(no->filhos[0], t);
            texto_add(t, " = ");
            texto_expr_rec(no->filhos[1], t);
            break;
        case NO_BINARIA: {
            int prec = precedencia(no->texto);
            int p_esq = precisa_parenteses(no->filhos[0], prec, 0);
            int p_dir = precisa_parenteses(no->filhos[1], prec, 1);
            if (p_esq) texto_add(t, "(");
            texto_expr_rec(no->filhos[0], t);
            if (p_esq) texto_add(t, ")");
            texto_add(t, " ");
            texto_add(t, no->texto ? no->texto : "");
            texto_add(t, " ");
            if (p_dir) texto_add(t, "(");
            texto_expr_rec(no->filhos[1], t);
            if (p_dir) texto_add(t, ")");
            break;
        }
        case NO_UNARIA: {
            const NoAST* filho = no->filhos[0];
            int par = filho && (filho->no == NO_BINARIA || filho->no == NO_ATRIBUICAO);
            texto_add(t, no->texto ? no->texto : "");
            if (par) texto_add(t, "(");
            texto_expr_rec(filho, t);
            if (par) texto_add(t, ")");
            break;
        }
        default:
            break;
    }
}

/* Representação textual (sem efeitos colaterais) de uma expressão. Deve ser liberada. */
static char* texto_expr(const NoAST* no) {
    Texto t = {0};
    texto_add(&t, "");
    texto_expr_rec(no, &t);
    return t.dados;
}

/* ==============================================================================
 * ANÁLISE DE FLUXO (retorno garantido)
 * ============================================================================== */

/* 1 se o comando contém um break que sai do laço corrente (não desce em laços internos). */
static int contem_break(const NoAST* st) {
    if (!st) return 0;
    switch (st->no) {
        case NO_BREAK: return 1;
        case NO_BLOCO:
            for (size_t i = 0; i < st->quantidade; i++) {
                if (contem_break(st->itens[i])) return 1;
            }
            return 0;
        case NO_SE:
            return contem_break(st->filhos[1]) || contem_break(st->filhos[2]);
        default:
            return 0;
    }
}

/* 1 se o comando nunca termina normalmente (todo caminho executa return
 * ou fica preso em um laço infinito sem break). */
static int sempre_retorna(const NoAST* st) {
    if (!st) return 0;
    switch (st->no) {
        case NO_RETORNO: return 1;
        case NO_BLOCO:
            for (size_t i = 0; i < st->quantidade; i++) {
                if (sempre_retorna(st->itens[i])) return 1;
            }
            return 0;
        case NO_SE:
            return st->filhos[2] && sempre_retorna(st->filhos[1]) && sempre_retorna(st->filhos[2]);
        case NO_ENQUANTO:
            return eh_literal_verdadeiro(st->filhos[0]) && !contem_break(st->filhos[1]);
        case NO_PARA:
            return (!st->filhos[1] || eh_literal_verdadeiro(st->filhos[1])) && !contem_break(st->filhos[3]);
        default:
            return 0;
    }
}

/* ==============================================================================
 * TABELA DE SÍMBOLOS E ESCOPOS
 * ============================================================================== */

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_PARAM
} CategoriaSimbolo;

typedef struct {
    const char* nome;
    Tipo tipo;
    int is_array;
} ParametroInfo;

/* Os nomes apontam para os lexemas da AST/scanner, que vivem durante toda a análise;
 * assim não há truncamento de identificadores longos. */
typedef struct Simbolo Simbolo;
struct Simbolo {
    const char* nome;
    Tipo tipo;
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
    if (!tab) {
        fprintf(stderr, "Erro: Memória insuficiente.\n");
        exit(1);
    }
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

static Tipo tipo_completo(const Simbolo* s) {
    return s->is_array ? tipo_vetor_de(s->tipo) : s->tipo;
}

/* ==============================================================================
 * DIAGNÓSTICOS
 * ============================================================================== */

static char** erros_semanticos = NULL;
static size_t qtd_erros_semanticos = 0;
static size_t cap_erros_semanticos = 0;

static void registrar(const char* codigo, int linha, int coluna, char* msg) {
    if (qtd_erros_semanticos == cap_erros_semanticos) {
        size_t cap = cap_erros_semanticos ? cap_erros_semanticos * 2 : 16;
        char** novo = realloc(erros_semanticos, cap * sizeof(*novo));
        if (!novo) {
            fprintf(stderr, "Erro: Memória insuficiente.\n");
            exit(1);
        }
        erros_semanticos = novo;
        cap_erros_semanticos = cap;
    }
    erros_semanticos[qtd_erros_semanticos++] =
        formatar("%s — linha %d, coluna %d: %s", codigo, linha, coluna, msg);
    free(msg);
}

static void add_error_sem001(const char* name, int line, int col) {
    registrar("SEM001", line, col, formatar("Identificador “%s” não declarado neste escopo.", name));
}

static void add_error_sem002(const char* name, int line, int col, const Simbolo* prev) {
    registrar("SEM002", line, col,
              formatar("“%s” já declarado neste escopo; declaração anterior na linha %d, coluna %d (tipo %s).",
                       name, prev->linha, prev->coluna, tipo_completo(prev)));
}

static void add_error_sem003(const char* target_name, Tipo target_type, Tipo expr_type,
                             const char* expr_text, int line, int col) {
    registrar("SEM003", line, col,
              formatar("Não é possível atribuir %s a %s sem conversão permitida (destino “%s”; expressão “%s”).",
                       expr_type, target_type, target_name, expr_text));
}

static void add_error_sem005(const char* cmd_name, Tipo received_type, const char* expr_text,
                             int line, int col) {
    registrar("SEM005", line, col,
              formatar("Condição de %s deve ter tipo bool; recebeu %s (expressão “%s”).",
                       cmd_name, received_type, expr_text));
}

static void add_error_sem006(const char* array_name, Tipo received_type, const char* expr_text,
                             int line, int col) {
    registrar("SEM006", line, col,
              formatar("Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",
                       array_name, received_type, expr_text));
}

static void add_error_sem007(const char* func_name, size_t expected, size_t received, int line, int col) {
    registrar("SEM007", line, col,
              formatar("“%s” espera %zu argumento%s, mas recebeu %zu.",
                       func_name, expected, expected != 1 ? "s" : "", received));
}

static void add_error_sem008(size_t arg_idx, const char* func_name, Tipo expected_type,
                             Tipo received_type, const char* expr_text, int line, int col) {
    registrar("SEM008", line, col,
              formatar("Argumento %zu de “%s”: esperado %s, recebido %s (expressão “%s”).",
                       arg_idx, func_name, expected_type, received_type, expr_text));
}

static void add_error_sem009(Tipo expr_type, Tipo func_type, const char* func_name, int line, int col) {
    registrar("SEM009", line, col,
              formatar("Retorno %s incompatível com o tipo %s da função “%s”; conversão implícita de %s para %s não permitida.",
                       expr_type, func_type, func_name, expr_type, func_type));
}

static void add_error_sem009_void(const char* func_name, const char* expr_text, int line, int col) {
    registrar("SEM009", line, col,
              formatar("A função “%s” tem tipo void e não pode retornar valor (expressão “%s”).",
                       func_name, expr_text));
}

static void add_error_sem011(const char* func_name, Tipo return_type, const char* motivo, int line, int col) {
    registrar("SEM011", line, col,
              formatar("A função “%s” pode terminar sem retornar %s; %s.", func_name, return_type, motivo));
}

static void add_error_sem012(const char* func_name, const char* contexto, int line, int col) {
    registrar("SEM012", line, col,
              formatar("Função “%s” não produz valor (retorno void) e não pode ser usada %s.",
                       func_name, contexto));
}

static void add_error_sem013(char* motivo, int line, int col) {
    registrar("SEM013", line, col, formatar("Destino de atribuição não é atribuível; %s.", motivo));
    free(motivo);
}

static void add_error_sem014_bin(const char* op, Tipo l, Tipo r, const char* expr_text, int line, int col) {
    registrar("SEM014", line, col,
              formatar("Operador “%s” não se aplica aos tipos %s e %s (expressão “%s”).", op, l, r, expr_text));
}

static void add_error_sem014_un(const char* op, Tipo t, const char* expr_text, int line, int col) {
    registrar("SEM014", line, col,
              formatar("Operador “%s” não se aplica ao tipo %s (expressão “%s”).", op, t, expr_text));
}

static void add_error_sem015(const char* name, const char* type_desc, int line, int col) {
    registrar("SEM015", line, col,
              formatar("“%s” não é vetor e não pode ser indexado (tipo %s).", name, type_desc));
}

static void add_error_sem016(const char* name, int line, int col) {
    registrar("SEM016", line, col,
              formatar("“%s” é uma função e não pode ser usada como valor sem ser chamada.", name));
}

static void add_error_sem017(const char* name, const char* type_desc, int line, int col) {
    registrar("SEM017", line, col,
              formatar("“%s” não é uma função e não pode ser chamado (tipo %s).", name, type_desc));
}

static void add_error_sem018(const char* cmd, int line, int col) {
    registrar("SEM018", line, col, formatar("Comando %s fora de um laço (while ou for).", cmd));
}

static void add_error_sem019(int line, int col) {
    registrar("SEM019", line, col, formatar("Comando return fora de uma função."));
}

static void add_error_sem020(const char* name, int line, int col) {
    registrar("SEM020", line, col, formatar("Tamanho do vetor “%s” deve ser maior que zero.", name));
}

/* ==============================================================================
 * ESTADO DO ANALISADOR
 * ============================================================================== */

static TabelaSimbolos* escopo_global = NULL;
static TabelaSimbolos* escopo_atual = NULL;
static const NoAST* funcao_atual = NULL;
static int profundidade_laco = 0;

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

static Tipo checar_expressao(const NoAST* no);
static void visitar_comando(const NoAST* stmt);

/* Checa uma expressão usada como valor: chamadas void não são permitidas (SEM012). */
static Tipo valor_de(const NoAST* no, const char* contexto) {
    Tipo t = checar_expressao(no);
    if (tipo_igual(t, "void")) {
        char* nome = texto_expr(no && no->no == NO_CHAMADA ? no->filhos[0] : no);
        add_error_sem012(nome, contexto, no->linha, no->coluna);
        free(nome);
        return NULL;
    }
    return t;
}

/* ==============================================================================
 * EXPRESSÕES
 * ============================================================================== */

static const char* descricao_literal(const char* tipo) {
    if (!tipo) return "o literal";
    if (strcmp(tipo, "int") == 0) return "o literal inteiro";
    if (strcmp(tipo, "real") == 0 || strcmp(tipo, "float") == 0) return "o literal real";
    if (strcmp(tipo, "bool") == 0) return "o literal booleano";
    if (strcmp(tipo, "char") == 0) return "o literal de caractere";
    if (strcmp(tipo, "string") == 0) return "o literal de cadeia";
    return "o literal";
}

/* Valida o destino de uma atribuição (valor-L) e devolve seu tipo escalar. */
static Tipo checar_destino(const NoAST* dest, const NoAST* node) {
    if (!dest) return NULL;
    int line = dest->linha ? dest->linha : node->linha;
    int col = dest->coluna ? dest->coluna : node->coluna;

    switch (dest->no) {
        case NO_IDENTIFICADOR: {
            Simbolo* sym = tabela_buscar(escopo_atual, dest->texto);
            if (!sym) {
                add_error_sem001(dest->texto, line, col);
                return NULL;
            }
            if (sym->categoria == SYM_FUNC) {
                add_error_sem013(formatar("“%s” é uma função e não designa uma variável ou elemento de vetor",
                                          dest->texto), line, col);
                return NULL;
            }
            if (sym->is_array) {
                add_error_sem013(formatar("o vetor “%s” não pode ser atribuído como um todo; atribua a um elemento",
                                          dest->texto), line, col);
                return NULL;
            }
            return sym->tipo;
        }
        case NO_INDICE:
            return checar_expressao(dest);
        case NO_LITERAL:
            add_error_sem013(formatar("%s “%s” não designa uma variável ou elemento de vetor",
                                      descricao_literal(dest->tipo), dest->texto ? dest->texto : ""),
                             line, col);
            return NULL;
        default: {
            /* Qualquer outra expressão (aritmética, chamada, atribuição...) não é atribuível */
            checar_expressao(dest);
            char* txt = texto_expr(dest);
            add_error_sem013(formatar("a expressão “%s” não designa uma variável ou elemento de vetor", txt),
                             line, col);
            free(txt);
            return NULL;
        }
    }
}

/* Analisa uma atribuição e devolve o tipo do destino (tipo da expressão). */
static Tipo visitar_atribuicao(const NoAST* node) {
    const NoAST* dest = node->filhos[0];
    const NoAST* val = node->filhos[1];

    Tipo target_type = checar_destino(dest, node);
    Tipo expr_type = valor_de(val, "como expressão de atribuição");

    if (target_type && expr_type && !tipo_atribuivel(target_type, expr_type)) {
        char* target_name = texto_expr(dest && dest->no == NO_INDICE ? dest->filhos[0] : dest);
        char* expr_text = texto_expr(val);
        add_error_sem003(target_name, target_type, expr_type, expr_text,
                         val ? val->linha : node->linha, val ? val->coluna : node->coluna);
        free(target_name);
        free(expr_text);
    }
    return target_type;
}

static Tipo checar_indice(const NoAST* no) {
    const NoAST* base = no->filhos[0];
    const NoAST* indice = no->filhos[1];
    int b_line = base ? base->linha : no->linha;
    int b_col = base ? base->coluna : no->coluna;
    char* base_name = texto_expr(base);
    Tipo elem = NULL;

    if (base && base->no == NO_IDENTIFICADOR) {
        Simbolo* sym = tabela_buscar(escopo_atual, base->texto);
        if (!sym) {
            add_error_sem001(base->texto, b_line, b_col);
        } else if (sym->categoria == SYM_FUNC) {
            add_error_sem015(base_name, "função", b_line, b_col);
        } else if (!sym->is_array) {
            add_error_sem015(base_name, sym->tipo, b_line, b_col);
        } else {
            elem = sym->tipo;
        }
    } else {
        Tipo b_type = valor_de(base, "como vetor");
        if (b_type && !eh_vetor(b_type)) {
            add_error_sem015(base_name, b_type, b_line, b_col);
        } else if (b_type) {
            elem = tipo_elemento(b_type);
        }
    }

    Tipo idx_type = valor_de(indice, "como índice de vetor");
    if (idx_type && strcmp(idx_type, "int") != 0) {
        char* idx_txt = texto_expr(indice);
        add_error_sem006(base_name, idx_type, idx_txt,
                         indice ? indice->linha : no->linha, indice ? indice->coluna : no->coluna);
        free(idx_txt);
    }
    free(base_name);
    return elem;
}

static Tipo checar_chamada(const NoAST* no) {
    const NoAST* func = no->filhos[0];
    int f_line = func ? func->linha : no->linha;
    int f_col = func ? func->coluna : no->coluna;
    char* func_name = texto_expr(func);
    Simbolo* sym = NULL;

    if (func && func->no == NO_IDENTIFICADOR) {
        /* Respeita o escopo léxico: uma variável local pode sombrear uma função global */
        sym = tabela_buscar(escopo_atual, func_name);
        if (!sym) {
            add_error_sem001(func_name, f_line, f_col);
        } else if (sym->categoria != SYM_FUNC) {
            add_error_sem017(func_name, tipo_completo(sym), f_line, f_col);
            sym = NULL;
        }
    } else {
        Tipo f_type = checar_expressao(func);
        if (f_type) add_error_sem017(func_name, f_type, f_line, f_col);
    }

    if (!sym) {
        /* Ainda analisa os argumentos para reportar erros contidos neles */
        for (size_t i = 0; i < no->quantidade; i++) checar_expressao(no->itens[i]);
        free(func_name);
        return NULL;
    }

    if (no->quantidade != sym->qtd_params) {
        add_error_sem007(func_name, sym->qtd_params, no->quantidade, f_line, f_col);
    }

    char* contexto = formatar("como argumento de “%s”", func_name);
    for (size_t i = 0; i < no->quantidade; i++) {
        const NoAST* arg = no->itens[i];
        Tipo a_type = valor_de(arg, contexto);
        if (i >= sym->qtd_params || !a_type) continue;

        const ParametroInfo* p = &sym->params[i];
        Tipo expected = p->is_array ? tipo_vetor_de(p->tipo) : p->tipo;
        int match = p->is_array ? tipo_igual(a_type, expected) : tipo_atribuivel(p->tipo, a_type);
        if (!match) {
            char* a_text = texto_expr(arg);
            add_error_sem008(i + 1, func_name, expected, a_type, a_text,
                             arg ? arg->linha : f_line, arg ? arg->coluna : f_col);
            free(a_text);
        }
    }
    free(contexto);
    free(func_name);
    return sym->tipo;
}

static Tipo checar_binaria(const NoAST* no) {
    const char* op = no->texto ? no->texto : "";
    char* contexto = formatar("como operando de “%s”", op);
    Tipo l = valor_de(no->filhos[0], contexto);
    Tipo r = valor_de(no->filhos[1], contexto);
    free(contexto);
    int ambos = l && r;
    int erro = 0;
    Tipo resultado = NULL;

    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (ambos) {
            if (!(eh_numerico(l) && eh_numerico(r))) {
                erro = 1;
            } else {
                resultado = (tipo_igual(l, "float") || tipo_igual(r, "float")) ? "float" : "int";
            }
        } else {
            Tipo conhecido = l ? l : r;
            resultado = eh_numerico(conhecido) ? conhecido : NULL;
        }
    } else if (strcmp(op, "%") == 0) {
        if (ambos && !(tipo_igual(l, "int") && tipo_igual(r, "int"))) erro = 1;
        else resultado = "int";
    } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
               strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        if (ambos && !((eh_numerico(l) && eh_numerico(r)) ||
                       (tipo_igual(l, "char") && tipo_igual(r, "char")))) erro = 1;
        resultado = "bool";
    } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        if (ambos && !((eh_numerico(l) && eh_numerico(r)) ||
                       (tipo_igual(l, r) && (tipo_igual(l, "bool") || tipo_igual(l, "char"))))) erro = 1;
        resultado = "bool";
    } else if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        if (ambos && !(tipo_igual(l, "bool") && tipo_igual(r, "bool"))) erro = 1;
        resultado = "bool";
    }

    if (erro) {
        char* txt = texto_expr(no);
        add_error_sem014_bin(op, l, r, txt, no->linha, no->coluna);
        free(txt);
    }
    return resultado;
}

static Tipo checar_unaria(const NoAST* no) {
    const char* op = no->texto ? no->texto : "";
    char* contexto = formatar("como operando de “%s”", op);
    Tipo t = valor_de(no->filhos[0], contexto);
    free(contexto);

    if (strcmp(op, "!") == 0) {
        if (t && !tipo_igual(t, "bool")) {
            char* txt = texto_expr(no);
            add_error_sem014_un(op, t, txt, no->linha, no->coluna);
            free(txt);
        }
        return "bool";
    }
    /* menos unário */
    if (t && !eh_numerico(t)) {
        char* txt = texto_expr(no);
        add_error_sem014_un(op, t, txt, no->linha, no->coluna);
        free(txt);
        return NULL;
    }
    return t;
}

static Tipo checar_expressao(const NoAST* no) {
    if (!no) return NULL;

    switch (no->no) {
        case NO_LITERAL:
            return tipo_canonico(no->tipo);

        case NO_IDENTIFICADOR: {
            Simbolo* sym = tabela_buscar(escopo_atual, no->texto);
            if (!sym) {
                add_error_sem001(no->texto, no->linha, no->coluna);
                return NULL;
            }
            if (sym->categoria == SYM_FUNC) {
                add_error_sem016(no->texto, no->linha, no->coluna);
                return NULL;
            }
            return tipo_completo(sym);
        }

        case NO_INDICE:     return checar_indice(no);
        case NO_CHAMADA:    return checar_chamada(no);
        case NO_ATRIBUICAO: return visitar_atribuicao(no);
        case NO_BINARIA:    return checar_binaria(no);
        case NO_UNARIA:     return checar_unaria(no);

        default:
            return NULL;
    }
}

/* ==============================================================================
 * DECLARAÇÕES E COMANDOS
 * ============================================================================== */

static void visitar_declaracao_variavel(const NoAST* node) {
    Simbolo* sym = calloc(1, sizeof(*sym));
    if (!sym) {
        fprintf(stderr, "Erro: Memória insuficiente.\n");
        exit(1);
    }
    sym->nome = node->texto;
    sym->tipo = tipo_canonico(node->tipo);
    sym->categoria = SYM_VAR;
    sym->linha = node->linha;
    sym->coluna = node->coluna;
    sym->is_array = (node->filhos[0] != NULL);
    Tipo tipo_var = sym->tipo;

    const Simbolo* prev = NULL;
    if (!tabela_definir(escopo_atual, sym, &prev)) {
        add_error_sem002(node->texto, node->linha, node->coluna, prev);
        free(sym);
    }

    const NoAST* tamanho = node->filhos[0];
    if (tamanho && tamanho->texto && strtol(tamanho->texto, NULL, 10) <= 0) {
        add_error_sem020(node->texto, tamanho->linha, tamanho->coluna);
    }

    const NoAST* valor = node->filhos[1];
    if (valor) {
        Tipo val_type = valor_de(valor, "como expressão de atribuição");
        if (val_type && !tipo_atribuivel(tipo_var, val_type)) {
            char* txt = texto_expr(valor);
            add_error_sem003(node->texto, tipo_var, val_type, txt, valor->linha, valor->coluna);
            free(txt);
        }
    }
}

static void checar_condicao(const NoAST* cond, const char* cmd, const NoAST* stmt) {
    char* contexto = formatar("como condição de %s", cmd);
    Tipo t = valor_de(cond, contexto);
    free(contexto);
    if (t && strcmp(t, "bool") != 0) {
        char* txt = texto_expr(cond);
        add_error_sem005(cmd, t, txt, cond ? cond->linha : stmt->linha, cond ? cond->coluna : stmt->coluna);
        free(txt);
    }
}

static void visitar_corpo_laco(const NoAST* corpo) {
    profundidade_laco++;
    visitar_comando(corpo);
    profundidade_laco--;
}

static void visitar_retorno(const NoAST* stmt) {
    const NoAST* val = stmt->filhos[0];

    if (!funcao_atual) {
        add_error_sem019(stmt->linha, stmt->coluna);
        if (val) checar_expressao(val);
        return;
    }

    Tipo expected = tipo_canonico(funcao_atual->tipo);
    if (tipo_igual(expected, "void")) {
        if (val) {
            checar_expressao(val);
            char* txt = texto_expr(val);
            add_error_sem009_void(funcao_atual->texto, txt, val->linha, val->coluna);
            free(txt);
        }
        return;
    }

    if (!val) {
        add_error_sem009("void", expected, funcao_atual->texto, stmt->linha, stmt->coluna);
        return;
    }

    Tipo e_type = valor_de(val, "como valor de retorno");
    if (e_type && !tipo_atribuivel(expected, e_type)) {
        add_error_sem009(e_type, expected, funcao_atual->texto, val->linha, val->coluna);
    }
}

static void visitar_comando(const NoAST* stmt) {
    if (!stmt) return;

    switch (stmt->no) {
        case NO_DECLARACAO:
            visitar_declaracao_variavel(stmt);
            break;

        case NO_BLOCO:
            entrar_escopo();
            for (size_t i = 0; i < stmt->quantidade; i++) visitar_comando(stmt->itens[i]);
            sair_escopo();
            break;

        case NO_SE:
            checar_condicao(stmt->filhos[0], "if", stmt);
            visitar_comando(stmt->filhos[1]);
            if (stmt->filhos[2]) visitar_comando(stmt->filhos[2]);
            break;

        case NO_ENQUANTO:
            checar_condicao(stmt->filhos[0], "while", stmt);
            visitar_corpo_laco(stmt->filhos[1]);
            break;

        case NO_PARA:
            if (stmt->filhos[0]) checar_expressao(stmt->filhos[0]);
            if (stmt->filhos[1]) checar_condicao(stmt->filhos[1], "for", stmt);
            if (stmt->filhos[2]) checar_expressao(stmt->filhos[2]);
            visitar_corpo_laco(stmt->filhos[3]);
            break;

        case NO_RETORNO:
            visitar_retorno(stmt);
            break;

        case NO_ATRIBUICAO:
            visitar_atribuicao(stmt);
            break;

        case NO_COMANDO_EXPRESSAO:
            /* Em posição de comando, uma chamada void é permitida */
            if (stmt->filhos[0]) checar_expressao(stmt->filhos[0]);
            break;

        case NO_IMPRIMIR:
            for (size_t i = 0; i < stmt->quantidade; i++) {
                valor_de(stmt->itens[i], "como argumento de print");
            }
            break;

        case NO_BREAK:
        case NO_CONTINUE:
            if (profundidade_laco == 0) {
                add_error_sem018(stmt->no == NO_BREAK ? "break" : "continue", stmt->linha, stmt->coluna);
            }
            break;

        default:
            /* NO_LER e NO_VAZIO não têm restrições semânticas */
            break;
    }
}

/* Descreve, para o SEM011, por qual caminho o fim do corpo é alcançado. Deve ser liberada. */
static char* motivo_queda(const NoAST* corpo) {
    if (corpo) {
        for (size_t k = corpo->quantidade; k > 0; k--) {
            const NoAST* st = corpo->itens[k - 1];
            if (!st || st->no != NO_SE) continue;
            int ret_entao = sempre_retorna(st->filhos[1]);
            int ret_senao = sempre_retorna(st->filhos[2]);
            if (ret_entao != ret_senao) {
                char* cond = texto_expr(st->filhos[0]);
                char* motivo = formatar("o ramo em que “%s” é %s alcança o fim do corpo",
                                        cond, ret_entao ? "falso" : "verdadeiro");
                free(cond);
                return motivo;
            }
        }
    }
    return formatar("o fim do corpo pode ser alcançado sem um comando return");
}

static void visitar_declaracao_funcao(const NoAST* node) {
    funcao_atual = node;
    profundidade_laco = 0;
    entrar_escopo();

    /* Registrar parâmetros no escopo local da função */
    for (size_t i = 0; i < node->quantidade; i++) {
        const NoAST* p = node->itens[i];
        Simbolo* psym = calloc(1, sizeof(*psym));
        if (!psym) {
            fprintf(stderr, "Erro: Memória insuficiente.\n");
            exit(1);
        }
        psym->nome = p->texto;
        psym->tipo = tipo_canonico(p->tipo);
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

    /* O corpo compartilha o escopo dos parâmetros */
    const NoAST* corpo = node->filhos[0];
    if (corpo) {
        for (size_t i = 0; i < corpo->quantidade; i++) visitar_comando(corpo->itens[i]);
    }

    /* Validação de retorno obrigatório em funções não-void (SEM011) */
    Tipo tipo_ret = tipo_canonico(node->tipo);
    if (!tipo_igual(tipo_ret, "void") && !sempre_retorna(corpo)) {
        char* motivo = motivo_queda(corpo);
        add_error_sem011(node->texto, tipo_ret, motivo, node->linha, node->coluna);
        free(motivo);
    }

    sair_escopo();
    funcao_atual = NULL;
}

static int executar_analise_semantica(const NoAST* arvore) {
    if (!arvore || arvore->no != NO_PROGRAMA) return 0;

    escopo_global = tabela_criar(NULL);
    escopo_atual = escopo_global;

    /* Passada 1: coletar assinaturas das funções globais (permite recursão e chamadas
     * para funções definidas mais adiante). Duplicatas são reportadas na passada 2,
     * para que os diagnósticos saiam na ordem do código-fonte. */
    const Simbolo** duplicada_de = calloc(arvore->quantidade ? arvore->quantidade : 1, sizeof(*duplicada_de));
    Simbolo** simbolos_dup = calloc(arvore->quantidade ? arvore->quantidade : 1, sizeof(*simbolos_dup));
    if (!duplicada_de || !simbolos_dup) {
        fprintf(stderr, "Erro: Memória insuficiente.\n");
        exit(1);
    }

    for (size_t i = 0; i < arvore->quantidade; i++) {
        const NoAST* item = arvore->itens[i];
        if (!item || item->no != NO_FUNCAO) continue;

        Simbolo* fsym = calloc(1, sizeof(*fsym));
        if (!fsym) {
            fprintf(stderr, "Erro: Memória insuficiente.\n");
            exit(1);
        }
        fsym->nome = item->texto;
        fsym->tipo = tipo_canonico(item->tipo);
        fsym->categoria = SYM_FUNC;
        fsym->linha = item->linha_nome ? item->linha_nome : item->linha;
        fsym->coluna = item->coluna_nome ? item->coluna_nome : item->coluna;
        fsym->qtd_params = item->quantidade;
        if (fsym->qtd_params > 0) {
            fsym->params = calloc(fsym->qtd_params, sizeof(ParametroInfo));
            if (!fsym->params) {
                fprintf(stderr, "Erro: Memória insuficiente.\n");
                exit(1);
            }
            for (size_t j = 0; j < item->quantidade; j++) {
                const NoAST* p = item->itens[j];
                fsym->params[j].nome = p->texto ? p->texto : "";
                fsym->params[j].tipo = tipo_canonico(p->tipo);
                fsym->params[j].is_array = p->vetor;
            }
        }

        const Simbolo* prev = NULL;
        if (!tabela_definir(escopo_global, fsym, &prev)) {
            duplicada_de[i] = prev;
            simbolos_dup[i] = fsym; /* liberado ao final */
        }
    }

    /* Passada 2: analisar declarações globais, funções e comandos, em ordem */
    for (size_t i = 0; i < arvore->quantidade; i++) {
        const NoAST* item = arvore->itens[i];
        if (!item) continue;
        if (item->no == NO_DECLARACAO) {
            visitar_declaracao_variavel(item);
        } else if (item->no == NO_FUNCAO) {
            if (duplicada_de[i]) {
                add_error_sem002(item->texto, simbolos_dup[i]->linha, simbolos_dup[i]->coluna, duplicada_de[i]);
            }
            visitar_declaracao_funcao(item);
        } else {
            visitar_comando(item);
        }
    }

    /* Emissão de diagnósticos */
    if (qtd_erros_semanticos == 0) {
        printf("Análise semântica concluída: 0 erros; programa aceito.");
    } else {
        for (size_t i = 0; i < qtd_erros_semanticos; i++) {
            printf("%s\n", erros_semanticos[i]);
        }
        printf("Análise semântica concluída: %zu erro%s; programa rejeitado.",
               qtd_erros_semanticos, qtd_erros_semanticos > 1 ? "s" : "");
    }

    int aceito = (qtd_erros_semanticos == 0);

    for (size_t i = 0; i < arvore->quantidade; i++) {
        if (simbolos_dup[i]) {
            free(simbolos_dup[i]->params);
            free(simbolos_dup[i]);
        }
    }
    free(simbolos_dup);
    free(duplicada_de);
    for (size_t i = 0; i < qtd_erros_semanticos; i++) free(erros_semanticos[i]);
    free(erros_semanticos);
    erros_semanticos = NULL;
    qtd_erros_semanticos = cap_erros_semanticos = 0;

    tabela_liberar(escopo_global);
    escopo_global = NULL;
    escopo_atual = NULL;

    return aceito;
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
    int aceito = executar_analise_semantica(arvore);

    liberar_parser(p);
    free_scanner();
    /* Código de saída 0 = programa aceito; 1 = programa rejeitado */
    return aceito ? 0 : 1;
}
