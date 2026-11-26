/*
 * ============================================================================
 *  APLICATIVO DE NAVEGAÇÃO - ROTAS ENTRE CIDADES (GRAFOS EM C)
 * ============================================================================
 *
 *  Representação : lista de adjacência (grafo NÃO direcionado e ponderado)
 *  Vértices      : cidades
 *  Arestas       : estradas entre cidades (peso = distância em km)
 *
 *  Funcionalidades:
 *    1. Menu interativo
 *    2. DFS  - busca em profundidade (recursiva, com backtracking)
 *    3. BFS  - busca em largura (com fila)
 *    4. Caminho percorrido (ordem de visita de cada algoritmo)
 *    5. Dados gerais da busca (origem, visitados, qtd de vértices/arestas,
 *       caminho encontrado e se o objetivo foi encontrado)
 *
 *  Compilar:  gcc -Wall -Wextra -o navegacao navegacao_grafos.c
 *  Executar:  ./navegacao
 *
 *  Convenções:
 *    - No menu as cidades são numeradas de 1 a N (para o usuário);
 *      internamente os índices vão de 0 a N-1.
 *    - Destino = 0 significa "sem destino": percorre todo o grafo
 *      alcançável a partir da origem.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h> /* para SetConsoleOutputCP (acentos no Windows) */
#endif

#define MAX_CIDADES 30
#define MAX_NOME 40

/* ------------------------------------------------------------------------- */
/*  ESTRUTURAS DE DADOS                                                       */
/* ------------------------------------------------------------------------- */

/* Nó da lista de adjacência: representa uma estrada saindo de uma cidade. */
typedef struct No {
    int destino;       /* índice da cidade vizinha */
    int peso;          /* distância em km          */
    struct No *prox;   /* próxima estrada da lista */
} No;

/* Grafo: vetor de nomes + vetor de listas de adjacência. */
typedef struct {
    int n;                              /* quantidade de cidades          */
    char nomes[MAX_CIDADES][MAX_NOME];  /* nome de cada vértice           */
    No *adj[MAX_CIDADES];               /* adj[v] = lista de vizinhos de v*/
} Grafo;

/* Resultado de uma busca (usado por DFS e BFS). */
typedef struct {
    int origem;
    int destino;                /* -1 = sem destino (percorrer tudo)      */
    int ordem[MAX_CIDADES];     /* vértices na ordem em que foram visitados */
    int qtdVisitados;
    int arestas;                /* arestas percorridas (ver README abaixo) */
    int retrocessos;            /* backtracking (só faz sentido no DFS)    */
    int encontrado;             /* 1 se o destino foi encontrado           */
    int caminho[MAX_CIDADES];   /* rota origem -> destino                  */
    int tamCaminho;
    int distancia;              /* soma dos pesos da rota (km)             */
} Resultado;

/*
 * Definição usada para "arestas percorridas":
 *   conta cada aresta atravessada para chegar a um vértice AINDA NÃO
 *   visitado (arestas que levam a vértices já visitados são ignoradas).
 *   No DFS isso inclui arestas de ramos que depois sofreram backtracking.
 */

/* ------------------------------------------------------------------------- */
/*  OPERAÇÕES BÁSICAS DO GRAFO                                                */
/* ------------------------------------------------------------------------- */

void inicializarGrafo(Grafo *g) {
    g->n = 0;
    for (int i = 0; i < MAX_CIDADES; i++) {
        g->adj[i] = NULL;
    }
}

/* Libera toda a memória das listas de adjacência. */
void liberarGrafo(Grafo *g) {
    for (int i = 0; i < g->n; i++) {
        No *p = g->adj[i];
        while (p != NULL) {
            No *prox = p->prox;
            free(p);
            p = prox;
        }
        g->adj[i] = NULL;
    }
    g->n = 0;
}

