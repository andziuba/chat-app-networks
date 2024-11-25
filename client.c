#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MESSAGE_SIZE 1024

void authenticate(int client_socket) {
    int choice;
    char buffer[50], username[50], password[50];

    // Wybor miedzy rejestracja a logowaniem
    while (1) {
        printf("Enter '1' to register or '2' to login: ");
        scanf("%d", &choice);
        getchar();
    
        if (choice == 1) {
            strcpy(buffer, "register");
            break;  
        } else if (choice == 2) {
            strcpy(buffer, "login");
            break; 
        } else {
            printf("Invalid choice.\n");
        }
    }

    // Wyslanie wyboru (rejestracja/logowanie) do serwera
    send(client_socket, buffer, sizeof(buffer), 0);

    // Podanie hasla i nazwy uzytkownika przez klienta oraz wyslanie ich do serwea
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;
    
    send(client_socket, username, sizeof(username), 0);

    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;
    
    send(client_socket, password, sizeof(password), 0);

    // Odebranie wiadomosci zwrotnej od serwera
    bzero(buffer, sizeof(buffer));
    recv(client_socket, buffer, sizeof(buffer), 0);
    printf("Server: %s\n", buffer);
}

void communicate(int client_socket) {
    char message[MESSAGE_SIZE];
    int n;

    while (1) {
        bzero(message, MESSAGE_SIZE);
        printf("Enter message: ");
        fgets(message, sizeof(message), stdin);

        send(client_socket, message, strlen(message), 0);

        bzero(message, MESSAGE_SIZE);
        n = recv(client_socket, message, sizeof(message), 0);
        if (n > 0) {
            printf("Server: %s\n", message);
        } else {
            break;
        }
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    socklen_t addr_size;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1100);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    addr_size = sizeof server_addr;
    connect(client_socket, (struct sockaddr *)&server_addr, addr_size);

    // Rejestracja lub logowanie
    authenticate(client_socket);

    // Komunikacja z serwerem
    communicate(client_socket);

    close(client_socket);
    return 0;
}
