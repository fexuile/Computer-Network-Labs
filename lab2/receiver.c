#include "rtp.h"
#include "util.h"
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

int listen_port, window_size, mode;
char file_path[256];
char buffer[1500];
int sockfd;
uint32_t y;
socklen_t addr_len;
struct sockaddr_in lstAddr;
rtp_packet_t receiver_packet, sender_packet;


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
                LOG_DEBUG("Receiver: received Needed packet\n");
                break;
            }
        }
        if(flag == -1) break;
        sendto(fd, buffer, size, 0, Addr, sizeof(Addr));
        LOG_DEBUG("Receiver: resent My packet\n");
    }
    if(flag > 50) LOG_FATAL("Receiver: No Need Flag packet. Exiting..\n");
}

void conn() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LOG_FATAL("Receiver: socket creation failed\n");
    }
    memset(&lstAddr, 0, sizeof(lstAddr));  // lstAddr is the address on which the receiver listens
    lstAddr.sin_family = AF_INET;
    lstAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // receiver accepts incoming data from any address
    lstAddr.sin_port = htons(listen_port);  // but only accepts data coming from a certain port
    bind(sockfd, &lstAddr, sizeof(lstAddr));  // assign the address to the listening socket

    LOG_DEBUG("Receiver: listening on port %d\n", listen_port);
    addr_len = sizeof(lstAddr);
    clock_t t = clock();
    int flag = 0;
    while((double)(clock() - t) / CLOCKS_PER_SEC < 5.0) {
        int recv_byte = recvfrom(sockfd, &receiver_packet, sizeof(receiver_packet), MSG_DONTWAIT, (struct sockaddr *)&lstAddr, &addr_len);
        if(recv_byte == 0 || !checksum(receiver_packet)) continue;
        if(receiver_packet.rtp.flags == RTP_SYN) {
            flag = 1;
            LOG_DEBUG("Receiver: received SYN packet\n");
            break;
        }
    }
    if(!flag) LOG_FATAL("Receiver: No SYN packet, exiting.");
    sender_packet.rtp.seq_num = receiver_packet.rtp.seq_num + 1;
    sender_packet.rtp.length = 0;
    sender_packet.rtp.flags = RTP_SYN | RTP_ACK;
    sender_packet.rtp.checksum = 0;
    sender_packet.rtp.checksum = compute_checksum(&sender_packet, sizeof(rtp_header_t) + sender_packet.rtp.length);
    sendto(sockfd, &sender_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&lstAddr, sizeof(lstAddr));
    LOG_DEBUG("Receiver: sent SYN-ACK packet\n");

    resend_loop(sockfd, &sender_packet, sizeof(rtp_header_t), (struct sockaddr *)&lstAddr, &addr_len, RTP_ACK);
    y = receiver_packet.rtp.seq_num;
    LOG_DEBUG("Receiver: connection established\n");
}

void fin(int seq_num) {
    sender_packet.rtp.seq_num = seq_num;
    sender_packet.rtp.length = 0;
    sender_packet.rtp.flags = RTP_FIN | RTP_ACK;
    sender_packet.rtp.checksum = 0;
    sender_packet.rtp.checksum = compute_checksum(&sender_packet, sizeof(rtp_header_t) + sender_packet.rtp.length);
    sendto(sockfd, &sender_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&lstAddr, sizeof(lstAddr));
    LOG_DEBUG("Receiver: send FIN packet\n");
}