/* Retorna o índice da cidade com esse nome, ou -1 se não existir. */
int buscarCidade(const Grafo *g, const char *nome) {
    for (int i = 0; i < g->n; i++) {
        if (strcmp(g->nomes[i], nome) == 0) return i;
    }
    return -1;
}

/* Adiciona um vértice. Retorna o índice criado, ou -1 em caso de erro. */
int adicionarCidade(Grafo *g, const char *nome) {
    if (g->n >= MAX_CIDADES) return -1;          /* grafo cheio        */
    if (buscarCidade(g, nome) != -1) return -1;  /* nome repetido      */
    strncpy(g->nomes[g->n], nome, MAX_NOME - 1);
    g->nomes[g->n][MAX_NOME - 1] = '\0';
    g->adj[g->n] = NULL;
    return g->n++;
}

/* Retorna o peso da aresta a-b, ou -1 se ela não existir. */
int pesoAresta(const Grafo *g, int a, int b) {
    for (No *p = g->adj[a]; p != NULL; p = p->prox) {
        if (p->destino == b) return p->peso;
    }
    return -1;
}

/* Insere no FIM da lista (mantém a ordem de cadastro, resultado previsível). */
static void inserirNoFim(Grafo *g, int origem, int destino, int peso) {
    No *novo = (No *)malloc(sizeof(No));
    if (novo == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente.\n");
        exit(1);
    }
    novo->destino = destino;
    novo->peso = peso;
    novo->prox = NULL;

    if (g->adj[origem] == NULL) {
        g->adj[origem] = novo;
    } else {
        No *p = g->adj[origem];
        while (p->prox != NULL) p = p->prox;
        p->prox = novo;
    }
}

/* Adiciona uma rota (aresta) nos dois sentidos. Retorna 1 se deu certo. */
int adicionarRota(Grafo *g, int a, int b, int km) {
    if (a == b || a < 0 || b < 0 || a >= g->n || b >= g->n) return 0;
    if (pesoAresta(g, a, b) != -1) return 0;     /* já existe */
    inserirNoFim(g, a, b, km);
    inserirNoFim(g, b, a, km);                   /* grafo não direcionado */
    return 1;
}

/* ------------------------------------------------------------------------- */
/*  BUSCA EM PROFUNDIDADE (DFS) - RECURSIVA COM BACKTRACKING                  */
/* ------------------------------------------------------------------------- */

/*
 * Visita o vértice v e explora cada vizinho não visitado em profundidade.
 * Retorna 1 se o destino foi encontrado (interrompe a recursão),
 * ou 0 se este ramo não levou ao destino (backtracking).
 *
 * r->caminho funciona como uma PILHA com a rota atual:
 *   - entrou no vértice  -> empilha
 *   - ramo sem saída     -> desempilha (BACKTRACKING)
 */
static int dfsRec(const Grafo *g, int v, int destino,
                  int visitado[], Resultado *r) {
    visitado[v] = 1;
    r->ordem[r->qtdVisitados++] = v;      /* registra ordem de visita */
    r->caminho[r->tamCaminho++] = v;      /* empilha na rota atual    */

    if (v == destino) {                   /* caso base: achou         */
        r->encontrado = 1;
        return 1;
    }

    for (No *p = g->adj[v]; p != NULL; p = p->prox) {
        if (!visitado[p->destino]) {
            r->arestas++;                 /* atravessa a aresta v -> w */
            if (dfsRec(g, p->destino, destino, visitado, r)) {
                return 1;                 /* propaga o sucesso         */
            }
        }
    }

    /* Nenhum vizinho levou ao destino: BACKTRACKING */
    r->tamCaminho--;                      /* desempilha v da rota      */
    r->retrocessos++;
    return 0;
}

void executarDFS(const Grafo *g, int origem, int destino, Resultado *r) {
    int visitado[MAX_CIDADES] = {0};
    memset(r, 0, sizeof(Resultado));
    r->origem = origem;
    r->destino = destino;
    dfsRec(g, origem, destino, visitado, r);

    /* Distância total da rota encontrada */
    r->distancia = 0;
    for (int i = 0; i + 1 < r->tamCaminho; i++) {
        r->distancia += pesoAresta(g, r->caminho[i], r->caminho[i + 1]);
    }
}

