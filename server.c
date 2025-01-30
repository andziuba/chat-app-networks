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

typedef struct {
    int socket;
    char *username;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_user_list() {
    char user_list[MESSAGE_SIZE] = "Users online: ";
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        strcat(user_list, clients[i].username);
        if (i < client_count - 1) {
            strcat(user_list, ", ");
        }
    }

    strcat(user_list, "\n");

    for (int i = 0; i < client_count; i++) {
        send(clients[i].socket, user_list, strlen(user_list), 0);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_message(const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != sender_socket) {
            if (send(clients[i].socket, message, strlen(message), 0) < 0) {
                perror("Failed to send message");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message_to_users(char *usernames, const char *message) {
    pthread_mutex_lock(&clients_mutex);
    char *user = strtok(usernames, " ");
    while (user != NULL) {
        for (int i = 0; i < client_count; i++) {
            if (strcmp(clients[i].username, user) == 0) {
                send(clients[i].socket, message, strlen(message), 0);
            }
        }
        user = strtok(NULL, " ");
    }
    pthread_mutex_unlock(&clients_mutex);
}

bool authenticate_client(int client_socket, char *action, char *username, char *password) {
    if (strcmp(action, "register") == 0) {
        return register_user(username, password) == 0; 
    } else if (strcmp(action, "login") == 0) {
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; i++) {
            if (strcmp(clients[i].username, username) == 0) {
                pthread_mutex_unlock(&clients_mutex);
                return false;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        return login_user(username, password) >= 0; 
    }
}

void logout_client() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            free(clients[i].username);
            clients[i] = clients[--client_count];
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* client_socket) {
    int socket = *(int*)client_socket;
    char buffer[MESSAGE_SIZE];
    char action[10], username[50], password[50];
    int n;

    bzero(action, sizeof(action));
    n = recv(socket, action, sizeof(action) - 1, 0);
    if (n <= 0) {
        perror("Error receiving action");
        close(socket);
        return NULL;
    }
    action[n] = '\0';
    
    bzero(username, sizeof(username));
    n = recv(socket, username, sizeof(username) - 1, 0);
    if (n <= 0) {
        perror("Error receiving username");
        close(socket);
        return NULL;
    }
    username[n] = '\0';

    bzero(password, sizeof(password));
    n = recv(socket, password, sizeof(password) - 1, 0);
    if (n <= 0) {
        perror("Error receiving password");
        close(socket);
        return NULL;
    }
    password[n] = '\0';
    
    if (authenticate_client(socket, action, username, password)) {
        send(socket, "Authentication successful", 25, 0);
        pthread_mutex_lock(&clients_mutex);
        clients[client_count].socket = socket;
        clients[client_count].username = strdup(username);
        client_count++;
        pthread_mutex_unlock(&clients_mutex);

        sleep(1);
        broadcast_user_list();

        while ((n = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
            buffer[n] = '\0';
            char recipient1[50], recipient2[50], message[MESSAGE_SIZE];

            printf("Received: '%s'\n", buffer);

            
            if (strcmp(buffer, "/list") == 0) {
                broadcast_user_list();
            } else if (strcmp(buffer, "/logout") == 0) {
                printf("logout");
                logout_client();
                broadcast_user_list();
            } else if (sscanf(buffer, "/msg %s %s %[^\n]", recipient1, recipient2, message) == 3) {
                char full_message[MESSAGE_SIZE];
                snprintf(full_message, sizeof(full_message), "%s: %s", username, message);
                send_message_to_users(recipient1, full_message);
                send_message_to_users(recipient2, full_message);
            } else if (sscanf(buffer, "/msg %s %[^\n]", recipient1, message) == 2) {
                char full_message[MESSAGE_SIZE];
                snprintf(full_message, sizeof(full_message), "%s: %s", username, message);
                send_message_to_users(recipient1, full_message);
            } else {
                char formatted_message[MESSAGE_SIZE];
                snprintf(formatted_message, sizeof(formatted_message), "%s: %s", username, buffer);
                broadcast_message(formatted_message, socket);
            }

        }

        logout_client();

        broadcast_user_list();
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
