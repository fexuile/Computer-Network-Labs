#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "helper.h"
int port;
char ip[16];

struct Info {
    int client;
    pthread_t thread;
}info[MAX_THREAD_COUNTS];

void* doit(void *args) {
    struct Info *info = (struct Info *)args;
    int client = info->client;
    while(1) {
        struct message msg;
        Recv(client, &msg, 12, 0);
        if (memcmp(msg.m_protocol, protocol, MAGIC_NUMBER_LENGTH) != 0) {
            printf("ERROR: protocol mismatch!\n");
            return NULL;
        }
        if (msg.m_type == 0xA1) {
            struct message reply = OPEN_CONN_REPLY;
            if (memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) != 0)
            {
                printf("ERROR: protocol mismatch!\n");
                return NULL;    
            }
            Send(client, &reply, 12, 0);
        }
        else if (msg.m_type == 0xAD) {
            struct message reply = QUIT_CONN_REPLY;
            Send(client, &reply, 12, 0);
            info->client = -1;
            pthread_exit(NULL);
        }
    }
    return NULL;
}
int main(int argc, char *argv[]) {
    if (argc <= 2) {
        printf("ERROR: missing ip or port number!\n");
        return 0;
    }
    strncpy(ip, argv[1], 16);
    port = atoi(argv[2]);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("ERROR: socket creation failed!\n");
        return 0;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(sockfd, 16);
    for(int i = 0; i < MAX_THREAD_COUNTS; i++)
        info[i].client = -1;
    while (1) {
        int client = accept(sockfd, nullptr, nullptr);
        while (client < 0)
            client = accept(sockfd, nullptr, nullptr);
        int id; 
        for(int i = 0; i < MAX_THREAD_COUNTS; i++)
            if(info[i].client == -1) {
                info[i].client = client;
                id = i;
                break;
            }
        pthread_create(&info[id].thread, NULL, doit, (void *)&info[id]);
        pthread_detach(info[id].thread); 
    }
    return 0;
}