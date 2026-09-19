#include <setjmp.h>
#include <stdint.h>

#ifdef SCANNER_WITH_PARSER
#include "../scanner_c/scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else

#define SCANNER_NO_MAIN
#include "../scanner_c/scanner.c"
#endif

#include "tabela_ll1.h"

typedef enum {
    NO_PROGRAMA, NO_BLOCO, NO_DECLARACAO, NO_FUNCAO, NO_PARAMETRO,
    NO_IDENTIFICADOR, NO_LITERAL, NO_BINARIA, NO_UNARIA, NO_ATRIBUICAO,
    NO_INDICE, NO_CHAMADA, NO_COMANDO_EXPRESSAO, NO_RETORNO, NO_SE,
    NO_ENQUANTO, NO_PARA, NO_IMPRIMIR, NO_BREAK, NO_CONTINUE, NO_LER,
    NO_VAZIO
} TipoNo;

typedef struct NoAST NoAST;
struct NoAST {
    TipoNo no;
    const char* tipo;
    const char* texto;
    int vetor;
    NoAST* filhos[4];
    NoAST** itens;
    size_t quantidade;
    size_t capacidade;
    NoAST* proximo_alocado;
};

typedef struct {
    Token* entrada;
    size_t quantidade;
    size_t posicao;
    NoAST* alocados;
    jmp_buf erro;
    char mensagem[256];
    int linha_erro;
    int coluna_erro;
} ParserLL1;

static NoAST* expressao(ParserLL1* p);
static NoAST* comando(ParserLL1* p);
static NoAST* bloco(ParserLL1* p);
static NoAST* expressao_unaria(ParserLL1* p);

static const Token* atual(const ParserLL1* p) {
    static const Token eof = {"EOF", "", ATTR_NONE, 0, 0, NULL, 1, 1};
    return p->posicao < p->quantidade ? &p->entrada[p->posicao] : &eof;
}

static int verificar(const ParserLL1* p, const char* tipo) {
    return strcmp(atual(p)->token, tipo) == 0;
}

static _Noreturn void falhar(ParserLL1* p, const char* mensagem) {
    const Token* token = atual(p);
    snprintf(p->mensagem, sizeof(p->mensagem), "%s", mensagem);
    p->linha_erro = token->line;
    p->coluna_erro = token->column;
    longjmp(p->erro, 1);
}

static _Noreturn void sem_memoria(ParserLL1* p) {
    longjmp(p->erro, 2);
}

static const Token* consumir(ParserLL1* p, const char* tipo) {
    if (!verificar(p, tipo)) {
        char mensagem[256];
        snprintf(mensagem, sizeof(mensagem), "esperado %s", tipo);
        falhar(p, mensagem);
    }
    return &p->entrada[p->posicao++];
}

static int aceitar(ParserLL1* p, const char* tipo) {
    if (!verificar(p, tipo)) return 0;
    p->posicao++;
    return 1;
}

static int consultar_tabela(const ParserLL1* p, const char* variavel) {
    const char* terminal = atual(p)->token;
    if (strcmp(terminal, "NEQ") == 0) terminal = "NE";
    for (size_t i = 0; i < sizeof(TABELA_LL1) / sizeof(TABELA_LL1[0]); i++) {
        if (strcmp(TABELA_LL1[i].variavel, variavel) == 0 &&
            strcmp(TABELA_LL1[i].terminal, terminal) == 0) {
            return TABELA_LL1[i].regra;
        }
    }
    return 0;
}

static NoAST* novo_no(ParserLL1* p, TipoNo tipo) {
    NoAST* no = calloc(1, sizeof(*no));
    if (!no) sem_memoria(p);
    no->no = tipo;
    no->proximo_alocado = p->alocados;
    p->alocados = no;
    return no;
}

static void adicionar_item(ParserLL1* p, NoAST* lista, NoAST* item) {
    if (lista->quantidade == lista->capacidade) {
        if (lista->capacidade > SIZE_MAX / 2 / sizeof(*lista->itens)) sem_memoria(p);
        size_t capacidade = lista->capacidade ? lista->capacidade * 2 : 8;
        NoAST** itens = realloc(lista->itens, capacidade * sizeof(*itens));
        if (!itens) sem_memoria(p);
        lista->itens = itens;
        lista->capacidade = capacidade;
    }
    lista->itens[lista->quantidade++] = item;
}

static NoAST* literal(ParserLL1* p, const char* tipo, const char* lexema) {
    NoAST* no = novo_no(p, NO_LITERAL);
    no->tipo = tipo;
    no->texto = lexema;
    return no;
}

