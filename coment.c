#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

//THREAD RECEPTOR (Fluxo secundário)
void* receptor(void* arg) {
    int sock = *(int*)arg;
    char mensagem[2048];
    int tamanho;

    while ((tamanho = recv(sock, mensagem, 2048, 0)) > 0) {
        mensagem[tamanho] = '\0'; //Finalização manual da string com caractere nulo,
                                  //evitando a leitura excessiva de memória.
        printf("\n%s\n", mensagem); 
    }
    
    //Finaliza o processo caso o nó central feche a conexão.
    printf("\nConexão encerrada pelo servidor.\n");
    exit(0); 
}

//THREAD PRINCIPAL (Fluxo de envio)
int main() {
    int sock;
    char mensagem[1024];  
    char nome[50];        
    char pacote[2048];    
    
    pthread_t thread_carteiro;
    struct sockaddr_in servaddr;

    //Coleta do identificador do usuário e remoção do caractere de nova linha).
    printf("Digite seu nome: ");
    if (fgets(nome, 50, stdin) != NULL) {
        nome[strcspn(nome, "\n")] = 0; 
    }

    //Criação do socket utilizando a família de protocolos AF_INET (IPv4) e SOCK_STREAM (TCP).
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro na criacao do socket");
        return 1;
    }

    //Porta padrão 8080 e endereçamento Localhost.
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);            
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 

    //Tentativa de conexão com o servidor seguindo o padrão de sockets POSIX.
    if (connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erro no connect");
        return 1;
    }
    
    //Notificação de ingresso do novo membro.
    sprintf(pacote, "+++%s entrou no grupo+++", nome);
    send(sock, pacote, strlen(pacote), 0);

    printf("Conectado ao grupo. Pode digitar:\n");

    //Instanciação da thread receptora para processar a entrada de dados em paralelo.
    pthread_create(&thread_carteiro, NULL, receptor, (void*)&sock);

    //Loop de Envio: mantêm o terminal responsivo para captura de mensagens do cliente.
    while (1) {
        printf("[Você]: ");
        fflush(stdout); //Garantia da renderização imediata no terminal,
                        //impedindo que mensagens sobreponham.
        
        //Leitura bloqueante do teclado dedicada exclusivamente ao envio de dados.
        if (fgets(mensagem, 1024, stdin) != NULL) {
            mensagem[strcspn(mensagem, "\n")] = 0; 

            if(strlen(mensagem) > 0) {
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
