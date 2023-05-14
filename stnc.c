#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netdb.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/un.h>
#include <fcntl.h>
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024
#define MAX_DATA_SIZE 104857600

int quiet = 0;
int ipv4 = 0;
int ipv6 = 0;
int uds = 0;
int udp = 0;
int tcp = 0;
int mmap_file = 0;
int pipe_file = 0;
int stream = 0;
int dgram = 0;
int server_performance = 0;
int client_performance = 0;
// SERVER
unsigned char calculate_checksum_file(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Unable to open file");
        return 1;
    }

    unsigned char checksum = 0;
    unsigned char buffer;
    while (fread(&buffer, 1, 1, file) == 1)
    {
        checksum ^= buffer;
    }

    fclose(file);
    return checksum;
}

int isIPv4(const char *ipAddress)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress, &(sa.sin_addr)) == 1;
}

int isIPv6(const char *ipAddress)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, ipAddress, &(sa.sin6_addr)) == 1;
}
void generate_data(char *data, size_t bufferSize)
{
    // Generate random data to fill the buffer
    for (size_t i = 0; i < bufferSize; ++i)
    {
        data[i] = rand() % 256; // Fill with random byte values (0-255)
    }
}
unsigned char calculate_checksum(char *data, size_t len)
{
    unsigned char checksum = 0;
    for (size_t i = 0; i < len; i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}
unsigned char calculateChecksumData(int fileDescriptor)
{
    unsigned char checksum = 0;
    ssize_t bytesRead;
    char buffer[BUFFER_SIZE];

    // Read data from the file descriptor and update the checksum
    while ((bytesRead = read(fileDescriptor, buffer, BUFFER_SIZE)) > 0)
    {
        for (ssize_t i = 0; i < bytesRead; i++)
        {
            checksum ^= buffer[i];
        }
    }

    if (bytesRead == -1)
    {
        perror("Error reading data");
        exit(1);
    }

    return checksum;
}
void receiveFilePipe(char *fifoName, int quiet)
{
    int fifoDescriptor = open(fifoName, O_RDONLY);
    if (fifoDescriptor == -1)
    {
        perror("Error opening FIFO");
        exit(1);
    }

    // Receive the checksum from the client
    unsigned char clientChecksum;
    ssize_t checksumBytesReceived = read(fifoDescriptor, &clientChecksum, sizeof(clientChecksum));
    if (checksumBytesReceived != sizeof(clientChecksum))
    {
        perror("Error receiving checksum from client");
        exit(1);
    }

    // Calculate the received data checksum
    unsigned char receivedChecksum = calculateChecksumData(fifoDescriptor);

    // Validate the checksum
    if (clientChecksum != receivedChecksum)
    {
        printf("Checksum validation failed. Expected: %d, Received: %d\n", clientChecksum, receivedChecksum);
        exit(1);
    }
    else
    {
        printf("Checksum validation successful. Checksum: %d\n", clientChecksum);
    }

    close(fifoDescriptor);
}
void receiveFileMmap(int clientSocket, char *SHARED_MEMORY_NAME, unsigned char receivedchecksum)
{
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size

    int shm_fd = shm_open(SHARED_MEMORY_NAME, O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(1);
    }

    char *shmPtr = mmap(NULL, dataSize, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shmPtr == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    unsigned char calculatedChecksum = calculate_checksum(shmPtr, dataSize);

    if (receivedchecksum == calculatedChecksum)
    {
        printf("Checksum verification successful. Checksum: %d\n", receivedchecksum);
    }
    else
    {
        printf("Checksum verification failed. Expected: %d, Received: %d\n", receivedchecksum, calculatedChecksum);
    }

    if (munmap(shmPtr, dataSize) == -1)
    {
        perror("munmap");
        exit(1);
    }

    if (shm_unlink(SHARED_MEMORY_NAME) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }

    close(shm_fd);
}
void receiveChunkDataIPV6_UDP(int SERVER_PORT)
{
    int serverSocket;
    struct sockaddr_in6 serverAddr, clientAddr;
    socklen_t clientLen;

    // Create socket
    serverSocket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (serverSocket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_any;
    serverAddr.sin6_port = htons(SERVER_PORT);

    // Bind socket to address and port
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    // Receive the checksum
    unsigned char checksum;
    ssize_t bytesReceived = recvfrom(serverSocket, &checksum, sizeof(checksum), 0,
                                     (struct sockaddr *)&clientAddr, &clientLen);
    if (bytesReceived == -1)
    {
        perror("recvfrom");
        exit(1);
    }

    // Receive the data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }

    ssize_t totalBytesReceived = 0;
    while (totalBytesReceived < dataSize)
    {
        ssize_t bytesRead = recvfrom(serverSocket, data + totalBytesReceived, dataSize - totalBytesReceived, 0,
                                     (struct sockaddr *)&clientAddr, &clientLen);
        if (bytesRead == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        totalBytesReceived += bytesRead;
    }

    // Calculate the received data checksum
    unsigned char receivedChecksum = calculate_checksum(data, dataSize);

    // Verify the received checksum
    if (checksum != receivedChecksum)
    {
        printf("Checksum verification failed. Expected: %d, Received: %d\n", checksum, receivedChecksum);
    }
    else
    {
        printf("Checksum verification successful. Checksum: %d\n", checksum);
    }

    // Clean up
    free(data);
    close(serverSocket);
}
void receiveChunkDataIPV4_UDP(int SERVER_PORT)
{
    int socket_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    // Create socket
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind socket to address and port
    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    // Receive the checksum
    unsigned char checksum;
    ssize_t bytesReceived = recvfrom(socket_fd, &checksum, sizeof(checksum), 0, (struct sockaddr *)&client_addr, &client_len);
    if (bytesReceived == -1)
    {
        perror("recvfrom");
        exit(1);
    }

    // Receive the data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }

    ssize_t totalBytesReceived = 0;
    while (totalBytesReceived < dataSize)
    {
        ssize_t bytesRead = recvfrom(socket_fd, data + totalBytesReceived, dataSize - totalBytesReceived, 0, (struct sockaddr *)&client_addr, &client_len);
        if (bytesRead == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        totalBytesReceived += bytesRead;
    }

    // Calculate the received data checksum
    unsigned char receivedChecksum = calculate_checksum(data, dataSize);

    // Verify the received checksum
    if (checksum != receivedChecksum)
    {
        printf("Checksum verification failed. Expected: %d, Received: %d\n", checksum, receivedChecksum);
    }
    else
    {
        printf("Checksum verification successful. Checksum: %d\n", checksum);

        // Process the received data as needed
        printf("Received Data:\n");
        for (size_t i = 0; i < dataSize; i++)
        {
            printf("%c", data[i]);
        }
        printf("\n");
    }

    // Clean up
    free(data);
    close(socket_fd);
}
void receiveChunkDataIPV6_TCP(int SERVER_PORT)
{
    int serverSocket, clientSocket;
    struct sockaddr_in6 serverAddr, clientAddr;
    socklen_t clientLen;

    // Create socket
    serverSocket = socket(AF_INET6, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_any;
    serverAddr.sin6_port = htons(SERVER_PORT);

    // Bind socket to address and port
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 1) == -1)
    {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    // Accept client connection
    clientLen = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
    if (clientSocket == -1)
    {
        perror("accept");
        exit(1);
    }

    printf("Client connected\n");

    // Receive the checksum
    unsigned char checksum;
    ssize_t bytesReceived = recv(clientSocket, &checksum, sizeof(checksum), 0);
    if (bytesReceived == -1)
    {
        perror("recv");
        exit(1);
    }

    // Receive the data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }

    ssize_t totalBytesReceived = 0;
    while (totalBytesReceived < dataSize)
    {
        ssize_t bytesRead = recv(clientSocket, data + totalBytesReceived, dataSize - totalBytesReceived, 0);
        if (bytesRead == -1)
        {
            perror("recv");
            exit(1);
        }
        totalBytesReceived += bytesRead;
    }

    // Calculate the received data checksum
    unsigned char receivedChecksum = calculate_checksum(data, dataSize);

    // Verify the received checksum
    if (checksum != receivedChecksum)
    {
        printf("Checksum verification failed. Expected: %d, Received: %d\n", checksum, receivedChecksum);
    }
    else
    {
        printf("Checksum verification successful. Checksum: %d\n", checksum);
    }

    // Clean up
    free(data);
    close(clientSocket);
    close(serverSocket);
}

void receiveChunkDataUDS_DGRAM(int clientSocket)
{
    // Receive the checksum
    unsigned char checksum;
    ssize_t bytesReceived = recv(clientSocket, &checksum, sizeof(checksum), 0);
    if (bytesReceived == -1)
    {
        perror("recv");
        exit(1);
    }

    // Open a new socket for receiving data
    int dataSocket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (dataSocket == -1)
    {
        perror("socket");
        exit(1);
    }

    // Set up server address structure
    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, "uds_socket.sock", sizeof(serverAddr.sun_path) - 1);

    // Bind the data socket to the server address
    if (bind(dataSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    // Receive the data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }

    ssize_t totalBytesReceived = 0;
    while (totalBytesReceived < dataSize)
    {
        ssize_t bytesRead = recv(dataSocket, data + totalBytesReceived, dataSize - totalBytesReceived, 0);
        if (bytesRead == -1)
        {
            perror("recv");
            exit(1);
        }
        totalBytesReceived += bytesRead;
    }

    // Calculate the received data checksum
    unsigned char receivedChecksum = calculate_checksum(data, dataSize);

    // Verify the received checksum
    if (checksum != receivedChecksum)
    {
        printf("Checksum verification failed. Expected: %d, Received: %d\n", checksum, receivedChecksum);
    }
    else
    {
        printf("Checksum verification successful. Checksum: %d\n", checksum);

        // Process the received data as needed
        printf("Received Data:\n");
        for (size_t i = 0; i < dataSize; i++)
        {
            printf("%c", data[i]);
        }
        printf("\n");
    }

    // Clean up
    free(data);
    close(dataSocket);
}
void receiveChunkDataUDS_STREAM(int clientSocket)
{
    // Receive the checksum
    unsigned char checksum;
    ssize_t bytesReceived = recv(clientSocket, &checksum, sizeof(checksum), 0);
    if (bytesReceived == -1)
    {
        perror("recv");
        exit(1);
    }

    // Open a new socket for receiving data
    int dataSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dataSocket == -1)
    {
        perror("socket");
        exit(1);
    }

    // Set up server address structure
    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, "uds_socket.sock", sizeof(serverAddr.sun_path) - 1);

    // Connect the data socket to the server address
    if (connect(dataSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("connect");
        exit(1);
    }

    // Receive the data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }

    ssize_t totalBytesReceived = 0;
    while (totalBytesReceived < dataSize)
    {
        ssize_t bytesRead = recv(dataSocket, data + totalBytesReceived, dataSize - totalBytesReceived, 0);
        if (bytesRead == -1)
        {
            perror("recv");
            exit(1);
        }
        totalBytesReceived += bytesRead;
    }

    // Calculate the received data checksum
    unsigned char receivedChecksum = calculate_checksum(data, dataSize);

    // Verify the received checksum
    if (checksum != receivedChecksum)
    {
        printf("Checksum verification failed. Expected: %d, Received: %d\n", checksum, receivedChecksum);
    }
    else
    {
        printf("Checksum verification successful. Checksum: %d\n", checksum);
    }

    // Clean up
    free(data);
    close(dataSocket);
}
void receiveChunkDataIPV4_TCP(int clientSocket)
{
    // Receive the checksum
    unsigned char receivedChecksum;
    ssize_t bytesReceived = recv(clientSocket, &receivedChecksum, sizeof(receivedChecksum), 0);
    if (bytesReceived == -1)
    {
        perror("recv");
        exit(1);
    }
    else if (bytesReceived == 0)
    {
        printf("Connection closed by the client.\n");
        return;
    }

    // Receive the data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }

    ssize_t totalBytesReceived = 0;
    while (totalBytesReceived < dataSize)
    {
        bytesReceived = recv(clientSocket, data + totalBytesReceived, dataSize - totalBytesReceived, 0);
        if (bytesReceived == -1)
        {
            perror("recv");
            exit(1);
        }
        else if (bytesReceived == 0)
        {
            printf("Connection closed by the client.\n");
            free(data);
            return;
        }
        totalBytesReceived += bytesReceived;
    }

    printf("Data received successfully.\n");

    // Calculate the checksum of received data
    unsigned char calculatedChecksum = calculate_checksum(data, dataSize);
    printf("Received Checksum: %d\n", receivedChecksum);
    printf("Calculated Checksum: %d\n", calculatedChecksum);

    // Compare the received checksum with the calculated checksum
    if (receivedChecksum != calculatedChecksum)
    {
        printf("Checksum mismatch. Data may be corrupted.\n");
    }
    else
    {
        printf("Checksum match. Data is intact.\n");
    }

    // Clean up
    free(data);
}

// CLIENT
int getFilesize(char *filename)
{
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_size;
    return -1;
}
void close_file(int fd)
{
    if (close(fd) < 0)
    {
        perror("ERROR closing file");
        exit(1);
    }
}

void unmap_memory(char *ptr, int size)
{
    if (munmap(ptr, size) < 0)
    {
        perror("ERROR unmapping shared memory");
        exit(1);
    }
}
void create_file(char *filename)
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
void sendFileMmap(char *filename, char *sharedMemoryName)
{
    int sourceFileDescriptor = open(filename, O_RDONLY);
    if (sourceFileDescriptor < 0)
    {
        perror("ERROR opening source file");
        exit(1);
    }

    int sourceFileSize = getFilesize(filename);

    int sharedMemoryDescriptor = shm_open(sharedMemoryName, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (sharedMemoryDescriptor < 0)
    {
        close_file(sourceFileDescriptor);
        perror("ERROR opening shared memory");
        exit(1);
    }

    if (ftruncate(sharedMemoryDescriptor, sourceFileSize) < 0)
    {
        close_file(sourceFileDescriptor);
        close_file(sharedMemoryDescriptor);
        perror("ERROR truncating shared memory");
        exit(1);
    }

    char *sharedMemoryPointer = mmap(NULL, sourceFileSize, PROT_WRITE, MAP_SHARED, sharedMemoryDescriptor, 0);
    if (sharedMemoryPointer == MAP_FAILED)
    {
        close_file(sourceFileDescriptor);
        close_file(sharedMemoryDescriptor);
        perror("ERROR mapping shared memory");
        exit(1);
    }

    char *sourceFilePointer = mmap(NULL, sourceFileSize, PROT_READ, MAP_SHARED, sourceFileDescriptor, 0);
    if (sourceFilePointer == MAP_FAILED)
    {
        unmap_memory(sharedMemoryPointer, sourceFileSize);
        close_file(sourceFileDescriptor);
        close_file(sharedMemoryDescriptor);
        perror("ERROR mapping source file");
        exit(1);
    }

    memcpy(sharedMemoryPointer, sourceFilePointer, sourceFileSize);

    unmap_memory(sharedMemoryPointer, sourceFileSize);
    unmap_memory(sourceFilePointer, sourceFileSize);
    close_file(sourceFileDescriptor);
    close_file(sharedMemoryDescriptor);
}
void sendFilePipe(const char *filename, const char *fifoPath)
{
    char readWriteBuffer[BUFFER_SIZE] = {0};
    int sourceFileDescriptor = open(filename, O_RDONLY);
    if (sourceFileDescriptor < 0)
    {
        perror("ERROR opening source file");
        exit(1);
    }

    // Create fifo
    if (mkfifo(fifoPath, 0666) < 0)
    {
        close(sourceFileDescriptor);
        perror("ERROR creating fifo");
        exit(1);
    }

    int fifoDescriptor = open(fifoPath, O_WRONLY);
    if (fifoDescriptor < 0)
    {
        close(sourceFileDescriptor);
        perror("ERROR opening fifo");
        exit(1);
    }

    // Calculate XOR checksum for the file
    unsigned char checksum = calculate_checksum_file(filename);

    // Write the checksum to the fifo
    ssize_t bytesWritten = write(fifoDescriptor, &checksum, sizeof(checksum));
    if (bytesWritten != sizeof(checksum))
    {
        close(sourceFileDescriptor);
        close(fifoDescriptor);
        perror("ERROR writing checksum to fifo");
        exit(1);
    }
    sleep(1);
    ssize_t bytesRead, totalBytesWritten = 0;
    while ((bytesRead = read(sourceFileDescriptor, readWriteBuffer, BUFFER_SIZE)) > 0)
    {
        bytesWritten = write(fifoDescriptor, readWriteBuffer, bytesRead);
        if (bytesWritten != bytesRead)
        {
            close(sourceFileDescriptor);
            close(fifoDescriptor);
            perror("ERROR writing to fifo");
            exit(1);
        }
        totalBytesWritten += bytesWritten;
    }

    if (bytesRead < 0)
    {
        close(sourceFileDescriptor);
        close(fifoDescriptor);
        perror("ERROR reading from file");
        exit(1);
    }

    printf("File sent successfully. Total bytes written: %zd\n", totalBytesWritten);

    close(sourceFileDescriptor);
    close(fifoDescriptor);
}
int delete_file(const char *filename)
{
    if (remove(filename) == 0)
    {
        printf("File '%s' deleted successfully.\n", filename);
        return 0;
    }
    else
    {
        perror("Failed to delete file");
        return 1;
    }
    return 0;
}
void sendChunkDataIPV6(const char *serverIP, int serverPort, int isTCP)
{
    int sockfd;
    struct sockaddr_in6 serverAddr;

    // Create a socket
    int protocol = (isTCP) ? SOCK_STREAM : SOCK_DGRAM;
    if ((sockfd = socket(AF_INET6, protocol, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    // Set up server address structure
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_port = htons(serverPort);
    if (inet_pton(AF_INET6, serverIP, &(serverAddr.sin6_addr)) <= 0)
    {
        perror("inet_pton");
        exit(1);
    }

    // Connect (for TCP) or sendto (for UDP)
    if (isTCP)
    {
        // Connect to the server (TCP)
        if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
        {
            perror("connect");
            exit(1);
        }
    }

    // Generate the chunk data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }
    generate_data(data, dataSize);
    unsigned char checksum = calculate_checksum(data, dataSize);
    fprintf(stdout, "checksum: %d\n", checksum);
    // Send the data
    ssize_t bytesSent;
    if (isTCP)
    {
        bytesSent = send(sockfd, &checksum, sizeof(checksum), 0);
    }
    else
    {
        bytesSent = sendto(sockfd, &checksum, sizeof(checksum), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    }
    if (bytesSent == -1)
    {
        perror((isTCP) ? "send" : "sendto");
        exit(1);
    }
    sleep(1);

    // Send the data
    if (isTCP)
    {
        bytesSent = send(sockfd, data, dataSize, 0);
    }
    else
    {
        bytesSent = sendto(sockfd, data, dataSize, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    }
    if (bytesSent == -1)
    {
        perror((isTCP) ? "send" : "sendto");
        exit(1);
    }
    printf("Data sent successfully.\n");

    // Close the socket
    close(sockfd);
    free(data);
}

void sendChunkDataIPv4(const char *serverIP, int serverPort, int isTCP)
{
    int sockfd;
    struct sockaddr_in serverAddr;

    // Create a socket
    int protocol = (isTCP) ? SOCK_STREAM : SOCK_DGRAM;
    if ((sockfd = socket(AF_INET, protocol, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    // Set up server address structure
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP, &(serverAddr.sin_addr)) <= 0)
    {
        perror("inet_pton");
        exit(1);
    }

    // Connect (for TCP) or sendto (for UDP)
    if (isTCP)
    {
        // Connect to the server (TCP)
        if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
        {
            perror("connect");
            exit(1);
        }
    }

    // Generate the chunk data
    size_t dataSize = 100 * 1024 * 1024; // 100MB data size
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("malloc");
        exit(1);
    }
    generate_data(data, dataSize);
    unsigned char checksum = calculate_checksum(data, dataSize);
    fprintf(stdout, "checksum: %d\n", checksum);

    // Send the checksum and data using sendto for both TCP and UDP
    ssize_t bytesSent = sendto(sockfd, &checksum, sizeof(checksum), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bytesSent == -1)
    {
        perror("sendto");
        exit(1);
    }
    sleep(1);

    bytesSent = sendto(sockfd, data, dataSize, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bytesSent == -1)
    {
        perror("sendto");
        exit(1);
    }

    printf("Data sent successfully.\n");

    // Close the socket
    close(sockfd);
    free(data);
}
void sendChunkDataUDSDgram(const char *socketPath)
{
    int socket_fd;
    struct sockaddr_un server_addr;

    // Create socket
    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socketPath, sizeof(server_addr.sun_path) - 1);

    // Generate data
    int dataSize = 100 * 1024 * 1024;
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(1);
    }
    generate_data(data, dataSize);

    // Send the checksum
    unsigned char checksum = calculate_checksum(data, dataSize);
    ssize_t bytesSent = sendto(socket_fd, &checksum, sizeof(checksum), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytesSent == -1)
    {
        perror("sendto");
        exit(1);
    }
    sleep(1);

    // Send chunk data
    size_t totalSent = 0;
    while (totalSent < dataSize)
    {
        size_t remaining = dataSize - totalSent;
        size_t chunkSize = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;

        bytesSent = sendto(socket_fd, data + totalSent, chunkSize, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (bytesSent == -1)
        {
            perror("sendto");
            exit(1);
        }

        totalSent += bytesSent;
    }

    free(data);
    close(socket_fd);
}
void sendChunkDataUDSStream(const char *socketPath)
{
    int socket_fd;
    struct sockaddr_un server_addr;

    // Create socket
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socketPath, sizeof(server_addr.sun_path) - 1);

    // Generate data
    int dataSize = 100 * 1024 * 1024;
    char *data = (char *)malloc(dataSize);
    if (data == NULL)
    {
        perror("Failed to allocate memory");
        exit(1);
    }
    generate_data(data, dataSize);

    // Calculate checksum
    unsigned char checksum = calculate_checksum(data, dataSize);
    printf("Checksum: %d\n", checksum);

    // Connect to the server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Failed to connect");
        exit(1);
    }

    // Send checksum and chunk data
    ssize_t bytesSent = send(socket_fd, &checksum, sizeof(checksum), 0);
    if (bytesSent == -1)
    {
        perror("send");
        exit(1);
    }
    sleep(1);

    size_t totalSent = 0;
    while (totalSent < dataSize)
    {
        size_t remaining = dataSize - totalSent;
        size_t chunkSize = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;

        bytesSent = send(socket_fd, data + totalSent, chunkSize, 0);
        if (bytesSent == -1)
        {
            perror("send");
            exit(1);
        }

        totalSent += bytesSent;
    }

    free(data);
    close(socket_fd);
}
// CHAT SERVER
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
    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));

    fds[0].fd = client_socket;
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    char input[BUFFER_SIZE];

    while (1)
    {
        int pollResult = poll(fds, 2, -1);
        if (pollResult == -1)
        {
            perror("poll");
            exit(1);
        }

        // Handle input from client socket
        if (fds[0].revents & POLLIN)
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
        if (fds[1].revents & POLLIN)
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

void start_client(const char *ip, int port, char *filename)
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
    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));

    fds[0].fd = client_socket;
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    char input[BUFFER_SIZE];

    while (1)
    {
        int pollResult = poll(fds, 2, -1);
        if (pollResult == -1)
        {
            perror("poll");
            exit(1);
        }

        // Handle input from server socket
        if (fds[0].revents & POLLIN)
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
        if (fds[1].revents & POLLIN)
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
void run_client_performance(const char *ip, int port, char *filename)
{
    int client_socket;
    if (isIPv4(ip) == 1)
    {
        struct sockaddr_in server_addr;

        // Create socket
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1)
        {
            perror("Failed to create socket");
            exit(1);
        }

        // Set server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(ip);
        server_addr.sin_port = htons(port);
        int rval = inet_pton(AF_INET, ip, &server_addr.sin_addr);
        if (rval <= 0)
        {
            printf("ERROR inet_pton() failed\n");
            exit(1);
        }
        // Connect to the server
        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            perror("Failed to connect");
            exit(1);
        }

        printf("Connected to server\n");
    }
    else if (isIPv6(ip) == 1)
    {
        int client_socket;
        if ((client_socket = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
        {
            perror("socket");
            exit(1);
        }
        // Set up server address structure
        struct sockaddr_in6 server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_port = htons(port);
        if (inet_pton(AF_INET6, ip, &(server_addr.sin6_addr)) <= 0)
        {
            perror("inet_pton");
            exit(1);
        }

        // Connect to the server
        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            perror("connect");
            exit(1);
        }

        printf("Connected to server\n");
    }
    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));

    fds[0].fd = client_socket;
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    char input[BUFFER_SIZE];
    while (1)
    {

        if (tcp == 1 && ipv6 == 1)
        {
            ssize_t bytesSent = send(client_socket, "ipv6_tcp", strlen("ipv6_tcp"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        else if (tcp == 1 && ipv4 == 1)
        {
            ssize_t bytesSent = send(client_socket, "ipv4_tcp", strlen("ipv4_tcp"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        else if (udp == 1 && ipv6 == 1)
        {
            ssize_t bytesSent = send(client_socket, "ipv6_udp", strlen("ipv6_udp"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        else if (udp == 1 && ipv4 == 1)
        {
            ssize_t bytesSent = send(client_socket, "ipv4_udp", strlen("ipv4_udp"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        else if (uds == 1 && dgram == 1)
        {
            ssize_t bytesSent = send(client_socket, "uds_dgram", strlen("uds_dgram"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        else if (uds == 1 && stream == 1)
        {
            ssize_t bytesSent = send(client_socket, "uds_stream", strlen("uds_stream"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        else if (mmap_file == 1)
        {
            ssize_t bytesSent = send(client_socket, "mmap", strlen("mmap"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        else if (pipe_file == 1)
        {
            ssize_t bytesSent = send(client_socket, "pipe", strlen("pipe"), 0);
            if (bytesSent == -1)
            {
                perror("send");
                exit(1);
            }
        }
        sleep(1);
        if (tcp == 1 && ipv6 == 1)
        {
            sendChunkDataIPV6(ip, port, 1);
        }
        else if (udp == 1 && ipv6 == 1)
        {
            sendChunkDataIPV6(ip, port, 0);
        }
        else if (tcp == 1 && ipv4 == 1)
        {
            sendChunkDataIPv4(ip, port, 1);
        }
        else if (udp == 1 && ipv4 == 1)
        {
            sendChunkDataIPv4(ip, port, 0);
        }
        else if (uds == 1 && dgram == 1)
        {
            sendChunkDataUDSDgram("uds_socket.sock");
        }
        else if (uds == 1 && stream == 1)
        {
            sendChunkDataUDSStream("uds_socket.sock");
        }
        else if (pipe_file == 1 && filename != NULL)
        {
            sleep(0.1);
            int bytesSent = send(client_socket, filename, strlen(filename), 0);
            if (bytesSent < 0)
            {
                printf("ERROR send() failed\n");
                exit(1);
            }
            char *pipename = "mypipe";
            sendFilePipe(pipename, filename);
        }
        else if (mmap_file == 1 && filename != NULL)
        {
            unsigned char checksum = calculate_checksum_file(filename);
            sleep(0.1);
            int bytesSent1 = send(client_socket, &checksum, sizeof(checksum), 0);
            if (bytesSent1 < 0)
            {
                printf("ERROR send() failed\n");
                exit(1);
            }
            sleep(0.1);
            int bytesSent = send(client_socket, filename, strlen(filename), 0);
            if (bytesSent < 0)
            {
                printf("ERROR send() failed\n");
                exit(1);
            }
            sleep(0.1);
            sendFileMmap(filename, filename);
        }
        int pollResult = poll(fds, 2, -1);
        if (pollResult < 0)
        {
            printf("ERROR poll() failed\n");
            exit(1);
        }
        if (fds[0].revents & POLLIN)
        {
            fgets(input, BUFFER_SIZE, stdin);
            send(client_socket, input, strlen(input), 0);
        }
        if (fds[1].revents & POLLIN)
        {
            int bytes_received = recv(client_socket, input, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
            {
                printf("Server disconnected\n");
                break;
            }

            input[bytes_received] = '\0';
            printf("Received: %s", input);
            fflush(stdout);
        }
        if (filename != NULL && delete_file(filename) == 0)
        {
            perror("Delete file");
        }
    }

    close(client_socket);
}
void run_server_performance(int port, int quietFlag)
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        exit(1);
    }

    // Listen for incoming
    if (listen(server_socket, 1) == -1)
    {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d...\n", port);

    // Create an array of pollfd structures
    struct pollfd fds[2];
    int nfds = 2;

    // Add the server socket to the pollfd array
    fds[0].fd = server_socket;
    fds[0].events = POLLIN;

    while (1)
    {
        // Use poll to wait for incoming events
        int pollResult = poll(fds, nfds, -1);
        if (pollResult == -1)
        {
            perror("poll");
            exit(1);
        }

        // Check if there is a new client connection
        if (fds[0].revents & POLLIN)
        {
            // Accept incoming connection
            client_len = sizeof(client_addr);
            client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
            if (client_socket == -1)
            {
                perror("accept");
                exit(1);
            }

            // Receive client's request
            char request[BUFFER_SIZE];
            ssize_t bytesReceived = recv(client_socket, request, BUFFER_SIZE, 0);
            if (bytesReceived <= 0)
            {
                perror("recv");
                exit(1);
            }

            // Check the client's request
            if (strcmp(request, "ipv4_tcp") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                // Open new socket and get ready to receive the checksum and data
                receiveChunkDataIPV4_TCP(client_socket);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else if (strcmp(request, "ipv4_udp") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                // Open new socket and get ready to receive the checksum and data
                receiveChunkDataIPV4_UDP(port);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else if (strcmp(request, "ipv6_tcp") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                // Open new socket and get ready to receive the checksum and data
                receiveChunkDataIPV6_TCP(port);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else if (strcmp(request, "ipv6_udp") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                // Open new socket and get ready to receive the checksum and data
                receiveChunkDataIPV6_UDP(port);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else if (strcmp(request, "uds_dgram") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                // Open new socket and get ready to receive the checksum and data
                receiveChunkDataUDS_DGRAM(client_socket);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else if (strcmp(request, "uds_stream") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                // Open new socket and get ready to receive the checksum and data
                receiveChunkDataUDS_STREAM(client_socket);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else if (strcmp(request, "pipe") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                char *fifoname;
                ssize_t bytesReceived = recv(client_socket, &fifoname, sizeof(fifoname), 0);
                if (bytesReceived == -1)
                {
                    perror("recv");
                    exit(1);
                }
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                // Open new socket and get ready to receive the checksum and data
                receiveFilePipe(fifoname, quiet);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else if (strcmp(request, "mmap") == 0)
            {
                if (quiet == 0)
                {
                    printf("Received request: %s\n", request);
                }
                unsigned char receivedchecksum;
                ssize_t bytesReceived = recv(client_socket, &receivedchecksum, sizeof(receivedchecksum), 0);
                if (bytesReceived == -1)
                {
                    perror("recv");
                    exit(1);
                }
                sleep(0.1);
                struct timeval start, end;
                gettimeofday(&start, NULL); // Record the starting time
                char *SharedMemoryName;
                ssize_t bytesReceived1 = recv(client_socket, &SharedMemoryName, sizeof(SharedMemoryName), 0);
                if (bytesReceived1 == -1)
                {
                    perror("recv");
                    exit(1);
                }
                // Open new socket and get ready to receive the checksum and data
                receiveFileMmap(client_socket, SharedMemoryName, receivedchecksum);
                gettimeofday(&end, NULL); // Record the ending time

                long seconds = end.tv_sec - start.tv_sec;
                long microseconds = end.tv_usec - start.tv_usec;
                double elapsed_time = (seconds * 1000.0) + (microseconds / 1000.0); // Calculate elapsed time in milliseconds

                printf("Elapsed time: %f\n", elapsed_time);
            }
            else
            {
                printf("Unknown request: %s\n", request);
            }

            // Close the client socket
            close(client_socket);
        }
    }

    // Close the server socket
    close(server_socket);
}

int main(int argc, char *argv[])
{

    if (argc < 3 || argc > 7)
    {
        printf("Usage:\n");
        printf("Server side: ./stnc -s PORT || Server side: ./stnc -s PORT [-p] [-q](optional)\n");
        printf("Client side: ./stnc -c IP PORT || Client side: ./stnc -c IP PORT -p <type> <param>\n");
        return 1;
    }
    // Check if running in server mode or client mode
    if (strcmp(argv[1], "-s") == 0)
    {
        if (argc == 3)
        {
            int port = atoi(argv[2]);
            start_server(port);
        }
        int port = atoi(argv[2]);
        if (strcmp(argv[3], "-p") == 0)
        {
            server_performance = 1;
            if (strcmp(argv[4], "-q") == 0)
            {
                quiet = 1;
            }
            run_server_performance(port, quiet);
        }
    }
    else if (strcmp(argv[1], "-c") == 0)
    {
        if (argc == 4)
        {
            const char *ip = argv[2];
            int port = atoi(argv[3]);
            start_client(ip, port, NULL);
        }
        const char *ip = argv[2];
        int port = atoi(argv[3]);
        if (strcmp(argv[4], "-p") == 0)
        {
            client_performance = 1;
            if (strcmp(argv[5], "ipv6") == 0)
            {
                ipv6 = 1;
                if (strcmp(argv[6], "tcp") == 0)
                {
                    tcp = 1;
                    run_client_performance(ip, port, NULL);
                }
                if (strcmp(argv[6], "udp") == 0)
                {
                    udp = 1;
                    run_client_performance(ip, port, NULL);
                }
            }
            if (strcmp(argv[5], "ipv4") == 0)
            {
                ipv4 = 1;
                if (strcmp(argv[6], "tcp") == 0)
                {
                    tcp = 1;
                    run_client_performance(ip, port, NULL);
                }
                if (strcmp(argv[6], "udp") == 0)
                {
                    udp = 1;
                    run_client_performance(ip, port, NULL);
                }
            }
            if (strcmp(argv[5], "uds") == 0)
            {
                if (strcmp(argv[6], "dgram") == 0)
                {
                    dgram = 1;
                    run_client_performance(ip, port, NULL);
                }
                if (strcmp(argv[6], "stream") == 0)
                {
                    stream = 1;
                    run_client_performance(ip, port, NULL);
                }
            }
            if (strcmp(argv[5], "mmap") == 0)
            {
                mmap_file = 1;
                if (argv[6] != NULL)
                {
                    create_file(argv[6]);
                    run_client_performance(ip, port, argv[6]);
                }
            }
            if (strcmp(argv[5], "pipe") == 0)
            {
                pipe_file = 1;
                if (argv[6] != NULL)
                {
                    create_file(argv[6]);
                    run_client_performance(ip, port, argv[6]);
                }
            }
        }
    }
    else
    {
        printf("Invalid arguments\n");
        return 1;
    }

    return 0;
}