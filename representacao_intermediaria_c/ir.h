#ifndef MINIC_IR_H
#define MINIC_IR_H

#include <math.h>
#include <limits.h>
#include <errno.h>

typedef struct {
    const char* op;
    const char* args[4];
    int count;
    int removed;
} IRInstruction;

typedef struct IRBinding IRBinding;
struct IRBinding {
    const char* source;
    const char* name;
    const char* type;
    const char* known;
    int depth;
    IRBinding* next;
};

typedef struct IRLoop IRLoop;
struct IRLoop {
    const char* end;
    const char* step;
    IRLoop* parent;
};

typedef struct {
    const char* text;
    const char* type;
} IRValue;

typedef struct {
    IRInstruction* code;
    size_t count;
    size_t capacity;
    char** strings;
    size_t string_count;
    size_t string_capacity;
    IRBinding* bindings;
    IRLoop* loop;
    const NoAST* tree;
    const char* return_type;
    int optimize;
    int counter;
    int labels;
    int depth;
} IRGenerator;

static void* ir_resize(void* pointer, size_t size) {
    void* result = realloc(pointer, size);
    if (!result) {
        fprintf(stderr, "Erro de memória na geração de IR.\n");
        exit(1);
    }
    return result;
}

static const char* ir_format(IRGenerator* g, const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        fprintf(stderr, "Erro ao formatar IR.\n");
        exit(1);
    }
    char* text = ir_resize(NULL, (size_t)length + 1);
    vsnprintf(text, (size_t)length + 1, format, args);
    va_end(args);
    if (g->string_count == g->string_capacity) {
        g->string_capacity = g->string_capacity ? g->string_capacity * 2 : 64;
        g->strings = ir_resize(g->strings, g->string_capacity * sizeof(*g->strings));
    }
    g->strings[g->string_count++] = text;
    return text;
}

static int ir_equal(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
}

static void ir_emit(IRGenerator* g, const char* op, int count, ...) {
    if (g->count == g->capacity) {
        g->capacity = g->capacity ? g->capacity * 2 : 128;
        g->code = ir_resize(g->code, g->capacity * sizeof(*g->code));
    }
    IRInstruction* instruction = &g->code[g->count++];
    memset(instruction, 0, sizeof(*instruction));
    instruction->op = op;
    instruction->count = count;
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        instruction->args[i] = va_arg(args, const char*);
    }
    va_end(args);
}

static const char* ir_temp(IRGenerator* g) {
    return ir_format(g, "%%t%d", ++g->counter);
}

static const char* ir_label(IRGenerator* g) {
    return ir_format(g, "L%d", ++g->labels);
}

static IRBinding* ir_lookup(IRGenerator* g, const char* name) {
    for (IRBinding* b = g->bindings; b; b = b->next) {
        if (b->depth >= 0 && ir_equal(b->source, name)) {
            return b;
        }
    }
    return NULL;
}

static IRBinding* ir_bind(IRGenerator* g, const NoAST* node) {
    IRBinding* b = ir_resize(NULL, sizeof(*b));
    b->source = node->texto;
    b->name = ir_format(g, "%s%d_%s", g->depth ? "%v" : "@g", ++g->counter, node->texto);
    int array = node->vetor || (node->no == NO_DECLARACAO && node->filhos[0]);
    b->type = array ? ir_format(g, "%s[]", node->tipo) : node->tipo;
    b->known = NULL;
    b->depth = g->depth;
    b->next = g->bindings;
    g->bindings = b;
    return b;
}

static void ir_leave(IRGenerator* g) {
    for (IRBinding* b = g->bindings; b; b = b->next) {
        if (b->depth == g->depth) {
            b->depth = -1;
        }
    }
    g->depth--;
}

static void ir_forget(IRGenerator* g, int globals_only) {
    for (IRBinding* b = g->bindings; b; b = b->next) {
        if (!globals_only || b->name[0] == '@') {
            b->known = NULL;
        }
    }
}

