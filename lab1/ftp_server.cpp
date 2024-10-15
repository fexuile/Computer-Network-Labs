#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "helper.h"

int main(int argc, char* argv[]) {
    if(argc <= 2) {
        printf("ERROR: missing ip or port number!\n");
        return 0;
    }
    int port = atoi(argv[2]);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        printf("ERROR: socket creation failed!\n");
        return 0;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sockfd, 128);
    int client = accept(sockfd, nullptr, nullptr);
    if(client < 0) {
        printf("ERROR: accept failed!\n");
        return 0;
    }
    while(1) {
        struct message msg;
        Recv(client, &msg, 12, 0);
        if(memcmp(msg.m_protocol, protocol, MAGIC_NUMBER_LENGTH) != 0) {
            printf("ERROR: protocol mismatch!\n");
            return 0;
        }
        printf("%s %x\n", msg.m_protocol, msg.m_type);
        if(msg.m_type == 0xA1) {
            struct message reply = OPEN_CONN_REPLY;
            if(memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) != 0) {
                printf("ERROR: protocol mismatch!\n");
                return 0;
            }
            puts("OK");
            Send(client, &reply, 12, 0);
        }
        if(msg.m_type == 0xAD) {
            struct message reply = QUIT_CONN_REPLY;
            Send(client, &reply, 12, 0);
        }
    }
    return 0;
}