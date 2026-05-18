# light-someip TC375 사용 예제

`light-someip`는 UDP 기반의 최소 SOME/IP Request / Response / Event Notification 예제 라이브러리입니다.

## 네트워크 설정

```text
Vehicle Computer : 192.168.10.1
Front ZCU / TC375: 192.168.10.2

Vehicle Computer Client ID : 0x0001
Front ZCU Client ID        : 0x0002

SOME/IP UDP Port           : 30500
```

TC375의 `Ifx_Lwip.c`에서 정적 IP를 설정합니다.

```c
IP4_ADDR(&default_ipaddr,  192, 168, 10, 2);
IP4_ADDR(&default_netmask, 255, 255, 255, 0);
IP4_ADDR(&default_gw,      0,   0,   0,   0);
```

`lwipopts.h`에서 DHCP를 끕니다.

```c
#define LWIP_DHCP 0
```

`light_someip_udp.c`에서 TC375 backend를 선택합니다.

```c
/* #define SOMEIP_UDP_PLATFORM_RPI */
#define SOMEIP_UDP_PLATFORM_TC375
```

## 최소 예제 코드

아래 예제는 TC375에서 Ethernet/lwIP 초기화 후 `light_someip_init()`을 호출하고, loop에서 수신 처리와 1초 주기 Event Notification을 수행합니다.

```c
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxStm.h"
#include "IfxScuWdt.h"
#include "IfxGeth_Eth.h"
#include "Configuration.h"
#include "ConfigurationIsr.h"
#include "Ifx_Lwip.h"
#include "light_someip.h"

#include <stdint.h>

IfxCpu_syncEvent g_cpuSyncEvent = 0;

static void AppSomeip_Process(const LightSomeipConfig *cfg, uint32_t now_ms);

void core0_main(void) {
    IfxStm_CompareConfig stmCompareConfig;
    eth_addr_t eth_adr;

    IfxCpu_enableInterrupts();
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1u);

    IfxStm_initCompareConfig(&stmCompareConfig);
    stmCompareConfig.triggerPriority     = ISR_PRIORITY_OS_TICK;
    stmCompareConfig.comparatorInterrupt = IfxStm_ComparatorInterrupt_ir0;
    stmCompareConfig.ticks               = IFX_CFG_STM_TICKS_PER_MS * 10u;
    stmCompareConfig.typeOfService       = IfxSrc_Tos_cpu0;
    IfxStm_initCompare(&MODULE_STM0, &stmCompareConfig);

    IfxGeth_enableModule(&MODULE_GETH);

    eth_adr.addr[0] = 0xDE;
    eth_adr.addr[1] = 0xAD;
    eth_adr.addr[2] = 0xBE;
    eth_adr.addr[3] = 0xEF;
    eth_adr.addr[4] = 0xFE;
    eth_adr.addr[5] = 0xED;
    Ifx_Lwip_init(eth_adr);

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

    while (1) {
        Ifx_Lwip_pollTimerFlags();
        Ifx_Lwip_pollReceiveFlags();
        AppSomeip_Process(&someip_cfg, (uint32_t)g_TickCount_1ms);
    }
}

IFX_INTERRUPT(update_lwip_stack_isr, 0, ISR_PRIORITY_OS_TICK);

void update_lwip_stack_isr(void) {
    IfxStm_increaseCompare(&MODULE_STM0, IfxStm_Comparator_0, IFX_CFG_STM_TICKS_PER_MS);
    g_TickCount_1ms++;
    Ifx_Lwip_onTimerTick();
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

## API 사용 요약

```c
light_someip_recv(&rx, remote_ip, &remote_port);
light_someip_respond(remote_ip, remote_port, &rx, &tx);

light_someip_packet_init(&tx, service_id, method_id, payload, payload_len);
light_someip_request(&tx);

light_someip_packet_init(&tx, service_id, event_id, payload, payload_len);
light_someip_event_notify(&endpoint, &tx);
```
