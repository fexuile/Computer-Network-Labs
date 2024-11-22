#include "rtp.h"
#include "util.h"
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define MAX_PACKET_SIZE 1500
#define MTU_SIZE 1461
#define MAX_WINDOW_SIZE 20010

char receiver_ip[16], file_path[256];
int receiver_port, window_size, mode, x;
int sockfd;
struct sockaddr_in dstAddr;
socklen_t addr_len;
rtp_packet_t send_packet, receiver_packet;
rtp_packet_t packets[MAX_WINDOW_SIZE];
int packets_size;

bool checksum(rtp_packet_t packet) {
    int check_sum = packet.rtp.checksum;
    packet.rtp.checksum = 0;
    int calc_check_sum = compute_checksum(&packet, sizeof(rtp_header_t) + packet.rtp.length);
    if(check_sum != calc_check_sum) return false;
    return true;
}

bool out_of_window(uint32_t seq, uint32_t start, int size) {
    for(uint32_t i = 0; i < size; i++) {
        uint32_t pos = start + i;
        if (pos == seq) return false;
    }
    return true;
}

void resend_loop(int fd, void* buffer, size_t size, const struct sockaddr *Addr, socklen_t *addr_len, rtp_header_flag_t Flags) {
    int flag = 0;
    clock_t t = clock();
    while(flag <= 50) {
        flag++; t = clock();
        while((double)(clock() - t) / CLOCKS_PER_SEC < 0.1) {
            int recv_byte = recvfrom(fd, &receiver_packet, sizeof(receiver_packet), MSG_DONTWAIT, Addr, &addr_len);
            if(recv_byte == 0 || !checksum(receiver_packet)) continue;
            if(receiver_packet.rtp.flags & Flags) {
                flag = -1;
                LOG_DEBUG("Sender: received Needed packet\n");
                break;
            }
        }
        if(flag == -1) break;
        sendto(fd, buffer, size, 0, Addr, sizeof(Addr));
        LOG_DEBUG("Sender: resent My packet\n");
    }
    if(flag > 50) LOG_FATAL("Sender: No Need Flag packet. Exiting..\n");
}

void conn() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd <= 0) {
        LOG_FATAL("Failed to create socket\n");
    }
    memset(&dstAddr, 0, sizeof(&dstAddr));
    dstAddr.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &dstAddr.sin_addr);
    dstAddr.sin_port = htons(receiver_port);

    x = rand();
    send_packet.rtp.seq_num = x;
    send_packet.rtp.length = 0;
    send_packet.rtp.flags = RTP_SYN;
    send_packet.rtp.checksum = 0;
    send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t) + send_packet.rtp.length);
    memset(send_packet.payload, 0, PAYLOAD_MAX);
    sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
    LOG_DEBUG("Sender: sent SYN packet\n");

    addr_len = sizeof(dstAddr);
    resend_loop(sockfd, &send_packet, sizeof(rtp_header_t), (struct sockaddr *)&dstAddr, &addr_len, RTP_SYN | RTP_ACK);

    x = receiver_packet.rtp.seq_num;
    send_packet.rtp.seq_num = x;
    send_packet.rtp.length = 0;
    send_packet.rtp.flags = RTP_ACK;
    send_packet.rtp.checksum = 0;
    send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t) + send_packet.rtp.length);
    sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
    LOG_DEBUG("Sender: sent ACK packet\n");

    int t = clock();
    while(clock() - t < 2000) {
        int recv_byte = recvfrom(sockfd, &receiver_packet, sizeof(rtp_packet_t), MSG_DONTWAIT, (struct sockaddr *)&dstAddr, &addr_len);
        if(recv_byte > 0 && receiver_packet.rtp.flags & (RTP_ACK | RTP_SYN)) {
            LOG_DEBUG("Sender: received SYN-ACK packet again %d %d\n", receiver_packet.rtp.seq_num, receiver_packet.rtp.flags);
            sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
            LOG_DEBUG("Sender: sent ACK packet again\n");
            t = clock();
        }
    }
    LOG_DEBUG("Sender: connection established\n");
}

void move_on(uint32_t l, uint32_t r) {
    int delta = 0;
    while(l != r) {l++; delta++; }
    for(int i = 0; i + delta < packets_size; i++)
        packets[i] = packets[i + delta];
    packets_size -= delta;
}

