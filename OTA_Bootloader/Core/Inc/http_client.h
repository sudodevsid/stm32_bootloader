/*
 * Tiny HTTP/1.1 client on top of one W5500 TCP socket. Only what the OTA
 * flow needs: HEAD (to check Content-Length/ETag) and a streaming GET
 * (body delivered via callback in chunks, never buffered whole in RAM).
 */
#ifndef __HTTP_CLIENT_H
#define __HTTP_CLIENT_H

#include "main.h"

typedef struct {
  int      status_code;
  uint32_t content_length;
  char     etag[64];
} http_response_t;

/* Returns 0 on success (response headers parsed), -1 on network/parse failure. */
int http_head(const uint8_t server_ip[4], uint16_t port, const char *host,
              const char *path, http_response_t *resp);

typedef void (*http_body_cb)(const uint8_t *data, uint16_t len, void *ctx);

/* Streams the GET body to cb() as it arrives. Returns total body bytes
 * delivered on success (== resp->content_length), -1 on failure. */
int http_get_stream(const uint8_t server_ip[4], uint16_t port, const char *host,
                     const char *path, http_response_t *resp,
                     http_body_cb cb, void *ctx);

#endif /* __HTTP_CLIENT_H */
