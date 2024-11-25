#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MESSAGE_SIZE 2000

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

    close(client_socket);
    return 0;
}
