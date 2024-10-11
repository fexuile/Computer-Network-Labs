#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>

const int MAX_LINE_LENGTH = 128;
const int MAGIC_NUMBER_LENGTH = 6;
char cmdline[MAX_LINE_LENGTH];

struct message{
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));

enum type {
    BUILTIN_OPEN,
    BUILTIN_LS,
    BUILTIN_CD,
    BUILTIN_GET,
    BUILTIN_PUT,
    BUILTIN_SHA256,
    BUILTIN_QUIT,
    ERROR_COMMAND
};
struct command {
    int port;
    char ip[16];
    type command_type;
    char arg[128];
};

void parse_command(char *cmdline, struct command *cmd) {
    cmdline = strtok(cmdline, "\n");
    char *token = strtok(cmdline, " ");
    if(strcmp(token, "quit") == 0) {
        cmd->command_type = BUILTIN_QUIT;
        return;
    }
    if(strcmp(token, "open") == 0) {
        cmd->command_type = BUILTIN_OPEN;
        token = strtok(NULL, " ");
        strcpy(cmd->ip, token);
        token = strtok(NULL, " ");
        cmd->port = atoi(token);
        return;
    }
    if(strcmp(token, "ls") == 0) {
        cmd->command_type = BUILTIN_LS;
        return;
    }
    if(strcmp(token, "cd") == 0) {
        cmd->command_type = BUILTIN_CD;
        token = strtok(NULL, " ");
        strcpy(cmd->arg, token);
        return;
    }
    if(strcmp(token, "get") == 0) {
        cmd->command_type = BUILTIN_GET;
        token = strtok(NULL, " ");
        strcpy(cmd->arg, token);
        return;
    }
    if(strcmp(token, "put") == 0) {
        cmd->command_type = BUILTIN_PUT;
        token = strtok(NULL, " ");
        strcpy(cmd->arg, token);
        return;
    }
    if(strcmp(token, "sha256") == 0) {
        cmd->command_type = BUILTIN_SHA256;
        token = strtok(NULL, " ");
        strcpy(cmd->arg, token);
        return;
    }
    cmd->command_type = ERROR_COMMAND;
    return;
}
void open(char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        printf("ERROR: socket creation failed!\n");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    
}

int main() {
    while(1) {
        printf("Client(none)>");
        fgets(cmdline, MAX_LINE_LENGTH, stdin);
        struct command *cmd = (struct command *)malloc(sizeof(struct command));
        parse_command(cmdline, cmd);
        switch(cmd->command_type){
            case BUILTIN_QUIT:
                return 0;
            case BUILTIN_OPEN:
                open(cmd->ip, cmd->port);
                break;
            case BUILTIN_LS:
                printf("ls\n");
                break;
            case BUILTIN_CD:
                printf("cd\n");
                break;
            case BUILTIN_GET:
                printf("get\n");
                break;  
            case BUILTIN_PUT:   
                printf("put\n");
                break;  
            case BUILTIN_SHA256:    
                printf("sha256\n");
                break;  
            case ERROR_COMMAND: 
                printf("Bad request!\n");
                break;
        }
    }
    return 0;
}