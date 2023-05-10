#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (1)
    {
        // Receive message from the client
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
            break;

        buffer[bytes_received] = '\0';
        printf("Received: %s", buffer);
    }
}

void start_server(int port)
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Failed to bind");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, 1) == -1)
    {
        perror("Failed to listen");
        exit(1);
    }

    printf("Server started. Listening on port %d\n", port);

    // Accept client connection
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket == -1)
    {
        perror("Failed to accept connection");
        exit(1);
    }

    printf("Client connected\n");

    // Handle client communication
    handle_client(client_socket);

    // Close sockets
    close(client_socket);
    close(server_socket);
}

void start_client(const char *ip, int port)
{
    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Failed to connect");
        exit(1);
    }

    printf("Connected to server\n");
    printf("Enter the msg:\n");
    printf(">\n");

    // Read input from keyboard and send to server
    char input[BUFFER_SIZE];
    while (fgets(input, BUFFER_SIZE, stdin) != NULL)
    {
        if (send(client_socket, input, strlen(input), 0) == -1)
        {
            perror("Failed to send message");
            exit(1);
        }
    }

    // Close socket
    close(client_socket);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage:\n");
        printf("Server side: stnc -s PORT\n");
        printf("Client side: stnc -c IP PORT\n");
        return 1;
    }

    // Check if running in server mode or client mode
    if (strcmp(argv[1], "-s") == 0)
    {
        int port = atoi(argv[2]);
        start_server(port);
    }
    else if (strcmp(argv[1], "-c") == 0)
    {
        const char *ip = argv[2];
        int port = atoi(argv[3]);
        start_client(ip, port);
    }
    else
    {
        printf("Invalid arguments\n");
        return 1;
    }

    return 0;
}