static void ir_invalidate(IRGenerator* g, const NoAST* node) {
    if (!node) {
        return;
    }
    if (node->no == NO_CHAMADA) {
        ir_forget(g, 1);
    }
    if (node->no == NO_ATRIBUICAO && node->filhos[0]->no == NO_IDENTIFICADOR) {
        IRBinding* b = ir_lookup(g, node->filhos[0]->texto);
        if (b) {
            b->known = NULL;
        }
    }
    for (int i = 0; i < 4; i++) {
        ir_invalidate(g, node->filhos[i]);
    }
    for (size_t i = 0; i < node->quantidade; i++) {
        ir_invalidate(g, node->itens[i]);
    }
}

static int ir_number(const char* text, double* value) {
    if (ir_equal(text, "true") || ir_equal(text, "false")) {
        *value = ir_equal(text, "true");
        return 1;
    }
    if (!text || !*text) {
        return 0;
    }
    char* end;
    errno = 0;
    *value = strtod(text, &end);
    return *end == '\0' && end != text && errno != ERANGE;
}

static const char* ir_numeric_text(IRGenerator* g, double value, const char* type) {
    if (ir_equal(type, "bool")) {
        return value ? "true" : "false";
    }
    if (ir_equal(type, "float")) {
        const char* text = ir_format(g, "%.17g", value);
        return strpbrk(text, ".eE") ? text : ir_format(g, "%s.0", text);
    }
    return ir_format(g, "%.0f", value == 0 ? 0.0 : value);
}

static IRValue ir_value(const char* text, const char* type) {
    IRValue value = {text, type};
    return value;
}

static IRValue ir_cast(IRGenerator* g, IRValue value, const char* type) {
    if (ir_equal(type, "float") && ir_equal(value.type, "int")) {
        double number;
        if (g->optimize && ir_number(value.text, &number)) {
            return ir_value(ir_numeric_text(g, number, "float"), "float");
        }
        const char* dest = ir_temp(g);
        ir_emit(g, "i2f", 2, dest, value.text);
        return ir_value(dest, "float");
    }
    return value;
}

static void ir_save(IRGenerator* g, IRBinding* binding, const char* text) {
    double value;
    ir_emit(g, "mov", 2, binding->name, text);
    binding->known = g->optimize && ir_number(text, &value) ? text : NULL;
}

static const NoAST* ir_function(IRGenerator* g, const char* name) {
    for (size_t i = 0; i < g->tree->quantidade; i++) {
        const NoAST* node = g->tree->itens[i];
        if (node->no == NO_FUNCAO && ir_equal(node->texto, name)) {
            return node;
        }
    }
    return NULL;
}

static const char* ir_binary_op(const char* op) {
    static const char* symbols[] = {"+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!="};
    static const char* names[] = {"add", "sub", "mul", "div", "mod", "lt", "gt", "le", "ge", "eq", "ne"};
    for (size_t i = 0; i < sizeof(symbols) / sizeof(*symbols); i++) {
        if (ir_equal(op, symbols[i])) {
            return names[i];
        }
    }
    return NULL;
}

static int ir_comparison(const char* op) {
    return ir_equal(op, "lt") || ir_equal(op, "gt") || ir_equal(op, "le") ||
           ir_equal(op, "ge") || ir_equal(op, "eq") || ir_equal(op, "ne");
}

static const char* ir_fold(IRGenerator* g, const char* op, double x, double y, const char* type) {
    double result = 0;
    if ((ir_equal(op, "div") || ir_equal(op, "mod")) &&
        (y == 0 || (x == INT_MIN && y == -1))) {
        return NULL;
    }
    if (ir_equal(op, "add")) {
        result = x + y;
    } else if (ir_equal(op, "sub")) {
        result = x - y;
    } else if (ir_equal(op, "mul")) {
        result = x * y;
    } else if (ir_equal(op, "div")) {
        result = x / y;
        if (!ir_equal(type, "float")) {
            result = (double)(long long)result;
        }
    } else if (ir_equal(op, "mod")) {
        result = x - (double)(long long)(x / y) * y;
    } else if (ir_equal(op, "lt")) {
        result = x < y;
    } else if (ir_equal(op, "gt")) {
        result = x > y;
    } else if (ir_equal(op, "le")) {
        result = x <= y;
    } else if (ir_equal(op, "ge")) {
        result = x >= y;
    } else if (ir_equal(op, "eq")) {
        result = x == y;
    } else if (ir_equal(op, "ne")) {
        result = x != y;
    }
    if (!isfinite(result) || (ir_equal(type, "int") && (result < INT_MIN || result > INT_MAX))) {
        return NULL;
    }
    return ir_numeric_text(g, result, type);
}

