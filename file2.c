#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024

volatile int send_flag = 0;    // Флаг остановки потока отправки
volatile int receive_flag = 0; // Флаг остановки потока приема

// Поток отправки запросов на сервер 
void* send_requests(void* arg) {
    int sock = *(int*)arg;
    struct sockaddr_in server_addr;
    int request_num = 1;

    // Настраиваем адрес сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8080);

    while (!send_flag) {
        char request[BUFFER_SIZE];
        snprintf(request, BUFFER_SIZE, "%d", request_num);

        // Отправляем запрос на сервер
        int rv = sendto(sock, request, strlen(request)+1, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (rv == -1) {
            perror("sendto");
            sleep(1);
        } else {
            printf("sent request %d\n", request_num);
            request_num++;
        }
        sleep(1);
    }
    return NULL;
}

// Поток приема ответов от сервера 
void* receive_responses(void* arg) {
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    
    while (!receive_flag) {
        // Принимаем ответ от сервера 
        ssize_t recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, NULL, NULL);
        
        if (recv_len > 0) {
            printf("received response: %s\n", buffer);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
    }
    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in client_addr;
    pthread_t send_thread, receive_thread;
    
    // Создаем UDP сокет 
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    fcntl(sock, F_SETFL, O_NONBLOCK);
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    client_addr.sin_port = 0; 
    
    // Привязываем сокет к адресу 
    if (bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    socklen_t addr_len = sizeof(client_addr);
    getsockname(sock, (struct sockaddr*)&client_addr, &addr_len);
    printf("Client address is %s:%d\n",
          inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    

    pthread_create(&send_thread, NULL, send_requests, &sock);
    pthread_create(&receive_thread, NULL, receive_responses, &sock);
    printf("Press Enter to stop...\n");
    getchar();

    send_flag = 1;
    receive_flag = 1;
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);
    close(sock);
    return 0;
}