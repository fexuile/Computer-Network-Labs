#include "rtp.h"
#include "util.h"
#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

int port, mode, window_size;
char file_path[256];
int sockfd;
struct sockaddr_in lstAddr;
rtp_packet_t recv_packet, send_packet;
socklen_t addr_len;

uint32_t recv_base;
rtp_packet_t packets[MAX_WINDOW_SIZE];
int packets_size;

void resend_loop(int flags) {
    int count = 0, flag = 0;
    clock_t t = clock();
    while(count < 50) {
        count++;
        while((double)(clock() - t) / CLOCKS_PER_SEC < 0.1) {
            addr_len = sizeof(lstAddr);
            int recv_byte = recvfrom(sockfd, &recv_packet, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sock_addr *)&lstAddr, &addr_len);
            if(recv_byte > 0 && recv_byte == sizeof(rtp_header_t) + recv_packet.rtp.length && checksum(&recv_packet) && (recv_packet.rtp.flags & flags)){
                flag = 1;
                LOG_DEBUG("Receiver: Receive ACK packet.\n");
                break;
            }
        }
        if(flag) break;
        t = clock();
    }
    if(!flag) LOG_FATAL("Receiver: reached max attempts to resend data.\n");
}

void Connect() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&lstAddr, 0, sizeof(lstAddr));
    lstAddr.sin_family = AF_INET;
    lstAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    lstAddr.sin_port = htons(port);
    bind(sockfd, (struct sockaddr *)&lstAddr, sizeof(lstAddr));

    int flag = 0;
    clock_t t = clock();
    while((double)(clock() - t) / CLOCKS_PER_SEC < 5.0) {
        addr_len = sizeof(lstAddr);
        int recv_byte = recvfrom(sockfd, &recv_packet, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sock_addr *)&lstAddr, &addr_len);
        if(recv_byte > 0 && recv_byte == sizeof(rtp_header_t) + recv_packet.rtp.length && checksum(&recv_packet) && recv_packet.rtp.flags == RTP_SYN) {
            flag = 1;
            break;
        }
    }
    if(!flag) LOG_FATAL("Receiver: Can't receive SYN packet.\n");
    LOG_DEBUG("Receiver: Receive SYN packet.\n");
    send_packet.rtp.checksum = 0;
    send_packet.rtp.flags = RTP_ACK | RTP_SYN;
    send_packet.rtp.length = 0;
    send_packet.rtp.seq_num = next_seq(recv_packet.rtp.seq_num);
    send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t));
    sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sock_addr *)&lstAddr, addr_len);
    LOG_DEBUG("Receiver: Send SYN+ACK packet.\n");
    resend_loop(RTP_ACK);

    recv_base = recv_packet.rtp.seq_num;
    LOG_DEBUG("Receiver: Connection established.\n");
}

void GBN() {
    FILE *fp = fopen(file_path, "wb");
    clock_t t = clock();
    while((double)(clock() - t) / CLOCKS_PER_SEC < 5.0) {
        addr_len = sizeof(lstAddr);
        int recv_byte = recvfrom(sockfd, &recv_packet, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sock_addr *)&lstAddr, &addr_len);
        if(recv_byte > 0 && recv_byte == sizeof(rtp_header_t) + recv_packet.rtp.length && checksum(&recv_packet)) {
            if(recv_packet.rtp.flags & (RTP_ACK | RTP_SYN)) {
                t = clock();
                LOG_DEBUG("Receiver: Receive Bad Packet.\n");
                continue;
            }
            if(recv_packet.rtp.flags == RTP_FIN) return;
            if(out_of_window(recv_packet.rtp.seq_num, recv_base, window_size)) {
                t = clock();
                LOG_DEBUG("Receiver: Receive out of window Packet. Seq_num = %d\n", recv_packet.rtp.seq_num);
                continue;
            }
            if(recv_packet.rtp.seq_num != recv_base) {
                t = clock();
                LOG_DEBUG("Receiver: Receive Dissorted Packet. Seq_num = %d\n", recv_packet.rtp.seq_num);
                continue;
            }
            fwrite(recv_packet.payload, 1, recv_packet.rtp.length, fp);
            LOG_DEBUG("Receiver: Receive %d seq_num Packet...Store Data. Length is %d\n", recv_packet.rtp.seq_num, recv_packet.rtp.length);

            recv_base = next_seq(recv_base);
            send_packet.rtp.checksum = 0;
            send_packet.rtp.flags = RTP_ACK;
            send_packet.rtp.length = 0;
            send_packet.rtp.seq_num = recv_base;
            send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t));
            sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sock_addr *)&lstAddr, addr_len);
            t = clock();
        }
    }
    LOG_FATAL("Receiver: Sender Disconnected.\n");
}

