# light-someip

`light-someip`는 UDP 기반의 가벼운 SOME/IP 라이브러리입니다.

AUTOSAR SOME/IP 전체 스택을 구현하는 목적이 아니라, 임베디드 예제나 작은 프로젝트에서 Request, Response, Event Notification 흐름을 단순하게 다루기 위한 경량 라이브러리입니다.

## 주요 기능

- SOME/IP 기본 헤더 생성 및 파싱
- Request 송신
- Request에 대한 Response 송신
- Event Notification 송신
- non-blocking 수신 처리
- 정적 route table 기반 목적지 선택
- UDP transport 계층 분리

## 파일 구성

```text
light_someip.h       Public API, packet/config 구조체, 상태 코드
light_someip.c       SOME/IP packet 생성, 파싱, 송수신 흐름
light_someip_udp.h   UDP backend 인터페이스
light_someip_udp.c   플랫폼별 UDP 구현
```

`light_someip.c`는 SOME/IP 계층만 담당하고, 실제 UDP 송수신은 `light_someip_udp.c`로 분리되어 있습니다.

## 플랫폼 선택

`light_someip_udp.c`에서 사용할 UDP backend를 하나만 선택합니다.

```c
/* #define SOMEIP_UDP_PLATFORM_RPI */
#define SOMEIP_UDP_PLATFORM_TC375
```

현재 backend는 다음 두 가지 형태를 기준으로 나뉘어 있습니다.

```text
SOMEIP_UDP_PLATFORM_RPI    Linux socket 기반 UDP
SOMEIP_UDP_PLATFORM_TC375  TC375 + lwIP raw UDP
```

## 기본 사용 흐름

1. `LightSomeipConfig`에 local client ID, local UDP port, service route를 채웁니다.
2. `light_someip_init()`으로 라이브러리를 초기화합니다.
3. 주기적으로 `light_someip_recv()`를 호출해 수신 packet을 확인합니다.
4. Request를 보내려면 `light_someip_request()`를 사용합니다.
5. 받은 Request에 응답하려면 `light_someip_respond()`를 사용합니다.
6. Event를 보내려면 `light_someip_event_notify()`를 사용합니다.

## 설정 예시

정적 route table은 `service_id`를 기준으로 목적지 IP와 port를 찾습니다.

```c
LightSomeipConfig someip_cfg = {
    .client_id = 0x0002u,
    .port = 30500u,
    .routes = {
        { .service_id = 0x0001u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } }, /* DriveService */
        { .service_id = 0x0002u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } }, /* SensorService */
        { .service_id = 0x0006u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } }, /* AebService */
        { .service_id = 0x0007u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } }  /* InfoService */
    }
};

(void)light_someip_init(&someip_cfg);
```

## 예시 코드

아래 코드는 application loop에서 수신 Request를 처리하고, 1초마다 Event Notification을 보내는 예시입니다.

```c
#include "light_someip.h"

#include <stdint.h>

static void AppSomeip_Process(const LightSomeipConfig *cfg, uint32_t now_ms);

int main(void) {
    LightSomeipConfig someip_cfg = {
        .client_id = 0x0002u,
        .port = 30500u,
        .routes = {
            { .service_id = 0x0001u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } },
            { .service_id = 0x0002u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } },
            { .service_id = 0x0006u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } },
            { .service_id = 0x0007u, .endpoint = { .ip = "192.168.10.1", .port = 30500u } }
        }
    };

    if (light_someip_init(&someip_cfg) != SOMEIP_OK) {
        return -1;
    }

    while (1) {
        AppSomeip_Process(&someip_cfg, GetTickMs());
    }
}

static void AppSomeip_Process(const LightSomeipConfig *cfg, uint32_t now_ms) {
    static uint32_t last_event_tick_ms = 0u;

    LightSomeipPacket rx;
    LightSomeipPacket tx;
    char remote_ip[SOMEIP_IP_LEN];
    uint16_t remote_port;

    if (light_someip_recv(&rx, remote_ip, &remote_port) == SOMEIP_OK) {
        uint8_t is_version_request = (rx.message_type == SOMEIP_MSGTYPE_REQUEST) && (rx.service_id == 0x0007u) && (rx.method_id == 0x1003u);

        if (is_version_request != 0u) {
            const uint8_t payload[] = "1.0.0";
            uint32_t payload_len = (uint32_t)(sizeof(payload) - 1u);

            if (light_someip_packet_init(&tx, rx.service_id, rx.method_id, payload, payload_len) == SOMEIP_OK) {
                (void)light_someip_respond(remote_ip, remote_port, &rx, &tx);
            }
        }
    }

    if ((now_ms - last_event_tick_ms) >= 1000u) {
        uint8_t payload[4];
        uint32_t payload_len = (uint32_t)sizeof(payload);

        last_event_tick_ms = now_ms;

        payload[0] = (uint8_t)((now_ms >> 24) & 0xFFu);
        payload[1] = (uint8_t)((now_ms >> 16) & 0xFFu);
        payload[2] = (uint8_t)((now_ms >> 8) & 0xFFu);
        payload[3] = (uint8_t)(now_ms & 0xFFu);

        if (light_someip_packet_init(&tx, 0x0002u, 0x2003u, payload, payload_len) == SOMEIP_OK) {
            (void)light_someip_event_notify(&cfg->routes[1].endpoint, &tx);
        }
    }
}
```

`GetTickMs()`는 예시용 함수입니다. 사용하는 플랫폼의 tick counter나 timer 값으로 바꿔 사용하면 됩니다.

## API 요약

```c
LightSomeipStatus light_someip_init(const LightSomeipConfig *config_ptr);
LightSomeipStatus light_someip_packet_init(LightSomeipPacket *packet_ptr, uint16_t service_id, uint16_t method_id, const uint8_t *payload_arr, uint32_t payload_len);

LightSomeipStatus light_someip_request(LightSomeipPacket *packet_ptr);
LightSomeipStatus light_someip_respond(const char *remote_ip, uint16_t remote_port, const LightSomeipPacket *request_packet_ptr, LightSomeipPacket *response_packet_ptr);
LightSomeipStatus light_someip_event_notify(const LightSomeipEndpoint *dst_endpoint_ptr, LightSomeipPacket *packet_ptr);
LightSomeipStatus light_someip_recv(LightSomeipPacket *packet_ptr, char remote_ip[SOMEIP_IP_LEN], uint16_t *remote_port);
```

## 참고

- Service Discovery는 포함하지 않습니다.
- route는 정적으로 설정합니다.
- payload 최대 길이는 `SOMEIP_MAX_PAYLOAD_LEN` 기준입니다.
