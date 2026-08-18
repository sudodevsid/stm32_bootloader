/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : OTA bootloader - checks the shared control block on
  *                   every boot; if the app requested an update, downloads
  *                   the new firmware from the server over the W5500,
  *                   verifies it, installs it, then jumps to the app.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "w5500.h"
#include "http_client.h"
#include "ota_shared.h"
#include "flash_if.h"
#include "network_config.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint32_t offset;
  uint32_t crc;
  int      error;
} stage_ctx_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
static void LedSet(GPIO_PinState s);
static void LedBlink(uint32_t on_ms, uint32_t off_ms, int cycles);
static int  SanityCheckImage(uint32_t read_from_addr);
static void StageWriteCb(const uint8_t *data, uint16_t len, void *ctx_);
static int  PerformOtaUpdate(ota_ctrl_t *ctrl);
static void JumpToApp(uint32_t app_base) __attribute__((noreturn));
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void LedSet(GPIO_PinState s)
{
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, s);
}

static void LedBlink(uint32_t on_ms, uint32_t off_ms, int cycles)
{
  for (int i = 0; i < cycles; i++) {
    LedSet(GPIO_PIN_SET);
    HAL_Delay(on_ms);
    LedSet(GPIO_PIN_RESET);
    HAL_Delay(off_ms);
  }
}

/* Vector table sanity check: word0 (initial SP) must sit in RAM, word1
 * (reset handler) must point into the app's linked flash range with the
 * Thumb bit set. This is the only integrity check available since the
 * OTA server doesn't publish an independent checksum for the binary. */
static int SanityCheckImage(uint32_t read_from_addr)
{
  uint32_t sp = *(volatile uint32_t *)read_from_addr;
  uint32_t reset_vec = *(volatile uint32_t *)(read_from_addr + 4u);

  if (sp < 0x20000000u || sp > (0x20000000u + 96u * 1024u)) return 0;
  if ((reset_vec & 1u) == 0u) return 0;

  uint32_t addr = reset_vec & ~1u;
  if (addr < OTA_APP_ACTIVE_ADDR || addr >= (OTA_APP_ACTIVE_ADDR + OTA_APP_ACTIVE_SIZE)) return 0;
  return 1;
}

static void StageWriteCb(const uint8_t *data, uint16_t len, void *ctx_)
{
  stage_ctx_t *ctx = (stage_ctx_t *)ctx_;
  if (ctx->error) return;
  if (ctx->offset + len > OTA_APP_STAGING_SIZE) { ctx->error = 1; return; }
  if (FlashIf_Write(OTA_APP_STAGING_ADDR + ctx->offset, data, len) != 0) { ctx->error = 1; return; }
  ctx->crc = OTA_Crc32_Update(ctx->crc, data, len);
  ctx->offset += len;
}

/* Downloads the new firmware into the staging slot, verifies it, and only
 * then copies it into the active slot. Returns 0 on a fully-installed
 * update, -1 on any failure (old app in the active slot is left untouched
 * either way). On success, fills in ctrl's installed_etag/staged_* fields
 * and clears update_pending; the caller still has to persist ctrl. */
static int PerformOtaUpdate(ota_ctrl_t *ctrl)
{
  const uint8_t server_ip[4] = SERVER_IP;

  W5500_Init();
  uint32_t link_start = HAL_GetTick();
  while (!W5500_LinkUp()) {
    if ((HAL_GetTick() - link_start) > 5000u) return -1;
  }

  if (FlashIf_EraseSectors(OTA_APP_STAGING_SECTOR, OTA_APP_STAGING_SECTOR_COUNT) != 0) return -1;

  stage_ctx_t ctx = { 0, 0xFFFFFFFFu, 0 };
  http_response_t resp;
  int n = http_get_stream(server_ip, SERVER_PORT, SERVER_HOST, FIRMWARE_PATH, &resp, StageWriteCb, &ctx);

  if (n < 0 || ctx.error) return -1;
  if ((uint32_t)n != resp.content_length) return -1;
  if (ctx.offset != resp.content_length) return -1;
  if (!SanityCheckImage(OTA_APP_STAGING_ADDR)) return -1;

  uint32_t staged_crc = ctx.crc ^ 0xFFFFFFFFu;

  if (FlashIf_EraseSectors(OTA_APP_ACTIVE_SECTOR, OTA_APP_ACTIVE_SECTOR_COUNT) != 0) return -1;
  if (FlashIf_Write(OTA_APP_ACTIVE_ADDR, (const uint8_t *)OTA_APP_STAGING_ADDR, ctx.offset) != 0) return -1;

  uint32_t verify_crc = OTA_Crc32_Update(0xFFFFFFFFu, (const uint8_t *)OTA_APP_ACTIVE_ADDR, ctx.offset) ^ 0xFFFFFFFFu;
  if (verify_crc != staged_crc) return -1;

  ctrl->staged_length = ctx.offset;
  ctrl->staged_crc32 = staged_crc;
  strncpy(ctrl->installed_etag, resp.etag, sizeof(ctrl->installed_etag) - 1);
  ctrl->installed_etag[sizeof(ctrl->installed_etag) - 1] = 0;
  ctrl->update_pending = 0;
  memset(ctrl->pending_etag, 0, sizeof(ctrl->pending_etag));

  return 0;
}

static void JumpToApp(uint32_t app_base)
{
  uint32_t app_stack = *(volatile uint32_t *)app_base;
  uint32_t app_reset_vector = *(volatile uint32_t *)(app_base + 4u);

  HAL_SPI_DeInit(&hspi1);
  HAL_RCC_DeInit();

  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  __set_MSP(app_stack);
  SCB->VTOR = app_base;

  typedef void (*app_entry_t)(void);
  app_entry_t app_entry = (app_entry_t)app_reset_vector;
  app_entry();

  for (;;) { /* never reached */ }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  ota_ctrl_t ctrl;
  OTA_Ctrl_Load(&ctrl);

  if (ctrl.update_pending) {
    LedSet(GPIO_PIN_SET); /* solid ON while an update is in progress */

    if (PerformOtaUpdate(&ctrl) == 0) {
      OTA_Ctrl_Save(&ctrl);
      LedSet(GPIO_PIN_RESET);
    } else {
      /* Update failed for any reason: clear the pending flag so we don't
       * retry the network on every boot, leave the old (still-good) app
       * untouched, blink an error pattern, then boot it anyway. The app
       * will notice the ETag mismatch again on its next poll and retry. */
      ctrl.update_pending = 0;
      OTA_Ctrl_Save(&ctrl);
      LedBlink(100, 100, 10);
    }
  }

  JumpToApp(OTA_APP_ACTIVE_ADDR);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