void SR() {
    FILE *fp = fopen(file_path, "wb");
    clock_t t = clock();
    while((double)(clock() - t) / CLOCKS_PER_SEC < 5.0) {
        addr_len = sizeof(lstAddr);
        int recv_byte = recvfrom(sockfd, &recv_packet, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sock_addr *)&lstAddr, &addr_len);
        if(recv_byte > 0 && recv_byte == sizeof(rtp_header_t) + recv_packet.rtp.length && checksum(&recv_packet)) {
            if(recv_packet.rtp.flags & (RTP_ACK | RTP_SYN)) {
                t = clock();
                LOG_DEBUG("Receiver: Receive Bad Packet.\n");
                continue;
            }
            if(recv_packet.rtp.flags == RTP_FIN) {
                Finish();
                exit(0);
            }
            int seq_num = recv_packet.rtp.seq_num;
            if(!out_of_window(seq_num, recv_base, window_size)) {
                send_packet.rtp.checksum = 0;
                send_packet.rtp.flags = RTP_ACK;
                send_packet.rtp.length = 0;
                send_packet.rtp.seq_num = seq_num;
                send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t));
                sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sock_addr *)&lstAddr, addr_len);

                packets[packets_size++] = recv_packet;
                if(seq_num == recv_base) {
                    while(packets_size > 0) {
                        int i = 0;
                        for(i = 0; i < packets_size; i++) {
                            if(packets[i].rtp.seq_num == recv_base) {
                                fwrite(packets[i].payload, 1, packets[i].rtp.length, fp);
                                LOG_DEBUG("Receiver: Store Data..Seq_num = %d, Length = %d\n", packets[i].rtp.seq_num, packets[i].rtp.length);
                                break;
                            }
                        }
                        if(i == packets_size) break; 
                        if(i != packets_size - 1) swap(&packets[packets_size - 1], &packets[i]);
                        packets_size--; recv_base = next_seq(recv_base);
                    } 
                }
            }
            else if(before_window(seq_num, recv_base, window_size)) {
                send_packet.rtp.checksum = 0;
                send_packet.rtp.flags = RTP_ACK;
                send_packet.rtp.length = 0;
                send_packet.rtp.seq_num = seq_num;
                send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t));
                sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sock_addr *)&lstAddr, addr_len);
                LOG_DEBUG("Receiver: Ack before recv_window packet %d\n", seq_num);
            }
            else LOG_DEBUG("Receiver: Ignoring this packet.\n");
            t = clock();
        }
    }
    LOG_FATAL("Receiver: Sender Disconnected.\n");
}

void Finish() {
    LOG_DEBUG("Receiver: Receive FIN packet.\n");
    send_packet.rtp.checksum = 0;
    send_packet.rtp.flags = RTP_ACK | RTP_FIN;
    send_packet.rtp.length = 0;
    send_packet.rtp.seq_num = recv_packet.rtp.seq_num;
    send_packet.rtp.checksum = compute_checksum(&send_packet, sizeof(rtp_header_t));
    sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sock_addr *)&lstAddr, addr_len);
    LOG_DEBUG("Receiver: Send FIN+ACK Packet.\n");
    clock_t t = clock();
    while((double)(clock() - t) < 5.0) {
        addr_len = sizeof(lstAddr);
        int recv_byte = recvfrom(sockfd, &recv_packet, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sock_addr *)&lstAddr, &addr_len);
        if(recv_byte > 0 && recv_byte == sizeof(rtp_header_t) + recv_packet.rtp.length && checksum(&recv_packet)) {
            if(recv_packet.rtp.flags == RTP_FIN) {
                sendto(sockfd, &send_packet, sizeof(rtp_header_t), 0, (struct sock_addr *)&lstAddr, addr_len);
                LOG_DEBUG("Receiver: Resend FIN+ACK Packet.\n");
            }
            t = clock();
        }
    }
    LOG_DEBUG("Receiver: exiting...\n");
}

int main(int argc, char **argv) {
    if (argc != 5) {
        LOG_FATAL("Usage: ./receiver [listen port] [file path] [window size] "
                  "[mode]\n");
    }

    port = atoi(argv[1]);
    strcpy(file_path, argv[2]);
    window_size =  atoi(argv[3]);
    mode = atoi(argv[4]);

    Connect();
    if(mode == 0) GBN();
    else SR();

    Finish();
    return 0;
}
 