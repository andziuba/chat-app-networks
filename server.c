#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include "db.h"

#define MESSAGE_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    int socket;
    char *username;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Funckja wysylajaca liste aktualnie zalogowanych uzytkownikow do wszystkich klientow
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

// Funckja wysylajaca wiadomosc do okreslonych uzytkownikow
void send_message_to_users(char *username, const char *message, char *recipients_list) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            char full_message[MESSAGE_SIZE];
            snprintf(full_message, sizeof(full_message), "To %s: %s", recipients_list, message);
            send(clients[i].socket, full_message, strlen(full_message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Autentykacja klienta (rejestracja lub logowanie)
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
    return false;
}

// Funkcja obslugujaca klienta
void* handle_client(void* client_socket) {
    int socket = *(int*)client_socket;
    char buffer[MESSAGE_SIZE];
    char action[10], username[50], password[50];
    int n;
    
    // Petla autentykacji
    while (1) {
        bzero(action, sizeof(action));
        n = recv(socket, action, sizeof(action) - 1, 0);
        if (n <= 0) break;
        action[n] = '\0';

        bzero(username, sizeof(username));
        n = recv(socket, username, sizeof(username) - 1, 0);
        if (n <= 0) break;
        username[n] = '\0';

        bzero(password, sizeof(password));
        n = recv(socket, password, sizeof(password) - 1, 0);
        if (n <= 0) break;
        password[n] = '\0';

        if (authenticate_client(socket, action, username, password)) {
            send(socket, "Authentication successful", 25, 0);
            break;
        } else {
            send(socket, "Authentication failed", 21, 0);
        }
    }
    
    // Dodanie uzytkownika do listy zalogowanych po poprawnej autentykacji
    send(socket, "Authentication successful", 25, 0);
    pthread_mutex_lock(&clients_mutex);
    clients[client_count].socket = socket;
    clients[client_count].username = strdup(username);
    client_count++;
    pthread_mutex_unlock(&clients_mutex);

    sleep(1);
    broadcast_user_list();  // rozsylanie listy zalogowanych uzytkownikow

    //petla obslugujacja komunikacje z klientem
    while ((n = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
    buffer[n] = '\0';
    char recipient1[50], recipient2[50], message[MESSAGE_SIZE];
        
    if (strcmp(buffer, "/logout") == 0) {  // odebranie zadania o wylogowanie
        break;
    } else if (strcmp(buffer, "/list") == 0) {  // odebranie zadania o liste zalogowanych uzytkownikow
        broadcast_user_list();
    } else if (strncmp(buffer, "/msg", 4) == 0) {
        
        // Parsowanie wiadomości
        char *token = strtok(buffer + 5, " ");  // pomijamy "/msg " i dzielimy na tokeny
        int recipient_count = 0;
        char recipients[2][50];  // maksymalnie dwoch odbiorcow
        char message[MESSAGE_SIZE] = "";

        while (token != NULL) {
            if (token[0] == '@' && recipient_count < 2) {;
                //nazwa uzytkownika be @
                strncpy(recipients[recipient_count], token + 1, sizeof(recipients[recipient_count]) - 1);
                recipient_count++;
            } else {
                strncat(message, token, sizeof(message) - strlen(message) - 1);
                strncat(message, " ", sizeof(message) - strlen(message) - 1);
            }
            token = strtok(NULL, " ");
        }

        //Usuniecie ostatniej spacji
        if (strlen(message) > 0 && message[strlen(message) - 1] == ' ') {
            message[strlen(message) - 1] = '\0';
        }

        //Tworzenie listy odbiorców
        char recipients_list[MESSAGE_SIZE] = "";
        for (int i = 0; i < recipient_count; i++) {
            strncat(recipients_list, recipients[i], sizeof(recipients_list) - strlen(recipients_list) - 1);
            if (i < recipient_count - 1) {
                strncat(recipients_list, ", ", sizeof(recipients_list) - strlen(recipients_list) - 1);
            }
        }

        char full_message[MESSAGE_SIZE];
        snprintf(full_message, sizeof(full_message), "%s: %s", username, message);

        // Wysyłanie wiadomości do każdego z odbiorców
        for (int i = 0; i < recipient_count; i++) {
            send_message_to_users(recipients[i], full_message, recipients_list);
        }
    }
}
    // Usuniecie uzytkownika z listy zalogowanych po wylogowaniu
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            free(clients[i].username);
            clients[i] = clients[--client_count];
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    // Rozeslanie zaktualizowanej listy zalogowanych uzytkownikow
    broadcast_user_list();
    close(socket);

    send(socket, "Authentication failed", 21, 0);
    close(socket);
    
    return NULL;
}

int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_storage server_storage;
    socklen_t addr_size;
    pthread_t thread_id;

    init_db();  // Inicjalizacja bazy danych

    // Tworzenie gniazda serwera
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(1100);
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 50);

    printf("Listening...\n");

    // Petla akceptujaca nowych klientow
    while (1) {
        addr_size = sizeof server_storage;
        new_socket = accept(server_socket, (struct sockaddr*)&server_storage, &addr_size);
        if (new_socket < 0) {
            perror("Error accepting connection");
            continue;
        }
        pthread_create(&thread_id, NULL, handle_client, &new_socket);  // Tworzenie nowego watku dla klienta
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
