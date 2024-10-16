#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "helper.h"

char cmd[128], ip[16];
int port, sockid;
bool connected = false;

void open() {
    if (connected) {
        std::cin >> cmd >> cmd;
        printf("Connection already established\n");
        return;
    }
    std::cin >> ip >> port;
    std::cout << ip <<" "<< port << std::endl;
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
void list() {
    if (!connected) {
        printf("Connection not established\n");
        return;
    }
    struct message msg = LS_CONN_REQUEST, reply;
    Send(sockid, &msg, 12, 0);
    Recv(sockid, &reply, 12, 0);
    static char buffer[MAX_LINE_LENGTH];
    if (memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) == 0 && reply.m_type == LS_CONN_REPLY.m_type) {
        int length = ntohl(reply.m_length) - 12;
        Recv(sockid, &buffer, length, 0);
        for(int i = 0; i < length; i++)
            printf("%c", buffer[i]);
    }
}
void chdir() {
    if(!connected) {
        printf("Connection not established\n");
        return;
    }
    std::cin >> cmd;
    struct message msg = CHANGE_DIR_REQUEST, reply;
    int length = strlen(cmd);
    msg.m_length = htonl(length + 13);
    Send(sockid, &msg, 12, 0);
    Send(sockid, &cmd, length + 1, 0);
    Recv(sockid, &reply, 12, 0);
    if(memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) == 0 && reply.m_type == CHANGE_DIR_REPLY.m_type) {
        if(reply.m_status == 0)
            puts("Directory does not exist.");
        else puts("Directory changed.");
    }
}
void get() {
    if (!connected) {
        printf("Connection not established\n");
        return;
    }
    struct message msg = GET_REQUEST, reply;
    std::cin >> cmd;
    int length = strlen(cmd);
    msg.m_length = htonl(length + 13);
    Send(sockid, &msg, 12, 0);
    Send(sockid, &cmd, length + 1, 0);
    Recv(sockid, &reply, 12, 0);
    if (memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) == 0 && reply.m_type == GET_REPLY.m_type) {
        if(reply.m_status == 0) {
            puts("File does not exist.");
            return;
        }
        FILE* file = fopen(cmd, "wb");
        Recv(sockid, &reply, 12, 0);
        if(memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) || reply.m_type != FILE_DATA.m_type) {
            puts("Error in file transfer.");
            return;
        }
        int filesize = ntohl(reply.m_length) - 12;
        static char file_detail[FILE_MAX_SIZE];
        Recv(sockid, &file_detail, filesize, 0);
        fwrite(file_detail, 1, filesize, file);
        fclose(file);
        fflush(stdout);
    }
}
void put() {
    if(!connected) {
        printf("Connection not established\n");
        return;
    }
    struct message msg = PUT_REQUEST, reply;
    std::cin >> cmd;
    FILE* file = fopen(cmd, "rb");
    if(file == NULL) {
        printf("File does not exist\n");
        return;
    }
    msg.m_length = htonl(strlen(cmd) + 13);
    Send(sockid, &msg, 12, 0);
    Send(sockid, &cmd, strlen(cmd) + 1, 0);
    Recv(sockid, &reply, 12, 0);
    if(memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) || reply.m_type != PUT_REPLY.m_type) {
        puts("Error in file transfer.");
        return;
    }
    static char file_detail[FILE_MAX_SIZE];
    fseek(file, 0, SEEK_END);
    int filesize = ftell(file);
    rewind(file);
    msg = FILE_DATA;
    msg.m_length = htonl(filesize + 12);
    Send(sockid, &msg, 12, 0);
    fread(file_detail, 1, filesize, file);
    Send(sockid, &file_detail, filesize, 0);
    fclose(file);
}
void sha256() {
    if(!connected) {
        printf("Connection not established\n");
        return;
    }
    struct message msg = SHA256_REQUEST, reply;
    std::cin >> cmd;
    int length = strlen(cmd);
    msg.m_length = htonl(length + 13);
    Send(sockid, &msg, 12, 0);
    Send(sockid, &cmd, length + 1, 0);
    Recv(sockid, &reply, 12, 0);
    if(memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) == 0 && reply.m_type == SHA256_REPLY.m_type) {
        if(reply.m_status == 0) {
            puts("File does not exist.");
            return;
        }
        Recv(sockid, &reply, 12, 0);
        if(memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) || reply.m_type != FILE_DATA.m_type) {
            puts("Error in file transfer.");
            return;
        }
        int length = ntohl(reply.m_length) - 12;
        static char buffer[FILE_MAX_SIZE];
        Recv(sockid, &buffer, length, 0);
        for(int i = 0; i < length; i++)
            printf("%c", buffer[i]);
    }
}
void quit() {
    if (!connected)
        exit(0);
    struct message msg = QUIT_CONN_REQUEST, reply;
    Send(sockid, &msg, 12, 0);
    Recv(sockid, &reply, 12, 0);
    if (memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) == 0 && reply.m_type == QUIT_CONN_REPLY.m_type) {
        printf("connection close ok\n");
        close(sockid);
        connected = false;
    }
}

int main() {
    while (1) {
        printf("Client");
        if (connected) printf("(%s:%d)> ", ip, port);
        else printf("(None)> ");
        std::cin >> cmd;
        if (strcmp(cmd, "open") == 0)
            open();
        else if(strcmp(cmd, "ls") == 0) 
            list();
        else if(strcmp(cmd, "cd") == 0) 
            chdir();
        else if(strcmp(cmd, "get") == 0) 
            get();
        else if(strcmp(cmd, "put") == 0) 
            put();
        else if(strcmp(cmd, "sha256") == 0) 
            sha256();
        else if (strcmp(cmd, "quit") == 0) 
            quit();
        else {
            printf("Bad request!\n");
            fflush(stdout);
        }
    }
    return 0;
}