static IRValue ir_expr(IRGenerator* g, const NoAST* node);
static int ir_statement(IRGenerator* g, const NoAST* node);

static IRValue ir_address(IRGenerator* g, const NoAST* node, const char** index) {
    IRValue base = ir_expr(g, node->filhos[0]);
    *index = ir_expr(g, node->filhos[1]).text;
    size_t length = strlen(base.type);
    return ir_value(base.text, ir_format(g, "%.*s", (int)length - 2, base.type));
}

static IRValue ir_logical(IRGenerator* g, const NoAST* node) {
    IRValue left = ir_expr(g, node->filhos[0]);
    int is_and = ir_equal(node->texto, "&&");
    double known;
    if (g->optimize && ir_number(left.text, &known)) {
        if ((known != 0) != is_and) {
            return ir_value(is_and ? "false" : "true", "bool");
        }
        return ir_expr(g, node->filhos[1]);
    }
    const char* dest = ir_temp(g);
    const char* end = ir_label(g);
    ir_emit(g, "mov", 2, dest, is_and ? "false" : "true");
    ir_emit(g, is_and ? "brfalse" : "brtrue", 2, left.text, end);
    ir_forget(g, 0);
    IRValue right = ir_expr(g, node->filhos[1]);
    ir_emit(g, "mov", 2, dest, right.text);
    ir_emit(g, "label", 1, end);
    ir_forget(g, 0);
    return ir_value(dest, "bool");
}

static IRValue ir_expr(IRGenerator* g, const NoAST* node) {
    switch (node->no) {
        case NO_LITERAL: {
            const char* type = ir_equal(node->tipo, "real") ? "float" : node->tipo;
            const char* text = node->texto;
            if (ir_equal(type, "float")) {
                text = ir_numeric_text(g, strtod(text, NULL), type);
            }
            return ir_value(text, type);
        }
        case NO_IDENTIFICADOR: {
            IRBinding* b = ir_lookup(g, node->texto);
            if (eh_vetor(b->type)) {
                return ir_value(b->name, b->type);
            }
            if (g->optimize && b->known) {
                return ir_value(b->known, b->type);
            }
            const char* dest = ir_temp(g);
            ir_emit(g, "mov", 2, dest, b->name);
            return ir_value(dest, b->type);
        }
        case NO_INDICE: {
            const char* index;
            IRValue base = ir_address(g, node, &index);
            const char* dest = ir_temp(g);
            ir_emit(g, "load", 3, dest, base.text, index);
            return ir_value(dest, base.type);
        }
        case NO_ATRIBUICAO: {
            const NoAST* target = node->filhos[0];
            if (target->no == NO_INDICE) {
                const char* index;
                IRValue base = ir_address(g, target, &index);
                IRValue value = ir_cast(g, ir_expr(g, node->filhos[1]), base.type);
                ir_emit(g, "store", 3, base.text, index, value.text);
                return value;
            }
            IRBinding* b = ir_lookup(g, target->texto);
            IRValue value = ir_cast(g, ir_expr(g, node->filhos[1]), b->type);
            ir_save(g, b, value.text);
            return value;
        }
        case NO_CHAMADA: {
            const NoAST* function = ir_function(g, node->filhos[0]->texto);
            IRValue* values = ir_resize(NULL, (node->quantidade + 1) * sizeof(*values));
            for (size_t i = 0; i < node->quantidade; i++) {
                values[i] = ir_cast(g, ir_expr(g, node->itens[i]), function->itens[i]->tipo);
            }
            for (size_t i = 0; i < node->quantidade; i++) {
                ir_emit(g, "param", 1, values[i].text);
            }
            free(values);
            const char* dest = ir_temp(g);
            ir_emit(g, "call", 3, dest, function->texto, ir_format(g, "%zu", node->quantidade));
            ir_forget(g, 1);
            return ir_value(dest, function->tipo);
        }
        case NO_UNARIA: {
            IRValue value = ir_expr(g, node->filhos[0]);
            const char* op = ir_equal(node->texto, "!") ? "not" : "neg";
            double number;
            if (g->optimize && ir_number(value.text, &number)) {
                double result = ir_equal(op, "not") ? !number : -number;
                if (ir_equal(value.type, "float") || (result >= INT_MIN && result <= INT_MAX)) {
                    return ir_value(ir_numeric_text(g, result, value.type), value.type);
                }
            }
            const char* dest = ir_temp(g);
            ir_emit(g, op, 2, dest, value.text);
            return ir_value(dest, value.type);
        }
        case NO_BINARIA: {
            if (ir_equal(node->texto, "&&") || ir_equal(node->texto, "||")) {
                return ir_logical(g, node);
            }
            IRValue left = ir_expr(g, node->filhos[0]);
            IRValue right = ir_expr(g, node->filhos[1]);
            const char* common = ir_equal(left.type, "float") || ir_equal(right.type, "float") ? "float" : left.type;
            left = ir_cast(g, left, common);
            right = ir_cast(g, right, common);
            const char* op = ir_binary_op(node->texto);
            const char* type = ir_comparison(op) ? "bool" : common;
            double x = 0;
            double y = 0;
            int known_x = ir_number(left.text, &x);
            int known_y = ir_number(right.text, &y);
            if (g->optimize && known_x && known_y) {
                const char* folded = ir_fold(g, op, x, y, type);
                if (folded) {
                    return ir_value(folded, type);
                }
            }
            if (g->optimize && ir_equal(type, "int")) {
                if (known_y && (((ir_equal(op, "add") || ir_equal(op, "sub")) && y == 0) ||
                                ((ir_equal(op, "mul") || ir_equal(op, "div")) && y == 1))) {
                    return ir_value(left.text, type);
                }
                if (known_x && ((ir_equal(op, "add") && x == 0) || (ir_equal(op, "mul") && x == 1))) {
                    return ir_value(right.text, type);
                }
            }
            const char* dest = ir_temp(g);
            ir_emit(g, op, 3, dest, left.text, right.text);
            return ir_value(dest, type);
        }
        default:
            fprintf(stderr, "Expressão sem suporte na IR.\n");
            exit(1);
    }
}

