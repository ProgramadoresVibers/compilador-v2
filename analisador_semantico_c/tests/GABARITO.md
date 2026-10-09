# Gabarito — suíte de testes semânticos MINIC

## Convenções e compatibilidade

Os testes 01–20 usam as construções básicas da gramática (`int`, `float`, `bool`, `void`, funções, blocos, `if/else`, `while`, vetores, chamadas e operadores). Os testes 21–34 exercitam também construções aceitas pelo lexer/parser do projeto (`char`, literais `true/false`, cadeias, `for`, `break`, `continue`, `print` e `read`) e cobrem as regras semânticas adicionais SEM014–SEM020.

**Importante:** o material de projeto disponível contém o parser sintático, mas não uma implementação de analisador semântico. Portanto, as mensagens abaixo são o **gabarito de referência completo** conforme as regras da apostila da Aula 15 — não uma transcrição garantida da saída de um executável existente. Para tornar testes automatizáveis, o gabarito define códigos, posições e texto canônico. A política supõe escopo léxico com sombreamento, coleta de assinaturas para recursão, promoção `int → float` e proibição de estreitamento implícito.

Formato de sucesso esperado: `Análise semântica concluída: 0 erros; programa aceito.` Em caso de erro, listam-se todos os diagnósticos semânticos independentes em ordem de origem; a última linha informa a contagem e rejeição. Colunas são 1-based.

## Testes de aceitação

### `01_promocao_numerica.c` — ACEITO

Conversões int→float nos dois argumentos de soma. O retorno permanece int e é compatível com principal. A variável r é float.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `02_sombreamento_lexico.c` — ACEITO

O x float do bloco interno sombreia temporariamente o x int externo. Ao fechar o bloco, a resolução volta ao símbolo externo; x + 1 é int.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `03_logica_e_comparacoes.c` — ACEITO

< e == produzem bool; ! e && recebem bool. menor retorna bool, e a condição if(ok) está tipada corretamente.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `04_vetores_e_parametro_array.c` — ACEITO

dados é parâmetro array de int; i é int e o acesso tem tipo int. A soma promove int para float; xs é passado ao parâmetro array de int.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `05_lacos_aninhados.c` — ACEITO

As duas condições de while são comparações int que resultam em bool. Escopos, atribuições e expressões aritméticas são compatíveis; retorno int.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `06_recursao_e_retorno.c` — ACEITO

A assinatura de fatorial deve estar disponível ao analisar o próprio corpo. A chamada recursiva recebe int, ambos os ramos retornam int, e principal retorna int.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `07_void_como_comando.c` — ACEITO

inicializa(v) é chamada sem valor em posição de comando; return sem expressão é válido em função void. O índice é int e elemento int.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `08_retornos_nos_dois_ramos.c` — ACEITO

A comparação n < 0 é bool. Os dois ramos do if/else retornam int, portanto não há caminho de queda; chamada retorna int.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `09_atribuicao_associativa.c` — ACEITO

A atribuição é associativa à direita: b = 3 e depois a recebe o resultado compatível. Ambas são variáveis int atribuíveis.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

### `10_vetor_bool_e_funcoes.c` — ACEITO

A comparação produz bool para os retornos de contem; o parâmetro vetor recebe valores[2] de tipo array int. O if/else de principal cobre ambos os retornos.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

## Testes de rejeição

### `11_estreitamento_implicito.c` — REJEITADO

Diagnósticos esperados:

```text
SEM003 — linha 3, coluna 9: Não é possível atribuir float a int sem conversão permitida (destino “n”; expressão “2.5”).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `12_identificador_nao_declarado.c` — REJEITADO

Diagnósticos esperados:

```text
SEM001 — linha 3, coluna 9: Identificador “ausente” não declarado neste escopo.
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `13_declaracao_duplicada.c` — REJEITADO

Diagnósticos esperados:

```text
SEM002 — linha 3, coluna 11: “medida” já declarado neste escopo; declaração anterior na linha 2, coluna 9 (tipo int).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `14_condicao_nao_booleana.c` — REJEITADO

Diagnósticos esperados:

```text
SEM005 — linha 4, coluna 12: Condição de while deve ter tipo bool; recebeu int (expressão “n”).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `15_indice_float.c` — REJEITADO

