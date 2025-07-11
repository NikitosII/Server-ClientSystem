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
#define QUEUE_SIZE 100

// Структура для хранения запроса клиента 
typedef struct {
    int num;                      // Номер запроса
    struct sockaddr_in client_addr; // Адрес клиента
    socklen_t addr_len;           // Длина структуры адреса
} Request;

// Структура очереди запросов 
typedef struct {
    Request requests[QUEUE_SIZE]; // Массив запросов
    int front;                   // Индекс начала очереди
    int rear;                    // Индекс конца очереди
    int count;                   // Текущее количество запросов
    pthread_mutex_t mutex;       // Мьютекс для синхронизации доступа
} RequestQueue;

RequestQueue queue;             
volatile int receive_flag = 0;   
volatile int process_flag = 0;   

// Инициализация очереди запросов 
void init_queue() {
    queue.front = 0;
    queue.rear = -1;
    queue.count = 0;
    pthread_mutex_init(&queue.mutex, NULL);
}

// Добавление запроса в очередь 
void enqueue(Request req) {
    pthread_mutex_lock(&queue.mutex);
    if (queue.count < QUEUE_SIZE) {
        queue.rear = (queue.rear + 1) % QUEUE_SIZE;
        queue.requests[queue.rear] = req;
        queue.count++;
    }
    pthread_mutex_unlock(&queue.mutex);
}

// Извлечение запроса из очереди 
Request dequeue() {
    Request req = {0};
    pthread_mutex_lock(&queue.mutex);
    if (queue.count > 0) {
        req = queue.requests[queue.front];
        queue.front = (queue.front + 1) % QUEUE_SIZE;
        queue.count--;
    }
    pthread_mutex_unlock(&queue.mutex);
    return req;
}

// Поток приема запросов от клиентов 
void* receive_requests(void* arg) {
    int sock = *(int*)arg;       
    char buffer[BUFFER_SIZE];
    
    while (!receive_flag) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        // принимаем сообщение от клиента 
        ssize_t recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        
        if (recv_len > 0) {
            int num = atoi(buffer);
            printf("received request %d from client %s:%d\n", num, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            Request req = {num, client_addr, addr_len};
            enqueue(req);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
    }
    return NULL;
}

// Поток обработки запросов и отправки ответов 
void* process_requests(void* arg) {
    int sock = *(int*)arg;

    while (!process_flag) {
        Request req = dequeue();
        if (req.num > 0) {
            long host_id = gethostid();
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "%d:%ld", req.num, host_id);

            int rv = sendto(sock, response, strlen(response)+1, 0, (struct sockaddr*)&req.client_addr, req.addr_len);
            if (rv == -1) {
                perror("sendto");
                sleep(1);
            } else {
                printf("sent response %s to client %s:%d\n", response, inet_ntoa(req.client_addr.sin_addr), ntohs(req.client_addr.sin_port));
            }
        } else {
            usleep(100000);
        }
    }
    return NULL;
}
int main() {
    int sock;
    struct sockaddr_in server_addr;
    pthread_t receive_thread, process_thread;
    
    // Создаем UDP сокет 
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    // устанавливаем неблокирующий режим 
    fcntl(sock, F_SETFL, O_NONBLOCK);
    
    // адрес сервера 
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(8080);       

    // Привязываем сокет к адресу
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on port %d\n", ntohs(server_addr.sin_port));
    
    init_queue();
    
    pthread_create(&receive_thread, NULL, receive_requests, &sock);
    pthread_create(&process_thread, NULL, process_requests, &sock);
    
    printf("Press Enter to stop...\n");
    getchar();
    
    // Останавливаем потоки 
    receive_flag = 1;
    process_flag = 1;
    pthread_join(receive_thread, NULL);
    pthread_join(process_thread, NULL);
    close(sock);
    pthread_mutex_destroy(&queue.mutex);
    return 0;
}