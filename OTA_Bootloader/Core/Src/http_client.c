#include "http_client.h"
#include "w5500.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define HDR_BUF_SIZE 1024

static uint8_t s_buf[HDR_BUF_SIZE];

static int starts_with_ci(const char *s, const char *prefix)
{
  while (*prefix) {
    if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
    s++;
    prefix++;
  }
  return 1;
}

static void parse_headers(char *text, http_response_t *resp)
{
  resp->status_code = 0;
  resp->content_length = 0;
  resp->etag[0] = 0;

  int first = 1;
  char *line = strtok(text, "\r\n");
  while (line) {
    if (first) {
      first = 0;
      char *sp = strchr(line, ' ');
      if (sp) resp->status_code = (int)strtol(sp + 1, NULL, 10);
    } else if (starts_with_ci(line, "content-length:")) {
      char *v = line + strlen("content-length:");
      while (*v == ' ') v++;
      resp->content_length = (uint32_t)strtoul(v, NULL, 10);
    } else if (starts_with_ci(line, "etag:")) {
      char *v = line + strlen("etag:");
      while (*v == ' ') v++;
      strncpy(resp->etag, v, sizeof(resp->etag) - 1);
      resp->etag[sizeof(resp->etag) - 1] = 0;
    }
    line = strtok(NULL, "\r\n");
  }
}

/* Reads from sock until the header/body separator is found, parses the
 * headers, and reports where in s_buf the body (if any arrived already
 * in the same read) starts. */
static int recv_headers(uint8_t sock, http_response_t *resp, uint16_t *body_off, uint16_t *buf_len)
{
  uint16_t total = 0;
  uint32_t stall_start = HAL_GetTick();

  for (;;) {
    if (total >= HDR_BUF_SIZE) return -1; /* malformed / oversized headers */

    int n = W5500_TCP_Recv(sock, &s_buf[total], (uint16_t)(HDR_BUF_SIZE - total), 5000);
    if (n < 0) return -1;
    if (n == 0) {
      if ((HAL_GetTick() - stall_start) > 5000u) return -1;
      continue;
    }
    stall_start = HAL_GetTick();
    total = (uint16_t)(total + n);

    for (uint16_t i = 0; (uint16_t)(i + 4) <= total; i++) {
      if (s_buf[i] == '\r' && s_buf[i + 1] == '\n' && s_buf[i + 2] == '\r' && s_buf[i + 3] == '\n') {
        s_buf[i] = 0; /* null-terminate header text for strtok/strchr */
        parse_headers((char *)s_buf, resp);
        *body_off = (uint16_t)(i + 4);
        *buf_len = total;
        return 0;
      }
    }
  }
}

static void build_request(char *req, size_t cap, const char *method, const char *host, const char *path)
{
  req[0] = 0;
  strncat(req, method, cap - strlen(req) - 1);
  strncat(req, " ", cap - strlen(req) - 1);
  strncat(req, path, cap - strlen(req) - 1);
  strncat(req, " HTTP/1.1\r\nHost: ", cap - strlen(req) - 1);
  strncat(req, host, cap - strlen(req) - 1);
  strncat(req, "\r\nConnection: close\r\n\r\n", cap - strlen(req) - 1);
}

int http_head(const uint8_t server_ip[4], uint16_t port, const char *host,
              const char *path, http_response_t *resp)
{
  if (W5500_TCP_Connect(W5500_SOCK_HTTP, server_ip, port, 51000, 5000) != 0) return -1;

  char req[256];
  build_request(req, sizeof(req), "HEAD", host, path);
  if (W5500_TCP_Send(W5500_SOCK_HTTP, (uint8_t *)req, (uint16_t)strlen(req)) < 0) {
    W5500_TCP_Close(W5500_SOCK_HTTP);
    return -1;
  }

  uint16_t body_off, buf_len;
  int rc = recv_headers(W5500_SOCK_HTTP, resp, &body_off, &buf_len);
  W5500_TCP_Close(W5500_SOCK_HTTP);
  return rc;
}

int http_get_stream(const uint8_t server_ip[4], uint16_t port, const char *host,
                     const char *path, http_response_t *resp,
                     http_body_cb cb, void *ctx)
{
  if (W5500_TCP_Connect(W5500_SOCK_HTTP, server_ip, port, 51001, 5000) != 0) return -1;

  char req[256];
  build_request(req, sizeof(req), "GET", host, path);
  if (W5500_TCP_Send(W5500_SOCK_HTTP, (uint8_t *)req, (uint16_t)strlen(req)) < 0) {
    W5500_TCP_Close(W5500_SOCK_HTTP);
    return -1;
  }

  uint16_t body_off, buf_len;
  if (recv_headers(W5500_SOCK_HTTP, resp, &body_off, &buf_len) != 0) {
    W5500_TCP_Close(W5500_SOCK_HTTP);
    return -1;
  }
  if (resp->status_code != 200) {
    W5500_TCP_Close(W5500_SOCK_HTTP);
    return -1;
  }

  uint32_t received = 0;
  if (buf_len > body_off) {
    uint16_t first_chunk = (uint16_t)(buf_len - body_off);
    cb(&s_buf[body_off], first_chunk, ctx);
    received += first_chunk;
  }

  static uint8_t chunk[1024];
  uint32_t stall_start = HAL_GetTick();
  while (received < resp->content_length) {
    int n = W5500_TCP_Recv(W5500_SOCK_HTTP, chunk, sizeof(chunk), 5000);
    if (n < 0) {
      W5500_TCP_Close(W5500_SOCK_HTTP);
      return -1;
    }
    if (n == 0) {
      if ((HAL_GetTick() - stall_start) > 10000u) {
        W5500_TCP_Close(W5500_SOCK_HTTP);
        return -1;
      }
      continue;
    }
    stall_start = HAL_GetTick();
    cb(chunk, (uint16_t)n, ctx);
    received += (uint32_t)n;
  }

  W5500_TCP_Close(W5500_SOCK_HTTP);
  return (int)received;
}