Diagnósticos esperados:

```text
SEM006 — linha 3, coluna 11: Índice do vetor “dados” deve ser int; recebeu float (expressão “1.0”).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `16_aridade_incorreta.c` — REJEITADO

Diagnósticos esperados:

```text
SEM007 — linha 5, coluna 12: “combina” espera 2 argumentos, mas recebeu 1.
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `17_tipos_de_argumentos.c` — REJEITADO

Diagnósticos esperados:

```text
SEM008 — linha 5, coluna 20: Argumento 1 de “combina”: esperado int, recebido bool (expressão “1 < 2”).
SEM008 — linha 5, coluna 27: Argumento 2 de “combina”: esperado float, recebido bool (expressão “3 < 4”).
Análise semântica concluída: 2 erros; programa rejeitado.
```

### `18_uso_de_void_como_valor.c` — REJEITADO

Diagnósticos esperados:

```text
SEM012 — linha 6, coluna 9: Função “acao” não produz valor (retorno void) e não pode ser usada como expressão de atribuição.
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `19_retorno_e_cobertura.c` — REJEITADO

Diagnósticos esperados:

```text
SEM009 — linha 2, coluna 12: Retorno float incompatível com o tipo int da função “fracionario”; conversão implícita de float para int não permitida.
SEM011 — linha 4, coluna 1: A função “incompleta” pode terminar sem retornar int; o ramo em que “condicao” é falso alcança o fim do corpo.
Análise semântica concluída: 2 erros; programa rejeitado.
```

### `20_destino_nao_atribuivel.c` — REJEITADO

Diagnósticos esperados:

```text
SEM013 — linha 3, coluna 5: Destino de atribuição não é atribuível; o literal inteiro “3” não designa uma variável ou elemento de vetor.
Análise semântica concluída: 1 erro; programa rejeitado.
```

## Testes adicionais de cobertura semântica

### `21_operandos_aritmeticos.c` — REJEITADO

Operadores aritméticos exigem operandos numéricos; `%` exige dois `int`; o menos unário exige operando numérico. `char` não participa de aritmética.

Diagnósticos esperados:

```text
SEM014 — linha 4, coluna 9: Operador “+” não se aplica aos tipos bool e int (expressão “true + 1”).
SEM014 — linha 5, coluna 9: Operador “%” não se aplica aos tipos float e float (expressão “2.5 % 2.0”).
SEM014 — linha 6, coluna 9: Operador “-” não se aplica ao tipo bool (expressão “-true”).
SEM014 — linha 7, coluna 9: Operador “*” não se aplica aos tipos char e int (expressão “'a' * 2”).
Análise semântica concluída: 4 erros; programa rejeitado.
```

### `22_operandos_logicos_e_comparacoes.c` — REJEITADO

`&&`, `||` e `!` exigem `bool`; relacionais exigem operandos numéricos; `==` não compara `string` com `int`. A última atribuição (`true != false`) é válida.

Diagnósticos esperados:

```text
SEM014 — linha 3, coluna 9: Operador “&&” não se aplica aos tipos int e int (expressão “1 && 2”).
SEM014 — linha 4, coluna 9: Operador “!” não se aplica ao tipo int (expressão “!5”).
SEM014 — linha 5, coluna 9: Operador “<” não se aplica aos tipos int e bool (expressão “1 < true”).
SEM014 — linha 6, coluna 9: Operador “==” não se aplica aos tipos string e int (expressão “"abc" == 3”).
Análise semântica concluída: 4 erros; programa rejeitado.
```

### `23_indexacao_de_nao_vetor.c` — REJEITADO

Somente vetores podem ser indexados, tanto como destino quanto como valor; um nome de função também não é indexável.

Diagnósticos esperados:

```text
SEM015 — linha 6, coluna 5: “x” não é vetor e não pode ser indexado (tipo int).
SEM015 — linha 7, coluna 9: “dobro” não é vetor e não pode ser indexado (tipo função).
SEM015 — linha 8, coluna 12: “x” não é vetor e não pode ser indexado (tipo int).
Análise semântica concluída: 3 erros; programa rejeitado.
```

### `24_vetor_e_funcao_como_valor.c` — REJEITADO

Um vetor inteiro não é atribuível nem operando aritmético; o nome de uma função não pode ser usado como valor nem como destino sem chamada.

Diagnósticos esperados:

```text
SEM013 — linha 7, coluna 5: Destino de atribuição não é atribuível; o vetor “v” não pode ser atribuído como um todo; atribua a um elemento.
SEM014 — linha 8, coluna 9: Operador “+” não se aplica aos tipos int[] e int (expressão “v + 1”).
SEM016 — linha 9, coluna 9: “f” é uma função e não pode ser usada como valor sem ser chamada.
SEM013 — linha 10, coluna 5: Destino de atribuição não é atribuível; “f” é uma função e não designa uma variável ou elemento de vetor.
Análise semântica concluída: 4 erros; programa rejeitado.
```

### `25_chamada_de_nao_funcao.c` — REJEITADO

Só funções podem ser chamadas. A variável local `f` sombreia a função global `f`, portanto `f()` se refere à variável.

Diagnósticos esperados:

```text
SEM017 — linha 8, coluna 5: “x” não é uma função e não pode ser chamado (tipo int).
SEM017 — linha 10, coluna 12: “f” não é uma função e não pode ser chamado (tipo int).
Análise semântica concluída: 2 erros; programa rejeitado.
```

### `26_void_em_expressoes.c` — REJEITADO

Uma chamada `void` só é aceita em posição de comando; cada uso como valor (inicialização, operando, `print`, condição, retorno) gera SEM012.

Diagnósticos esperados:

```text
SEM012 — linha 5, coluna 13: Função “g” não produz valor (retorno void) e não pode ser usada como expressão de atribuição.
SEM012 — linha 6, coluna 13: Função “g” não produz valor (retorno void) e não pode ser usada como operando de “+”.
SEM012 — linha 7, coluna 11: Função “g” não produz valor (retorno void) e não pode ser usada como argumento de print.
SEM012 — linha 8, coluna 9: Função “g” não produz valor (retorno void) e não pode ser usada como condição de if.
SEM012 — linha 9, coluna 12: Função “g” não produz valor (retorno void) e não pode ser usada como valor de retorno.
Análise semântica concluída: 5 erros; programa rejeitado.
```

### `27_destino_expressao.c` — REJEITADO

Expressões aritméticas, unárias e literais de qualquer tipo não designam uma posição de memória e não podem receber atribuição.

Diagnósticos esperados:

```text
SEM013 — linha 4, coluna 5: Destino de atribuição não é atribuível; a expressão “a + b” não designa uma variável ou elemento de vetor.
SEM013 — linha 5, coluna 5: Destino de atribuição não é atribuível; a expressão “-a” não designa uma variável ou elemento de vetor.
SEM013 — linha 6, coluna 5: Destino de atribuição não é atribuível; o literal booleano “true” não designa uma variável ou elemento de vetor.
SEM013 — linha 7, coluna 5: Destino de atribuição não é atribuível; o literal real “2.5” não designa uma variável ou elemento de vetor.
Análise semântica concluída: 4 erros; programa rejeitado.
```

### `28_break_continue_return_fora.c` — REJEITADO

`return` no nível global e `break`/`continue` fora de laço são rejeitados; o `break` dentro do `while` é válido.

Diagnósticos esperados:

```text
SEM019 — linha 1, coluna 1: Comando return fora de uma função.
SEM018 — linha 3, coluna 5: Comando break fora de um laço (while ou for).
SEM018 — linha 4, coluna 5: Comando continue fora de um laço (while ou for).
Análise semântica concluída: 3 erros; programa rejeitado.
```

### `29_argumentos_excedentes_e_encadeada.c` — REJEITADO

Na atribuição encadeada `a = b = 3`, o identificador não declarado é reportado uma única vez. Argumentos excedentes também são analisados, revelando o identificador não declarado após o SEM007.

Diagnósticos esperados:

```text
SEM001 — linha 6, coluna 9: Identificador “b” não declarado neste escopo.
SEM007 — linha 7, coluna 12: “f” espera 1 argumento, mas recebeu 2.
SEM001 — linha 7, coluna 17: Identificador “naoexiste” não declarado neste escopo.
Análise semântica concluída: 3 erros; programa rejeitado.
```

### `30_cobertura_de_retorno.c` — REJEITADO

Sem `if`, a queda é genérica; quando só o `else` retorna, o ramo verdadeiro alcança o fim; `while (true)` com `break` pode terminar; `while (true)` sem `break` nunca alcança o fim do corpo e é aceito.

Diagnósticos esperados:

```text
SEM011 — linha 1, coluna 1: A função “semif” pode terminar sem retornar int; o fim do corpo pode ser alcançado sem um comando return.
SEM011 — linha 5, coluna 1: A função “senaoretorna” pode terminar sem retornar int; o ramo em que “c” é verdadeiro alcança o fim do corpo.
SEM011 — linha 12, coluna 1: A função “lacocombreak” pode terminar sem retornar int; o fim do corpo pode ser alcançado sem um comando return.
Análise semântica concluída: 3 erros; programa rejeitado.
```

### `31_vetor_tamanho_zero.c` — REJEITADO

Vetores devem ter tamanho positivo.

Diagnósticos esperados:

```text
SEM020 — linha 2, coluna 11: Tamanho do vetor “v” deve ser maior que zero.
Análise semântica concluída: 1 erro; programa rejeitado.
```

### `32_ordem_e_funcao_duplicada.c` — REJEITADO

Os diagnósticos saem na ordem do código-fonte: o erro no corpo da primeira `f` precede a duplicata da segunda `f`.

Diagnósticos esperados:

```text
SEM001 — linha 2, coluna 12: Identificador “ausente” não declarado neste escopo.
SEM002 — linha 4, coluna 5: “f” já declarado neste escopo; declaração anterior na linha 1, coluna 5 (tipo int).
Análise semântica concluída: 2 erros; programa rejeitado.
```

### `33_retorno_em_funcao_void.c` — REJEITADO

Funções `void` não podem retornar valor, nem mesmo o resultado de outra chamada `void`.

Diagnósticos esperados:

```text
SEM009 — linha 5, coluna 12: A função “h” tem tipo void e não pode retornar valor (expressão “g(...)”).
SEM009 — linha 8, coluna 12: A função “k” tem tipo void e não pode retornar valor (expressão “1”).
Análise semântica concluída: 2 erros; programa rejeitado.
```

### `34_programa_completo_aceito.c` — ACEITO

Exercita variável global, `for` com `break`/`continue`, `print`, `read`, comparação de `char`, promoção `int → float`, `%`, atribuição encadeada em vetor, sombreamento em bloco e laços infinitos com retorno.

**Saída/gabarito:** `Análise semântica concluída: 0 erros; programa aceito.`

## Catálogo de códigos usado

- **SEM001** identificador não declarado; **SEM002** declaração duplicada; **SEM003** incompatibilidade de atribuição/conversão.
- **SEM005** condição não booleana; **SEM006** índice de vetor inválido; **SEM007** aridade incorreta; **SEM008** tipo de argumento incompatível.
- **SEM009** retorno incompatível (inclui `return;` em função não `void` e retorno de valor em função `void`); **SEM011** possível queda de função não `void`; **SEM012** chamada `void` usada como valor; **SEM013** destino não atribuível.
- **SEM014** operandos incompatíveis com o operador; **SEM015** indexação de não-vetor; **SEM016** função usada como valor sem chamada; **SEM017** chamada de não-função; **SEM018** `break`/`continue` fora de laço; **SEM019** `return` fora de função; **SEM020** vetor com tamanho não positivo.

Os números mantêm os códigos descritos na apostila quando disponíveis (SEM004 e SEM010 ficam reservados); **SEM013** nomeia explicitamente a regra de valor-L e **SEM014–SEM020** cobrem regras que a suíte original não exercitava. Diagnósticos sintáticos ou lexicais não pertencem a este gabarito.
