#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "scanner.h"

#ifdef _WIN32
    #include <direct.h>
    #define MAKE_DIR(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #define MAKE_DIR(path) mkdir(path, 0777)
#endif

// --- CONSTANTES ---
const char* RESERVED_WORDS[] = {
    "int", "float", "char", "double", "void", "if", "else", "for", "while", "return",
    "do", "switch", "case", "break", "continue", "default", "struct", "bool", "true", "false",
    "print", "read"
};
const int NUM_RESERVED = 22;

typedef enum {
    STATE_INITIAL, STATE_IDENTIFIER, STATE_INT, STATE_REAL, STATE_FLOAT_SUFFIX,
    STATE_SYMBOL, STATE_STRING, STATE_CHAR, STATE_ESCAPE_STRING, STATE_ESCAPE_CHAR,
    STATE_CHAR_WITH_ESCAPE, STATE_LINE_COMMENT, STATE_BLOCK_COMMENT, STATE_BLOCK_COMMENT_END
} State;

// --- ESTRUTURAS DINÂMICAS ---
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StringBuffer;

void sb_init(StringBuffer* sb) {
    sb->cap = 64;
    sb->len = 0;
    sb->data = (char*)malloc(sb->cap);
    sb->data[0] = '\0';
}

void sb_append(StringBuffer* sb, char c) {
    if (sb->len + 1 >= sb->cap) {
        sb->cap *= 2;
        sb->data = (char*)realloc(sb->data, sb->cap);
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

void sb_append_str(StringBuffer* sb, const char* str) {
    while (*str) sb_append(sb, *str++);
}

void sb_clear(StringBuffer* sb) {
    sb->len = 0;
    sb->data[0] = '\0';
}

void sb_free(StringBuffer* sb) {
    free(sb->data);
}

// Arrays Dinâmicos para Tokens e Erros
Token* tokens = NULL;
size_t tokens_count = 0;
size_t tokens_cap = 0;

Error* errors = NULL;
size_t errors_count = 0;
size_t errors_cap = 0;

// strdup não faz parte do C11; esta versão mantém o scanner portável.
static char* scanner_duplicate_string(const char* source) {
    size_t length = strlen(source) + 1;
    char* copy = (char*)malloc(length);
    if (!copy) {
        fprintf(stderr, "Erro: Memória insuficiente no scanner.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, source, length);
    return copy;
}

void add_token(const char* tok, const char* lex, AttrType type, int a_int, double a_float, const char* a_str, int line, int col) {
    if (tokens_count >= tokens_cap) {
        tokens_cap = tokens_cap == 0 ? 128 : tokens_cap * 2;
        tokens = (Token*)realloc(tokens, tokens_cap * sizeof(Token));
    }
    Token* t = &tokens[tokens_count++];
    t->token = tok;
    t->lexeme = scanner_duplicate_string(lex);
    t->attr_type = type;
    t->attr_int = a_int;
    t->attr_float = a_float;
    t->attr_str = a_str ? scanner_duplicate_string(a_str) : NULL;
    t->line = line;
    t->column = col;
}

void add_error(const char* err_type, const char* lex, int line, int col) {
    if (errors_count >= errors_cap) {
        errors_cap = errors_cap == 0 ? 32 : errors_cap * 2;
        errors = (Error*)realloc(errors, errors_cap * sizeof(Error));
    }
    Error* e = &errors[errors_count++];
    e->error = err_type;
    e->lexeme = scanner_duplicate_string(lex);
    e->line = line;
    e->column = col;
}

// --- FUNÇÕES AUXILIARES ---
bool is_reserved(const char* str) {
    for (int i = 0; i < NUM_RESERVED; i++) {
        if (strcmp(str, RESERVED_WORDS[i]) == 0) return true;
    }
    return false;
}

void free_scanner(void) {
    for (size_t i = 0; i < tokens_count; i++) {
        // Os nomes de palavras reservadas são os únicos tokens alocados.
        if (is_reserved(tokens[i].lexeme)) free((void*)tokens[i].token);
        free(tokens[i].lexeme);
        free(tokens[i].attr_str);
    }
    free(tokens);
    tokens = NULL;
    tokens_count = 0;
    tokens_cap = 0;

    for (size_t i = 0; i < errors_count; i++) {
        free(errors[i].lexeme);
    }
    free(errors);
    errors = NULL;
    errors_count = 0;
    errors_cap = 0;
}

void toupper_str(char* dest, const char* src) {
    while (*src) {
        *dest++ = toupper((unsigned char)*src++);
    }
    *dest = '\0';
}

const char* map_simple_symbol(char c) {
    switch(c) {
        case '<': return "LT"; case '>': return "GT"; case '=': return "ASSIGN";
        case '+': return "PLUS"; case '-': return "MINUS"; case '*': return "STAR";
        case '/': return "SLASH"; case '.': return "DOT"; case '!': return "NOT";
        case '&': return "AMPERSAND"; case '|': return "PIPE"; case '%': return "PERCENT";
        default: return NULL;
    }
}

const char* map_bracket_symbol(char c) {
    switch(c) {
        case '(': return "LPAREN"; case ')': return "RPAREN"; case '{': return "LBRACE";
        case '}': return "RBRACE"; case '[': return "LBRACKET"; case ']': return "RBRACKET";
        case ',': return "COMMA"; case ';': return "SEMICOLON"; case ':': return "COLON";
        default: return NULL;
    }
}

void recover_tokens(const char* lexema, int line, int col) {
    int pos = strlen(lexema) - 1;
    while (pos >= 0 && strchr(" \t\r", lexema[pos])) pos--;

    int count = 0;
    char found_chars[128];
    const char* found_tokens[128];
    int found_cols[128];

    while (pos >= 0) {
        const char* t = map_bracket_symbol(lexema[pos]);
        if (!t) t = map_simple_symbol(lexema[pos]);
        if (t) {
            found_chars[count] = lexema[pos];
            found_tokens[count] = t;
            found_cols[count] = pos;
            count++;
            pos--;
        } else {
            break;
        }
    }

    for (int i = count - 1; i >= 0; i--) {
        char lex[2] = { found_chars[i], '\0' };
        add_token(found_tokens[i], lex, ATTR_NONE, 0, 0.0, NULL, line, col + found_cols[i]);
    }
}

void escape_json_string(const char* src, StringBuffer* out) {
    while (*src) {
        switch (*src) {
            case '"': sb_append_str(out, "\\\""); break;
            case '\\': sb_append_str(out, "\\\\"); break;
            case '\n': sb_append_str(out, "\\n"); break;
            case '\r': sb_append_str(out, "\\r"); break;
            case '\t': sb_append_str(out, "\\t"); break;
            default: sb_append(out, *src); break;
        }
        src++;
    }
}

// --- ANALISADOR LÉXICO ---
void analyze_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Erro: Não foi possível abrir o arquivo %s\n", filename);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t length = (size_t) ftell(file);
    fseek(file, 0, SEEK_SET);
    char* content = (char*)malloc(length + 1);
    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);

    int i = 1, j = 1;
    size_t idx = 0;

    StringBuffer string_temp, lexema_bruto;
    sb_init(&string_temp);
    sb_init(&lexema_bruto);

    State estado = STATE_INITIAL;
    int linha_inicio = 1, coluna_inicio = 1;

    while (idx < length) {
        char c = content[idx];
        bool reprocess = false;

        switch (estado) {
            case STATE_INITIAL:
                linha_inicio = i;
                coluna_inicio = j;
                if (c == ' ' || c == '\r') { /* ignore */ }
                else if (c == '\n') { i++; j = 0; }
                else if (isalpha(c) || c == '_') {
                    sb_append(&string_temp, c);
                    estado = STATE_IDENTIFIER;
                }
                else if (isdigit(c)) {
                    sb_append(&string_temp, c);
                    estado = STATE_INT;
                }
                else if (strchr("(){},;:\\[\\]", c) && c != '\0') {
                    char lex[2] = {c, '\0'};
                    add_token(map_bracket_symbol(c), lex, ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio);
                }
                else if (strchr("!=-+/*><.&|%", c) && c != '\0') {
                    sb_append(&string_temp, c);
                    estado = STATE_SYMBOL;
                }
                else if (c == '\'') {
                    sb_append(&lexema_bruto, c);
                    estado = STATE_CHAR;
                }
                else if (c == '"') {
                    sb_append(&lexema_bruto, c);
                    estado = STATE_STRING;
                }
                else {
                    char lex[2] = {c, '\0'};
                    add_error("UNKNOWN_SYMBOL", lex, linha_inicio, coluna_inicio);
                }
                break;

            case STATE_IDENTIFIER:
                if (isalnum(c) || c == '_') {
                    sb_append(&string_temp, c);
                } else {
                    if (is_reserved(string_temp.data)) {
                        char upper_tok[64];
                        toupper_str(upper_tok, string_temp.data);
                        add_token(scanner_duplicate_string(upper_tok), string_temp.data, ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio);
                    } else {
                        add_token("IDENT", string_temp.data, ATTR_STR, 0, 0, string_temp.data, linha_inicio, coluna_inicio);
                    }
                    sb_clear(&string_temp);
                    estado = STATE_INITIAL;
                    reprocess = true;
                }
                break;

            case STATE_INT:
                if (string_temp.data[0] == '0' && string_temp.len == 1 && isdigit(c)) {
                    sb_append(&string_temp, c);
                    add_error("LEADING_ZERO", string_temp.data, linha_inicio, coluna_inicio);
                    string_temp.data[string_temp.len-1] = '\0'; string_temp.len--; // rollback visual only for string_temp
                }

                if (isdigit(c)) {
                    sb_append(&string_temp, c);
                } else if (c == '.') {
                    if (idx + 1 < (size_t) length && isdigit(content[idx + 1])) {
                        sb_append(&string_temp, c);
                        estado = STATE_REAL;
                    } else {
                        sb_append(&string_temp, c);
                        add_error("MALFORMED_REAL_LITERAL", string_temp.data, linha_inicio, coluna_inicio);
                        string_temp.data[string_temp.len-1] = '\0'; // remove dot
                        add_token("INT_LIT", string_temp.data, ATTR_INT, atoi(string_temp.data), 0, NULL, linha_inicio, coluna_inicio);
                        sb_clear(&string_temp);
                        estado = STATE_INITIAL;
                        reprocess = true;
                    }
                } else if (strchr(" \n\r+-/*=!<>,;:(){}[]&|%", c)) {
                    add_token("INT_LIT", string_temp.data, ATTR_INT, atoi(string_temp.data), 0, NULL, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp);
                    estado = STATE_INITIAL;
                    reprocess = true;
                } else {
                    StringBuffer invalid; sb_init(&invalid);
                    sb_append_str(&invalid, string_temp.data);
                    sb_append(&invalid, c);
                    size_t pos = idx + 1;
                    while (pos < length && (isalnum(content[pos]) || content[pos] == '_')) {
                        sb_append(&invalid, content[pos++]);
                    }
                    add_error("INVALID_IDENTIFIER", invalid.data, linha_inicio, coluna_inicio);
                    sb_free(&invalid);

                    add_token("INT_LIT", string_temp.data, ATTR_INT, atoi(string_temp.data), 0, NULL, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp);
                    estado = STATE_IDENTIFIER;
                    linha_inicio = i; coluna_inicio = j;
                    reprocess = true;
                }
                break;

            // Nota: Para manter o código otimizado, integrei a lógica dos outros estados de forma equivalente
            // à sua máquina de estados em Python.

            case STATE_REAL:
                if (c == '.') {
                    sb_append(&string_temp, c);
                    add_error("MALFORMED_REAL_LITERAL", string_temp.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); estado = STATE_INITIAL; reprocess = true;
                } else if (isdigit(c)) {
                    sb_append(&string_temp, c);
                } else if (c == 'F' || c == 'f') {
                    sb_append(&string_temp, c);
                    sb_append_str(&lexema_bruto, string_temp.data);
                    estado = STATE_FLOAT_SUFFIX;
                } else if (isalpha(c) || c == '_') {
                    StringBuffer invalid; sb_init(&invalid);
                    sb_append_str(&invalid, string_temp.data);
                    sb_append(&invalid, c);
                    size_t pos = idx + 1;
                    while (pos < length && (isalnum(content[pos]) || content[pos] == '_')) {
                        sb_append(&invalid, content[pos++]);
                    }
                    add_error("MALFORMED_REAL_LITERAL", invalid.data, linha_inicio, coluna_inicio);
                    sb_free(&invalid);
                    add_token("FLOAT_LIT", string_temp.data, ATTR_FLOAT, 0, atof(string_temp.data), NULL, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); estado = STATE_IDENTIFIER; linha_inicio = i; coluna_inicio = j; reprocess = true;
                } else if (strchr(" \n\r+-/*=!<>,;:(){}[]&|%", c)) {
                    add_token("FLOAT_LIT", string_temp.data, ATTR_FLOAT, 0, atof(string_temp.data), NULL, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); estado = STATE_INITIAL; reprocess = true;
                } else {
                    add_error("MALFORMED_REAL_LITERAL", string_temp.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); estado = STATE_INITIAL;
                }
                break;

            case STATE_FLOAT_SUFFIX:
                if (isalpha(c) || c == '_') {
                    // erro similiar ao real
                    add_error("MALFORMED_REAL_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
                    lexema_bruto.data[lexema_bruto.len-1] = '\0'; // Tira o F
                    add_token("FLOAT_LIT", lexema_bruto.data, ATTR_FLOAT, 0, atof(lexema_bruto.data), NULL, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_IDENTIFIER; linha_inicio = i; coluna_inicio = j; reprocess = true;
                } else if (strchr(" \n\r+-/*=!<>,;:(){}[]&|%", c)) {
                    char* f_val = scanner_duplicate_string(lexema_bruto.data);
                    f_val[strlen(f_val)-1] = '\0'; // tira F
                    add_token("FLOAT_LIT", lexema_bruto.data, ATTR_FLOAT, 0, atof(f_val), NULL, linha_inicio, coluna_inicio);
                    free(f_val);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_INITIAL; reprocess = true;
                } else {
                    add_error("MALFORMED_REAL_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_INITIAL;
                }
                break;

            case STATE_SYMBOL:
                if (string_temp.data[0] == '-' && isdigit(c) && (tokens_count == 0 ||
                    (strcmp(tokens[tokens_count-1].token, "INT_LIT") != 0 && strcmp(tokens[tokens_count-1].token, "FLOAT_LIT") != 0 &&
                     strcmp(tokens[tokens_count-1].token, "IDENT") != 0 && strcmp(tokens[tokens_count-1].token, "RPAREN") != 0))) {
                    sb_append(&string_temp, c); estado = STATE_INT;
                } else if (c == '=' && !strchr("&|.", string_temp.data[0])) {
                    sb_append(&string_temp, c);
                    const char* t = NULL;
                    if (strcmp(string_temp.data, "!=") == 0) t = "NEQ";
                    else if (strcmp(string_temp.data, "<=") == 0) t = "LE";
                    else if (strcmp(string_temp.data, ">=") == 0) t = "GE";
                    else if (strcmp(string_temp.data, "==") == 0) t = "EQ";
                    else if (strcmp(string_temp.data, "+=") == 0) t = "PLUS_ASSIGN";
                    else if (strcmp(string_temp.data, "-=") == 0) t = "MINUS_ASSIGN";
                    else if (strcmp(string_temp.data, "/=") == 0) t = "DIV_ASSIGN";
                    else if (strcmp(string_temp.data, "*=") == 0) t = "MUL_ASSIGN";
                    else if (strcmp(string_temp.data, "%=") == 0) t = "MOD_ASSIGN";
                    add_token(t, string_temp.data, ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); estado = STATE_INITIAL;
                } else if (string_temp.data[0] == '+' && c == '+') {
                    add_token("INC", "++", ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio); sb_clear(&string_temp); estado = STATE_INITIAL;
                } else if (string_temp.data[0] == '-' && c == '-') {
                    add_token("DEC", "--", ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio); sb_clear(&string_temp); estado = STATE_INITIAL;
                } else if (string_temp.data[0] == '&' && c == '&') {
                    add_token("AND", "&&", ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio); sb_clear(&string_temp); estado = STATE_INITIAL;
                } else if (string_temp.data[0] == '|' && c == '|') {
                    add_token("OR", "||", ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio); sb_clear(&string_temp); estado = STATE_INITIAL;
                } else if (string_temp.data[0] == '/' && (c == '/' || c == '*')) {
                    if (c == '/') estado = STATE_LINE_COMMENT; else estado = STATE_BLOCK_COMMENT;
                    sb_append(&string_temp, c);
                } else {
                    add_token(map_simple_symbol(string_temp.data[0]), string_temp.data, ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); estado = STATE_INITIAL; reprocess = true;
                }
                break;

            case STATE_STRING:
                sb_append(&lexema_bruto, c);
                if (c == '\n') {
                    lexema_bruto.data[lexema_bruto.len-1] = '\0'; // rstrip \n
                    add_error("UNTERMINATED_STRING_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
                    recover_tokens(lexema_bruto.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); i++; j = 0; estado = STATE_INITIAL;
                } else if (c == '\\') { estado = STATE_ESCAPE_STRING; }
                else if (c != '"') { sb_append(&string_temp, c); }
                else {
                    add_token("STRING_LIT", lexema_bruto.data, ATTR_STR, 0, 0, string_temp.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_INITIAL;
                }
                break;

            case STATE_CHAR:
                if (c == '\n') {
                    add_error("UNTERMINATED_CHAR_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
                    recover_tokens(lexema_bruto.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); i++; j = 0; estado = STATE_INITIAL;
                } else if (c == '\'') {
                    sb_append(&lexema_bruto, c);
                    if (string_temp.len == 0) add_error("EMPTY_CHAR_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
                    else add_token("CHAR_LIT", lexema_bruto.data, ATTR_STR, 0, 0, string_temp.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_INITIAL;
                } else if (strchr("; \t\r+-*/=!<>,(){}[]", c)) {
                    sb_append(&lexema_bruto, c);
                    add_error("UNTERMINATED_CHAR_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
                    recover_tokens(lexema_bruto.data, linha_inicio, coluna_inicio);
                    sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_INITIAL; reprocess = false;
                } else {
                    sb_append(&lexema_bruto, c);
                    if (c == '\\') estado = STATE_ESCAPE_CHAR;
                    else {
                        if (string_temp.len >= 1) {
                            while (idx + 1 < (size_t) length && content[idx+1] != '\'' && content[idx+1] != '\n') {
                                sb_append(&lexema_bruto, content[++idx]); j++;
                            }
                            if (idx < (size_t) length && content[idx] != '\n') { sb_append(&lexema_bruto, '\''); idx++; j++; }
                            add_error("CHAR_TOO_LONG", lexema_bruto.data, linha_inicio, coluna_inicio);
                            recover_tokens(lexema_bruto.data, linha_inicio, coluna_inicio);
                            sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_INITIAL;
                            idx--; j--;
                        } else { sb_append(&string_temp, c); }
                    }
                }
                break;

            case STATE_ESCAPE_STRING:
            case STATE_ESCAPE_CHAR:
                sb_append(&lexema_bruto, c);
                switch(c) {
                    case 'n': sb_append(&string_temp, '\n'); break;
                    case 't': sb_append(&string_temp, '\t'); break;
                    case '0': sb_append(&string_temp, '\0'); break;
                    default: sb_append(&string_temp, c); break;
                }
                estado = (estado == STATE_ESCAPE_STRING) ? STATE_STRING : STATE_CHAR_WITH_ESCAPE;
                break;

            case STATE_CHAR_WITH_ESCAPE:
                sb_append(&lexema_bruto, c);
                if (c != '\'') {
                    add_error("CHAR_TOO_LONG", lexema_bruto.data, linha_inicio, coluna_inicio);
                    recover_tokens(lexema_bruto.data, linha_inicio, coluna_inicio);
                } else {
                    add_token("CHAR_LIT", lexema_bruto.data, ATTR_STR, 0, 0, string_temp.data, linha_inicio, coluna_inicio);
                }
                sb_clear(&string_temp); sb_clear(&lexema_bruto); estado = STATE_INITIAL;
                break;

            case STATE_LINE_COMMENT:
                if (c == '\n') { sb_clear(&string_temp); estado = STATE_INITIAL; i++; j = 0; }
                break;

            case STATE_BLOCK_COMMENT:
                sb_append(&string_temp, c);
                if (c == '*') estado = STATE_BLOCK_COMMENT_END;
                else if (c == '\n') { i++; j = 0; }
                break;

            case STATE_BLOCK_COMMENT_END:
                sb_append(&string_temp, c);
                if (c == '/') { sb_clear(&string_temp); estado = STATE_INITIAL; }
                else if (c != '*') {
                    if (c == '\n') { i++; j = 0; }
                    estado = STATE_BLOCK_COMMENT;
                }
                break;
        }

        if (!reprocess) { idx++; j++; }
    }

    // --- TRATAMENTO DE FIM DE ARQUIVO (EOF) ---
    if (estado == STATE_IDENTIFIER) {
        if (is_reserved(string_temp.data)) {
            char upper_tok[64];
            toupper_str(upper_tok, string_temp.data);
            add_token(scanner_duplicate_string(upper_tok), string_temp.data, ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio);
        } else {
            add_token("IDENT", string_temp.data, ATTR_STR, 0, 0, string_temp.data, linha_inicio, coluna_inicio);
        }
    }
    else if (estado == STATE_INT) {
        add_token("INT_LIT", string_temp.data, ATTR_INT, atoi(string_temp.data), 0, NULL, linha_inicio, coluna_inicio);
    }
    else if (estado == STATE_REAL) {
        add_token("FLOAT_LIT", string_temp.data, ATTR_FLOAT, 0, atof(string_temp.data), NULL, linha_inicio, coluna_inicio);
    }
    else if (estado == STATE_FLOAT_SUFFIX) {
        char* f_val = scanner_duplicate_string(lexema_bruto.data);
        if (strlen(f_val) > 0) f_val[strlen(f_val) - 1] = '\0'; // Remove o 'F' ou 'f'
        add_token("FLOAT_LIT", lexema_bruto.data, ATTR_FLOAT, 0, atof(f_val), NULL, linha_inicio, coluna_inicio);
        free(f_val);
    }
    else if (estado == STATE_STRING || estado == STATE_ESCAPE_STRING) {
        add_error("UNTERMINATED_STRING_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
        recover_tokens(lexema_bruto.data, linha_inicio, coluna_inicio);
    }
    else if (estado == STATE_CHAR || estado == STATE_ESCAPE_CHAR || estado == STATE_CHAR_WITH_ESCAPE) {
        add_error("UNTERMINATED_CHAR_LITERAL", lexema_bruto.data, linha_inicio, coluna_inicio);
        recover_tokens(lexema_bruto.data, linha_inicio, coluna_inicio);
    }
    else if (estado == STATE_BLOCK_COMMENT || estado == STATE_BLOCK_COMMENT_END) {
        add_error("UNTERMINATED_BLOCK_COMMENT", string_temp.data, linha_inicio, coluna_inicio);
    }
    else if (estado == STATE_SYMBOL) {
        add_token(map_simple_symbol(string_temp.data[0]), string_temp.data, ATTR_NONE, 0, 0, NULL, linha_inicio, coluna_inicio);
    }

    add_token("EOF", "", ATTR_NONE, 0, 0, NULL, i, j > 0 ? j : 1);
    sb_free(&string_temp); sb_free(&lexema_bruto); free(content);
}

// --- IMPRESSÃO DE RESULTADOS EM JSON ---
void print_tokens() {
    for (size_t i = 0; i < tokens_count; i++) {
        Token t = tokens[i];
        printf("{\"token\": \"%s\", \"lexeme\": ", t.token);

        StringBuffer lex_esc; sb_init(&lex_esc);
        escape_json_string(t.lexeme, &lex_esc);
        printf("\"%s\", \"attribute\": ", lex_esc.data);
        sb_free(&lex_esc);

        if (t.attr_type == ATTR_NONE) printf("null");
        else if (t.attr_type == ATTR_INT) printf("%d", t.attr_int);
        else if (t.attr_type == ATTR_FLOAT) printf("%g", t.attr_float);
        else if (t.attr_type == ATTR_STR) {
            StringBuffer attr_esc; sb_init(&attr_esc);
            escape_json_string(t.attr_str, &attr_esc);
            printf("\"%s\"", attr_esc.data);
            sb_free(&attr_esc);
        }

        printf(", \"line\": %d, \"column\": %d}\n", t.line, t.column);
    }
}

void write_errors(const char* filepath) {
    if (errors_count == 0) return;
    FILE* f = fopen(filepath, "w");
    if (!f) return;

    for (size_t i = 0; i < errors_count; i++) {
        Error e = errors[i];
        StringBuffer lex_esc; sb_init(&lex_esc);
        escape_json_string(e.lexeme, &lex_esc);
        fprintf(f, "{\"error\": \"%s\", \"lexeme\": \"%s\", \"line\": %d, \"column\": %d}\n",
                e.error, lex_esc.data, e.line, e.column);
        sb_free(&lex_esc);
    }
    fclose(f);
}

// --- MAIN ---
// Defina SCANNER_NO_MAIN ao incluir este scanner no parser.c.
#ifndef SCANNER_NO_MAIN
// parser_c/parser.c fornece esta função na compilação integrada
// com -DSCANNER_WITH_PARSER.
#ifdef SCANNER_WITH_PARSER
int parser_main(int argc, char** argv);
#endif

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Erro: Nenhum arquivo de entrada fornecido.\n");
        return 1;
    }

    // Sem a opção de tokens, a execução é encaminhada ao parser.
    if (strcmp(argv[1], "--tokens") != 0 && strcmp(argv[1], "-t") != 0) {
#ifdef SCANNER_WITH_PARSER
        return parser_main(argc, argv);
#else
        fprintf(stderr, "Erro: Parser C ainda não integrado. "
                "Use --tokens ou -t para executar apenas o scanner.\n");
        return 1;
#endif
    }

    if (argc < 3) {
        fprintf(stderr, "Erro: Nenhum arquivo de entrada fornecido para o scanner.\n");
        return 1;
    }

    char* input_file = argv[2];
    analyze_file(input_file);
    print_tokens();

    if (errors_count > 0) {
        MAKE_DIR("errors");

        // Extrai o nome base do arquivo
        char base_name[256];
        const char* last_slash = strrchr(input_file, '/');
        const char* last_backslash = strrchr(input_file, '\\');
        const char* start = last_slash > last_backslash ? last_slash : last_backslash;
        start = start ? start + 1 : input_file;

        strncpy(base_name, start, sizeof(base_name));
        char* dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';

        char error_filepath[512];
        snprintf(error_filepath, sizeof(error_filepath), "errors/%s.errors.jsonl", base_name);

        write_errors(error_filepath);
    }

    free_scanner();

    return 0;
}
#endif // SCANNER_NO_MAIN