static void adaptar_tokens(ParserLL1* p, const Token* originais, size_t quantidade) {
    if (quantidade > (SIZE_MAX / sizeof(Token) - 1) / 2) sem_memoria(p);
    p->entrada = malloc((quantidade * 2 + 1) * sizeof(*p->entrada));
    if (!p->entrada) sem_memoria(p);
    for (size_t i = 0; i < quantidade; i++) {
        Token token = originais[i];
        if ((strcmp(token.token, "INT_LIT") == 0 || strcmp(token.token, "FLOAT_LIT") == 0)
                && token.lexeme[0] == '-') {
            Token sinal = token;
            sinal.token = "MINUS";
            sinal.lexeme = "-";
            sinal.attr_type = ATTR_NONE;
            sinal.attr_str = NULL;
            p->entrada[p->quantidade++] = sinal;

            token.lexeme++;
            token.column++;
        }
        if (strcmp(token.token, "NEQ") == 0) token.token = "NE";
        p->entrada[p->quantidade++] = token;
    }
    if (!p->quantidade || strcmp(p->entrada[p->quantidade - 1].token, "EOF") != 0) {
        Token eof = {"EOF", "", ATTR_NONE, 0, 0, NULL, 1, 1};
        p->entrada[p->quantidade++] = eof;
    }
}

static void liberar_parser(ParserLL1* p) {

    NoAST* no = p->alocados;
    while (no) {
        NoAST* proximo = no->proximo_alocado;
        free(no->itens);
        free(no);
        no = proximo;
    }
    free(p->entrada);
    free(p);
}

static const char* ler_tipo(ParserLL1* p, int permite_void) {
    if (!consultar_tabela(p, permite_void ? "tipo_retorno" : "tipo")) {
        falhar(p, "esperado tipo: int, float, bool ou char (void apenas no retorno)");
    }
    return consumir(p, atual(p)->token)->lexeme;
}

static NoAST* declarador(ParserLL1* p, const char* tipo, const char* nome) {
    NoAST* no = novo_no(p, NO_DECLARACAO);
    no->tipo = tipo;
    no->texto = nome;
    if (aceitar(p, "LBRACKET")) {
        no->filhos[0] = literal(p, "int", consumir(p, "INT_LIT")->lexeme);
        consumir(p, "RBRACKET");
    } else if (aceitar(p, "ASSIGN")) {
        no->filhos[1] = expressao(p);
    }
    return no;
}

static void resto_declaracao(ParserLL1* p, NoAST* lista, const char* tipo, const char* nome) {
    adicionar_item(p, lista, declarador(p, tipo, nome));
    while (aceitar(p, "COMMA")) {
        nome = consumir(p, "IDENT")->lexeme;
        adicionar_item(p, lista, declarador(p, tipo, nome));
    }
    consumir(p, "SEMICOLON");
}

static void lista_parametros(ParserLL1* p, NoAST* funcao) {
    if (verificar(p, "RPAREN")) return;
    do {
        const char* tipo = ler_tipo(p, 0);
        const char* nome = consumir(p, "IDENT")->lexeme;
        NoAST* parametro = novo_no(p, NO_PARAMETRO);
        parametro->tipo = tipo;
        parametro->texto = nome;
        parametro->vetor = aceitar(p, "LBRACKET");
        if (parametro->vetor) consumir(p, "RBRACKET");
        adicionar_item(p, funcao, parametro);
    } while (aceitar(p, "COMMA"));
}

static void decl_ou_funcao(ParserLL1* p, NoAST* programa) {
    const char* tipo = ler_tipo(p, 1);
    const char* nome = consumir(p, "IDENT")->lexeme;
    if (aceitar(p, "LPAREN")) {
        NoAST* funcao = novo_no(p, NO_FUNCAO);
        funcao->tipo = tipo;
        funcao->texto = nome;
        lista_parametros(p, funcao);
        consumir(p, "RPAREN");
        funcao->filhos[0] = bloco(p);
        adicionar_item(p, programa, funcao);
    } else {
        if (strcmp(tipo, "void") == 0) falhar(p, "void exige uma funcao");
        resto_declaracao(p, programa, tipo, nome);
    }
}

