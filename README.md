  
# Compilador MiniC (Front-End)

Este repositório apresenta o desenvolvimento do **front-end de um compilador para a linguagem MiniC**. Atualmente, o projeto conta com quatro etapas implementadas: **Analisador Léxico (Scanner)**, **Analisador Sintático (Parser)**, **Analisador Semântico** e **Geração de Representação Intermediária (IR)**, com modos sem otimização (`-O0`) e com otimizações conservadoras (`-O1`).

---

## Pré-requisitos e Dependências

Para rodar o projeto e executar os testes com sucesso, certifique-se de ter as seguintes ferramentas instaladas no seu ambiente (como Linux, macOS ou Git Bash no Windows):

* **GCC (GNU Compiler Collection):** Necessário para compilar as versões em C do scanner, do parser e do analisador semântico.
* **Python 3.10 ou superior:** Necessário para executar as versões em Python do scanner, do parser e do analisador semântico e os scripts de automação (`bash`).

---

## Etapas do Compilador

| Etapa | Status | Descrição |
| :--- | :--- | :--- |
| **Analisador Léxico (Scanner)** | **Concluído** | Conversão do código-fonte em tokens e geração de diagnósticos de erro. |
| **Analisador Sintático (Parser)** | **Concluído** | Verificação da gramática utilizando a técnica **LL(1)** e construção da estrutura sintática a partir dos tokens. |
| **Analisador Semântico** | **Concluído** | Checagem de tipos, escopos, aridade, coerções numéricas e consistência semântica. |
| **Geração de Código Intermediário** | **Concluído** | IR de três endereços, controle de fluxo, chamadas, vetores e otimizações conservadoras. |

---

## Estrutura do Projeto

O repositório está organizado em diretórios separados para isolar as implementações e os ambientes de teste:

* **`scanner_c/`** — Contém o código-fonte do scanner em C (`scanner.c`) e o script de automação (`testar_scanner_c.sh`).
* **`scanner_python/`** — Contém o código-fonte do scanner em Python (`scanner.py`) e o script de automação (`testar_scanner_python.sh`).
* **`parser_c/`** — Contém o código-fonte do parser em C (`parser.c`) e o script de automação (`testar_parser_c.sh`).
* **`parser_python/`** — Contém o código-fonte do parser em Python (`parser.py`) e o script de automação (`testar_parser_python.sh`).
* **`analisador_semantico_c/`** — Contém o código-fonte do analisador semântico em C (`minic.c`), script de automação (`testes_semanticos_c.sh`) e casos de teste.
* **`analisador_semantico_python/`** — Contém o código-fonte do analisador semântico em Python (`minic.py`), script de automação (`testes_semanticos_py.sh`) e casos de teste.
* **`representacao_intermediaria_c/`** — Gerador nativo em C (`representacao_intermediaria.c`, `ir.h` e `formatar_ir.h`), script `test_ir_c.sh` e suíte de IR.
* **`representacao_intermediaria_python/`** — Gerador em Python (`representacao_intermediaria.py`), script `test_ir_python.sh` e a mesma suíte de IR.
* **`validar_ir.py`** — Validador compartilhado da suíte fornecida, máquina virtual de IR e intérprete independente da AST.
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

---

# Analisador Semântico

O analisador semântico corresponde à terceira etapa do desenvolvimento do compilador MiniC. Foram desenvolvidas duas implementações equivalentes, uma em **C** (`analisador_semantico_c/minic.c`) e outra em **Python** (`analisador_semantico_python/minic.py`), ambas integradas diretamente ao analisador sintático (o qual invoca o analisador léxico).

O analisador semântico percorre a Árvore Sintática Abstrata (AST) construída pelo parser, realizando checagem estática de tipos, controle de visibilidade e resolução de escopos com suporte a sombreamento (*shadowing*), coleta prévia de assinaturas para recursão mútua, validação de regras de coerção numérica e verificação de retorno estrito em funções.

---

## Funcionalidades

- **Gerenciamento de Escopo e Sombreamento**: Resolução léxica de identificadores em múltiplos níveis de escopo (global, funções e blocos aninhados), permitindo que variáveis internas sombreiem variáveis externas legitimamente.
- **Resolução de Recursão e Chamadas**: Coleta prévia das assinaturas de todas as funções globais na primeira passada, permitindo que uma função chame a si mesma recursivamente ou chame outras funções declaradas posteriormente.
- **Sistema de Tipos e Coerções**:
  - Tipos primitivos: `int`, `float`, `bool`, `char`, `void` e vetores (`tipo[]`).
  - Promoção numérica implícita permitida: `int → float`.
  - Proibição de estreitamento implícito (*narrowing*): `float` não pode ser atribuído a `int` sem conversão explícita.
