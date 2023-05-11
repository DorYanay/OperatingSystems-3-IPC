#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#define BUFFER_SIZE 1024

void start_client(const char *type, const char *ip, int port, const char *param)
{
    if (strcmp(type, "ipv4") == 0 || strcmp(type, "ipv6") == 0)
    {
        int client_socket;
        int is_ipv6 = strcmp(type, "ipv6") == 0;
        struct sockaddr_in server_addr;
        struct sockaddr_in6 server_addr6;

        // Create socket
        if (is_ipv6)
        {
            client_socket = socket(AF_INET6, SOCK_STREAM, 0);
        }
        else
        {
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
        }

        if (client_socket == -1)
        {
            perror("Failed to create socket");
            exit(1);
        }

        // Set server address
        if (is_ipv6)
        {
            server_addr6.sin6_family = AF_INET6;
            inet_pton(AF_INET6, ip, &(server_addr6.sin6_addr));
            server_addr6.sin6_port = htons(port);
        }
        else
        {
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(ip);
            server_addr.sin_port = htons(port);
        }

        // Connect to the server
        int connect_result;
        if (is_ipv6)
        {
            connect_result = connect(client_socket, (struct sockaddr *)&server_addr6, sizeof(server_addr6));
        }
        else
        {
            connect_result = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
        }

        if (connect_result == -1)
        {
            perror("Failed to connect");
            exit(1);
        }

        printf("Connected to server\n");

        // Perform the performance test
        performance_test(client_socket, type, param, ip, port);

        // Close socket
        close(client_socket);
    }
    else if (strcmp(type, "mmap") == 0)
    {
        performance_test_mmap(param);
    }
    else if (strcmp(type, "pipe") == 0)
    {
        performance_test_named_pipe(param);
    }
    else if (strcmp(type, "uds") == 0)
    {
        performance_test_uds(param, ip); // ip parameter is used to pass "dgram" or "stream" for UDS
    }
    else
    {
        printf("Invalid type\n");
    }
}