/* ------------------------------------------------------------------------- */
/*  BUSCA EM LARGURA (BFS) - COM FILA                                         */
/* ------------------------------------------------------------------------- */

/*
 * Usa uma fila (FIFO) implementada em vetor. Como cada vértice entra na fila
 * no máximo uma vez, MAX_CIDADES posições são suficientes.
 *
 * pai[w] = vértice pelo qual w foi descoberto; permite reconstruir o caminho
 * (que na BFS é o de MENOS ARESTAS, mas não necessariamente o de menos km).
 */
void executarBFS(const Grafo *g, int origem, int destino, Resultado *r) {
    int fila[MAX_CIDADES];
    int ini = 0, fim = 0;               /* índices da fila             */
    int visitado[MAX_CIDADES] = {0};    /* marcado ao ENTRAR na fila   */
    int pai[MAX_CIDADES];
    for (int i = 0; i < MAX_CIDADES; i++) pai[i] = -1;

    memset(r, 0, sizeof(Resultado));
    r->origem = origem;
    r->destino = destino;

    visitado[origem] = 1;
    fila[fim++] = origem;               /* enfileira a origem          */

    while (ini < fim) {                 /* enquanto a fila não estiver vazia */
        int v = fila[ini++];            /* desenfileira                */
        r->ordem[r->qtdVisitados++] = v; /* v é "visitado" ao sair da fila */

        if (v == destino) {
            r->encontrado = 1;
            break;
        }

        for (No *p = g->adj[v]; p != NULL; p = p->prox) {
            int w = p->destino;
            if (!visitado[w]) {
                visitado[w] = 1;
                pai[w] = v;
                r->arestas++;           /* aresta v -> w que descobriu w */
                fila[fim++] = w;        /* enfileira o vizinho         */
            }
        }
    }

    /* Reconstrói a rota de trás para frente usando o vetor pai[] */
    if (r->encontrado) {
        int temp[MAX_CIDADES], tam = 0;
        for (int v = destino; v != -1; v = pai[v]) temp[tam++] = v;
        for (int i = tam - 1; i >= 0; i--) {
            r->caminho[r->tamCaminho++] = temp[i];
        }
        for (int i = 0; i + 1 < r->tamCaminho; i++) {
            r->distancia += pesoAresta(g, r->caminho[i], r->caminho[i + 1]);
        }
    }
}

/* ------------------------------------------------------------------------- */
/*  ENTRADA DE DADOS (segura, com fgets)                                      */
/* ------------------------------------------------------------------------- */

/* Lê um inteiro entre min e max; repete até o usuário digitar algo válido. */
int lerInteiro(const char *msg, int min, int max) {
    char buf[64], extra;
    int valor;
    while (1) {
        printf("%s", msg);
        if (fgets(buf, sizeof(buf), stdin) == NULL) exit(0); /* EOF */
        if (sscanf(buf, "%d %c", &valor, &extra) == 1 &&
            valor >= min && valor <= max) {
            return valor;
        }
        printf("  Entrada invalida. Digite um numero entre %d e %d.\n",
               min, max);
    }
}

/* Lê uma linha de texto e remove o '\n' final. */
void lerTexto(const char *msg, char *dest, int tam) {
    printf("%s", msg);
    if (fgets(dest, tam, stdin) == NULL) exit(0);
    dest[strcspn(dest, "\n")] = '\0';
}

/* ------------------------------------------------------------------------- */
/*  SAÍDA (listagens e relatório da busca)                                    */
/* ------------------------------------------------------------------------- */

void listarCidades(const Grafo *g) {
    printf("\n  Cidades cadastradas:\n");
    for (int i = 0; i < g->n; i++) {
        printf("   %2d - %s\n", i + 1, g->nomes[i]);
    }
}

