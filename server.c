#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define nmax_membros 5      // Máximo de clientes
#define nmax_historico 10   // Tamanho do histórico

// Variáveis Globais
int membros[nmax_membros];
int qtd_membros = 0;
pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER; // Mutex para controle de concorrência

// Estrutura do Histórico
char historico[nmax_historico][1024];
int qtd_historico = 0;

// Adiciona mensagem ao histórico (Lógica FIFO)
void adicionar_historico(char* msg) {
    pthread_mutex_lock(&trava);
    
    if (qtd_historico < nmax_historico) {
        // Ainda tem espaço: adiciona no fim
        strcpy(historico[qtd_historico], msg);
        qtd_historico++;
    } else {
        // Cheio: desloca tudo para cima (remove a mais antiga)
        for (int i = 0; i < nmax_historico - 1; i++) {
            strcpy(historico[i], historico[i+1]);
        }
        // Insere na última posição
        strcpy(historico[nmax_historico-1], msg);
    }
    
    pthread_mutex_unlock(&trava);
}

// Envia todo o histórico acumulado para um cliente específico
void transmitir_historico(int sock) {
    char ult_mens[13000] = ""; // Buffer para o histórico completo
    
    pthread_mutex_lock(&trava);
    
    if (qtd_historico > 0) {
        strcat(ult_mens, "\n=== HISTORICO RECENTE ===\n");
        for (int i = 0; i < qtd_historico; i++) {
            strcat(ult_mens, historico[i]);
            strcat(ult_mens, "\n"); 
        }
        strcat(ult_mens, "=========================\n");
        
        send(sock, ult_mens, strlen(ult_mens), 0);
    }
    
    pthread_mutex_unlock(&trava);
}

// Remove socket da lista e reorganiza o array
void remover_membro(int sock) {
    pthread_mutex_lock(&trava);
    for (int i = 0; i < qtd_membros; i++) {
        if (membros[i] == sock) {
            // Shift left para cobrir o buraco
            for (int j = i; j < qtd_membros - 1; j++) {
                membros[j] = membros[j + 1];
            }
            qtd_membros--;
            break;
        }
    }
    pthread_mutex_unlock(&trava);
}

// Broadcast: envia mensagem para todos, exceto remetente
void transmissao(char* msg, int remetente) {
    pthread_mutex_lock(&trava);
    for (int i = 0; i < qtd_membros; i++) {
        if (membros[i] != remetente) {
            if (send(membros[i], msg, strlen(msg), 0) < 0) {
                perror("Erro no send");
            }
        }
    }
    pthread_mutex_unlock(&trava);
}

// Thread para gerenciar cada cliente
void* receptor(void* arg) {
    int sock = *(int*)arg;
    char mensagem[1024];
    int tamanho;

    // Envia histórico assim que conecta
    transmitir_historico(sock);

    // Loop de recebimento de mensagens
    while ((tamanho = recv(sock, mensagem, 1024, 0)) > 0) {
        mensagem[tamanho] = '\0';
        
        adicionar_historico(mensagem); // Salva no histórico
        transmissao(mensagem, sock);   // Encaminha para os outros
    }

    // Cliente desconectou
    printf("Membro %d saiu do grupo\n", sock);
    remover_membro(sock);
    close(sock);
    free(arg);
    return NULL;
}

int main() {
    int servsock, conecsock, *novosock;
    struct sockaddr_in servaddr;

    // Criação do socket
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro na criacao do socket");
        return 1;
    }

    // Configuração do endereço
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(8080);

    // Bind
    if (bind(servsock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no bind");
        return 1;
    }

    // Listen
    listen(servsock, 4);

    printf("Servidor aberto (porta 8080)\n");

    // Loop de aceitação de conexões
    while ((conecsock = accept(servsock, NULL, NULL))) {
        pthread_mutex_lock(&trava);
        
        if(qtd_membros < nmax_membros) {
            membros[qtd_membros++] = conecsock;
            pthread_mutex_unlock(&trava);

            // Cria thread para o novo cliente
            pthread_t novathread;
            novosock = malloc(sizeof(int));
            *novosock = conecsock;

            pthread_create(&novathread, NULL, receptor, (void*)novosock);
        } else {
            // Servidor cheio
            pthread_mutex_unlock(&trava);
            printf("Servidor cheio! Rejeitando conexão.\n");
            close(conecsock);
        }
    }

    return 0;
}