static int ir_block(IRGenerator* g, const NoAST* block) {
    int stopped = 0;
    for (size_t i = 0; i < block->quantidade; i++) {
        if (g->optimize && stopped) {
            break;
        }
        int current = ir_statement(g, block->itens[i]);
        stopped = current || stopped;
    }
    return stopped;
}

static int ir_statement(IRGenerator* g, const NoAST* node) {
    if (!node) {
        return 0;
    }
    switch (node->no) {
        case NO_BLOCO: {
            g->depth++;
            int stopped = ir_block(g, node);
            ir_leave(g);
            return stopped;
        }
        case NO_DECLARACAO: {
            IRBinding* b = ir_bind(g, node);
            ir_emit(g, "alloc", 3, b->name, b->type, node->filhos[0] ? node->filhos[0]->texto : "1");
            if (node->filhos[1]) {
                IRValue value = ir_cast(g, ir_expr(g, node->filhos[1]), b->type);
                ir_save(g, b, value.text);
            }
            break;
        }
        case NO_COMANDO_EXPRESSAO:
            ir_expr(g, node->filhos[0]);
            break;
        case NO_IMPRIMIR:
            for (size_t i = 0; i < node->quantidade; i++) {
                IRValue value = ir_expr(g, node->itens[i]);
                ir_emit(g, "print", 1, value.text);
            }
            break;
        case NO_LER:
            ir_emit(g, "read", 0);
            break;
        case NO_RETORNO:
            if (node->filhos[0]) {
                IRValue value = ir_cast(g, ir_expr(g, node->filhos[0]), g->return_type);
                ir_emit(g, "ret", 1, value.text);
            } else {
                ir_emit(g, "ret", 0);
            }
            return 1;
        case NO_BREAK:
        case NO_CONTINUE:
            ir_emit(g, "jmp", 1, node->no == NO_BREAK ? g->loop->end : g->loop->step);
            return 1;
        case NO_SE: {
            IRValue cond = ir_expr(g, node->filhos[0]);
            double known;
            if (g->optimize && ir_number(cond.text, &known)) {
                return ir_statement(g, known ? node->filhos[1] : node->filhos[2]);
            }
            const char* other = ir_label(g);
            const char* end = ir_label(g);
            ir_emit(g, "brfalse", 2, cond.text, other);
            ir_forget(g, 0);
            int first = ir_statement(g, node->filhos[1]);
            ir_emit(g, "jmp", 1, end);
            ir_emit(g, "label", 1, other);
            ir_forget(g, 0);
            int second = ir_statement(g, node->filhos[2]);
            ir_emit(g, "label", 1, end);
            ir_forget(g, 0);
            return first && second;
        }
        case NO_ENQUANTO:
        case NO_PARA: {
            int is_for = node->no == NO_PARA;
            if (is_for && node->filhos[0]) {
                ir_expr(g, node->filhos[0]);
            }
            ir_invalidate(g, node);
            const char* test = ir_label(g);
            const char* step = ir_label(g);
            const char* end = ir_label(g);
            ir_emit(g, "label", 1, test);
            const NoAST* condition = node->filhos[is_for ? 1 : 0];
            if (condition) {
                IRValue value = ir_expr(g, condition);
                ir_emit(g, "brfalse", 2, value.text, end);
            }
            IRLoop loop = {end, step, g->loop};
            g->loop = &loop;
            ir_statement(g, node->filhos[is_for ? 3 : 1]);
            g->loop = loop.parent;
            ir_emit(g, "label", 1, step);
            ir_invalidate(g, node);
            if (is_for && node->filhos[2]) {
                ir_expr(g, node->filhos[2]);
            }
            ir_emit(g, "jmp", 1, test);
            ir_emit(g, "label", 1, end);
            ir_invalidate(g, node);
            break;
        }
        default:
            break;
    }
    return 0;
}

