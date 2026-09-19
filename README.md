# Compilador MiniC (Front-End)

Este repositório apresenta o desenvolvimento do **front-end de um compilador para a linguagem MiniC**. Atualmente, o projeto encontra-se na sua primeira etapa concluída: a implementação do **Analisador Léxico (Scanner)**. 

---

## Pré-requisitos e Dependências

Para rodar o projeto e executar os testes com sucesso, certifique-se de ter as seguintes ferramentas instaladas no seu ambiente (como Linux, macOS ou Git Bash no Windows):

* **GCC (GNU Compiler Collection):** Necessário para compilar a versão em C do scanner.
* **Python 3.10 ou superior:** Necessário para executar a versão em Python do scanner e os scripts de automação (`bash`).

---


## Etapas do Compilador

| Etapa | Status | Descrição |
| :--- | :--- | :--- |
| **Analisador Léxico (Scanner)** | **Concluído** | Conversão do código-fonte em tokens e geração de diagnósticos de erro. |
| **Analisador Sintático (Parser)** | Em breve | Verificação da gramática e construção da Árvore Sintática Abstrata (AST). |
| **Analisador Semântico** | Planejado | Checagem de tipos, escopos e consistência semântica. |
| **Geração de Código Intermediário** | Planejado | Tradução para representação intermediária. |

---

## Estrutura do Projeto

O repositório está organizado em diretórios separados para isolar as implementações e os ambientes de teste:

* **`scanner_c/`** — Contém o código-fonte do scanner em C (`scanner.c`) e o script de automação (`testar_scanner_c.sh`).
* **`scanner_python/`** — Contém o código-fonte do scanner em Python (`scanner.py`) e o script de automação (`testar_scanner_python.sh`).
* **`tests/`** *(presente nas subpastas)* — Armazena os códigos de teste (`.c` e `.minic`) e seus respectivos gabaritos esperados (`.expected.jsonl`).
* **`errors/`** *(gerado automaticamente)* — Recebe os arquivos `.errors.jsonl` contendo os diagnósticos caso ocorram falhas léxicas (como caracteres inválidos ou literais malformados).

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
```

>**Observação:** Caso ocorra algum erro na compilação automática pelo script, compile manualmente o código-fonte executando o comando abaixo antes de rodar os testes:
> ```bash
> gcc -Wall -Wextra -std=c11 scanner.c -o scanner
> ```

### 2. Testando o Scanner em Python
A versão em Python roda diretamente por interpretador, sem necessidade de compilação prévia.
```bash
cd scanner_python/
./testar_scanner_python.sh scanner.py tests/

```
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