void listarMapa(const Grafo *g) {
    printf("\n=========== MAPA (cidades e estradas) ===========\n");
    for (int i = 0; i < g->n; i++) {
        printf("%2d. %s\n", i + 1, g->nomes[i]);
        if (g->adj[i] == NULL) printf("      (sem rotas)\n");
        for (No *p = g->adj[i]; p != NULL; p = p->prox) {
            printf("      -> %s (%d km)\n", g->nomes[p->destino], p->peso);
        }
    }
    printf("=================================================\n");
}

/* Imprime uma sequência de vértices no formato  A -> B -> C. */
static void imprimirSequencia(const Grafo *g, const int seq[], int tam) {
    for (int i = 0; i < tam; i++) {
        printf("%s", g->nomes[seq[i]]);
        if (i + 1 < tam) printf(" -> ");
    }
    printf("\n");
}

void exibirResultado(const Grafo *g, const char *titulo, const Resultado *r) {
    printf("\n==================== %s ====================\n", titulo);

    /* --- Dados gerais --- */
    printf(" Vertice de origem        : %s\n", g->nomes[r->origem]);
    if (r->destino >= 0)
        printf(" Objetivo (destino)       : %s\n", g->nomes[r->destino]);
    else
        printf(" Objetivo (destino)       : nenhum (percorrer tudo)\n");
    printf(" Vertices visitados (qtd) : %d\n", r->qtdVisitados);
    printf(" Arestas percorridas (qtd): %d\n", r->arestas);
    if (r->retrocessos > 0 || strncmp(titulo, "DFS", 3) == 0)
        printf(" Retrocessos (backtrack)  : %d\n", r->retrocessos);

    /* --- Ordem de visita --- */
    printf("\n Ordem de visita:\n");
    for (int i = 0; i < r->qtdVisitados; i++) {
        printf("   %2d. %s\n", i + 1, g->nomes[r->ordem[i]]);
    }
    printf("\n Caminho percorrido (visitas): ");
    imprimirSequencia(g, r->ordem, r->qtdVisitados);

    /* --- Resultado do objetivo --- */
    if (r->destino >= 0) {
        if (r->encontrado) {
            printf("\n Objetivo encontrado?     : SIM\n");
            printf(" Rota encontrada          : ");
            imprimirSequencia(g, r->caminho, r->tamCaminho);
            printf(" Arestas da rota          : %d\n", r->tamCaminho - 1);
            printf(" Distancia da rota        : %d km\n", r->distancia);
        } else {
            printf("\n Objetivo encontrado?     : NAO (sem rota entre as cidades)\n");
        }
    } else {
        printf("\n Objetivo encontrado?     : nao se aplica (sem destino)\n");
    }
    printf("=====================================================\n");
}

/* ------------------------------------------------------------------------- */
/*  OPÇÕES DO MENU                                                            */
/* ------------------------------------------------------------------------- */

void menuCadastrarCidade(Grafo *g) {
    char nome[MAX_NOME];
    lerTexto("Nome da nova cidade: ", nome, sizeof(nome));
    if (strlen(nome) == 0) {
        printf("  Nome vazio. Operacao cancelada.\n");
    } else if (adicionarCidade(g, nome) == -1) {
        printf("  Nao foi possivel cadastrar (cidade repetida ou limite de %d atingido).\n",
               MAX_CIDADES);
    } else {
        printf("  Cidade \"%s\" cadastrada.\n", nome);
    }
}

void menuCadastrarRota(Grafo *g) {
    if (g->n < 2) {
        printf("  Cadastre pelo menos 2 cidades antes.\n");
        return;
    }
    listarCidades(g);
    int a = lerInteiro("Cidade A (numero): ", 1, g->n) - 1;
    int b = lerInteiro("Cidade B (numero): ", 1, g->n) - 1;
    int km = lerInteiro("Distancia em km: ", 1, 10000);
    if (adicionarRota(g, a, b, km))
        printf("  Rota %s <-> %s (%d km) cadastrada.\n",
               g->nomes[a], g->nomes[b], km);
    else
        printf("  Nao foi possivel cadastrar (mesma cidade ou rota ja existente).\n");
}

