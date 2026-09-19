#ifndef SCANNER_H
#define SCANNER_H

#include <stddef.h>

typedef enum {
    ATTR_NONE, ATTR_INT, ATTR_FLOAT, ATTR_STR
} AttrType;

typedef struct {
    const char* token;
    char* lexeme;
    AttrType attr_type;
    int attr_int;
    double attr_float;
    char* attr_str;
    int line;
    int column;
} Token;

typedef struct {
    const char* error;
    char* lexeme;
    int line;
    int column;
} Error;

extern Token* tokens;
extern size_t tokens_count;
extern size_t tokens_cap;
extern Error* errors;
extern size_t errors_count;
extern size_t errors_cap;

void analyze_file(const char* filename);
void free_scanner(void);

#endif