- **Checagem de Operandos**: Operadores aritméticos (`+ - * /`) exigem operandos numéricos; `%` exige `int`; relacionais (`< > <= >=`) exigem operandos numéricos (ou dois `char`); igualdade (`== !=`) exige operandos numéricos ou do mesmo tipo `bool`/`char`; lógicos (`&& || !`) exigem `bool`; o menos unário exige operando numérico.
- **Validação de Valor-L (L-Value)**: Verificação semântica de que apenas variáveis escalares e elementos indexados de vetores podem figurar no lado esquerdo de atribuições (literais, expressões, vetores inteiros e nomes de função são rejeitados).
- **Uso Correto de Vetores e Funções**: Só vetores podem ser indexados; só funções podem ser chamadas (respeitando o sombreamento por variáveis locais); nome de função não pode ser usado como valor sem chamada; vetores devem ter tamanho maior que zero.
- **Uso de `void` como Valor**: Chamadas de funções `void` só são aceitas em posição de comando; são rejeitadas como operando, argumento, condição, índice, valor de retorno, inicialização ou atribuição.
- **Garantia de Cobertura de Retorno**: Verificação em funções não-`void` de que todos os caminhos de execução retornam um valor de tipo compatível, detectando potenciais quedas de fluxo sem retorno (laços `while (true)` / `for (;;)` sem `break` são reconhecidos como não terminantes). Funções `void` não podem retornar valor.
- **Comandos Fora de Contexto**: `break`/`continue` fora de laço e `return` fora de função.
- **Validação de Aridade e Tipos de Argumentos**: Checagem de quantidade e compatibilidade de tipos para cada parâmetro formal e argumento real passado na chamada; argumentos excedentes também são analisados.
- **Diagnósticos Canônicos com Coordenadas Precisas**: Emissão padronizada de mensagens de erro com códigos (`SEM001` a `SEM020`), linha e coluna exatas do elemento causador da inconformidade, em ordem de ocorrência no código-fonte.
- **Código de Saída**: `0` quando o programa é aceito e `1` quando é rejeitado (por erro léxico, sintático ou semântico).

---

## Catálogo de Erros Semânticos

| Código | Descrição |
| :--- | :--- |
| **SEM001** | Identificador não declarado no escopo corrente ou em escopos ancestrais. |
| **SEM002** | Declaração duplicada de identificador no mesmo escopo, informando linha/coluna anterior. |
| **SEM003** | Incompatibilidade de atribuição/conversão entre o tipo de destino e o tipo da expressão. |
| **SEM005** | Condição de comando de controle (`if`, `while`, `for`) com tipo diferente de `bool`. |
| **SEM006** | Expressão de índice de vetor com tipo diferente de `int`. |
| **SEM007** | Número de argumentos incompatível com a aridade esperada da função. |
| **SEM008** | Tipo de argumento incompatível com o tipo declarado do parâmetro formal correspondente. |
| **SEM009** | Tipo da expressão de retorno incompatível com o tipo de retorno declarado da função (inclui `return;` em função não-`void` e `return expr;` em função `void`). |
| **SEM011** | Função não-`void` pode alcançar o final do corpo sem retornar valor em determinados ramos. |
| **SEM012** | Chamada de função com retorno `void` utilizada como valor (atribuição, operando, argumento, condição, índice, retorno, `print`). |
| **SEM013** | Destino de atribuição não atribuível (literal, expressão, vetor inteiro ou nome de função em posição de valor-L). |
| **SEM014** | Operador aplicado a operandos de tipos incompatíveis. |
| **SEM015** | Indexação de identificador ou expressão que não é vetor. |
| **SEM016** | Nome de função usado como valor sem ser chamado. |
| **SEM017** | Chamada de identificador ou expressão que não é função. |
| **SEM018** | `break` ou `continue` fora de um laço. |
| **SEM019** | `return` fora de uma função. |
| **SEM020** | Vetor declarado com tamanho menor ou igual a zero. |

---

## Como Executar os Testes do Analisador Semântico

Os testes do analisador semântico devem ser executados a partir de suas respectivas pastas utilizando o **Git Bash** (ou terminal Bash):

### 1. Testando o Analisador Semântico em C

Entre na pasta `analisador_semantico_c/` e execute o script de testes:

```bash
cd analisador_semantico_c/
./testes_semanticos_c.sh minic.c ./tests
```

> **Observação:** O script compila o analisador automaticamente com `gcc -Wall -Wextra`, integrando o parser e o scanner.

### 2. Testando o Analisador Semântico em Python

Entre na pasta `analisador_semantico_python/` e execute o script de testes:

```bash
cd analisador_semantico_python/
./testes_semanticos_py.sh minic.py ./tests
```

---

# Representação Intermediária / IR