void resend_all_packet() {
    for(int i = 0; i < packets_size; i++) {
        sendto(sockfd, &packets[i], sizeof(rtp_header_t) + packets[i].rtp.length, 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
        printf("%d ", packets[i].rtp.seq_num);
    }
    puts("");
    LOG_DEBUG("Sender: Resend ALL window packets.\n");
}

void GBN() {
    uint32_t send_base = x;
    uint32_t next_seq_num = x;
    int N = window_size, finished = 0;
    FILE *fp = fopen(file_path, "rb");
    if(fp == NULL) {
        LOG_FATAL("Sender: failed to open file\n");
    }
    clock_t t = clock();
    char *buffer = (char*)malloc(MTU_SIZE);
    while(1) {
        while((double)(clock() - t) / CLOCKS_PER_SEC < 0.1) {
            if(!out_of_window(next_seq_num, send_base, window_size)) {
                int len = fread(buffer, 1, MTU_SIZE, fp);
                if(len != 0) {
                    send_packet.rtp.seq_num = next_seq_num;
                    send_packet.rtp.length = len;
                    send_packet.rtp.flags = 0;
                    memcpy(send_packet.payload, buffer, len);
                    send_packet.rtp.checksum = 0;
                    send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t) + send_packet.rtp.length);
                    sendto(sockfd, &send_packet, sizeof(rtp_header_t) + send_packet.rtp.length, 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
                    LOG_DEBUG("Sender: sent packet %d\n", next_seq_num);
                    next_seq_num++;
                    packets[packets_size++] = send_packet;
                }
                else if(send_base == next_seq_num) {
                    finished = 1;
                    break;
                }
            }
            int recv_byte = recvfrom(sockfd, &receiver_packet, sizeof(rtp_packet_t), MSG_DONTWAIT, (struct sockaddr *)&dstAddr, &addr_len);
            if(recv_byte > 0 && checksum(receiver_packet) && receiver_packet.rtp.flags == RTP_ACK) {
                move_on(send_base, receiver_packet.rtp.seq_num);
                send_base = receiver_packet.rtp.seq_num;
                if (send_base != next_seq_num) t = clock();
                LOG_DEBUG("Sender: received ACK packet %d\n", receiver_packet.rtp.seq_num - 1);
            }
        }
        if(finished) break;
        resend_all_packet();
        t = clock();
    }
    x = next_seq_num;
    LOG_DEBUG("Sender: sent all packets\n");
}

void SR() {
    int send_base = x - 1;
    int next_seq_num = x;
    int N = window_size;
    FILE *fp = fopen(file_path, "rb");
    if(fp == NULL) {
        LOG_FATAL("Sender: failed to open file\n");
    }
    char *buffer = (char*)malloc(MTU_SIZE);
    int len = fread(buffer, 1 , MTU_SIZE, fp);
    while(next_seq_num <= send_base + N) {
        if(len != 0) {
            send_packet.rtp.seq_num = next_seq_num;
            send_packet.rtp.length = len;
            send_packet.rtp.flags = 0;
            memcpy(send_packet.payload, buffer, len);
            send_packet.rtp.checksum = 0;
            send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t) + send_packet.rtp.length);
            sendto(sockfd, &send_packet, sizeof(rtp_header_t) + send_packet.rtp.length, 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
            LOG_DEBUG("Sender: sent packet %d %d\n", next_seq_num, len);
            next_seq_num++;
            len = fread(buffer, 1 , MTU_SIZE, fp);
        }
        else if(send_base == next_seq_num - 1) break;
        recvfrom(sockfd, &receiver_packet, sizeof(rtp_packet_t), 0, (struct sockaddr *)&dstAddr, &addr_len);
        if(receiver_packet.rtp.flags != RTP_ACK) {
            LOG_FATAL("Sender: ACK packet is corrupted\n");
        }
        send_base++;
        LOG_DEBUG("Sender: received ACK packet %d\n", receiver_packet.rtp.seq_num);
    }
    x = next_seq_num;
    LOG_DEBUG("Sender: sent all packets\n");
}

void fin() {
    send_packet.rtp.seq_num = x;
    send_packet.rtp.length = 0;
    send_packet.rtp.flags = RTP_FIN;
    send_packet.rtp.checksum = 0;
    send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t) + send_packet.rtp.length);
    sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&dstAddr, sizeof(dstAddr));
    LOG_DEBUG("Sender: sent FIN packet\n");

    resend_loop(sockfd, &send_packet, sizeof(rtp_header_t), (struct sockaddr *)&dstAddr, &addr_len, RTP_FIN | RTP_ACK);
}

int main(int argc, char **argv) {
    if (argc != 6) {
        LOG_FATAL("Usage: ./sender [receiver ip] [receiver port] [file path] "
                  "[window size] [mode]\n");
    }

    strcpy(receiver_ip, argv[1]);
    receiver_port = atoi(argv[2]);
    strcpy(file_path, argv[3]);
    window_size = atoi(argv[4]);
    mode = atoi(argv[5]);

    conn();
    if(mode == 0) GBN();
    else SR();

    fin();

    LOG_DEBUG("Sender: exiting...\n");
    return 0;
}