static NoAST* bloco(ParserLL1* p) {
    consumir(p, "LBRACE");
    NoAST* no = novo_no(p, NO_BLOCO);
    while (!verificar(p, "RBRACE")) {
        switch (consultar_tabela(p, "item_bloco")) {
            case 26: {
                const char* tipo = ler_tipo(p, 0);
                const char* nome = consumir(p, "IDENT")->lexeme;
                resto_declaracao(p, no, tipo, nome);
                break;
            }
            case 27:
                adicionar_item(p, no, comando(p));
                break;
            default:
                falhar(p, "esperado declaracao, comando ou '}' para fechar o bloco");
        }
    }
    consumir(p, "RBRACE");
    return no;
}

static NoAST* parse(ParserLL1* p) {
    if (!consultar_tabela(p, "programa")) falhar(p, "esperado inicio de programa");
    NoAST* programa = novo_no(p, NO_PROGRAMA);
    while (!verificar(p, "EOF")) {
        switch (consultar_tabela(p, "elemento")) {
            case 4: decl_ou_funcao(p, programa); break;
            case 5: adicionar_item(p, programa, comando(p)); break;
            default: falhar(p, "esperado declaracao, funcao ou comando");
        }
    }
    consumir(p, "EOF");
    return programa;
}

static NoAST* expressao_opcional(ParserLL1* p, const char* delimitador) {
    return verificar(p, delimitador) ? NULL : expressao(p);
}

static void lista_argumentos(ParserLL1* p, NoAST* no) {
    do {
        adicionar_item(p, no, expressao(p));
    } while (aceitar(p, "COMMA"));
}

static NoAST* comando_if(ParserLL1* p) {
    consumir(p, "IF");
    consumir(p, "LPAREN");
    NoAST* no = novo_no(p, NO_SE);
    no->filhos[0] = expressao(p);
    consumir(p, "RPAREN");
    no->filhos[1] = comando(p);

    if (aceitar(p, "ELSE")) no->filhos[2] = comando(p);
    return no;
}

static NoAST* comando_while(ParserLL1* p) {
    consumir(p, "WHILE");
    consumir(p, "LPAREN");
    NoAST* no = novo_no(p, NO_ENQUANTO);
    no->filhos[0] = expressao(p);
    consumir(p, "RPAREN");
    no->filhos[1] = comando(p);
    return no;
}

static NoAST* comando_for(ParserLL1* p) {
    consumir(p, "FOR");
    consumir(p, "LPAREN");
    NoAST* no = novo_no(p, NO_PARA);
    no->filhos[0] = expressao_opcional(p, "SEMICOLON");
    consumir(p, "SEMICOLON");
    no->filhos[1] = expressao_opcional(p, "SEMICOLON");
    consumir(p, "SEMICOLON");
    no->filhos[2] = expressao_opcional(p, "RPAREN");
    consumir(p, "RPAREN");
    no->filhos[3] = comando(p);
    return no;
}

static NoAST* comando_return(ParserLL1* p) {
    consumir(p, "RETURN");
    NoAST* no = novo_no(p, NO_RETORNO);
    no->filhos[0] = expressao_opcional(p, "SEMICOLON");
    consumir(p, "SEMICOLON");
    return no;
}

static NoAST* comando_print(ParserLL1* p) {
    consumir(p, "PRINT");
    consumir(p, "LPAREN");
    NoAST* no = novo_no(p, NO_IMPRIMIR);
    lista_argumentos(p, no);
    consumir(p, "RPAREN");
    consumir(p, "SEMICOLON");
    return no;
}

static NoAST* comando_read(ParserLL1* p) {
    consumir(p, "READ");
    consumir(p, "LPAREN");
    consumir(p, "RPAREN");
    consumir(p, "SEMICOLON");
    return novo_no(p, NO_LER);
}

static NoAST* comando(ParserLL1* p) {
    switch (consultar_tabela(p, "comando")) {
        case 37: return bloco(p);
        case 38: return comando_if(p);
        case 39: return comando_while(p);
        case 40: return comando_for(p);
        case 41: return comando_return(p);
        case 42:
        case 43: {
            TipoNo tipo = verificar(p, "BREAK") ? NO_BREAK : NO_CONTINUE;
            consumir(p, atual(p)->token);
            consumir(p, "SEMICOLON");
            return novo_no(p, tipo);
        }
        case 44: return comando_print(p);
        case 45: return comando_read(p);
        case 46: {
            if (aceitar(p, "SEMICOLON")) return novo_no(p, NO_VAZIO);
            NoAST* no = novo_no(p, NO_COMANDO_EXPRESSAO);
            no->filhos[0] = expressao(p);
            consumir(p, "SEMICOLON");
            return no;
        }
        default: falhar(p, "esperado inicio de comando");
    }
}

