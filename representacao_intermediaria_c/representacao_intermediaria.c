/* Driver nativo: scanner -> parser -> semântica -> IR. */
#define MINIC_NO_MAIN
#include "../analisador_semantico_c/minic.c"

int main(int argc, char** argv) {
    return ir_main(argc, argv);
}
