#include <stdbool.h>
#include "rtp.h"
#include "util.h"

#define MAX_WINDOW_SIZE 20001
#define MAX_PACKET_SIZE 1472

bool checksum(rtp_packet_t *packet) {
    uint32_t chksum = packet->rtp.checksum;
    packet->rtp.checksum = 0;
    uint32_t s_chksum = compute_checksum(packet, packet->rtp.length + sizeof(rtp_header_t));
    packet->rtp.checksum = chksum;
    return chksum == s_chksum;
}

uint32_t last_seq(uint32_t seq_num) {
    seq_num--;
    if(seq_num < 0) seq_num += (1<<30);
    return seq_num;
}

uint32_t next_seq(uint32_t seq_num) {
    seq_num++;
    if(seq_num >= (1<<30)) seq_num -= (1<<30);
    return seq_num;
}

bool out_of_window(uint32_t seq_num, uint32_t start, int window_size) {
    for(uint32_t i = 0; i < window_size; i++, start = next_seq(start))
        if(seq_num == start) return false;
    return true;
}

bool before_window(uint32_t seq_num, uint32_t start, int window_size) {
    start = last_seq(start);
    for(uint32_t i = 0; i < window_size; i++, start = last_seq(start))
        if(seq_num == start) return true;
    return false;
}

int id(uint32_t start, uint32_t seq) {
    int count = 0;
    while(start != seq) {
        start = next_seq(start);
        count++;
    }
    return count;
}

void swap(rtp_packet_t *a, rtp_packet_t *b) {
    rtp_packet_t tmp;
    tmp = *a; *a = *b; *b = tmp;
}