/* Serialização legível da IR; mantém a forma estruturada durante a geração. */
#ifndef MINIC_FORMATAR_IR_H
#define MINIC_FORMATAR_IR_H

typedef struct IRName IRName;
struct IRName {
    const char* raw;
    const char* pretty;
    IRName* next;
};

static int ir_read_start(const char* op) {
    return ir_pure(op) || ir_equal(op, "load") || ir_equal(op, "call") ||
           ir_equal(op, "alloc") || ir_equal(op, "arg");
}

static void ir_simplify_snapshots(IRGenerator* g) {
    for (size_t i = 0; i < g->count; i++) {
        IRInstruction* instruction = &g->code[i];
        if (instruction->removed || !ir_equal(instruction->op, "mov") ||
            strncmp(instruction->args[0], "%t", 2) != 0 ||
            (strncmp(instruction->args[1], "%v", 2) != 0 &&
             strncmp(instruction->args[1], "@g", 2) != 0)) {
            continue;
        }
        const char* dest = instruction->args[0];
        const char* source = instruction->args[1];
        size_t last = i;
        for (size_t j = i + 1; j < g->count; j++) {
            IRInstruction* next = &g->code[j];
            if (next->removed) {
                continue;
            }
            if (ir_equal(next->op, "endfunc")) {
                break;
            }
            for (int a = ir_read_start(next->op); a < next->count; a++) {
                if (ir_equal(next->args[a], dest)) {
                    last = j;
                }
            }
        }
        int safe = 1;
        for (size_t j = i + 1; j <= last; j++) {
            IRInstruction* next = &g->code[j];
            if (next->removed) {
                continue;
            }
            const char* op = next->op;
            if (ir_equal(op, "call") || ir_equal(op, "label") || ir_equal(op, "jmp") ||
                ((ir_equal(op, "brfalse") || ir_equal(op, "brtrue") || ir_equal(op, "ret")) && j < last) ||
                (ir_read_start(op) && ir_equal(next->args[0], source))) {
                safe = 0;
                break;
            }
        }
        if (!safe) {
            continue;
        }
        instruction->removed = 1;
        for (size_t j = i + 1; j <= last; j++) {
            IRInstruction* next = &g->code[j];
            if (next->removed) {
                continue;
            }
            for (int a = ir_read_start(next->op); a < next->count; a++) {
                if (ir_equal(next->args[a], dest)) {
                    next->args[a] = source;
                }
            }
        }
    }
}

static const char* ir_find_name(IRName* names, const char* raw) {
    for (IRName* name = names; name; name = name->next) {
        if (ir_equal(name->raw, raw)) {
            return name->pretty;
        }
    }
    return NULL;
}

static int ir_name_taken(IRName* names, const char* pretty) {
    for (IRName* name = names; name; name = name->next) {
        if (ir_equal(name->pretty, pretty)) {
            return 1;
        }
    }
    return 0;
}

static void ir_add_name(IRName** names, const char* raw, const char* pretty) {
    IRName* name = ir_resize(NULL, sizeof(*name));
    name->raw = raw;
    name->pretty = pretty;
    name->next = *names;
    *names = name;
}

static const char* ir_display(IRGenerator* g, IRName** names, const char* raw, int* counter) {
    const char* pretty = ir_find_name(*names, raw);
    if (pretty) {
        return pretty;
    }
    if (strncmp(raw, "%t", 2) != 0) {
        return raw;
    }
    do {
        pretty = ir_format(g, "t%d", ++*counter);
    } while (ir_name_taken(*names, pretty));
    ir_add_name(names, raw, pretty);
    return pretty;
}

