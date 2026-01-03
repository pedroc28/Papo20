#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

// --- Thread RECEPTOR ---
// Função que roda em paralelo para ficar ESCUTANDO o servidor
void* receptor(void* arg) {
    int sock = *(int*)arg;
    char mensagem[2048];
    int tamanho;

    // Fica bloqueado esperando dados chegarem do servidor
    while ((tamanho = recv(sock, mensagem, 2048, 0)) > 0) {
        mensagem[tamanho] = '\0'; // Finaliza a string para evitar lixo de memória
        
        // Exibe a mensagem recebida e desenha o prompt novamente
        printf("\n%s\n> ", mensagem);
        
        // Como o printf do prompt não tem \n, o texto pode ficar preso no buffer.
        // O fflush força o terminal a mostrar o "> " imediatamente.
        fflush(stdout); 
    }

    printf("\nConexão encerrada pelo servidor.\n");
    exit(0); // Encerra o programa todo se o servidor cair
}

// --- Thread PRINCIPAL (Sender) ---
int main() {
    int sock;
    char mensagem[1024];  // Buffer para o texto puro digitado
    char nome[50];        // Nome do usuário
    char pacote[2048];    // Buffer final formatado (Nome + Mensagem)
    
    pthread_t thread_carteiro;
    struct sockaddr_in servaddr;

    // 1. Coleta do Nome
    printf("Digite seu nome: ");
    if (fgets(nome, 50, stdin) != NULL) {
        nome[strcspn(nome, "\n")] = 0; // Remove o "Enter" que o fgets captura
    }

    // 2. Criação do Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro na criacao do socket");
        return 1;
    }

    // 3. Configuração do Servidor
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);            // Porta do servidor
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP do servidor (Localhost)

    // 4. Conexão
    if (connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no connect");
        return 1;
    }
    
    // 5. Envia mensagem automática de entrada
    sprintf(pacote, "+++%s entrou no grupo+++", nome);
    send(sock, pacote, strlen(pacote), 0);

    printf("Conectado ao grupo. Pode digitar:\n");

    // 6. Inicia a thread que vai ouvir as respostas
    pthread_create(&thread_carteiro, NULL, receptor, (void*)&sock);

    // 7. Loop de envio (Lê teclado -> Formata -> Envia)
    while (1) {
        printf("[Você]: ");
        fflush(stdout); // Garante que o "> " apareça antes de digitar
        
        // Lê do teclado com proteção contra estouro de buffer
        if (fgets(mensagem, 1024, stdin) != NULL) {
            mensagem[strcspn(mensagem, "\n")] = 0; // Remove o \n

            if(strlen(mensagem) > 0) {
                // Formata a string final: "[Pedro]: Olá tudo bem?"
                sprintf(pacote, "[%s]: %s", nome, mensagem);

                if (send(sock, pacote, strlen(pacote), 0) < 0) {
                    perror("Erro no send");
                    break;
                }
            }
        } else {
            // Se o usuário der Ctrl+D (EOF) ou ocorrer erro
            break;
        }
    }

    close(sock);
    return 0;
}
