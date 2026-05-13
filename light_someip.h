#ifndef __LIGHT_SOMEIP_H__
#define __LIGHT_SOMEIP_H__

#include <stdint.h>

#define SOMEIP_PROTOCOL_VER     (0x01)
#define SOMEIP_INTERFACE_VER    (0x01)
#define SOMEIP_HEADER_LEN       (16u)

#define SOMEIP_MAX_PAYLOAD_LEN  (128)   /* Payload에 최대 128bytes 넣어서 전송 가능 */
#define SOMEIP_MAX_ROUTES       (10)    /* 라우팅 테이블에 최대 10개의 서비스 등록 가능 */


typedef enum LightSomeipStatus {
    SOMEIP_OK = 0,
    SOMEIP_ERR,
    SOMEIP_UNKNOWN_SERVICE,
    SOMEIP_TOO_LONG_PAYLOAD,
    SOMEIP_NULL_PTR,
    SOMEIP_NOT_INITIALIZED
} LightSomeipStatus;

typedef enum LightSomeipMsgType {
    SOMEIP_MSGTYPE_REQUEST = 0x00,
    SOMEIP_MSGTYPE_RESPONSE = 0x80,
    SOMEIP_MSGTYPE_NOTIFICATION = 0x02
} LightSomeipMsgType;

typedef enum LightSomeipReturnCode {
    SOMEIP_RET_OK = 0x00
} LightSomeipRetCode;

typedef struct LightSomeipEndpoint {
    char ip[16];
    uint16_t port;
} LightSomeipEndpoint;

typedef struct LightSomeipServiceRoute {
    uint16_t service_id;
    LightSomeipEndpoint endpoint;
} LightSomeipServiceRoute;

typedef struct LightSomeipConfig {
    uint16_t client_id;
    uint16_t port;
    LightSomeipServiceRoute routes[SOMEIP_MAX_ROUTES];
} LightSomeipConfig;

int light_someip_send(uint16_t service_id, uint16_t method_id, const uint8_t* payload_arr, uint32_t payload_len);

/* Functions */
LightSomeipStatus light_someip_init(const LightSomeipConfig* config_ptr);
LightSomeipStatus light_someip_request();
LightSomeipStatus light_someip_respond();
LightSomeipStatus light_someip_event_notify();
LightSomeipStatus light_someip_recv();


#endif