static void ir_present(IRGenerator* g) {
    ir_simplify_snapshots(g);
    IRName* globals = NULL;
    for (size_t i = 0; i < g->count; i++) {
        IRInstruction* instruction = &g->code[i];
        if (!instruction->removed && ir_equal(instruction->op, "alloc") && instruction->args[0][0] == '@') {
            const char* raw = instruction->args[0];
            ir_add_name(&globals, raw, strchr(raw, '_') + 1);
        }
    }
    size_t index = 0;
    while (index < g->count) {
        IRInstruction* header = &g->code[index];
        size_t end = index + 1;
        while (!ir_equal(g->code[end].op, "endfunc")) {
            end++;
        }
        int empty = 1;
        for (size_t i = index + 1; i < end; i++) {
            if (!g->code[i].removed && !ir_equal(g->code[i].op, "ret")) {
                empty = 0;
            }
        }
        if (ir_equal(header->args[0], "$init") && empty) {
            index = end + 1;
            continue;
        }
        IRName* names = globals;
        for (size_t i = index + 1; i < end; i++) {
            IRInstruction* instruction = &g->code[i];
            if (instruction->removed ||
                (!ir_equal(instruction->op, "alloc") && !ir_equal(instruction->op, "arg")) ||
                ir_find_name(names, instruction->args[0])) {
                continue;
            }
            const char* raw = instruction->args[0];
            const char* base = strchr(raw, '_') + 1;
            if (ir_equal(header->args[0], "$init") && raw[0] == '%') {
                base = ir_format(g, "%s#init", base);
            }
            const char* pretty = base;
            int suffix = 2;
            while (ir_name_taken(names, pretty)) {
                pretty = ir_format(g, "%s#%d", base, suffix++);
            }
            ir_add_name(&names, raw, pretty);
        }
        int counter = 0;
        printf("func %s", header->args[0]);
        int formals = 0;
        for (size_t i = index + 1; i < end; i++) {
            IRInstruction* instruction = &g->code[i];
            if (!instruction->removed && ir_equal(instruction->op, "arg")) {
                printf("%s%s", formals++ ? ", " : "(", ir_display(g, &names, instruction->args[0], &counter));
            }
        }
        printf("%s\n", formals ? ")" : "");
        for (size_t i = index + 1; i < end; i++) {
            IRInstruction* instruction = &g->code[i];
            const char* op = instruction->op;
            if (instruction->removed || ir_equal(op, "arg")) {
                continue;
            }
            const char* values[4] = {NULL};
            for (int a = 0; a < instruction->count; a++) {
                values[a] = ir_display(g, &names, instruction->args[a], &counter);
            }
            if (ir_equal(op, "alloc")) {
                if (instruction->args[0][0] == '@' || eh_vetor(instruction->args[1])) {
                    printf("  alloc %s %s %s\n", values[0], values[1], values[2]);
                }
            } else if (ir_equal(op, "mov")) {
                printf("  %s = %s\n", values[0], values[1]);
            } else if (ir_pure(op) && instruction->count == 3) {
                static const char* ops[] = {"add", "sub", "mul", "div", "mod", "lt", "gt", "le", "ge", "eq", "ne"};
                static const char* symbols[] = {"+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!="};
                for (size_t a = 0; a < sizeof(ops) / sizeof(*ops); a++) {
                    if (ir_equal(op, ops[a])) {
                        printf("  %s = %s %s %s\n", values[0], values[1], symbols[a], values[2]);
                        break;
                    }
                }
            } else if (ir_equal(op, "neg") || ir_equal(op, "not") || ir_equal(op, "i2f")) {
                const char* symbol = "not";
                if (ir_equal(op, "neg")) {
                    symbol = "-";
                } else if (ir_equal(op, "i2f")) {
                    symbol = "int_to_float";
                }
                printf("  %s = %s %s\n", values[0], symbol, values[1]);
            } else if (ir_equal(op, "call")) {
                printf("  %s = call %s, %s\n", values[0], values[1], values[2]);
            } else if (ir_equal(op, "load")) {
                printf("  %s = load %s[%s]\n", values[0], values[1], values[2]);
            } else if (ir_equal(op, "store")) {
                printf("  store %s[%s], %s\n", values[0], values[1], values[2]);
            } else if (ir_equal(op, "brfalse") || ir_equal(op, "brtrue")) {
                printf("  %s %s goto %s\n", ir_equal(op, "brfalse") ? "ifFalse" : "ifTrue", values[0], values[1]);
            } else if (ir_equal(op, "jmp")) {
                printf("  goto %s\n", values[0]);
            } else if (ir_equal(op, "label")) {
                printf("%s:\n", values[0]);
            } else {
                printf("  %s", ir_equal(op, "ret") ? "return" : op);
                for (int a = 0; a < instruction->count; a++) {
                    printf(" %s", values[a]);
                }
                putchar('\n');
            }
        }
        puts("endfunc");
        while (names != globals) {
            IRName* next = names->next;
            free(names);
            names = next;
        }
        index = end + 1;
    }
    while (globals) {
        IRName* next = globals->next;
        free(globals);
        globals = next;
    }
}

#endif /* MINIC_FORMATAR_IR_H */
