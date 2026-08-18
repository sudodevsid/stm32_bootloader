/*
 * Minimal raw-register W5500 driver: soft reset, static IP config, and a
 * single TCP client hardware socket (open/connect/send/recv/close).
 * No interrupt pin is wired on this board, so socket status is polled.
 * Used by both the bootloader and the app over the shared hspi1 + CS_Pin.
 */
#ifndef __W5500_H
#define __W5500_H

#include "main.h"

#define W5500_SOCK_HTTP   0u

void W5500_Init(void);
int  W5500_LinkUp(void);

/* Blocking TCP client connect. Returns 0 on success, -1 on failure/timeout. */
int W5500_TCP_Connect(uint8_t sock, const uint8_t dst_ip[4], uint16_t dst_port,
                       uint16_t src_port, uint32_t timeout_ms);

/* Returns bytes sent (== len) on success, -1 on failure. */
int W5500_TCP_Send(uint8_t sock, const uint8_t *buf, uint16_t len);

/* Returns bytes read (>0), 0 on timeout with no data, -1 if peer closed/error. */
int W5500_TCP_Recv(uint8_t sock, uint8_t *buf, uint16_t maxlen, uint32_t timeout_ms);

void W5500_TCP_Close(uint8_t sock);

#endif /* __W5500_H */
