
# Compilador MiniC (Front-End)

Este repositório apresenta o desenvolvimento do **front-end de um compilador para a linguagem MiniC**. Atualmente, o projeto encontra-se com duas etapas concluídas: a implementação do **Analisador Léxico (Scanner)** e do **Analisador Sintático (Parser)**.

---

## Pré-requisitos e Dependências

Para rodar o projeto e executar os testes com sucesso, certifique-se de ter as seguintes ferramentas instaladas no seu ambiente (como Linux, macOS ou Git Bash no Windows):

* **GCC (GNU Compiler Collection):** Necessário para compilar as versões em C do scanner e do parser.
* **Python 3.10 ou superior:** Necessário para executar as versões em Python do scanner e do parser e os scripts de automação (`bash`).

---

## Etapas do Compilador

| Etapa | Status | Descrição |
| :--- | :--- | :--- |
| **Analisador Léxico (Scanner)** | **Concluído** | Conversão do código-fonte em tokens e geração de diagnósticos de erro. |
| **Analisador Sintático (Parser)** | **Concluído** | Verificação da gramática utilizando a técnica **LL(1)** e construção da estrutura sintática a partir dos tokens. |
| **Analisador Semântico** | Planejado | Checagem de tipos, escopos e consistência semântica. |
| **Geração de Código Intermediário** | Planejado | Tradução para representação intermediária. |

---

## Estrutura do Projeto

O repositório está organizado em diretórios separados para isolar as implementações e os ambientes de teste:

* **`scanner_c/`** — Contém o código-fonte do scanner em C (`scanner.c`) e o script de automação (`testar_scanner_c.sh`).
* **`scanner_python/`** — Contém o código-fonte do scanner em Python (`scanner.py`) e o script de automação (`testar_scanner_python.sh`).
* **`parser_c/`** — Contém o código-fonte do parser em C (`parser.c`) e o script de automação (`testar_parser_c.sh`).
* **`parser_python/`** — Contém o código-fonte do parser em Python (`parser.py`) e o script de automação (`testar_parser_python.sh`).
* **`tests/`** *(presente nas subpastas)* — Armazena os códigos de teste (`.c` e `.minic`) e seus respectivos gabaritos esperados.
* **`errors/`** *(gerado automaticamente pelo scanner)* — Recebe os arquivos `.errors.jsonl` contendo os diagnósticos de erros léxicos encontrados durante a execução do scanner.


---

# Analisador Léxico / Scanner

Analisador léxico (scanner) para a linguagem **MINIC**, desenvolvido em duas implementações equivalentes: **C** e **Python**. O projeto lê arquivos de código-fonte (`.minic` ou `.c`), identifica tokens com suas respectivas coordenadas/atributos e gera diagnósticos detalhados para erros léxicos em formato JSON Lines (`.jsonl`).

---

## Funcionalidades

- **Reconhecimento de Tokens**: Suporte a palavras reservadas (`if`, `else`, `while`, `int`, etc.), identificadores, literais (`INT_LIT`, `FLOAT_LIT`, `CHAR_LIT`, `STRING_LIT`) e operadores/pontuações (`==`, `!=`, `&&`, `{`, `}`, `;`, etc.).
- **Tratamento Avançado de Erros**: Detecta e categoriza erros como literais malformados, identificadores inválidos, strings/caracteres não terminados e zero à esquerda em numéricos.
- **Mecanismo de Recuperação**: Emite os símbolos estruturais válidos (ex: `)`, `;`) mesmo após a ocorrência de erros em literais não terminados.
- **Saída Estruturada**: Emite tokens diretamente na saída padrão como objetos JSON por linha (`.jsonl`).
- **Geração Automática de Logs de Erro**: Grava erros encontrados em arquivos na pasta `errors/`.
- **Suíte de Testes Automatizada**: Scripts em Bash para validar a conformidade das implementações contra gabaritos esperados.

---