/* Pede origem/destino e executa DFS (tipo = 1) ou BFS (tipo = 2). */
void menuBusca(const Grafo *g, int tipo) {
    if (g->n == 0) {
        printf("  Nenhuma cidade cadastrada.\n");
        return;
    }
    listarCidades(g);
    int origem = lerInteiro("Cidade de origem (numero): ", 1, g->n) - 1;
    int destino = lerInteiro("Cidade de destino (numero, 0 = percorrer tudo): ",
                             0, g->n) - 1;      /* 0 vira -1 = sem destino */

    Resultado r;
    if (tipo == 1) {
        executarDFS(g, origem, destino, &r);
        exibirResultado(g, "DFS - BUSCA EM PROFUNDIDADE", &r);
    } else {
        executarBFS(g, origem, destino, &r);
        exibirResultado(g, "BFS - BUSCA EM LARGURA", &r);
    }
}

/* Carrega um mapa de exemplo (distancias aproximadas, valores didaticos). */
void carregarMapaExemplo(Grafo *g) {
    const char *cidades[] = {
        "Cornélio Procópio", "Londrina", "Apucarana", "Maringá",
        "Jacarezinho", "Paranavaí", "Campo Mourão", "Cascavel",
        "Foz do Iguaçu", "Guarapuava", "Ponta Grossa", "Curitiba"
    };
    int qtd = sizeof(cidades) / sizeof(cidades[0]);
    for (int i = 0; i < qtd; i++) adicionarCidade(g, cidades[i]);

    /* {cidade A, cidade B, km} usando os índices do vetor acima */
    int rotas[][3] = {
        {0, 1,  90}, {0, 4,  95}, {1, 2,  50}, {1, 3, 115},
        {1, 4, 140}, {2, 3,  55}, {2, 10, 240}, {3, 5,  70},
        {3, 6,  90}, {4, 10, 230}, {6, 7, 130}, {6, 9, 180},
        {7, 8, 140}, {7, 9, 245}, {9, 10, 170}, {10, 11, 115}
    };
    int qtdRotas = sizeof(rotas) / sizeof(rotas[0]);
    for (int i = 0; i < qtdRotas; i++) {
        adicionarRota(g, rotas[i][0], rotas[i][1], rotas[i][2]);
    }
}

void exibirMenu(void) {
    printf("\n########## NAVEGACAO - ROTAS ENTRE CIDADES ##########\n");
    printf(" 1 - Listar mapa (cidades e estradas)\n");
    printf(" 2 - Cadastrar cidade\n");
    printf(" 3 - Cadastrar rota\n");
    printf(" 4 - Busca em profundidade (DFS)\n");
    printf(" 5 - Busca em largura (BFS)\n");
    printf(" 0 - Sair\n");
    printf("#####################################################\n");
}

/* ------------------------------------------------------------------------- */
/*  MAIN                                                                      */
/* ------------------------------------------------------------------------- */

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Grafo g;
    inicializarGrafo(&g);
    carregarMapaExemplo(&g);

    int opcao;
    do {
        exibirMenu();
        opcao = lerInteiro("Escolha uma opcao: ", 0, 5);

        switch (opcao) {
            case 1: listarMapa(&g);          break;
            case 2: menuCadastrarCidade(&g); break;
            case 3: menuCadastrarRota(&g);   break;
            case 4: menuBusca(&g, 1);        break;
            case 5: menuBusca(&g, 2);        break;
            case 0: printf("\nEncerrando...\n"); break;
        }
    } while (opcao != 0);

    liberarGrafo(&g);
    return 0;
}