static int ir_pure(const char* op) {
    return ir_equal(op, "mov") || ir_equal(op, "neg") || ir_equal(op, "not") ||
           ir_equal(op, "i2f") || ir_equal(op, "add") || ir_equal(op, "sub") ||
           ir_equal(op, "mul") || ir_equal(op, "div") || ir_equal(op, "mod") || ir_comparison(op);
}

static int ir_used(IRGenerator* g, const char* name) {
    for (size_t i = 0; i < g->count; i++) {
        IRInstruction* instruction = &g->code[i];
        if (instruction->removed) {
            continue;
        }
        const char* op = instruction->op;
        int first = ir_pure(op) || ir_equal(op, "load") || ir_equal(op, "call") ||
                    ir_equal(op, "alloc") || ir_equal(op, "arg");
        for (int a = first; a < instruction->count; a++) {
            if (ir_equal(instruction->args[a], name)) {
                return 1;
            }
        }
    }
    return 0;
}

static void ir_cleanup(IRGenerator* g) {
    int changed = 1;
    while (changed) {
        changed = 0;
        int reachable = 1;
        /* Mark in a separate pass so removal does not depend on traversal order. */
        int* remove = calloc(g->count ? g->count : 1, sizeof(*remove));
        if (!remove) {
            fprintf(stderr, "Erro de memória na otimização.\n");
            exit(1);
        }
        for (size_t i = 0; i < g->count; i++) {
            IRInstruction* instruction = &g->code[i];
            if (instruction->removed) {
                continue;
            }
            const char* op = instruction->op;
            if (ir_equal(op, "func") || ir_equal(op, "label") || ir_equal(op, "endfunc")) {
                reachable = 1;
            }
            int removable = ir_pure(op) && !ir_equal(op, "div") && !ir_equal(op, "mod");
            if (!reachable || (removable && instruction->args[0][0] == '%' && !ir_used(g, instruction->args[0]))) {
                remove[i] = 1;
            } else if (ir_equal(op, "jmp") || ir_equal(op, "ret")) {
                reachable = 0;
            }
        }
        for (size_t i = 0; i < g->count; i++) {
            if (remove[i]) {
                g->code[i].removed = 1;
                changed = 1;
            }
        }
        free(remove);
    }
}

