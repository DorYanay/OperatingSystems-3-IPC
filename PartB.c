#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/types.h>
#define BUFFER_SIZE 1024

void textfile(char *filename)
{
    int sizeInBytes = 100 * 1024 * 1024;
    FILE *file = fopen("file.txt", "w");
    if (file == NULL)
    {
        printf("Failed to create the file.\n");
        return;
    }

    // 1MB buffer
    const int bufferSize = 1024 * 1024;
    char buffer[bufferSize];
    long remainingBytes = sizeInBytes;

    // Fill the buffer with 'A's
    for (int i = 0; i < bufferSize; i++)
    {
        buffer[i] = 'A';
    }

    // Write the buffer repeatedly until the desired size is reached
    while (remainingBytes > 0)
    {
        long bytesToWrite = (remainingBytes < bufferSize) ? remainingBytes : bufferSize;
        fwrite(buffer, sizeof(char), bytesToWrite, file);
        remainingBytes -= bytesToWrite;
    }

    fclose(file);
    printf("File created successfully.\n");
}
unsigned char calculate_file_checksum(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Failed to open file");
        exit(1);
    }

    unsigned char checksum = 0;
    char buffer;
    while (fread(&buffer, sizeof(char), 1, file) == 1)
    {
        checksum ^= buffer;
    }

    fclose(file);
    return checksum;
}
int send_file_tcp(int socket, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Failed to open file");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0)
    {
        ssize_t bytes_sent = send(socket, buffer, bytes_read, 0);
        if (bytes_sent == -1)
        {
            perror("Failed to send data");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}
void send_file_udp(const char *filename, const char *ip, int port)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // Open file
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1)
    {
        perror("Failed to open file");
        exit(1);
    }

    // Send data in chunks
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0)
    {
        ssize_t total_sent = 0;
        while (total_sent < bytes_read)
        {
            ssize_t sent = sendto(sockfd, buffer + total_sent, bytes_read - total_sent, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            if (sent == -1)
            {
                perror("Failed to send data");
                exit(1);
            }
            total_sent += sent;
        }
    }

    // Close file and socket
    close(file_fd);
    close(sockfd);
}
void performance_test(int socket, const char *type, const char *param, char *ip, int port)
{
    // Generate 100MB of data in a file
    char *filename = file.txt;
    textfile(filename);
    // Calculate checksum
    unsigned char checksum = calculate_checksum(file.txt);

    // Perform the test for different communication types
    struct timeval start, end;
    long elapsed_ms;

    if (strcmp(type, "ipv4") == 0 || strcmp(type, "ipv6") == 0)
    {
        if (strcmp(param, "tcp") == 0)
        {
            gettimeofday(&start, NULL);
            send_file_tcp(socket, filename);
            gettimeofday(&end, NULL);
        }
        else if (strcmp(param, "udp") == 0)
        {
            gettimeofday(&start, NULL);
            send_file_udp(filename, ip, port);
            gettimeofday(&end, NULL)
        }
    }
    elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    printf("%s_%s,%ld\n", type, param, elapsed_ms);
}
void send_file_mmap(const char *filename)
{
    int fd;
    struct stat st;
    char *mapped_file;

    // Open the file
    fd = open(file_path, O_RDONLY);
    if (fd == -1)
    {
        perror("Failed to open file");
        exit(1);
    }

    // Get file size
    if (fstat(fd, &st) == -1)
    {
        perror("Failed to get file size");
        exit(1);
    }

    // Map the file into memory
    mapped_file = mmap(NULL, st.st.size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped_file == MAP_FAILED)
    {
        perror("Failed to map file");
        exit(1);
    }
    // Create socket
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Failed to connect");
        exit(1);
    }

    // Send data
    ssize_t total_sent = 0;
    ssize_t remaining = st.st_size;
    while (remaining > 0)
    {
        ssize_t sent = send(sockfd, mapped_file + total_sent, remaining, 0);
        if (sent == -1)
        {
            perror("Failed to send data");
            exit(1);
        }
        total_sent += sent;
        remaining -= sent;
    }

    // Unmap the file and close the socket
    munmap(mapped_file, st.st_size);
    close(fd);
    close(sockfd);
}
void performance_test_mmap(const char *filename)
{
    int fd;
    struct stat st;
    char *mapped_file;

    // Open the file
    fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Failed to open file");
        exit(1);
    }

    // Get file size
    if (fstat(fd, &st) == -1)
    {
        perror("Failed to get file size");
        exit(1);
    }

    // Map the file into memory
    mapped_file = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped_file == MAP_FAILED)
    {
        perror("Failed to map file");
        exit(1);
    }
    struct timeval start_time, end_time;

    gettimeofday(&start_time, NULL); // Start time
    // Perform the performance test with mapped_file and st.st_size
    send_file_mmap(filename);
    // ...
    gettimeofday(&end_time, NULL); // End time

    // Calculate the time taken in milliseconds
    long time_taken = (end_time.tv_sec - start_time.tv_sec) * 1000L + (end_time.tv_usec - start_time.tv_usec) / 1000;

    printf("pipe,%ld\n", time_taken);
    // Unmap the file and close the file descriptor
    munmap(mapped_file, st.st_size);
    close(fd);
}
void send_file_pipe(const char *file_path)
{
    int fd;
    char buffer[BUFFER_SIZE];

    // Open the named pipe
    fd = open(PIPE_PATH, O_WRONLY);
    if (fd == -1)
    {
        perror("Failed to open named pipe");
        exit(1);
    }

    // Open the file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
    {
        perror("Failed to open file");
        exit(1);
    }

    size_t bytes_read;

    // Send data in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        if (write(fd, buffer, bytes_read) == -1)
        {
            perror("Failed to send data");
            exit(1);
        }
    }

    // Close the file and named pipe
    fclose(file);
    close(fd);
}
void performance_test_named_pipe(const char *filename)
{
    int fd;
    char buffer[BUFFER_SIZE];

    // Open the named pipe
    fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Failed to open named pipe");
        exit(1);
    }
    struct timeval start_time, end_time;

    gettimeofday(&start_time, NULL); // Start time
    // Read data from the named pipe and perform the performance test
    send_file_pipe(file_path);
    // ...
    gettimeofday(&end_time, NULL); // End time

    // Calculate the time taken in milliseconds
    long time_taken = (end_time.tv_sec - start_time.tv_sec) * 1000L + (end_time.tv_usec - start_time.tv_usec) / 1000;

    printf("pipe,%ld\n", time_taken);
    // Close the named pipe
    close(fd);
}
void send_file_uds_stream(const char *file_path)
{
    int sockfd;
    struct sockaddr_un server_addr;

    // Create socket
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Failed to connect");
        exit(1);
    }

    // Open the file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
    {
        perror("Failed to open file");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Send data in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        if (send(sockfd, buffer, bytes_read, 0) == -1)
        {
            perror("Failed to send data");
            exit(1);
        }
    }

    // Close the file and socket
    fclose(file);
    close(sockfd);
}
void send_file_uds_dgram(const char *file_path)
{
    int sockfd;
    struct sockaddr_un server_addr;

    // Create socket
    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Open the file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
    {
        perror("Failed to open file");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Send data in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        if (sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            perror("Failed to send data");
            exit(1);
        }
    }

    // Close the file and socket
    fclose(file);
    close(sockfd);
}
void performance_test_uds(const char *socket_path, const char *param)
{
    int client_socket;
    struct sockaddr_un server_addr;
    int type = (strcmp(param, "dgram") == 0) ? SOCK_DGRAM : SOCK_STREAM;

    // Create socket
    client_socket = socket(AF_UNIX, type, 0);
    if (client_socket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Failed to connect");
        exit(1);
    }

    printf("Connected to UDS server\n");
    struct timeval start_time, end_time;

    gettimeofday(&start_time, NULL); // Start time
    // Perform the performance test
    if (strcmp(param, "dgram") == 0)
    {
        send_file_uds_dgram(file_path); // Implement this function to send data from a file using UDS datagram
    }
    else if (strcmp(param, "stream") == 0)
    {
        send_file_uds_stream(file_path); // Implement this function to send data from a file using UDS stream
    }
    // ...
    gettimeofday(&end_time, NULL); // End time

    // Calculate the time taken in milliseconds
    long time_taken = (end_time.tv_sec - start_time.tv_sec) * 1000L + (end_time.tv_usec - start_time.tv_usec) / 1000;

    printf("uds_%s,%ld\n", param, time_taken);
    // Close socket
    close(client_socket);
}