A representação intermediária corresponde à quarta etapa do compilador MiniC. As implementações em **C** (`representacao_intermediaria_c/representacao_intermediaria.c`) e **Python** (`representacao_intermediaria_python/representacao_intermediaria.py`) reutilizam o scanner, o parser e o analisador semântico existentes, sem repetir a gramática ou a checagem de tipos.

O fluxo completo é **código-fonte → tokens → AST → análise semântica → IR de três endereços → otimizações opcionais**. A IR só é emitida quando todas as etapas anteriores aceitam o programa. As duas versões utilizam o mesmo formato textual e a mesma ordem de avaliação.

---

## Funcionalidades

- **Expressões e Tipos**: Aritmética, módulo, comparações, operadores unários, atribuições associativas e promoção explícita `int → float` com `int_to_float`. A divisão entre inteiros trunca em direção a zero e o resto acompanha o sinal do dividendo.
- **Controle de Fluxo**: `if`/`else`, `while`, `for`, `break`, `continue` e `return`, com rótulos e desvios. No `for`, `continue` passa pelo incremento antes de testar novamente a condição.
- **Curto-Circuito**: `&&` e `||` usam desvios; o segundo operando só é avaliado quando necessário, preservando atribuições e chamadas com efeitos.
- **Funções e Recursão**: Parâmetros formais, argumentos, retorno, funções `void`, chamadas aninhadas e recursão direta ou mútua. Os argumentos são avaliados da esquerda para a direita antes de emitir `param`, preservando seus valores em temporários.
- **Escopos**: Cada variável recebe um nome de armazenamento único, preservando sombreamento entre blocos, parâmetros e globais. Temporários também não colidem com identificadores do programa.
- **Vetores**: `alloc`, `load` e `store` com índices em elementos, sem pressupor tamanho em bytes de uma arquitetura. Parâmetros vetoriais preservam a referência e seus efeitos sobre o vetor original.
- **Entrada e Saída**: `print` preserva a ordem de avaliação e impressão dos argumentos. `read()` gera uma instrução de leitura sem destino, conforme a gramática atual; não é uma expressão que retorna valor.
- **Inicialização Global**: Quando há declarações ou comandos globais, a função sintética `$init`, cujo nome não pode ser escrito como identificador MiniC, executa essa inicialização antes de `main`. Ela é omitida quando está vazia. O armazenamento global escalar possui valor inicial zero. Quando presente, o código consumidor da IR deve executar `$init` uma única vez.

---

## Formato da IR

A saída segue a notação dos gabaritos: nomes das variáveis do programa, temporários `t1`, `t2`, operações infixas e duas posições de indentação dentro das funções. Os temporários são numerados por função, evitando colisões com identificadores existentes. Variáveis sombreadas recebem sufixos como `x#2` para preservar a distinção entre seus armazenamentos. Literais `char` e `string` mantêm aspas e escapes.

A geração mantém internamente instruções estruturadas e nomes únicos. Na apresentação, declarações escalares locais não são impressas e cópias temporárias de leitura são omitidas somente quando não há redefinição ou transferência de controle que possa alterar o valor observado. Cópias necessárias para preservar efeitos de chamadas e atribuições continuam explícitas, inclusive em `-O0`.

| Instrução | Significado |
| :--- | :--- |
| `func nome` / `func nome(a, b)` / `endfunc` | Delimitam uma função, com parâmetros formais opcionais. |
| `alloc vetor tipo[] quantidade` | Reserva um vetor; alocações escalares globais também aparecem em `$init`. |
| `destino = origem` | Copia um valor. |
| `t1 = a + b` | Operação aritmética; também admite `-`, `*`, `/` e `%`. |
| `t1 = a < b` | Comparação; também admite `>`, `<=`, `>=`, `==` e `!=`. |
| `t1 = - a` / `t1 = not a` / `t1 = int_to_float a` | Operações unárias e conversão numérica. |
| `L1:` / `goto L1` | Define um rótulo ou desvia incondicionalmente. |
| `ifFalse condição goto L1` / `ifTrue condição goto L1` | Desvia conforme um booleano. |
| `param valor` / `t1 = call função, aridade` | Passa argumentos por valor; valores vetoriais são referências. |
| `t1 = load vetor[índice]` / `store vetor[índice], valor` | Lê ou grava um elemento. |
| `print valor` / `read` | Efeitos de entrada e saída. |
| `return [valor]` | Retorna da função, com valor opcional. |

Exemplo de saída sem otimização para o caso P01:

```text
func main
  a = 7
  b = 4
  t1 = a + b
  c = t1
  print c
  return 0
endfunc
```

---

## Modos de Otimização

- **`-O0` (padrão)**: Mantém as operações, sem propagação de constantes; a apresentação remove apenas cópias de leitura que podem ser substituídas com segurança pelo nome da variável.
- **`-O1`**: Propaga constantes conhecidas, dobra expressões constantes e conversões, simplifica identidades inteiras (`x + 0`, `x - 0`, `x * 1`, `x / 1`), seleciona ramos constantes e elimina definições locais sem uso e instruções após uma transferência incondicional.

