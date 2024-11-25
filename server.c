// gcc -o server.out server.c db.c -lsqlite3 -pthread

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "db.h"

#define MESSAGE_SIZE 1024

void authenticate_client(int client_socket) {
    char action[10], username[50], password[50];
    int n;

    bzero(action, sizeof(action));
    recv(socket, action, sizeof(action), 0);
    printf("Action: %s\n", action);

    // Odbieranie nazwy uzytkownika
    bzero(username, sizeof(username));
    n = recv(socket, username, sizeof(username), 0);
    if (n > 0) {
        username[n] = '\0';
        printf("Received username: %s\n", username);
    }

    // Odbieranie hasla
    bzero(password, sizeof(password));
    n = recv(socket, password, sizeof(password), 0);
    if (n > 0) {
        password[n] = '\0';
        printf("Received password: %s\n", password);
    }

    // Przeprowadzenie logowania lub rejestracji na bazie danych
    if (strcmp(action, "register") == 0) {
        int result = register_user(username, password);
        if (result == 0) {
            send(socket, "Registration successful", 24, 0);
        } else {
            send(socket, "Registration failed", 20, 0);
        }
    } else if (strcmp(action, "login") == 0) {
        int result = login_user(username, password);
        if (result >= 0) {
            send(socket, "Login successful", 17, 0);
        } else {
            send(socket, "Login failed", 12, 0);
        }
    }

}

void* socket_thread(void* client_socket) {
    int socket = *(int*)client_socket;
    char buffer[MESSAGE_SIZE];
    int n;

    printf("Client connected.\n");

    authenticate_client(socket);    

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
    return NULL;
}

int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_storage server_storage;
    socklen_t addr_size;

    // Inicjalizacja bazy danych
    init_db();

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