void GBN() {
    clock_t t = clock();
    FILE *fp = fopen(file_path, "wb");
    while((double)(clock() - t) / CLOCKS_PER_SEC < 10.0) {
        int recv_bytes = recvfrom(sockfd, &receiver_packet, sizeof(receiver_packet), MSG_DONTWAIT, (struct sockaddr *)&lstAddr, &addr_len);
        if(recv_bytes > 0) {
            if(checksum(receiver_packet)) {
                if (receiver_packet.rtp.flags != RTP_FIN && receiver_packet.rtp.flags != 0) {
                    LOG_DEBUG("Receiver: Fault Syntax.\n");
                    continue;
                }
                int seq_num = receiver_packet.rtp.seq_num;
                if(out_of_window(seq_num, y, window_size)) {
                    LOG_DEBUG("Receiver: Sequence Number Out of Range.\n");
                    continue;
                }
                if (receiver_packet.rtp.seq_num != y) {
                    sendto(sockfd, &sender_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&lstAddr, sizeof(lstAddr));
                    LOG_DEBUG("Receiver: Received Fault Packet. Resend seq %d\n", y - 1);
                }
                else {
                    if (receiver_packet.rtp.flags == RTP_FIN) {
                        LOG_DEBUG("Receiver: Connection Finished.\n");
                        fin(receiver_packet.rtp.seq_num);
                        return;
                    }
                    else{
                        int len = receiver_packet.rtp.length;
                        memcpy(buffer, receiver_packet.payload, len);
                        fwrite(buffer, 1, len, fp);
                        LOG_DEBUG("Receiver: Save the data.\n");

                        sender_packet.rtp.flags = RTP_ACK;
                        sender_packet.rtp.length = 0;
                        sender_packet.rtp.checksum = 0;
                        sender_packet.rtp.seq_num = (++y);
                        sender_packet.rtp.checksum = compute_checksum(&sender_packet, sizeof(rtp_header_t));
                        sendto(sockfd, &sender_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&lstAddr, sizeof(lstAddr));
                        LOG_DEBUG("Receiver: Received Packet seq %d\n", y - 1);
                    }
                }
            }
            t = clock();
        }
    }
    LOG_FATAL("Receiver: Disconnected.\n");
}

void SR() {
    clock_t t = clock();
    FILE *fp = fopen(file_path, "wb");
    while((double)(clock() - t) / CLOCKS_PER_SEC < 10.0) {
        int recv_bytes = recvfrom(sockfd, &receiver_packet, sizeof(receiver_packet), MSG_DONTWAIT, (struct sockaddr *)&lstAddr, &addr_len);
        if(recv_bytes > 0) {
            if(checksum(receiver_packet)) {
                if (receiver_packet.rtp.seq_num != y);
                else {
                    if (receiver_packet.rtp.flags == RTP_FIN) {
                        LOG_DEBUG("Receiver: Connection Finished.\n");
                        fin(receiver_packet.rtp.seq_num);
                        return;
                    }
                    else if(!receiver_packet.rtp.flags) {
                        int len = receiver_packet.rtp.length;
                        memcpy(buffer, receiver_packet.payload, len);
                        fwrite(buffer, 1, len, fp);
                        LOG_DEBUG("Receiver: Save the data.\n");

                        sender_packet.rtp.flags = RTP_ACK;
                        sender_packet.rtp.length = 0;
                        sender_packet.rtp.checksum = 0;
                        sender_packet.rtp.seq_num = receiver_packet.rtp.seq_num;
                        sender_packet.rtp.checksum = compute_checksum(&sender_packet, sizeof(rtp_header_t));
                        y++;
                        sendto(sockfd, &sender_packet, sizeof(rtp_header_t), 0, (struct sockaddr *)&lstAddr, sizeof(lstAddr));
                        LOG_DEBUG("Receiver: Received Packet seq %d\n", receiver_packet.rtp.seq_num);
                    }
                    else {
                        LOG_DEBUG("Receiver: Wrong Flag Syntax.\n");
                    }
                }
            }
            t = clock();
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 5) {
        LOG_FATAL("Usage: ./receiver [listen port] [file path] [window size] "
                  "[mode]\n");
    }

    listen_port = atoi(argv[1]);
    strcpy(file_path, argv[2]);
    window_size = atoi(argv[3]);
    mode = atoi(argv[4]);

    conn();
    if(mode == 0) GBN();
    else SR();

    LOG_DEBUG("Receiver: exiting...\n");
    return 0;
}