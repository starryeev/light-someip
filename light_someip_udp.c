#include "light_someip_udp.h"

#include <stdio.h>
#include <string.h>

/* 라이브러리 사용 플랫폼에 따라 활성/비활성화 할 것 */
// #define SOMEIP_UDP_PLATFORM_RPI
#define SOMEIP_UDP_PLATFORM_TC375


#ifdef SOMEIP_UDP_PLATFORM_RPI

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define UDP_SOCKET      (socket)
#define UDP_BIND        (bind)
#define UDP_SENDTO      (sendto)
#define UDP_RECVFROM    (recvfrom)
#define UDP_CLOSE       (close)
#define UDP_ERRNO       (errno)

#endif


#ifdef SOMEIP_UDP_PLATFORM_TC375

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/errno.h"

#define UDP_SOCKET      (lwip_socket)
#define UDP_BIND        (lwip_bind)
#define UDP_SENDTO      (lwip_sendto)
#define UDP_RECVFROM    (lwip_recvfrom)
#define UDP_ERRNO       (errno)

#endif


/* -------------------- Internal Variabls -------------------- */

static int g_udp_sock = -1;


/* -------------------- Static Function Implementations -------------------- */

static int set_nonblocking(int sock) {
    /* 라즈베리파이 */
    #ifdef SOMEIP_UDP_PLATFORM_RPI
    #endif


    /* TC375 */
    #ifdef SOMEIP_UDP_PLATFORM_TC375

    int nonblock = 1;
    if(lwip_ioctl(sock, FIONBIO, &nonblock) < 0) return -1;
    return 0;

    #endif
}


/* -------------------- Public Function Implementations -------------------- */

int light_someip_udp_init(uint16_t port) {
    struct sockaddr_in local_addr;
    if(g_udp_sock >= 0) return 0;

    g_udp_sock = UDP_SOCKET(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(g_udp_sock < 0) return -1;

    memset(&local_addr, 0, sizeof(local_addr));

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(port);

    if (UDP_BIND(g_udp_sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0)
    {
        UDP_CLOSE(g_udp_sock);
        g_udp_sock = -1;
        return -1;
    }

    if (set_nonblocking(g_udp_sock) != 0)
    {
        UDP_CLOSE(g_udp_sock);
        g_udp_sock = -1;
        return -1;
    }

    return 0;
}


int light_someip_udp_send(const char* dst_ip, uint16_t dst_port, const uint8_t* buf, uint32_t len) {
    struct sockaddr_in dst_addr;
    int sent_len;

    if (g_udp_sock < 0) return -1;
    if (dst_ip == NULL || buf == NULL || len == 0) return -1;

    memset(&dst_addr, 0, sizeof(dst_addr));

    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(dst_port);
    dst_addr.sin_addr.s_addr = inet_addr(dst_ip);

    if (dst_addr.sin_addr.s_addr == INADDR_NONE) return -1;

    sent_len = UDP_SENDTO(g_udp_sock, buf, len, 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));

    if (sent_len < 0) return -1;
    if ((uint32_t)sent_len != len) return -1;

    return 0;
}


int light_someip_udp_recv(uint8_t* buf, uint32_t buf_size, char remote_ip[IP_LEN], uint16_t* remote_port) {
    struct sockaddr_in remote_addr;
    socklen_t remote_addr_len;
    int recv_len;
    uint32_t ip_u32;

    if (g_udp_sock < 0) return -1;

    if (buf == NULL || remote_ip == NULL || remote_port == 0) return -1;

    if (buf_size == 0) return -1;

    remote_ip[0] = '\0';
    *remote_port = 0;

    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr_len = sizeof(remote_addr);

    recv_len = UDP_RECVFROM(g_udp_sock, buf, buf_size, 0, (struct sockaddr*)&remote_addr, &remote_addr_len);

    if (recv_len < 0) { /* 아직 받은 데이터가 없다면 */
        if (UDP_ERRNO == EAGAIN || UDP_ERRNO == EWOULDBLOCK) return 0;

        return -1;
    }
 
    if (recv_len == 0) return 0;

    ip_u32 = ntohl(remote_addr.sin_addr.s_addr);

    snprintf(
        remote_ip,
        IP_LEN,
        "%u.%u.%u.%u",
        (unsigned int)((ip_u32 >> 24) & 0xFF),
        (unsigned int)((ip_u32 >> 16) & 0xFF),
        (unsigned int)((ip_u32 >> 8) & 0xFF),
        (unsigned int)(ip_u32 & 0xFF)
    );

    *remote_port = ntohs(remote_addr.sin_port);

    return recv_len;
}