## Como Executar os Testes

Os scripts de automação varrem a pasta de testes, executam o scanner para cada arquivo de entrada e comparam a saída obtida com o padrão esperado.

### 1. Testando o Scanner em C

O script de automação compila o código automaticamente utilizando o `gcc` antes de iniciar as validações.

```bash
cd scanner_c/
./testar_scanner_c.sh scanner.c tests/
````

 > **Observação:** Caso ocorra algum erro na compilação automática pelo script, compile manualmente o código-fonte executando o comando abaixo antes de rodar os testes:
>
>
> ```
> gcc -Wall -Wextra -std=c11 scanner.c -o scanner
> ```

 ### 2\. Testando o Scanner em Python

 A versão em Python roda diretamente por interpretador, sem necessidade de compilação prévia.

```
cd scanner_python/
./testar_scanner_python.sh scanner.py tests/
```

---

 # Analisador Sintático / Parser

 O analisador sintático (**parser**) corresponde à segunda etapa do desenvolvimento do compilador MiniC. Foram desenvolvidas duas implementações equivalentes, uma em **C** e outra em **Python**.

 O parser utiliza a técnica de análise sintática **LL(1)**, realizando a leitura dos tokens produzidos pelo analisador léxico e verificando se a sequência de tokens está de acordo com a gramática definida para a linguagem MiniC.

 A análise LL(1) utiliza um único token de antecipação (_lookahead_) para determinar qual produção da gramática deve ser aplicada, permitindo realizar a análise sintática de forma determinística.

 ## Funcionalidades

 - **Análise Sintática LL(1)**: Verificação da sequência de tokens utilizando a técnica LL(1).
- **Reconhecimento da Gramática MiniC**: Validação das construções sintáticas definidas para a linguagem.
- **Implementações em C e Python**: O parser foi desenvolvido em duas versões equivalentes.
- **Tratamento de Erros Sintáticos**: Identificação de tokens inesperados e construções que não pertencem à gramática.
- **Suíte de Testes Automatizada**: Scripts em Bash executam os casos de teste e comparam os resultados obtidos com os resultados esperados.

---

 ## Gramática LL(1)

 A gramática utilizada pelo analisador sintático é apresentada abaixo:

```
programa -> lista_elementos EOF
lista_elementos -> elemento lista_elementos | epsilon
elemento -> decl_ou_funcao | comando
decl_ou_funcao -> tipo_retorno IDENT resto_decl_func
resto_decl_func -> LPAREN resto_df | resto_d resto_dl
resto_df -> parametros RPAREN bloco | RPAREN bloco
tipo_retorno -> tipo | VOID
tipo -> INT | FLOAT | BOOL | CHAR
parametros -> parametro resto_parametros
resto_parametros -> COMMA parametro resto_parametros | epsilon
parametro -> tipo IDENT resto_parametro
resto_parametro -> LBRACKET RBRACKET | epsilon
bloco -> LBRACE itens_bloco RBRACE
itens_bloco -> item_bloco itens_bloco | epsilon
item_bloco -> declaracao_local | comando
declaracao_local -> tipo IDENT resto_d resto_dl
resto_dl -> COMMA declarador resto_dl | SEMICOLON
declarador -> IDENT resto_d
resto_d -> inicializacao | LBRACKET tamanho RBRACKET | epsilon
tamanho -> INT_LIT
inicializacao -> ASSIGN expressao
comando -> comando_bloco
         | comando_if
         | comando_while
         | comando_for
         | comando_return
         | comando_break
         | comando_continue
         | comando_print
         | comando_read
         | comando_expressao
