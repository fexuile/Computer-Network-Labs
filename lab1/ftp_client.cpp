#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "helper.h"

char cmd[128], ip[16];
int port, sockid;
bool connected = false;

void open()
{
    if (connected)
    {
        std::cin >> cmd >> cmd;
        printf("Connection already established\n");
        return;
    }
    std::cin >> ip >> port;
    sockid = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    int ret = connect(sockid, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        printf("Connection Failed\n");
        return;
    }
    struct message msg = OPEN_CONN_REQUEST, reply;
    Send(sockid, &msg, 12, 0);
    Recv(sockid, &reply, 12, 0);
    if (memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) == 0 && reply.m_type == OPEN_CONN_REPLY.m_type && reply.m_status == 1) {
        printf("Connection Established\n");
        connected = true;
    }
}

int main() {
    while (1) {
        printf("Client");
        if (connected) printf("(%s:%d)> ", ip, port);
        else printf("(None)> ");
        std::cin >> cmd;
        if (strcmp(cmd, "open") == 0) {
            open();
            continue;
        }
        if (strcmp(cmd, "quit") == 0) {
            if (!connected)
                break;
            struct message msg = QUIT_CONN_REQUEST, reply;
            Send(sockid, &msg, 12, 0);
            Recv(sockid, &reply, 12, 0);
            if (memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) == 0 && reply.m_type == QUIT_CONN_REPLY.m_type) {
                printf("Connection Closed\n");
                close(sockid);
                connected = false;
            }
        }
    }
    return 0;
}