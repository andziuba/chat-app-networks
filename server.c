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
#include <stdbool.h>

#define MESSAGE_SIZE 1024
#define MAX_CLIENTS 10

int clients[MAX_CLIENTS];
char *client_usernames[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] != sender_socket) {
            if (send(clients[i], message, strlen(message), 0) < 0) {
                perror("Failed to send message");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

bool authenticate_client(int client_socket, char *action, char *username, char *password) {
    if (strcmp(action, "register") == 0) {
        return register_user(username, password) == 0; 
    } else if (strcmp(action, "login") == 0) {
        return login_user(username, password) >= 0; 
    }
    return false;
}

void* handle_client(void* client_socket) {
    int socket = *(int*)client_socket;
    char buffer[MESSAGE_SIZE];
    char action[10], username[50], password[50];
    int n;

    // Odbieranie akcji
    bzero(action, sizeof(action));
    n = recv(socket, action, sizeof(action) - 1, 0);
    if (n <= 0) {
        perror("Error receiving action");
        close(socket);
        return NULL;
    }
    action[n] = '\0';
    printf("Action: %s\n", action);
    
    // Odbieranie nazwy użytkownika
    bzero(username, sizeof(username));
    n = recv(socket, username, sizeof(username) - 1, 0);
    if (n <= 0) {
        perror("Error receiving username");
        close(socket);
        return NULL;
    }
    username[n] = '\0';
    printf("Received username: %s\n", username);

    // Odbieranie hasła
    bzero(password, sizeof(password));
    n = recv(socket, password, sizeof(password) - 1, 0);
    if (n <= 0) {
        perror("Error receiving password");
        close(socket);
        return NULL;
    }
    password[n] = '\0';
    printf("Received password: %s\n", password);
    
    // Autoryzacja klienta
    if (authenticate_client(socket, action, username, password)) {
        send(socket, "Authentication successful", 25, 0);

        // Dodaj klienta do listy aktywnych klientów
        pthread_mutex_lock(&clients_mutex);
        clients[client_count] = socket;
        client_usernames[client_count] = strdup(username);
        client_count++;
        pthread_mutex_unlock(&clients_mutex);

        // Komunikacja
        while ((n = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
            buffer[n] = '\0'; 
            printf("Received: %s\n", buffer);
            char message[MESSAGE_SIZE];
            snprintf(message, sizeof(message), "%s: %s", username, buffer);
            broadcast_message(message, socket);
        }

        // Usuwanie klienta po rozłączeniu
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; i++) {
            if (clients[i] == socket) {
                clients[i] = clients[--client_count]; 
                free(client_usernames[i]); 
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        close(socket);
    } else {
        send(socket, "Authentication failed", 21, 0);
        close(socket);
    }
    
    return NULL;
}

int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_storage server_storage;
    socklen_t addr_size;
    pthread_t thread_id;

    // Inicjalizacja bazy danych
    init_db();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(1100);
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 50);

    printf("Listening...\n");

    while (1) {
        addr_size = sizeof server_storage;
        new_socket = accept(server_socket, (struct sockaddr*)&server_storage, &addr_size);
        if (new_socket < 0) {
            perror("Error accepting connection");
            continue;
        }
        pthread_create(&thread_id, NULL, handle_client, &new_socket);
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
