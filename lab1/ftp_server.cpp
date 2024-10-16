#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "helper.h"
#include <filesystem>

int port;
char ip[16];

struct Info {
    int client;
    pthread_t thread;
    std::string dir;
}info[MAX_THREAD_COUNTS];

void* doit(void *args) {
    struct Info *info = (struct Info *)args;
    int client = info->client;
    while(1) {
        struct message msg;
        Recv(client, &msg, 12, 0);
        if (memcmp(msg.m_protocol, protocol, MAGIC_NUMBER_LENGTH) != 0) {
            printf("ERROR: protocol mismatch!\n");
            pthread_exit(NULL);
        }
        if (msg.m_type == 0xA1) {
            struct message reply = OPEN_CONN_REPLY;
            if (memcmp(reply.m_protocol, protocol, MAGIC_NUMBER_LENGTH) != 0)
            {
                printf("ERROR: protocol mismatch!\n");
                pthread_exit(NULL);
            }
            Send(client, &reply, 12, 0);
        }
        else if(msg.m_type == 0xA3) {
            struct message reply = LS_CONN_REPLY;
            int length;
            FILE *tmp;
            std::string cmdline = "ls " + info->dir;
            tmp = popen(cmdline.c_str(), "r");
            char *buffer = (char *)malloc(MAX_LINE_LENGTH * 8);
            length = fread(buffer, 1, MAX_LINE_LENGTH, tmp);
            pclose(tmp);
            buffer[length++] = '\0';
            reply.m_length = htonl(length + 12);
            Send(client, &reply, 12, 0);
            Send(client, buffer, length, 0); 
            free(buffer);
        }
        else if(msg.m_type == 0xA5) {
            struct message reply = CHANGE_DIR_REPLY;
            int length = ntohl(msg.m_length) - 12;
            char *dirname = (char *)malloc(length << 2);
            std::string cmdline;
            Recv(client, dirname, length, 0);
            cmdline = info->dir + "/" + std::string(dirname);
            if(std::filesystem::exists(std::string(cmdline)) == -1)
                reply.m_status = 0;
            else {
                reply.m_status = 1;
                info->dir = cmdline;
            }
            Send(client, &reply, 12, 0);
            free(dirname);
        }
        else if(msg.m_type == 0xA7) {
            struct message reply = GET_REPLY;
            int length = ntohl(msg.m_length) - 12;
            char *filename = (char *)malloc(length << 2);
            Recv(client, filename, length, 0);
            std::string cmdline = info->dir + "/" + filename;
            FILE *file = fopen(cmdline.c_str(), "r");
            if(file == NULL) {
                reply.m_status = 0;
                Send(client, &reply, 12, 0);
            }
            else {
                reply.m_status = 1;
                Send(client, &reply, 12, 0);
                reply = FILE_DATA;
                fseek(file, 0, SEEK_END);
                length = ftell(file);
                rewind(file);
                reply.m_length = htonl(length + 12);
                Send(client, &reply, 12, 0);
                char *buffer = (char *)malloc(FILE_MAX_SIZE << 2);
                length = fread(buffer, 1, FILE_MAX_SIZE, file);
                Send(client, buffer, length, 0);
            }
            fclose(file);
            free(filename);
        }
        else if(msg.m_type == 0xA9) {
            struct message reply = PUT_REPLY;
            int length = ntohl(msg.m_length) - 12;
            char *filename = (char *)malloc(sizeof(char) * (length << 1));
            Recv(client, filename, length, 0);
            Send(client, &reply, 12, 0);
            std::string cmdline = info->dir + "/" + std::string(filename);
            Recv(client, &msg, 12, 0);
            if(memcmp(msg.m_protocol, protocol, MAGIC_NUMBER_LENGTH) || msg.m_type != FILE_DATA.m_type) {
                puts("Error in file transfer.");
                free(filename);
                pthread_exit(NULL);
            }
            length = ntohl(msg.m_length) - 12;
            FILE *file = fopen(cmdline.c_str(), "wb");
            char* buffer = (char *)malloc(FILE_MAX_SIZE << 2);
            Recv(client, buffer, length, 0);
            fwrite(buffer, 1, length, file);
            fclose(file);
            free(buffer);
            free(filename);
        }
        else if(msg.m_type == 0xAB) {
            struct message reply = SHA256_REPLY;
            int length = ntohl(msg.m_length) - 12;
            char *filename = (char *)malloc(length << 2);
            Recv(client, filename, length, 0);
            std::string cmdline = info->dir + "/" + filename;
            FILE *file = fopen(cmdline.c_str(), "r");
            if(file == NULL) {
                reply.m_status = 0;
                Send(client, &reply, 12, 0);
            }
            else {
                reply.m_status = 1;
                Send(client, &reply, 12, 0);
                reply = FILE_DATA;
                FILE *tmp;
                std::string cmd_str;
                cmd_str = "sha256sum " + std::string(filename);
                tmp = popen(cmd_str.c_str(), "r");
                char* buffer = (char *)malloc(FILE_MAX_SIZE << 2);
                length = fread(buffer, 1, FILE_MAX_SIZE, tmp);
                pclose(tmp);
                buffer[length++] = '\0';
                reply.m_length = htonl(length + 12);
                Send(client, &reply, 12, 0);
                Send(client, buffer, length, 0);
                free(buffer);
            }
            fclose(file);free(filename);
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
        info[id].dir = ".";
        pthread_create(&info[id].thread, NULL, doit, (void *)&info[id]);
        pthread_detach(info[id].thread); 
    }
    return 0;
}