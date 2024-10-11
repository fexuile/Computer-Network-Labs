#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>

const int MAGIC_NUMBER_LENGTH = 6;

struct message{
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));

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
        recv(client, &msg, sizeof(msg), 0);
        printf("Received message: %s\n", msg.m_protocol);
    }
    return 0;
}