typedef NoAST* (*NivelExpressao)(ParserLL1*);

static int operador_atual(const ParserLL1* p, const char* const* operadores) {
    for (size_t i = 0; operadores[i]; i++) {
        if (verificar(p, operadores[i])) return 1;
    }
    return 0;
}

static NoAST* binaria(ParserLL1* p, NivelExpressao proximo, const char* const* operadores) {
    NoAST* esquerda = proximo(p);
    while (operador_atual(p, operadores)) {
        const char* operador = consumir(p, atual(p)->token)->lexeme;
        NoAST* no = novo_no(p, NO_BINARIA);
        no->texto = operador;
        no->filhos[0] = esquerda;
        no->filhos[1] = proximo(p);
        esquerda = no;
    }
    return esquerda;
}

static NoAST* expressao_multiplicativa(ParserLL1* p) {
    static const char* const ops[] = {"STAR", "SLASH", "PERCENT", NULL};
    return binaria(p, expressao_unaria, ops);
}

static NoAST* expressao_aditiva(ParserLL1* p) {
    static const char* const ops[] = {"PLUS", "MINUS", NULL};
    return binaria(p, expressao_multiplicativa, ops);
}

static NoAST* expressao_relacional(ParserLL1* p) {
    static const char* const ops[] = {"LT", "GT", "LE", "GE", NULL};
    return binaria(p, expressao_aditiva, ops);
}

static NoAST* expressao_igualdade(ParserLL1* p) {
    static const char* const ops[] = {"EQ", "NE", "NEQ", NULL};
    return binaria(p, expressao_relacional, ops);
}

static NoAST* expressao_e(ParserLL1* p) {
    static const char* const ops[] = {"AND", NULL};
    return binaria(p, expressao_igualdade, ops);
}

static NoAST* expressao_ou(ParserLL1* p) {
    static const char* const ops[] = {"OR", NULL};
    return binaria(p, expressao_e, ops);
}

static int destino_valido(const NoAST* no) {
    while (no && no->no == NO_INDICE) no = no->filhos[0];
    return no && no->no == NO_IDENTIFICADOR;
}

static NoAST* atribuicao(ParserLL1* p) {
    NoAST* esquerda = expressao_ou(p);
    if (verificar(p, "ASSIGN")) {
        if (!destino_valido(esquerda)) falhar(p, "destino de atribuicao invalido");
        consumir(p, "ASSIGN");
        NoAST* no = novo_no(p, NO_ATRIBUICAO);
        no->filhos[0] = esquerda;
        no->filhos[1] = atribuicao(p);
        return no;
    }
    return esquerda;
}

static NoAST* expressao(ParserLL1* p) {
    return atribuicao(p);
}

static NoAST* primario(ParserLL1* p) {
    int regra = consultar_tabela(p, "primario");
    if (regra == 103) {
        NoAST* no = novo_no(p, NO_IDENTIFICADOR);
        no->texto = consumir(p, "IDENT")->lexeme;
        return no;
    }
    if (regra >= 104 && regra <= 109) {
        static const char* const tipos[] = {"int", "real", "bool", "bool", "char", "string"};
        const Token* token = consumir(p, atual(p)->token);
        return literal(p, tipos[regra - 104], token->lexeme);
    }
    if (regra == 110) {
        consumir(p, "LPAREN");
        NoAST* no = expressao(p);
        consumir(p, "RPAREN");
        return no;
    }
    falhar(p, "esperado expressao: identificador, literal ou '('");
}

static NoAST* expressao_posfixa(ParserLL1* p) {
    NoAST* arvore = primario(p);
    for (;;) {
        if (aceitar(p, "LBRACKET")) {
            NoAST* no = novo_no(p, NO_INDICE);
            no->filhos[0] = arvore;
            no->filhos[1] = expressao(p);
            consumir(p, "RBRACKET");
            arvore = no;
        } else if (aceitar(p, "LPAREN")) {
            NoAST* no = novo_no(p, NO_CHAMADA);
            no->filhos[0] = arvore;
            if (!verificar(p, "RPAREN")) lista_argumentos(p, no);
            consumir(p, "RPAREN");
            arvore = no;
        } else {
            return arvore;
        }
    }
}

static NoAST* expressao_unaria(ParserLL1* p) {
    switch (consultar_tabela(p, "expressao_unaria")) {
        case 91:
        case 92: {
            const char* operador = consumir(p, atual(p)->token)->lexeme;
            NoAST* no = novo_no(p, NO_UNARIA);
            no->texto = operador;
            no->filhos[0] = expressao_unaria(p);
            return no;
        }
        case 93: return expressao_posfixa(p);
        default: falhar(p, "esperado inicio de expressao");
    }
}

