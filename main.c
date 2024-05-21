#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define SERVER_PORT "8080"

int main(int argc, char const *argv[])
{
    struct sockaddr_storage inboundAddr;
    struct addrinfo aInfo;
    struct addrinfo *results;

    memset(&aInfo, 0, sizeof(aInfo));
    aInfo.ai_family = AF_UNSPEC;
    aInfo.ai_socktype = SOCK_STREAM;
    aInfo.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, SERVER_PORT, &aInfo, &results);

    //Create a socket for the server to bind to
    int sockfd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    bind(sockfd, results->ai_addr, results->ai_addrlen);
    listen(sockfd, 10);

    printf("Waiting?\n");

    //Accept the connection
    socklen_t addr_size = sizeof(inboundAddr);
    int newfd = accept(sockfd, (struct sockaddr*)&inboundAddr, &addr_size);

    printf("Connection Successful");

    //Exit gracefully
    int closeCode = shutdown(sockfd, 2);
    printf("Hello World\n");
    return 0;
}
