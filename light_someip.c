#include "light_someip.h"


// /* 정적 라우팅 함수 */
// static const LightSomeipEndpoint* light_someip_find_endpoint(uint16_t service_id);

// // /* SOME/IP 라우팅 변수 */
// // static const LightSomeipServiceRoute* g_routes_ptr;

// /* SOME/IP 통신용 변수 설정 */
// static int g_initialized = 0;

// static LightSomeipConfig g_config;
// //static LightSomeipServiceRoute g_routes[MAX_ROUTES];
// static uint16_t g_client_id = 0;
// static uint16_t g_session_id = 1;

// /* SOME/IP 통신용 버퍼 */
// static uint8_t g_tx_buf[MAX_PAYLOAD_LEN];
// static uint8_t g_rx_buf[MAX_PAYLOAD_LEN];


static int g_initialized = 0;

static uint16_t g_client_id = 0;
static uint16_t g_port = 0;
static uint16_t g_session_id = 1;

static uint8_t g_tx_buf[SOMEIP_HEADER_LEN + SOMEIP_MAX_PAYLOAD_LEN];
static uint8_t g_rx_buf[SOMEIP_HEADER_LEN + SOMEIP_MAX_PAYLOAD_LEN];

static LightSomeipServiceRoute g_routes[SOMEIP_MAX_ROUTES];

/* service id를 보고 endpoint의 ptr을 반환하는 라우팅 헬퍼 함수 */
static const LightSomeipEndpoint* light_someip_find_endpoint(uint16_t service_id);


LightSomeipStatus light_someip_init(const LightSomeipConfig* config_ptr) {
    if(config_ptr == NULL) {            /* E: config nullptr */
        return SOMEIP_NULL_PTR;
    }

    memset(g_routes, 0, sizeof(g_routes));

    g_client_id = config_ptr->client_id;
    g_port = config_ptr->port;
    g_session_id = 1;

    for(int i = 0; i < SOMEIP_MAX_ROUTES; i++) {
        const LightSomeipServiceRoute* src_route_ptr = &config_ptr->routes[i];

        if(src_route_ptr->service_id == 0x0000) {
            continue;
        }

        g_routes[i] = *src_route_ptr;
    }

    g_initialized = 1;

    return SOMEIP_OK;
}

LightSomeipStatus light_someip_send(uint16_t service_id, uint16_t method_id, uint8_t message_type, const uint8_t* payload_ptr, uint32_t payload_len) {
    if(!g_initialized)
    {
        return SOMEIP_NOT_INITIALIZED;
    }

    if(payload_len > SOMEIP_MAX_PAYLOAD_LEN) {     /* 페이로드가 최대 크기를 초과했을 경우 */
        return SOMEIP_TOO_LONG_PAYLOAD;
    }

    if(payload_ptr == NULL) {
        return SOMEIP_NULL_PTR;
    }

    const LightSomeipEndpoint* dst_ptr = light_someip_find_endpoint(service_id);
    if(dst_ptr == NULL) {     /* 라우팅 목적지를 찾을 수 없는 경우 */
        return SOMEIP_UNKNOWN_SERVICE;
    }

    build_packet(g_tx_buf, service_id, method_id, g_client_id, g_session_id, message_type, SOMEIP_RET_OK, payload_ptr, payload_len);

    /*
    
    TODO: UDP 송신

    */

    g_session_id++;

    return SOMEIP_OK;
}

static void build_packet(uint8_t* buf_ptr, uint16_t service_id, uint16_t method_id, uint16_t client_id, uint16_t session_id, uint8_t message_type, uint8_t return_code, const uint8_t* payload_ptr, uint32_t payload_len) {
    uint32_t someip_length = 8u + payload_len;
    uint32_t packet_length = SOMEIP_HEADER_LEN + payload_len;

    /* SOME/IP Header, 16 bytes
    0~1    Service ID
    2~3    Method ID / Event ID
    4~7    Length
    8~9    Client ID
    10~11  Session ID
    12     Protocol Version
    13     Interface Version
    14     Message Type             REQUEST = 0x00, RESPONSE = 0x80, NOTIFICATION = 0x02
    15     Return Code              E_OK = 0x00     */

    buf_ptr[0] = (uint8_t)((service_id >> 8) & 0xFF);
    buf_ptr[1] = (uint8_t)(service_id & 0xFF);

    buf_ptr[2] = (uint8_t)((method_id >> 8) & 0xFF);
    buf_ptr[3] = (uint8_t)(method_id & 0xFF);

    buf_ptr[4] = (uint8_t)((someip_length >> 24) & 0xFF);
    buf_ptr[5] = (uint8_t)((someip_length >> 16) & 0xFF);
    buf_ptr[6] = (uint8_t)((someip_length >> 8) & 0xFF);
    buf_ptr[7] = (uint8_t)(someip_length & 0xFF);

    buf_ptr[8] = (uint8_t)((client_id >> 8) & 0xFF);
    buf_ptr[9] = (uint8_t)(client_id & 0xFF);

    buf_ptr[10] = (uint8_t)((session_id >> 8) & 0xFF);
    buf_ptr[11] = (uint8_t)(session_id & 0xFF);

    buf_ptr[12] = (uint8_t)SOMEIP_PROTOCOL_VER;

    buf_ptr[13] = (uint8_t)SOMEIP_INTERFACE_VER;
    
    buf_ptr[14] = (uint8_t)message_type;

    buf_ptr[15] = (uint8_t)return_code;

    if(payload_ptr != NULL && payload_len > 0) {
        for(int i = 0; i < payload_len; i++) {
            buf_ptr[SOMEIP_HEADER_LEN + i] = (uint8_t)(payload_ptr[i]);
        }
    }
}

static LightSomeipStatus parse_packet(const uint8_t* packet_ptr, uint32_t paket_len, const char* remote_ip, uint16_t remote_port, ) {
}

static const LightSomeipEndpoint* light_someip_find_endpoint(uint16_t service_id)
{
    if(!g_initialized) {
        return NULL;
    }

    for(int i = 0; i < SOMEIP_MAX_ROUTES; i++) {
        const LightSomeipServiceRoute* route_ptr = &g_routes[i];

        if(route_ptr->service_id == 0x0000) {
            continue;
        }

        if(route_ptr->service_id == service_id) {
            return &(route_ptr->endpoint);
        }
    }

    return NULL;
}