A propagação é conservadora: condições desconhecidas invalidam o conhecimento ao unir ramos; definições que podem mudar no laço são invalidadas antes de sua geração e ao sair; chamadas invalidam constantes globais. Constantes invariantes, como `x = 6` e `y = 7` em um laço que não altera essas variáveis, permitem substituir `x * y` por `42` no próprio código gerado.

---

## Como Executar

### 1. Gerador em C

```bash
cd representacao_intermediaria_c/
gcc -std=c11 -Wall -Wextra representacao_intermediaria.c -o representacao_intermediaria
./representacao_intermediaria --ir -O0 tests/01-validos-sem-otimizacao/P01/P01.c
./representacao_intermediaria -O1 --ir tests/03-otimizacoes/O01/O01.c
```

### 2. Gerador em Python

```bash
cd representacao_intermediaria_python/
python representacao_intermediaria.py --ir -O0 tests/01-validos-sem-otimizacao/P01/P01.c
python representacao_intermediaria.py -O1 --ir tests/03-otimizacoes/O01/O01.c
```

Também é possível chamar a nova etapa pelo **minic** existente, a partir da raiz do projeto:

```bash
python analisador_semantico_python/minic.py --ir -O1 arquivo.c
gcc -std=c11 -Wall -Wextra analisador_semantico_c/minic.c -o minic
./minic --ir -O1 arquivo.c
```

Sem `--ir`, `-O0` ou `-O1`, os comandos antigos de `minic` mantêm a análise semântica e sua saída original. Nos geradores dedicados, emitir IR é o comportamento padrão mesmo sem `--ir`.

| Código de saída no modo IR | Significado |
| :--- | :--- |
| `0` | Programa aceito, IR emitida em stdout. |
| `1` | Falha de entrada/infraestrutura. |
| `2` | Erro léxico (ou uso inválido da linha de comando). |
| `3` | Erro sintático. |
| `4` | Erro semântico. |

Os diagnósticos são enviados a stderr, sem IR executável em stdout para entradas rejeitadas. O modo semântico antigo continua retornando `0`/`1`.

---

## Como Executar os Testes da IR

As duas pastas `tests/` contêm cópias integrais das três categorias da suíte independente fornecida: **10 válidos sem otimização**, **10 inválidos** e **10 de otimização**, incluindo os fontes `.c` e seus gabaritos `.txt` originais.

### 1. Testando a IR em C

```bash
cd representacao_intermediaria_c/
./test_ir_c.sh representacao_intermediaria.c ./tests
```

### 2. Testando a IR em Python

```bash
cd representacao_intermediaria_python/
./test_ir_python.sh representacao_intermediaria.py ./tests
```

Os scripts usam Bash e Python 3.10+ para a validação; o script C também exige GCC. `PYTHON_BIN`, `CC` e `CFLAGS` permitem selecionar as ferramentas. O script C compila uma única vez em uma pasta temporária e a remove ao terminar. Os scripts procuram um interpretador funcional em `python3` e depois `python`, evitando o alias da Microsoft Store quando ele não funciona.

**Critério de comparação:** os gabaritos canônicos não fixam uma serialização byte a byte. Para cada fonte válido, o validador executa a AST em um intérprete de referência independente do gerador e executa as IRs `-O0` e `-O1` em uma máquina virtual, comparando impressões e retorno. Para inválidos, verifica rejeição, categoria e ausência de IR. A máquina virtual também verifica instruções, aridade, destinos de desvio e uso de valores não definidos; limita os passos para detectar laços involuntários.

Para comparar também a saída textual determinística dos dois geradores, compile o executável C e execute, na raiz:

```bash
python validar_ir.py --suite representacao_intermediaria_python/tests \
  --compiler representacao_intermediaria_python/representacao_intermediaria.py \
  --python --peer ./representacao_intermediaria_c/representacao_intermediaria
```

A máquina virtual e o intérprete são ferramentas de teste, não um backend de produção. Assembly, modelo de memória em bytes e validação dinâmica de programas com comportamento indefinido não fazem parte desta etapa.

---

## INTEGRANTES

- [Mikeias Lopes](https://github.com/MikeiasLopes)
- [Caio Mantia](https://github.com/CaioCastellani)
- [Estêvan Silva](https://github.com/estevanss160)
- [Vinicius Santos](https://github.com/vynnyss)
- [Gabriel Vicente](https://github.com/Gabriel-Aiala)
- [Diego Fonseca](https://github.com/Diegopkg100)
- [Caio Felix](https://github.com/Caio-Felix1)
- [Iarley Souza](https://github.com/IarleySouza)
