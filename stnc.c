#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_CLIENTS 1
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
        printf(">: ");
        fflush(stdout);
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
    if (listen(server_socket, MAX_CLIENTS) == -1)
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
    printf(">: ");
    fflush(stdout);

    // Handle client communication
    fd_set read_fds;
    int max_fd = client_socket;
    char input[BUFFER_SIZE];

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select failed");
            exit(1);
        }

        // Handle input from client socket
        if (FD_ISSET(client_socket, &read_fds))
        {
            int bytes_received = recv(client_socket, input, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
            {
                printf("Client disconnected\n");
                break;
            }

            input[bytes_received] = '\0';
            printf("Received: %s", input);
            printf(">: ");
            fflush(stdout);
        }

        // Handle input from keyboard
        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            fgets(input, BUFFER_SIZE, stdin);
            send(client_socket, input, strlen(input), 0);
            printf(">: ");
            fflush(stdout);
        }
    }

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
    printf(">: ");
    fflush(stdout);

    // Read input from keyboard and send to server
    fd_set read_fds;
    int max_fd = client_socket;
    char input[BUFFER_SIZE];

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select failed");
            exit(1);
        }

        // Handle input from server socket
        if (FD_ISSET(client_socket, &read_fds))
        {
            int bytes_received = recv(client_socket, input, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
            {
                printf("Server disconnected\n");
                break;
            }

            input[bytes_received] = '\0';
            printf("Received: %s", input);
            printf(">: ");
            fflush(stdout);
        }

        // Handle input from keyboard
        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            fgets(input, BUFFER_SIZE, stdin);
            send(client_socket, input, strlen(input), 0);
            printf(">: ");
            fflush(stdout);
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
