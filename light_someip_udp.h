#ifndef __LIGHT_SOMEIP_UDP_H__
#define __LIGHT_SOMEIP_UDP_H__


#include <stdint.h>

#define IP_LEN  (16)

#define SOMEIP_UDP_OK       (0)
#define SOMEIP_UDP_NO_DATA  (1)
#define SOMEIP_UDP_ERR      (-1)


int light_someip_udp_init(uint16_t port);
int light_someip_udp_send(const char* dst_ip, uint16_t dst_port, const uint8_t* buf, uint32_t len);
int light_someip_udp_recv(uint8_t* buf, uint32_t buf_size, char remote_ip[IP_LEN], uint16_t* remote_port);

#endif