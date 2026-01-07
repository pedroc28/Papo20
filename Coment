#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

// --- THREAD RECEPTOR (FLUXO SECUNDÁRIO) ---
// Implementação baseada em multithreading para evitar o bloqueio de I/O na interface principal.
// Esta thread dedica-se exclusivamente à escuta do socket (recv) para viabilizar a comunicação Full-Duplex.
void* receptor(void* arg) {
    int sock = *(int*)arg;
    char mensagem[2048];
    int tamanho;

    // A chamada recv() permanece bloqueada aguardando datagramas sem interromper o loop de envio no main.
    while ((tamanho = recv(sock, mensagem, 2048, 0)) > 0) {
        
        // Finalização manual da string com caractere nulo para evitar a leitura de lixo de memória.
        mensagem[tamanho] = '\0'; 
        
        // Exibição de mensagens recebidas via broadcast do servidor central.
        printf("\n%s\n", mensagem); 
    }

    // Tratamento de encerramento: finaliza o processo caso o nó central feche a conexão.
    printf("\nConexão encerrada pelo servidor.\n");
    exit(0); 
}

// --- THREAD PRINCIPAL (FLUXO DE ENVIO) ---
int main() {
    int sock;
    char mensagem[1024];  
    char nome[50];        
    char pacote[2048];    
    
    pthread_t thread_carteiro;
    struct sockaddr_in servaddr;

    // Coleta do identificador do usuário e sanitização do input (remoção do caractere de nova linha).
    printf("Digite seu nome: ");
    if (fgets(nome, 50, stdin) != NULL) {
        nome[strcspn(nome, "\n")] = 0; 
    }

    // Criação do socket utilizando a família de protocolos AF_INET (IPv4) e SOCK_STREAM (TCP).
    // O protocolo TCP garante a entrega confiável e a integridade exigida pela camada de aplicação.
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro na criacao do socket");
        return 1;
    }

    // Configuração da infraestrutura de rede: porta padrão 8080 e endereçamento Localhost.
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);            
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 

    // Tentativa de conexão com o servidor seguindo o padrão de sockets POSIX.
    if (connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no connect");
        return 1;
    }
    
    // Envio de mensagem de controle inicial para notificar o ingresso do novo membro.
    sprintf(pacote, "+++%s entrou no grupo+++", nome);
    send(sock, pacote, strlen(pacote), 0);

    printf("Conectado ao grupo. Pode digitar:\n");

    // Instanciação da thread receptora para processar a entrada de dados em paralelo.
    pthread_create(&thread_carteiro, NULL, receptor, (void*)&sock);

    // Loop de Envio: mantêm o terminal responsivo para captura de mensagens do cliente.
    while (1) {
        printf("[Você]: ");
        
        // O uso do fflush(stdout) garante a renderização imediata do prompt no terminal,
        // impedindo que mensagens recebidas via broadcast sobreponham a interface de digitação.
        fflush(stdout); 
        
        // Leitura bloqueante do teclado dedicada exclusivamente ao envio de dados.
        if (fgets(mensagem, 1024, stdin) != NULL) {
            mensagem[strcspn(mensagem, "\n")] = 0; 

            if(strlen(mensagem) > 0) {
                // Formatação do pacote de aplicação antes da transmissão via socket.
                sprintf(pacote, "[%s]: %s", nome, mensagem);

                if (send(sock, pacote, strlen(pacote), 0) < 0) {
                    perror("Erro no send");
                    break;
                }
            }
        } else {
            break;
        }
    }

    // Fechamento do descritor de arquivo e liberação dos recursos de rede.
    close(sock);
    return 0;
}
