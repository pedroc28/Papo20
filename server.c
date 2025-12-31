#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define nmax_membros 5 // Define o limite máximo de conexões simultâneas

// Variáveis globais compartilhadas entre as threads
int membros[nmax_membros]; // Array que armazena os sockets dos clientes
int qtd_membros = 0;       // Contador atual de pessoas na sala
pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER; // Mutex: A "chave" para evitar conflitos (Race Conditions)

// Função para remover um cliente da lista quando ele desconecta
void remover_membro(int sock) {
    pthread_mutex_lock(&trava); // Tranca a lista para ninguém mexer enquanto organizamos
    for (int i = 0; i < qtd_membros; i++) {
        if (membros[i] == sock) {
            // Lógica de "Shift Left": Puxa todo mundo um passo para trás para tapar o buraco
            for (int j = i; j < qtd_membros - 1; j++) {
                membros[j] = membros[j + 1];
            }

            qtd_membros--; // Decrementa o contador total
            break;
        }
    }
    pthread_mutex_unlock(&trava); // Destranca a lista
}

// Função de Broadcast: Envia a mensagem para todos, menos para quem enviou
void transmissao(char* msg, int remetente) {
    pthread_mutex_lock(&trava); // Tranca para garantir que a lista de membros não mude durante o envio
    for (int i = 0; i < qtd_membros; i++) {
        if (membros[i] != remetente) { // Não manda a mensagem de volta para quem escreveu
            if (send(membros[i], msg, strlen(msg), 0) < 0) {
                perror("Erro no send");
            }
        }
    }
    pthread_mutex_unlock(&trava); // Libera a lista
}

// Thread dedicada para cada cliente (O "Ouvido" do servidor)
void* receptor(void* arg) {
    int sock = *(int*)arg;
    char mensagem[1024];
    int tamanho;

    // Loop principal: Fica esperando chegar dados (recv é bloqueante)
    while ((tamanho = recv(sock, mensagem, 1024, 0)) > 0) {
        mensagem[tamanho] = '\0'; // Adiciona o terminador de string para não vir lixo de memória
        transmissao(mensagem, sock); // Repassa a mensagem para o grupo
    }

    // --- Se o código chegou aqui, o cliente desconectou (tamanho <= 0) ---

    // Opção de avisar o grupo sobre a saída (Mantido desativado conforme solicitado)
    //sprintf(mensagem, "---Membro %d saiu do grupo---", sock);
    //transmissao(mensagem, sock);

    printf("Membro %d saiu do grupo\n", sock); // Log no servidor
    
    // Limpeza final
    remover_membro(sock); // Tira da lista global
    close(sock);          // Fecha a conexão TCP
    free(arg);            // Libera a memória alocada no malloc do main
    return NULL;
}

int main() {
    int servsock, conecsock, *novosock;
    struct sockaddr_in servaddr;

    // 1. Criação do Socket (IPv4, TCP)
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro na criacao do socket");
        return 1;
    }

    // 2. Configuração do Endereço do Servidor
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer IP da máquina
    servaddr.sin_port = htons(8080);       // Define a porta 8080

    // 3. Bind: Liga o socket à porta 8080
    if (bind(servsock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no bind");
        return 1;
    }

    // 4. Listen: Define a fila de espera (Backlog de 4 pessoas na fila)
    listen(servsock, 4);

    printf("Servidor aberto (porta 8080)\n");

    // 5. Loop infinito para aceitar novas conexões
    while ((conecsock = accept(servsock, NULL, NULL))) {
        pthread_mutex_lock(&trava); // Protege a verificação de lotação
        
        if(qtd_membros < nmax_membros) {
            // Se tem vaga, adiciona na lista
            membros[qtd_membros++] = conecsock;
            pthread_mutex_unlock(&trava);

            // Cria uma nova thread para cuidar desse cliente
            pthread_t novathread;
            novosock = malloc(sizeof(int)); // Aloca memória para passar o ID do socket
            *novosock = conecsock;

            pthread_create(&novathread, NULL, receptor, (void*)novosock);
        } else {
            // Se lotou (5 pessoas), recusa a conexão
            pthread_mutex_unlock(&trava);
            printf("Servidor cheio! Rejeitando conexão.\n");
            close(conecsock); // Fecha na cara do cliente excedente
        }
    }

    return 0;
}
