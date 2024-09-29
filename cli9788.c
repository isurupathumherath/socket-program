#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFSIZE 1024
#define SERVER_IP "127.0.0.1"  // Server's IP address
#define SERVER_PORT 2138       // Port number

// Declare a global variable to store the file name
char receivedFileName[BUFSIZE];

void display_progress(int totalBytes, int fileSize) {
    int progress = (int)((double)totalBytes / fileSize * 100);
    printf("Progress: %d%%\r", progress);
    fflush(stdout);
}

int main() {
    int sockfd;
    struct sockaddr_in servAddr;
    char buffer[BUFSIZE];
    ssize_t bytesRead;
    FILE *file;
    int totalBytes = 0;
    int fileSize = 5000000;  // Example file size (5MB)
    int fileNumber;

    // Create a socket
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("socket() failed");
        exit(1);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(SERVER_PORT);   // Connect to port 2138
    servAddr.sin_addr.s_addr = inet_addr(SERVER_IP);  // Server's IP (127.0.0.1)

    // Output: Connecting to server
    printf("Connecting to server at %s:%d...\n", SERVER_IP, SERVER_PORT);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        perror("connect() failed");
        close(sockfd);
        exit(1);
    }

    // Output: Connected to server
    printf("Connected to the server.\n");

    // Receive and display the list of files from the server
    printf("Available files:\n");
    bytesRead = recv(sockfd, buffer, sizeof(buffer) - 1, 0);  // Use -1 for safety
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';  // Null-terminate the received string
        printf("%s", buffer);
    } else {
        perror("recv() failed");
        close(sockfd);
        exit(1);
    }

    // Ask user to select the file by number
    printf("Enter the file number to download: ");
    scanf("%d", &fileNumber);

    // Send the selected number to the server
    snprintf(buffer, sizeof(buffer), "%d", fileNumber);
    send(sockfd, buffer, strlen(buffer), 0);

    // Step 1: Receive the actual file name first
    bytesRead = recv(sockfd, buffer, sizeof(buffer) - 1, 0);  // Receive file name
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';  // Null-terminate the received string

        // Step 2: Assign the received file name to the global variable
        strncpy(receivedFileName, buffer, sizeof(receivedFileName) - 1);  // Copy safely
        receivedFileName[sizeof(receivedFileName) - 1] = '\0';  // Ensure null termination

        printf("Downloading %s...\n", receivedFileName);

        // Step 3: Open the file using the received file name
        file = fopen(receivedFileName, "wb");
        if (!file) {
            perror("File open failed");
            close(sockfd);
            exit(1);
        }

        // Step 4: Acknowledge the receipt of the file name
        send(sockfd, "ACK", 3, 0);
    } else {
        perror("recv() failed");
        close(sockfd);
        exit(1);
    }

    // Step 5: Receive the file content and write it to the file
    while ((bytesRead = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytesRead, file);
        totalBytes += bytesRead;
        display_progress(totalBytes, fileSize);
    }

    // Output: Download complete
    printf("Download complete. File saved as %s\n", receivedFileName);

    fclose(file);
    close(sockfd);
    return 0;
}

