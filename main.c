#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_PORT "8080"
#define MAX_BUFFER 1024
#define MAX_TOKENS 100

typedef struct {
    char **tokens;
    int count;
} StringList;

//Splits a string into an array of tokens
StringList splitString(const char *str, const char *delim) {
    StringList result;
    result.tokens = (char **)malloc(MAX_TOKENS * sizeof(char *));
    if (result.tokens == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    result.count = 0;

    // Make a copy of the input string to avoid modifying the original string
    char *str_copy = strdup(str);
    if (str_copy == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    char *saveptr;
    char *token = strtok_r(str_copy, delim, &saveptr);

    while (token != NULL && result.count < MAX_TOKENS) {
        result.tokens[result.count++] = strdup(token);
        token = strtok_r(NULL, delim, &saveptr);
    }

    // Free the copied string as it's no longer needed
    free(str_copy);

    return result;
}

void freeStringList(StringList *result) {
    for (int i = 0; i < result->count; i++) {
        free(result->tokens[i]);
    }
    free(result->tokens);
}

// Check if address is IPV4 or IPV6
void* getInAddr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char const* argv[])
{
    int addrStatus = 0;
    int serverSocket = 0;
    struct sockaddr_storage inboundAddr;
    struct addrinfo aInfo;
    struct addrinfo* results;

    memset(&aInfo, 0, sizeof(aInfo));
    aInfo.ai_family = AF_UNSPEC;
    aInfo.ai_socktype = SOCK_STREAM;
    aInfo.ai_flags = AI_PASSIVE;

    addrStatus = getaddrinfo(NULL, SERVER_PORT, &aInfo, &results);
    if (addrStatus != 0)
    {
        fprintf(stderr, "Failed to get address information\n");
        exit(1);
    }

    // Struct to hold results from addrinfo and go through the linked list without overwriting results
    struct addrinfo* i;
    // Look for free socket to use
    for (i = results; i != NULL; i = i->ai_next)
    {
        serverSocket = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if (serverSocket == -1)
        {
            continue;
        }

        // Setup the socket to reuse local address
        int yes = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            fprintf(stderr, "Failed to set socket options\n");
            exit(1);
        }

        // Bind socket
        if (bind(serverSocket, results->ai_addr, results->ai_addrlen) == -1)
        {
            shutdown(serverSocket, 0);
            fprintf(stderr, "Failed to bind server socket\n");
            continue;
        }
        break;
    }

    freeaddrinfo(results);

    if (i == NULL)
    {
        fprintf(stderr, "Failed to create server socket\n");
        exit(1);
    }

    // Socket for the server to bind to

    if (listen(serverSocket, 10) == -1)
    {
        fprintf(stderr, "Failed to listen on server socket\n");
    }

    printf("Waiting for connection\n");

    while (1)
    {
        // Accept the connection
        socklen_t addr_size = sizeof(inboundAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&inboundAddr, &addr_size);

        if (clientSocket == -1)
        {
            fprintf(stderr, "Failed to accept connection\n");
            exit(1);
            continue;
        }

        // Show who connected
        char addrString[INET6_ADDRSTRLEN];

        inet_ntop(inboundAddr.ss_family, getInAddr((struct sockaddr*)&inboundAddr), addrString, sizeof(addrString));

        printf("Connection Successful with %s\n", addrString);

        int messageSize = 0;
        char buffer[MAX_BUFFER];

        messageSize = recv(clientSocket, buffer, MAX_BUFFER - 1, 0);
        if (messageSize == -1)
        {
            fprintf(stderr, "Did not receive anything\n");
            exit(1);
        }
        buffer[messageSize] = '\0';

        StringList headers = splitString(buffer, "\n");
        StringList request = splitString(headers.tokens[0], " ");
        printf("Request = %s\n", request.tokens[0]);
        if(strcmp(request.tokens[0], "GET") == 0) {
            //Handle GET request. Returning 404 for now.
            char header[MAX_BUFFER] = "HTTP/1.1 404 Not Found\n\n";
            printf("404: File not found\n");
            int sendCode = send(clientSocket, &header, MAX_BUFFER-1, 0);
            if(sendCode == -1) {
                fprintf(stderr, "Failed to send message\n");
                exit(1);
            }
        }
        freeStringList(&headers);
        freeStringList(&request);

        // Exit gracefully
        int closeCode = shutdown(serverSocket, 2);
        int fCode = shutdown(clientSocket, 2);
        break;
    }

    return 0;
}
