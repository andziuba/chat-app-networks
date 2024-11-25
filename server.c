#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define MESSAGE_SIZE 2000

void* socket_thread(void* client_socket) {
    int socket = *(int*)client_socket;
    char buffer[MESSAGE_SIZE];
    int n;

    while ((n = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
        printf("Received: %s\n", buffer);
        send(socket, buffer, n, 0);
    }

    if (n == 0) {
        printf("Client disconnected.\n");
    } else if (n == -1) {
        perror("Read failed");
    }

    close(socket);
    free(client_socket);
    return NULL;
}

int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_storage server_storage;
    socklen_t addr_size;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(1100);

    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));

    listen(server_socket, 50);

    printf("Listening...\n");

    pthread_t thread_id;

    while (1) {
        addr_size = sizeof server_storage;
        new_socket = accept(server_socket, (struct sockaddr*)&server_storage, &addr_size);

        if (pthread_create(&thread_id, NULL, socket_thread, &new_socket) != 0) {
            printf("Thread creation failed");
        }

        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