comando_bloco -> bloco
comando_if -> IF LPAREN expressao RPAREN comando resto_cm_if
resto_cm_if -> ELSE comando | epsilon
comando_while -> WHILE LPAREN expressao RPAREN comando
comando_for -> FOR LPAREN exp_opcional SEMICOLON exp_opcional SEMICOLON exp_opcional RPAREN comando
comando_return -> RETURN exp_opcional SEMICOLON
comando_break -> BREAK SEMICOLON
comando_continue -> CONTINUE SEMICOLON
comando_print -> PRINT LPAREN argumento_print RPAREN SEMICOLON
argumento_print -> argumentos
comando_read -> READ LPAREN RPAREN SEMICOLON
comando_expressao -> exp_opcional SEMICOLON
exp_opcional -> expressao | epsilon
expressao -> expressao_or resto_atribuicao
resto_atribuicao -> ASSIGN expressao | epsilon
expressao_or -> expressao_and aux_exp_or
aux_exp_or -> OR expressao_and aux_exp_or | epsilon
expressao_and -> expressao_igualdade aux_exp_and
aux_exp_and -> AND expressao_igualdade aux_exp_and | epsilon
expressao_igualdade -> expressao_relacional aux_exp_igualdade
aux_exp_igualdade -> EQ expressao_relacional aux_exp_igualdade | NE expressao_relacional aux_exp_igualdade | epsilon
expressao_relacional -> expressao_aditiva aux_exp_relacional
aux_exp_relacional -> op_rel expressao_aditiva aux_exp_relacional | epsilon
op_rel -> LT | GT | LE | GE
expressao_aditiva -> expressao_multiplicativa aux_exp_aditiva
aux_exp_aditiva -> PLUS expressao_multiplicativa aux_exp_aditiva | MINUS expressao_multiplicativa aux_exp_aditiva
expressao_multiplicativa -> expressao_unaria aux_exp_multiplicativa
aux_exp_multiplicativa -> STAR expressao_unaria aux_exp_multiplicativa
                        | SLASH expressao_unaria aux_exp_multiplicativa
                        | PERCENT expressao_unaria aux_exp_multiplicativa
                        | epsilon
expressao_unaria -> MINUS expressao_unaria | NOT expressao_unaria | expressao_posfixa
expressao_posfixa -> primario aux_exp_posfixa
aux_exp_posfixa -> LBRACKET expressao RBRACKET aux_exp_posfixa
                 | LPAREN resto_chamada aux_exp_posfixa
                 | epsilon
resto_chamada -> RPAREN | argumentos RPAREN
argumentos -> expressao resto_argumentos
resto_argumentos -> COMMA expressao resto_argumentos | epsilon
primario -> IDENT
          | INT_LIT
          | FLOAT_LIT
          | TRUE
          | FALSE
          | CHAR_LIT
          | STRING_LIT
          | LPAREN expressao RPAREN
```

---

 ## Como Executar os Testes do Parser

 Os testes do parser devem ser executados a partir das respectivas pastas de implementação utilizando o **Git Bash**.

 ### 1\. Testando o Parser em C

 Entre na pasta `parser_c/` e execute o script de testes passando a pasta contendo os testes e o arquivo do parser:

```
cd parser_c/
./testar_parser_c.sh ./tests ./parser.c
```

 ### 2\. Testando o Parser em Python

 Entre na pasta `parser_python/` e execute o script de testes passando a pasta contendo os testes e o arquivo do parser:

```
cd parser_python/
./testar_parser_python.sh ./tests ./parser.py
```

 Os scripts executam os casos de teste disponíveis na pasta `tests/` e verificam se os resultados produzidos pelo parser estão de acordo com os resultados esperados.




## INTEGRANTES

- [Mikeias Lopes](https://github.com/MikeiasLopes)
- [Caio Mantia](https://github.com/CaioCastellani)
- [Estêvan Silva](https://github.com/estevanss160)
- [Vinicius Santos](https://github.com/vynnyss)
- [Gabriel Vicente](https://github.com/Gabriel-Aiala)
- [Diego Fonseca](https://github.com/Diegopkg100)
- [Caio Felix](https://github.com/Caio-Felix1)
- [Iarley Souza](https://github.com/IarleySouza)