static void ir_generate(const NoAST* tree, int optimize) {
    IRGenerator generator = {0};
    IRGenerator* g = &generator;
    g->tree = tree;
    g->optimize = optimize;
    g->return_type = "void";
    ir_emit(g, "func", 2, "$init", "void");
    for (size_t i = 0; i < tree->quantidade; i++) {
        if (tree->itens[i]->no != NO_FUNCAO) {
            ir_statement(g, tree->itens[i]);
        }
    }
    ir_emit(g, "ret", 0);
    ir_emit(g, "endfunc", 0);
    for (size_t i = 0; i < tree->quantidade; i++) {
        const NoAST* function = tree->itens[i];
        if (function->no != NO_FUNCAO) {
            continue;
        }
        ir_forget(g, 0);
        g->return_type = function->tipo;
        g->depth++;
        ir_emit(g, "func", 2, function->texto, function->tipo);
        for (size_t a = 0; a < function->quantidade; a++) {
            IRBinding* binding = ir_bind(g, function->itens[a]);
            ir_emit(g, "arg", 2, binding->name, binding->type);
        }
        ir_block(g, function->filhos[0]);
        if (ir_equal(function->tipo, "void")) {
            ir_emit(g, "ret", 0);
        }
        ir_emit(g, "endfunc", 0);
        ir_leave(g);
    }
    if (optimize) {
        ir_cleanup(g);
    }
    for (size_t i = 0; i < g->count; i++) {
        IRInstruction* instruction = &g->code[i];
        if (instruction->removed) {
            continue;
        }
        printf("%s", instruction->op);
        for (int a = 0; a < instruction->count; a++) {
            printf(" %s", instruction->args[a]);
        }
        putchar('\n');
    }
    while (g->bindings) {
        IRBinding* next = g->bindings->next;
        free(g->bindings);
        g->bindings = next;
    }
    for (size_t i = 0; i < g->string_count; i++) {
        free(g->strings[i]);
    }
    free(g->strings);
    free(g->code);
}

static int ir_main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(1, _O_BINARY); /* descritor padrão de stdout */
#endif
    int optimize = 0;
    int selected = -1;
    const char* source = NULL;
    for (int i = 1; i < argc; i++) {
        if (ir_equal(argv[i], "--ir")) {
            continue;
        }
        if (ir_equal(argv[i], "-O0") || ir_equal(argv[i], "-O1")) {
            int level = ir_equal(argv[i], "-O1");
            if (selected >= 0 && selected != level) {
                fprintf(stderr, "Erro: selecione apenas -O0 ou -O1.\n");
                return 2;
            }
            selected = optimize = level;
        } else if (argv[i][0] == '-' || source) {
            fprintf(stderr, "Uso: %s [--ir] [-O0|-O1] arquivo.c\n", argv[0]);
            return 2;
        } else {
            source = argv[i];
        }
    }
    if (!source) {
        fprintf(stderr, "Uso: %s [--ir] [-O0|-O1] arquivo.c\n", argv[0]);
        return 2;
    }
    FILE* input = fopen(source, "rb");
    if (!input) {
        fprintf(stderr, "Erro de entrada: %s\n", source);
        return 1;
    }
    fclose(input);
    free_scanner();
    analyze_file(source);
    if (errors_count) {
        fprintf(stderr, "Erro léxico: %s na linha %d, coluna %d.\n", errors[0].error, errors[0].line, errors[0].column);
        free_scanner();
        return 2;
    }
    ParserLL1* parser = calloc(1, sizeof(*parser));
    if (!parser) {
        free_scanner();
        return 1;
    }
    switch (setjmp(parser->erro)) {
        case 0:
            break;
        case 1:
            fprintf(stderr, "Erro sintático: %s na linha %d, coluna %d.\n",
                    parser->mensagem, parser->linha_erro, parser->coluna_erro);
            liberar_parser(parser);
            free_scanner();
            return 3;
        default:
            fprintf(stderr, "Erro de memória ao construir a AST.\n");
            liberar_parser(parser);
            free_scanner();
            return 1;
    }
    NoAST* tree = analisar_tokens(parser, tokens, tokens_count);
    sem_modo_ir = 1;
    int accepted = executar_analise_semantica(tree);
    sem_modo_ir = 0;
    if (accepted) {
        ir_generate(tree, optimize);
    }
    liberar_parser(parser);
    free_scanner();
    return accepted ? 0 : 4;
}

#endif /* MINIC_IR_H */
