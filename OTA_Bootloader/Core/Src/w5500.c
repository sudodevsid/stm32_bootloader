#include "w5500.h"
#include "network_config.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;

/* ---- W5500 SPI frame: 2-byte address, 1-byte control (BSB|RWB|OM=VDM), N data bytes ---- */
#define OM_VDM          0x00u
#define RWB_READ        0x00u
#define RWB_WRITE       0x04u

#define BSB_COMMON      0x00u
#define BSB_SOCK_REG(n)   (uint8_t)(0x01u + 4u * (n))
#define BSB_SOCK_TX(n)    (uint8_t)(0x02u + 4u * (n))
#define BSB_SOCK_RX(n)    (uint8_t)(0x03u + 4u * (n))

#define SOCK_BUF_SIZE   2048u

/* Common registers */
#define REG_MR          0x0000u
#define REG_GAR         0x0001u
#define REG_SUBR        0x0005u
#define REG_SHAR        0x0009u
#define REG_SIPR        0x000Fu
#define REG_PHYCFGR     0x002Eu

/* Socket registers (offsets within a socket's register block) */
#define SREG_MR         0x0000u
#define SREG_CR         0x0001u
#define SREG_IR         0x0002u
#define SREG_SR         0x0003u
#define SREG_PORT       0x0004u
#define SREG_DIPR       0x000Cu
#define SREG_DPORT      0x0010u
#define SREG_TX_FSR     0x0020u
#define SREG_TX_RD      0x0022u
#define SREG_TX_WR      0x0024u
#define SREG_RX_RSR     0x0026u
#define SREG_RX_RD      0x0028u

/* Socket commands */
#define SCMD_OPEN       0x01u
#define SCMD_CONNECT    0x04u
#define SCMD_DISCON     0x08u
#define SCMD_CLOSE      0x10u
#define SCMD_SEND       0x20u
#define SCMD_RECV       0x40u

/* Socket status */
#define SSR_CLOSED      0x00u
#define SSR_INIT        0x13u
#define SSR_ESTABLISHED 0x17u
#define SSR_CLOSE_WAIT  0x1Cu

/* Socket mode */
#define SMR_TCP         0x01u

static void CS_Low(void)  { HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET); }
static void CS_High(void) { HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET); }

static void SPI_Xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, HAL_MAX_DELAY);
}

/* Scratch buffer for the "other side" of a transfer we don't care about
 * (zero-filled dummy TX while reading, discarded RX while writing).
 * Sized for the largest single transfer we ever do: one socket buffer. */
static uint8_t s_scratch[SOCK_BUF_SIZE];

static void W5500_ReadN(uint16_t addr, uint8_t bsb, uint8_t *buf, uint16_t len)
{
  uint8_t hdr[3] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),
                     (uint8_t)((bsb << 3) | RWB_READ | OM_VDM) };
  CS_Low();
  SPI_Xfer(hdr, s_scratch, 3);
  memset(s_scratch, 0, len);
  SPI_Xfer(s_scratch, buf, len);
  CS_High();
}

static void W5500_WriteN(uint16_t addr, uint8_t bsb, const uint8_t *buf, uint16_t len)
{
  uint8_t hdr[3] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),
                     (uint8_t)((bsb << 3) | RWB_WRITE | OM_VDM) };
  CS_Low();
  SPI_Xfer(hdr, s_scratch, 3);
  SPI_Xfer((uint8_t *)buf, s_scratch, len);
  CS_High();
}

static uint8_t Reg_Read8(uint16_t addr)
{
  uint8_t v;
  W5500_ReadN(addr, BSB_COMMON, &v, 1);
  return v;
}

static void Reg_Write8(uint16_t addr, uint8_t v)
{
  W5500_WriteN(addr, BSB_COMMON, &v, 1);
}

static uint8_t SReg_Read8(uint8_t sock, uint16_t off)
{
  uint8_t v;
  W5500_ReadN(off, BSB_SOCK_REG(sock), &v, 1);
  return v;
}

static void SReg_Write8(uint8_t sock, uint16_t off, uint8_t v)
{
  W5500_WriteN(off, BSB_SOCK_REG(sock), &v, 1);
}

static uint16_t SReg_Read16(uint8_t sock, uint16_t off)
{
  uint8_t b[2];
  W5500_ReadN(off, BSB_SOCK_REG(sock), b, 2);
  return (uint16_t)((b[0] << 8) | b[1]);
}

static void SReg_Write16(uint8_t sock, uint16_t off, uint16_t v)
{
  uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
  W5500_WriteN(off, BSB_SOCK_REG(sock), b, 2);
}

static void SCmd(uint8_t sock, uint8_t cmd)
{
  SReg_Write8(sock, SREG_CR, cmd);
  while (SReg_Read8(sock, SREG_CR) != 0) { /* wait for command to be accepted */ }
}