static void formatar_ast(const NoAST* no, FILE* saida);

static void formatar_lista(const NoAST* no, FILE* saida) {
    for (size_t i = 0; i < no->quantidade; i++) {
        if (i) fputs(", ", saida);
        formatar_ast(no->itens[i], saida);
    }
}

static void formatar_filhos(const NoAST* no, FILE* saida, size_t quantidade) {
    for (size_t i = 0; i < quantidade; i++) {
        if (i) fputs(", ", saida);
        formatar_ast(no->filhos[i], saida);
    }
}

static void formatar_ast(const NoAST* no, FILE* saida) {
    if (!no) {
        fputs("NULL", saida);
        return;
    }
    switch (no->no) {
        case NO_PROGRAMA:
        case NO_BLOCO:
            fputs(no->no == NO_PROGRAMA ? "Program(" : "Block(", saida);
            formatar_lista(no, saida);
            break;
        case NO_DECLARACAO:
            fprintf(saida, "VarDecl(%s %s", no->tipo, no->texto);
            if (no->filhos[0]) {
                fputs(" size=", saida);
                formatar_ast(no->filhos[0], saida);
            }
            if (no->filhos[1]) {
                fputs(" = ", saida);
                formatar_ast(no->filhos[1], saida);
            }
            break;
        case NO_FUNCAO:
            fprintf(saida, "Function(%s %s(", no->tipo, no->texto);
            formatar_lista(no, saida);
            fputs(") ", saida);
            formatar_ast(no->filhos[0], saida);
            break;
        case NO_PARAMETRO:
            fprintf(saida, "%s %s%s", no->tipo, no->texto, no->vetor ? "[]" : "");
            return;
        case NO_IDENTIFICADOR: fprintf(saida, "Id(%s", no->texto); break;
        case NO_LITERAL: fprintf(saida, "Lit(%s,%s", no->tipo, no->texto); break;
        case NO_BINARIA:
            fprintf(saida, "Binary(%s, ", no->texto);
            formatar_filhos(no, saida, 2);
            break;
        case NO_UNARIA:
            fprintf(saida, "Unary(%s, ", no->texto);
            formatar_filhos(no, saida, 1);
            break;
        case NO_ATRIBUICAO:
            fputs("Assign(", saida);
            formatar_filhos(no, saida, 2);
            break;
        case NO_INDICE:
            fputs("Index(", saida);
            formatar_filhos(no, saida, 2);
            break;
        case NO_CHAMADA:
            fputs("Call(", saida);
            formatar_ast(no->filhos[0], saida);
            if (no->quantidade) fputs(", ", saida);
            formatar_lista(no, saida);
            break;
        case NO_COMANDO_EXPRESSAO:
        case NO_RETORNO:
            fputs(no->no == NO_RETORNO ? "Return(" : "ExprStmt(", saida);
            formatar_filhos(no, saida, 1);
            break;
        case NO_SE:
            fputs("If(", saida);
            formatar_filhos(no, saida, 3);
            break;
        case NO_ENQUANTO:
            fputs("While(", saida);
            formatar_filhos(no, saida, 2);
            break;
        case NO_PARA:
            fputs("For(", saida);
            formatar_filhos(no, saida, 4);
            break;
        case NO_IMPRIMIR:
            fputs("Print(", saida);
            formatar_lista(no, saida);
            break;
        case NO_BREAK: fputs("Break(", saida); break;
        case NO_CONTINUE: fputs("Continue(", saida); break;
        case NO_LER: fputs("Read(", saida); break;
        case NO_VAZIO: fputs("Empty(", saida); break;
    }
    fputc(')', saida);
}

static NoAST* analisar_tokens(ParserLL1* p, const Token* entrada, size_t quantidade) {
    adaptar_tokens(p, entrada, quantidade);
    return parse(p);
}

int parser_main(int argc, char** argv) {
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
            puts("NÃO HÁ AST: o parser deve rejeitar a entrada.");
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
    formatar_ast(arvore, stdout);
    putchar('\n');
    liberar_parser(p);
    free_scanner();
    return 0;
}

#if !defined(SCANNER_WITH_PARSER) && !defined(PARSER_NO_MAIN)
int main(int argc, char** argv) {
    return parser_main(argc, argv);
}
#endif
