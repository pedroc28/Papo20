#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

//Número máximo de clientes e número máximo de mensagens salvas no histórico 
#define nmax_membros 5
#define nmax_historico 10

//Recursos globais das threads
int membros[nmax_membros];
int qtd_membros = 0; 
pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER; //Mutex para garantir a exclusão mútua
char historico[nmax_historico][1024];
int qtd_historico = 0;

//Gerenciamento do histórico (lógica First-in First-out)
void adicionar_historico(char* msg) {
    pthread_mutex_lock(&trava); //Bloqueio da região crítica da memória para escrita

    //Lógica de fila circular
    if (qtd_historico < nmax_historico) {
        strcpy(historico[qtd_historico], msg);
        qtd_historico++;
    } else {
        for (int i = 0; i < nmax_historico - 1; i++) {
            strcpy(historico[i], historico[i+1]);
        }
        strcpy(historico[nmax_historico-1], msg);
    }
    
    pthread_mutex_unlock(&trava); //Desbloqueio
}

//Transmissão do histórico para o novo usuário conectado
void transmitir_historico(int sock) {
    char ult_mens[13000] = ""; 
    
    pthread_mutex_lock(&trava); 
    
    if (qtd_historico > 0) {
        strcat(ult_mens, "\n=== HISTORICO RECENTE ===\n");
        for (int i = 0; i < qtd_historico; i++) {
            strcat(ult_mens, historico[i]);
            strcat(ult_mens, "\n"); 
        }
        strcat(ult_mens, "=========================\n");
        
        //Envio das strings concatenadas
        send(sock, ult_mens, strlen(ult_mens), 0);
    }
    
    pthread_mutex_unlock(&trava);
}

//Atualização da lista de membros e reorganização do vetor de sockets ativos
void* remover_membro(int sock) {
    pthread_mutex_lock(&trava);
    for (int i = 0; i < qtd_membros; i++) {
        if (membros[i] == sock) {
            //Reorganização do vetor
            for (int j = i; j < qtd_membros - 1; j++) {
                membros[j] = membros[j + 1];
            }
            qtd_membros--;
            break;
        }
    }
    pthread_mutex_unlock(&trava);
}

//Propagação (broadcast) para todos os participantes, exceto o remetente
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

//THREADS SECUNDÁRIAS: tratamento individual de cada cliente conectado
void* receptor(void* arg) {
    int sock = *(int*)arg;
    char mensagem[1024];
    int tamanho;

    transmitir_historico(sock); //O usuário recebe o histórico antes de entrar no loop de escuta

    //Loop de recebimento
    while ((tamanho = recv(sock, mensagem, 1024, 0)) > 0) {
        mensagem[tamanho] = '\0';
        adicionar_historico(mensagem);
        transmissao(mensagem, sock);
    }

    //Fim da thread e desalocação dos recursos dinâmicos
    printf("Membro %d saiu do grupo\n", sock);
    remover_membro(sock);
    close(sock);
    free(arg); //Liberação da memória alocada na Heap para o descritor do socket
    return NULL;
}

//THREAD PRINCIPAL: configuração da infraestrutura de rede e aceitação de conexões
int main() {
    int servsock, conecsock, *novosock;
    struct sockaddr_in servaddr;

    //Instanciação do socket de entrada (IPv4 e TCP)
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro na criacao do socket");
        return 1;
    }

    //Porta padrão 8080 e aceita conexão de qualquer interface 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(8080);

    if (bind(servsock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no bind");
        return 1;
    }

    listen(servsock, 4);

    printf("Servidor aberto (porta 8080)\n");

    //Loop de aceitação: gerenciamento da concorrência de novos usuários
    while ((conecsock = accept(servsock, NULL, NULL))) {
        pthread_mutex_lock(&trava);
        
        if(qtd_membros < nmax_membros) {
            membros[qtd_membros++] = conecsock;
            pthread_mutex_unlock(&trava);

            pthread_t novathread;
            
            //Alocação na Heap para evitar condição de corrida no descritor do socket
            novosock = malloc(sizeof(int));
            *novosock = conecsock;

            //Criação de thread individual para o cliente 
            pthread_create(&novathread, NULL, receptor, (void*)novosock);
        } else {
            //Rejeição de conexão por limite de capacidade 
            pthread_mutex_unlock(&trava);
            printf("Servidor cheio! Rejeitando conexão.\n");
            close(conecsock);
        }
    }

    return 0;
}
