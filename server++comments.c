#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

// --- CONFIGURAÇÕES GERAIS ---
#define nmax_membros 5      // Limite máximo de usuários simultâneos
#define nmax_historico 10   // Quantas mensagens antigas ficam salvas na memória

// --- VARIÁVEIS GLOBAIS (Compartilhadas por todas as threads) ---
int membros[nmax_membros];            // Lista de sockets (IDs) dos clientes conectados
int qtd_membros = 0;                  // Contador de pessoas online agora
pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER; // O "Semáforo" para evitar conflitos de dados

// --- BANCO DE DADOS TEMPORÁRIO ---
char historico[nmax_historico][1024]; // Matriz que guarda as últimas 10 mensagens
int qtd_historico = 0;                // Contador de mensagens salvas

// Função 1: Salva a mensagem no histórico do servidor
void adicionar_historico(char* msg) {
    pthread_mutex_lock(&trava); // Bloqueia para ninguém mexer no array ao mesmo tempo
    
    // Cenário A: Ainda tem espaço vazio
    if (qtd_historico < nmax_historico) {
        strcpy(historico[qtd_historico], msg);
        qtd_historico++;
    } 
    // Cenário B: O histórico encheu (FIFO - First In, First Out)
    else {
        // Empurra todas as mensagens uma posição para cima (a mais velha some)
        for (int i = 0; i < nmax_historico - 1; i++) {
            strcpy(historico[i], historico[i+1]);
        }
        // Grava a nova mensagem na última posição livre
        strcpy(historico[nmax_historico-1], msg);
    }
    
    pthread_mutex_unlock(&trava); // Libera o acesso
}

// Função 2: Envia o "pacotão" de histórico para quem acabou de entrar
void transmitir_historico(int sock) {
    char ult_mens[13000] = ""; // Buffer gigante para caber todo o texto acumulado
    
    pthread_mutex_lock(&trava);
    
    if (qtd_historico > 0) {
        // Monta o cabeçalho visual
        strcat(ult_mens, "\n=== HISTORICO RECENTE ===\n");
        
        // Concatena (junta) todas as mensagens salvas numa única string
        for (int i = 0; i < qtd_historico; i++) {
            strcat(ult_mens, historico[i]);
            strcat(ult_mens, "\n"); 
        }
        strcat(ult_mens, "=========================\n");
        
        // Envia tudo de uma vez só para o cliente
        send(sock, ult_mens, strlen(ult_mens), 0);
    }
    
    pthread_mutex_unlock(&trava);
}

// Função 3: Remove um cliente da lista quando ele sai
void remover_membro(int sock) {
    pthread_mutex_lock(&trava);
    for (int i = 0; i < qtd_membros; i++) {
        if (membros[i] == sock) {
            // Reorganiza a lista para não deixar "buracos" (Shift Left)
            for (int j = i; j < qtd_membros - 1; j++) {
                membros[j] = membros[j + 1];
            }
            qtd_membros--; // Atualiza o total de online
            break;
        }
    }
    pthread_mutex_unlock(&trava);
}

// Função 4: Broadcast (Espalha a fofoca para todos, menos para o remetente)
void transmissao(char* msg, int remetente) {
    pthread_mutex_lock(&trava);
    for (int i = 0; i < qtd_membros; i++) {
        if (membros[i] != remetente) { // Garante que eu não receba minha própria mensagem
            if (send(membros[i], msg, strlen(msg), 0) < 0) {
                perror("Erro no envio broadcast");
            }
        }
    }
    pthread_mutex_unlock(&trava);
}

// --- THREAD PRINCIPAL DE CADA CLIENTE ---
void* receptor(void* arg) {
    int sock = *(int*)arg;
    char mensagem[1024];
    int tamanho;

    // PASSO 1: Assim que conecta, entrega o histórico para o novato
    transmitir_historico(sock);

    // PASSO 2: Loop eterno ouvindo o que esse cliente digita
    while ((tamanho = recv(sock, mensagem, 1024, 0)) > 0) {
        mensagem[tamanho] = '\0'; // Finaliza a string para segurança
        
        // Lógica de distribuição:
        adicionar_historico(mensagem); // 1. Guarda no servidor
        transmissao(mensagem, sock);   // 2. Manda para os amigos
    }

    // PASSO 3: Se saiu do loop, o cliente desconectou
    printf("Membro (Socket %d) saiu do grupo\n", sock); // Log local
    remover_membro(sock); // Tira da lista
    close(sock);          // Fecha a conexão
    free(arg);            // Limpa a memória
    return NULL;
}

int main() {
    int servsock, conecsock, *novosock;
    struct sockaddr_in servaddr;

    // 1. Cria o socket principal do servidor
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro na criacao do socket");
        return 1;
    }

    // 2. Configura IP e Porta
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // Aceita qualquer IP
    servaddr.sin_port = htons(8080);       // Porta 8080

    // 3. Liga o socket ao endereço (Bind)
    if (bind(servsock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no bind");
        return 1;
    }

    // 4. Inicia a escuta (Fila de espera de 4)
    listen(servsock, 4);

    printf(">>> SERVIDOR ONLINE (Porta 8080) <<<\n");
    printf(">>> Histórico ativado: %d mensagens <<<\n", nmax_historico);

    // 5. Loop principal: Aceita novos clientes
    while ((conecsock = accept(servsock, NULL, NULL))) {
        pthread_mutex_lock(&trava);
        
        // Verifica se a sala está cheia
        if(qtd_membros < nmax_membros) {
            membros[qtd_membros++] = conecsock; // Adiciona na lista
            pthread_mutex_unlock(&trava);

            // Cria uma thread exclusiva para cuidar deste novo usuário
            pthread_t novathread;
            novosock = malloc(sizeof(int));
            *novosock = conecsock;

            pthread_create(&novathread, NULL, receptor, (void*)novosock);
        } else {
            // Sala cheia: Rejeita a conexão
            pthread_mutex_unlock(&trava);
            printf("Servidor cheio! Rejeitando conexão.\n");
            close(conecsock);
        }
    }

    return 0;
}
