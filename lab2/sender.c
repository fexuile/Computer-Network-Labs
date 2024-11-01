#include "rtp.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

char receiver_ip[16];
char file_path[256];
int receiver_port, sock, window_size, mode;
struct sockaddr_in dstAddr;

void conn() {
    LOG_MSG("Connecting to %s:%d\n", receiver_ip, receiver_port);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&dstAddr, 0, sizeof(&dstAddr));  // dstAddr is the receiver's location in the network
    dstAddr.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &dstAddr.sin_addr);
    dstAddr.sin_port = htons(receiver_port);
    if(connect(sock, (struct sockaddr *)&dstAddr, sizeof(dstAddr)) < 0) {
        LOG_FATAL("Connection failed\n");
    }
    rtp_packet_t *send_packet = (rtp_packet_t *)malloc(sizeof(rtp_packet_t));
    rtp_packet_t *recv_packet = (rtp_packet_t *)malloc(sizeof(rtp_packet_t));
    
    send_packet->rtp.flags = RTP_SYN;
    send_packet->rtp.seq_num = rand();
    send_packet->rtp.length = 0;
    sendto(sock, send_packet, sizeof(send_packet), 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));

    recvfrom(sock, recv_packet, sizeof(rtp_packet_t), 0, NULL, NULL);
    if(recv_packet->rtp.flags != (RTP_SYN | RTP_ACK) || recv_packet->rtp.seq_num != send_packet->rtp.seq_num + 1) {
        LOG_FATAL("Connection failed\n");
    }

    send_packet->rtp.flags = RTP_ACK;
    send_packet->rtp.seq_num = recv_packet->rtp.seq_num + 1;
    send_packet->rtp.length = 0;
    sendto(sock, send_packet, sizeof(send_packet), 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
    
    LOG_DEBUG("Sender: Connected\n");
}

int main(int argc, char **argv) {
    if (argc != 6) {
        LOG_FATAL("Usage: ./sender [receiver ip] [receiver port] [file path] "
                  "[window size] [mode]\n");
    }

    strncpy(receiver_ip, argv[1], 16);
    receiver_port = atoi(argv[2]);
    strncpy(file_path, argv[3], 256);
    window_size = atoi(argv[4]);
    mode = atoi(argv[5]);

    conn();
    

    LOG_DEBUG("Sender: exiting...\n");
    return 0;
}
