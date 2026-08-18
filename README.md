# STM32 OTA Bootloader

Humne apna ek **OTA (Over-The-Air) update system** banaya hai jo STM32 microcontroller ke liye kaam karta hai. Device khud AWS server se latest firmware ka code check karta hai, use download karta hai, verify karta hai, aur khud ko automatically flash karke reboot/boot kar leta hai — bina kisi manual USB/ST-Link connection ke.

Ye ek **generic OTA infrastructure** hai — iska firmware server (Node.js backend, AWS par deployed) aur STM32 side ka bootloader flow kisi bhi device/site par **subscription-based model** me deploy kiya ja sakta hai. Matlab kal koi bhi client apna hardware laaye, hum unke liye ek firmware server instance + bootloader flash karke OTA update service subscription par de sakte hai — bina unki site par baar-baar jaake manually firmware update kiye.

## Yeh kaam kaise karta hai

1. **App image** har `OTA_POLL_INTERVAL_MS` (10 sec) par W5500 Ethernet module ke through AWS server ko HTTP `HEAD` request bhejta hai (`/api/firmware/latest`) aur server ka `ETag` check karta hai.
2. Agar server ka `ETag` device par installed version se match nahi karta, app apni shared control block me `update_pending = 1` likhkar reset kar deta hai.
3. Reset hone par **bootloader** boot hota hai, control block dekhta hai, aur:
   - Naya firmware server se stream karke flash ke "staging" slot me likhta hai (RAM me poora buffer kiye bina).
   - CRC32 se integrity verify karta hai + vector table sanity check karta hai.
   - Sab OK hone par staging se "active" slot me copy karta hai, dobara verify karta hai.
   - Verify pass hone par naya app boot kar deta hai; fail hone par purana (already-working) app hi boot hota hai — device kabhi "bricked" nahi hota.
4. Agar koi update pending nahi hai, bootloader seedha existing app boot kar deta hai.

Yeh sab **fully automatic** hai — device apne aap latest code AWS se lekar khud update ho jaata hai, kisi insaan ki zarurat nahi.

## Architecture

```
              ┌──────────────────────┐
              │   AWS EC2 Server     │
              │  Node.js backend     │
              │  /api/firmware/latest│
              └──────────┬───────────┘
                          │ HTTP (W5500 Ethernet)
                          ▼
   ┌────────────────────────────────────────────┐
   │              STM32F401RE (flash)            │
   │                                              │
   │  Sector 0-2  : Bootloader   (48 KB)          │
   │  Sector 3    : OTA control block (16 KB)     │
   │  Sector 4-5  : App ACTIVE slot  (192 KB)     │
   │  Sector 6-7  : App STAGING slot (256 KB)     │
   └────────────────────────────────────────────┘
```

- **App** polls the server, compares versions, and only ever *requests* an update.
- **Bootloader** is the only thing that downloads, flashes, and verifies — the running app is never overwritten until the new image is fully verified in staging.
- A shared **OTA control block** (magic + pending flag + ETag + CRC32) hands state between the app and bootloader across a reset.

## Repo structure

| Path              | Kya hai |
|-------------------|---------|
| `OTA/`             | STM32CubeIDE project — application firmware (LED blink + version polling) |
| `OTA_Bootloader/`  | STM32CubeIDE project — bootloader (download / flash / verify / boot) |
| `nodebackened/`    | Node.js/Express backend jo firmware serve karta hai (AWS par deployed) |
| `firmware/`        | Server ka runtime storage — uploaded `.bin`/`.hex` aur manifest (git-ignored, sirf `.gitkeep`) |

## Hardware

- MCU: STM32F401RETx
- Ethernet: WIZnet W5500 (SPI)
- Flash layout aur pin config `OTA/Core/Inc` aur `OTA_Bootloader/Core/Inc` me hai

## Configuration

`Core/Inc/network_config.h` (dono projects me identical) me device ki static IP aur OTA server ka address set hota hai — naya deployment/site ke liye sirf yeh file edit karke rebuild karna hota hai:

```c
#define DEVICE_IP       {192, 168, 1, 177}
#define SERVER_IP       {44, 223, 17, 249}   // AWS server
#define SERVER_HOST     "44.223.17.249"
#define FIRMWARE_PATH   "/api/firmware/latest"
```

## Server API (`nodebackened/`)

| Method | Path                  | Kaam |
|--------|-----------------------|------|
| POST   | `/api/firmware/upload`| Naya firmware (`.bin`/`.hex`) upload karo — auto version bump |
| GET    | `/api/firmware/latest` | Latest firmware download (yahi path device poll karta hai) |
| GET    | `/health`              | Server health check |

```bash
cd nodebackened
npm install
npm start
```

## Multi-site / subscription model

Yeh setup ek hi codebase se **kisi bhi client ke liye reusable** hai:

1. Client ke hardware me sirf `network_config.h` ka `SERVER_IP`/`SERVER_HOST` unke dedicated OTA server par point karo aur bootloader + app flash karo (one-time, physical).
2. Uske baad har firmware update sirf server par naya `.bin`/`.hex` upload karne se sab connected devices tak automatically pahunch jaata hai.
3. Har client/site ka apna server instance (ya same AWS server par isolated path) subscription ke basis par maintain kiya ja sakta hai — bina site par dobara jaaye.

## Build & Flash

Dono projects (`OTA/`, `OTA_Bootloader/`) STM32CubeIDE projects hain:

1. STM32CubeIDE me `OTA_Bootloader/` import karo, build karo, board par flash karo (address `0x08000000`).
2. `OTA/` import karo, build karo, active slot address (`0x08010000`) par flash karo.
3. Board reset karo — bootloader app boot karega; app har 10 sec me server check karega.
