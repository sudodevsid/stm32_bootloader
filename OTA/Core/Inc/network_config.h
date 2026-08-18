/*
 * Edit these to match YOUR LAN and OTA server.
 *
 * DEVICE_* is the static IP this board takes on your local network
 * (your router's subnet) - it has nothing to do with the server's address.
 * SERVER_* is the internet-facing OTA server the device connects OUT to;
 * your router/gateway NATs that connection to the internet, so
 * DEVICE_GATEWAY must be your router's LAN IP for this to work.
 */
#ifndef __NETWORK_CONFIG_H
#define __NETWORK_CONFIG_H

/* --- Device static IP (edit to match your LAN) --- */
#define DEVICE_MAC      {0x02, 0x00, 0x00, 0x44, 0x45, 0x4B}   /* locally-administered MAC, must be unique on your LAN */
#define DEVICE_IP       {192, 168, 1, 177}
#define DEVICE_GATEWAY  {192, 168, 1, 1}
#define DEVICE_SUBNET   {255, 255, 255, 0}

/* --- OTA firmware server (already confirmed reachable) --- */
#define SERVER_IP       {44, 223, 17, 249}
#define SERVER_PORT     3000
#define SERVER_HOST     "44.223.17.249"
#define FIRMWARE_PATH   "/api/firmware/latest"

/* Poll interval for the app's version check (ms) */
#define OTA_POLL_INTERVAL_MS   10000u

#endif /* __NETWORK_CONFIG_H */