void W5500_Init(void)
{
  const uint8_t mac[6]    = DEVICE_MAC;
  const uint8_t ip[4]     = DEVICE_IP;
  const uint8_t gw[4]     = DEVICE_GATEWAY;
  const uint8_t subnet[4] = DEVICE_SUBNET;

  CS_High();

  /* Software reset (MR.RST self-clears) */
  Reg_Write8(REG_MR, 0x80);
  HAL_Delay(2);

  W5500_WriteN(REG_GAR, BSB_COMMON, gw, 4);
  W5500_WriteN(REG_SUBR, BSB_COMMON, subnet, 4);
  W5500_WriteN(REG_SHAR, BSB_COMMON, mac, 6);
  W5500_WriteN(REG_SIPR, BSB_COMMON, ip, 4);
}

int W5500_LinkUp(void)
{
  return (Reg_Read8(REG_PHYCFGR) & 0x01) ? 1 : 0;
}

int W5500_TCP_Connect(uint8_t sock, const uint8_t dst_ip[4], uint16_t dst_port,
                       uint16_t src_port, uint32_t timeout_ms)
{
  SCmd(sock, SCMD_CLOSE);
  SReg_Write8(sock, SREG_IR, 0xFF); /* clear any stale interrupt flags */

  SReg_Write8(sock, SREG_MR, SMR_TCP);
  SReg_Write16(sock, SREG_PORT, src_port);
  SCmd(sock, SCMD_OPEN);
  if (SReg_Read8(sock, SREG_SR) != SSR_INIT) {
    return -1;
  }

  W5500_WriteN(SREG_DIPR, BSB_SOCK_REG(sock), dst_ip, 4);
  SReg_Write16(sock, SREG_DPORT, dst_port);
  SCmd(sock, SCMD_CONNECT);

  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout_ms) {
    uint8_t sr = SReg_Read8(sock, SREG_SR);
    if (sr == SSR_ESTABLISHED) {
      return 0;
    }
    if (sr == SSR_CLOSED) {
      return -1;
    }
  }
  SCmd(sock, SCMD_CLOSE);
  return -1;
}

int W5500_TCP_Send(uint8_t sock, const uint8_t *buf, uint16_t len)
{
  uint16_t sent = 0;
  while (sent < len) {
    uint16_t free_size = SReg_Read16(sock, SREG_TX_FSR);
    if (free_size == 0) {
      if (SReg_Read8(sock, SREG_SR) != SSR_ESTABLISHED) return -1;
      continue;
    }
    uint16_t chunk = (uint16_t)(len - sent);
    if (chunk > free_size) chunk = free_size;

    uint16_t wr = SReg_Read16(sock, SREG_TX_WR);
    uint16_t offset = wr & (SOCK_BUF_SIZE - 1);
    uint16_t first = (uint16_t)(SOCK_BUF_SIZE - offset);
    if (first > chunk) first = chunk;

    W5500_WriteN(offset, BSB_SOCK_TX(sock), &buf[sent], first);
    if (chunk > first) {
      W5500_WriteN(0, BSB_SOCK_TX(sock), &buf[sent + first], (uint16_t)(chunk - first));
    }
    SReg_Write16(sock, SREG_TX_WR, (uint16_t)(wr + chunk));
    SCmd(sock, SCMD_SEND);

    uint32_t start = HAL_GetTick();
    for (;;) {
      uint8_t ir = SReg_Read8(sock, SREG_IR);
      if (ir & 0x08) { /* TIMEOUT */
        SReg_Write8(sock, SREG_IR, 0x08);
        return -1;
      }
      if (ir & 0x10) { /* SENDOK */
        SReg_Write8(sock, SREG_IR, 0x10);
        break;
      }
      if ((HAL_GetTick() - start) > 3000u) return -1;
    }
    sent = (uint16_t)(sent + chunk);
  }
  return (int)sent;
}

int W5500_TCP_Recv(uint8_t sock, uint8_t *buf, uint16_t maxlen, uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  uint16_t rsr = 0;

  while ((HAL_GetTick() - start) < timeout_ms) {
    rsr = SReg_Read16(sock, SREG_RX_RSR);
    if (rsr > 0) break;
    uint8_t sr = SReg_Read8(sock, SREG_SR);
    if (sr != SSR_ESTABLISHED && sr != SSR_CLOSE_WAIT) {
      return -1;
    }
  }
  if (rsr == 0) {
    uint8_t sr = SReg_Read8(sock, SREG_SR);
    return (sr == SSR_CLOSE_WAIT) ? -1 : 0;
  }

  uint16_t to_read = (rsr < maxlen) ? rsr : maxlen;
  uint16_t rd = SReg_Read16(sock, SREG_RX_RD);
  uint16_t offset = rd & (SOCK_BUF_SIZE - 1);
  uint16_t first = (uint16_t)(SOCK_BUF_SIZE - offset);
  if (first > to_read) first = to_read;

  W5500_ReadN(offset, BSB_SOCK_RX(sock), buf, first);
  if (to_read > first) {
    W5500_ReadN(0, BSB_SOCK_RX(sock), &buf[first], (uint16_t)(to_read - first));
  }
  SReg_Write16(sock, SREG_RX_RD, (uint16_t)(rd + to_read));
  SCmd(sock, SCMD_RECV);

  return (int)to_read;
}

void W5500_TCP_Close(uint8_t sock)
{
  SCmd(sock, SCMD_DISCON);
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < 1000u) {
    if (SReg_Read8(sock, SREG_SR) == SSR_CLOSED) break;
  }
  SCmd(sock, SCMD_CLOSE);
}
