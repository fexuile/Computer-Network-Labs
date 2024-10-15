#include <string.h>
const int MAGIC_NUMBER_LENGTH = 6;
const int MAX_LINE_LENGTH = 128;
#define UNUSED 0
const char protocol[] = {'\xc1', '\xa1', '\x10', 'f', 't', 'p'};
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
struct message{
    uint8_t m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    uint8_t m_type;                          /* type (1 byte) */
    uint8_t m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
    message(uint8_t type = UNUSED, uint8_t status = UNUSED, uint32_t length = UNUSED){
        m_type = type;
        m_status = status;
        m_length = htonl(length);
        memcpy(m_protocol, protocol, MAGIC_NUMBER_LENGTH);
    }
} __attribute__ ((packed));
#define OPEN_CONN_REQUEST message(0xA1, UNUSED, 12)
#define OPEN_CONN_REPLY message(0xA2, 1, 12)
#define LS_CONN_REQUEST message(0xA3, UNUSED, 12)
// #define LS_CONN_REPLY message(0xA4, UNUSED, 12)
#define QUIT_CONN_REQUEST message(0xAD, UNUSED, 12)
#define QUIT_CONN_REPLY message(0xAE, UNUSED, 12)

void Send(int sock, void* buffer, int len, int flags) {
    size_t ret = 0;
    while (ret < len) {
        ssize_t b = send(sock, (char*)buffer + ret, len - ret, 0);
        if (b == 0) printf("socket Closed"); 
        if (b < 0) printf("Error ?"); 
        ret += b; 
    }
}
void Recv(int sock, void* buffer, int len, int flags) {
    size_t ret = 0;
    while (ret < len) {
        ssize_t b = recv(sock, (char*)buffer + ret, len - ret, 0);
        if (b == 0) printf("socket Closed"); 
        if (b < 0) printf("Error ?"); 
        ret += b; 
    }
}