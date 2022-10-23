/**
\file
\author JavaLandau
\copyright MIT License
\brief BSP UART module

The file includes implementation of BSP layer of the UART
*/

#include "common.h"
#include "bsp_uart.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx_ll_usart.h"

/** 
 * \defgroup bsp BSP
 * \brief Board suppport package
*/

/** 
 * \defgroup bsp_uart BSP UART
 * \brief Module of BSP UART
 * \ingroup bsp
 * @{
*/

/** MACRO BSP UART word length typecasting
 * 
 * The macro makes typecasting of \ref uart_wordlen to STM32 HAL UART word length
 * 
 * \param[in] X BSP UART word length
 * \return STM32 HAL UART word length
*/
#define HAL_UART_WORDLEN_TO(X)     (((X) == BSP_UART_WORDLEN_8) ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B)

/** MACRO BSP UART stop bits count typecasting
 * 
 * The macro makes typecasting of \ref uart_stopbits to STM32 HAL UART stop bits count
 * 
 * \param[in] X BSP UART stop bits count
 * \return STM32 HAL UART stop bits count
*/
#define HAL_UART_STOPBITS_TO(X)    (((X) == BSP_UART_STOPBITS_1) ? UART_STOPBITS_1 : UART_STOPBITS_2)

/** MACRO BSP UART parity type typecasting
 * 
 * The macro makes typecasting of \ref uart_parity to STM32 HAL UART parity type
 * 
 * \param[in] X BSP UART parity type
 * \return STM32 HAL UART parity type
*/
#define HAL_UART_PARITY_TO(X)      (((X) == BSP_UART_PARITY_NONE) ? UART_PARITY_NONE : \
                                    (((X) == BSP_UART_PARITY_EVEN) ? UART_PARITY_EVEN : UART_PARITY_ODD))

/// Context of the BSP UART instance
struct uart_ctx {
    struct uart_init_ctx init;  ///< Initializing context of the instance
    void *tx_buff;              ///< Sent buffer used by DMA TX
    void *rx_buff;              ///< Received buffer used by DMA RX
    uint16_t rx_idx_get;        ///< Read poisition in \ref rx_buff used as ring buffer
    uint16_t rx_idx_set;        ///< Write poisition in \ref rx_buff used as ring buffer
    bool frame_error;           ///< Flag whetner UART frame error is occured, used to separate LIN break from other frame errors
};

/** Array of BSP UART instances
 * 
 * Includes three instances:  
 * \ref BSP_UART_TYPE_CLI       - CLI using STM32 UART4 TX/RX  
 * \ref BSP_UART_TYPE_RS232_TX  - RS-232 TX channel using STM32 USART2 RX  
 * \ref BSP_UART_TYPE_RS232_RX  - RS-232 RX channel using STM32 USART3 RX
*/
static struct {
    UART_HandleTypeDef uart;    ///< STM32 HAL UART instance
    struct uart_ctx *ctx;       ///< Context of the instance
} uart_obj[BSP_UART_TYPE_MAX] = {
    {.uart = {.Instance = UART4}, .ctx = NULL},
    {.uart = {.Instance = USART2}, .ctx = NULL},
    {.uart = {.Instance = USART3}, .ctx = NULL}
};

/** Get BSP UART type by STM32 HAL UART instance
 * 
 * \param[in] instance STM32 HAL UART instance
 * \return BSP UART type on success \ref BSP_UART_TYPE_MAX otherwise
 */
static enum uart_type __uart_type_get(USART_TypeDef *instance)
{
    if (instance == UART4)
        return BSP_UART_TYPE_CLI;
    else if (instance == USART2)
        return BSP_UART_TYPE_RS232_TX;
    else if (instance == USART3)
        return BSP_UART_TYPE_RS232_RX;
    else
        return BSP_UART_TYPE_MAX;
}

/** STM32 DMA UART deinitialization
 * 
 * The function executes DMA TX/RX (according to BSP UART type) deinitialization
 * 
 * \param[in] type BSP UART type
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __uart_dma_deinit(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    switch (type) {
    case BSP_UART_TYPE_CLI:
        HAL_NVIC_DisableIRQ(DMA1_Stream2_IRQn);
        HAL_NVIC_DisableIRQ(DMA1_Stream4_IRQn);
        break;

    case BSP_UART_TYPE_RS232_TX:
        HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
        break;

    case BSP_UART_TYPE_RS232_RX:
        HAL_NVIC_DisableIRQ(DMA1_Stream1_IRQn);
        break;

    default:
        break;
    }

    DMA_HandleTypeDef *hdma_tx = uart_obj[type].uart.hdmatx;
    DMA_HandleTypeDef *hdma_rx = uart_obj[type].uart.hdmarx;

    if (hdma_tx) {
        if (HAL_DMA_DeInit(hdma_tx) != HAL_OK)
            return RES_NOK;

        uart_obj[type].uart.hdmatx = NULL;
        free(hdma_tx);
    }

    if (hdma_rx) {
        if (HAL_DMA_DeInit(hdma_rx) != HAL_OK)
            return RES_NOK;

        uart_obj[type].uart.hdmarx = NULL;
        free(hdma_rx);
    }

    return RES_OK;
}

/** STM32 UART MSP deinitialization
 * 
 * The function executes GPIO, DMA deinitialization, disables corresponding clocks
 * 
 * \param[in] type BSP UART type
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __uart_msp_deinit(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    uint8_t res = __uart_dma_deinit(type);

    if (res != RES_OK)
        return res;

    switch (type) {
    case BSP_UART_TYPE_CLI:
        HAL_NVIC_DisableIRQ(UART4_IRQn);
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1);
        __HAL_RCC_UART4_CLK_DISABLE();

        break;

    case BSP_UART_TYPE_RS232_TX:
        HAL_NVIC_DisableIRQ(USART2_IRQn);
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_3);
        __HAL_RCC_USART2_CLK_DISABLE();

        break;

    case BSP_UART_TYPE_RS232_RX:
        HAL_NVIC_DisableIRQ(USART3_IRQn);
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_5);
        __HAL_RCC_USART3_CLK_DISABLE();

        break;

    default:
        break;
    }

    return RES_OK;
}

/** STM32 DMA UART initialization
 * 
 * The function executes DMA TX/RX (according to BSP UART type) initialization
 * 
 * \param[in] type BSP UART type
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __uart_dma_init(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    if (__HAL_RCC_DMA1_IS_CLK_DISABLED())
        __HAL_RCC_DMA1_CLK_ENABLE();

    DMA_HandleTypeDef *hdma_tx = uart_obj[type].uart.hdmatx;
    DMA_HandleTypeDef *hdma_rx = uart_obj[type].uart.hdmarx;

    /* Free DMA UART instance if was allocated */
    if (hdma_tx) {
        uart_obj[type].uart.hdmatx = NULL;

        free(hdma_tx);
        hdma_tx = NULL;
    }

    if (hdma_rx) {
        uart_obj[type].uart.hdmarx = NULL;

        free(hdma_rx);
        hdma_rx = NULL;
    }

    uint8_t res = RES_OK;
    IRQn_Type irq_type;

    /* DMA UART initialization */
    switch (type) {
    case BSP_UART_TYPE_CLI:
        hdma_tx = (DMA_HandleTypeDef*)malloc(sizeof(DMA_HandleTypeDef));

        if (!hdma_tx) {
            res = RES_MEMORY_ERR;
            break;
        }

        hdma_tx->Instance                   = DMA1_Stream4;
        hdma_tx->Init.Channel               = DMA_CHANNEL_4;
        hdma_tx->Init.Direction             = DMA_MEMORY_TO_PERIPH;
        hdma_tx->Init.PeriphInc             = DMA_PINC_DISABLE;
        hdma_tx->Init.MemInc                = DMA_MINC_ENABLE;
        hdma_tx->Init.PeriphDataAlignment   = DMA_PDATAALIGN_BYTE;
        hdma_tx->Init.MemDataAlignment      = DMA_MDATAALIGN_BYTE;
        hdma_tx->Init.Mode                  = DMA_NORMAL;
        hdma_tx->Init.Priority              = DMA_PRIORITY_LOW;
        hdma_tx->Init.FIFOMode              = DMA_FIFOMODE_DISABLE;
        hdma_tx->Init.FIFOThreshold         = DMA_FIFO_THRESHOLD_FULL;
        hdma_tx->Init.MemBurst              = DMA_MBURST_INC4;
        hdma_tx->Init.PeriphBurst           = DMA_PBURST_INC4;

        if (HAL_DMA_Init(hdma_tx) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        uart_obj[type].uart.hdmatx = hdma_tx;
        hdma_tx->Parent = &uart_obj[type].uart;

        hdma_rx = (DMA_HandleTypeDef*)malloc(sizeof(DMA_HandleTypeDef));

        if (!hdma_rx) {
            res = RES_MEMORY_ERR;
            break;
        }

        hdma_rx->Instance                   = DMA1_Stream2;
        hdma_rx->Init.Channel               = DMA_CHANNEL_4;
        hdma_rx->Init.Direction             = DMA_PERIPH_TO_MEMORY;
        hdma_rx->Init.PeriphInc             = DMA_PINC_DISABLE;
        hdma_rx->Init.MemInc                = DMA_MINC_ENABLE;
        hdma_rx->Init.PeriphDataAlignment   = DMA_PDATAALIGN_BYTE;
        hdma_rx->Init.MemDataAlignment      = DMA_MDATAALIGN_BYTE;
        hdma_rx->Init.Mode                  = DMA_CIRCULAR;
        hdma_rx->Init.Priority              = DMA_PRIORITY_LOW;
        hdma_rx->Init.FIFOMode              = DMA_FIFOMODE_DISABLE;
        hdma_rx->Init.FIFOThreshold         = DMA_FIFO_THRESHOLD_FULL;
        hdma_rx->Init.MemBurst              = DMA_MBURST_INC4;
        hdma_rx->Init.PeriphBurst           = DMA_MBURST_INC4;

        if (HAL_DMA_Init(hdma_rx) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        uart_obj[type].uart.hdmarx = hdma_rx;
        hdma_rx->Parent = &uart_obj[type].uart;

        HAL_NVIC_ClearPendingIRQ(DMA1_Stream2_IRQn);
        HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);

        HAL_NVIC_ClearPendingIRQ(DMA1_Stream4_IRQn);
        HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

        break;

    case BSP_UART_TYPE_RS232_TX:
    case BSP_UART_TYPE_RS232_RX:
        hdma_rx = (DMA_HandleTypeDef*)malloc(sizeof(DMA_HandleTypeDef));

        hdma_rx->Instance                   = (uart_obj[type].uart.Instance == USART2) ? DMA1_Stream5 : DMA1_Stream1;
        hdma_rx->Init.Channel               = DMA_CHANNEL_4;
        hdma_rx->Init.Direction             = DMA_PERIPH_TO_MEMORY;
        hdma_rx->Init.PeriphInc             = DMA_PINC_DISABLE;
        hdma_rx->Init.MemInc                = DMA_MINC_ENABLE;
        hdma_rx->Init.PeriphDataAlignment   = DMA_PDATAALIGN_HALFWORD;
        hdma_rx->Init.MemDataAlignment      = DMA_PDATAALIGN_HALFWORD;
        hdma_rx->Init.Mode                  = DMA_CIRCULAR;
        hdma_rx->Init.Priority              = DMA_PRIORITY_LOW;
        hdma_rx->Init.FIFOMode              = DMA_FIFOMODE_DISABLE;
        hdma_rx->Init.FIFOThreshold         = DMA_FIFO_THRESHOLD_FULL;
        hdma_rx->Init.MemBurst              = DMA_MBURST_INC4;
        hdma_rx->Init.PeriphBurst           = DMA_MBURST_INC4;

        if (HAL_DMA_Init(hdma_rx) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        uart_obj[type].uart.hdmarx = hdma_rx;
        hdma_rx->Parent = &uart_obj[type].uart;

        irq_type = (uart_obj[type].uart.Instance == USART2) ? DMA1_Stream5_IRQn : DMA1_Stream1_IRQn;

        HAL_NVIC_ClearPendingIRQ(irq_type);
        HAL_NVIC_SetPriority(irq_type, 5, 0);
        HAL_NVIC_EnableIRQ(irq_type);

        break;

    default:
        break;
    }

    /* If initialization failed free allocated DMA UART instances */
    if (res != RES_OK) {
        uart_obj[type].uart.hdmatx = NULL;
        uart_obj[type].uart.hdmarx = NULL;

        if (hdma_tx)
            free(hdma_tx);

        if (hdma_rx)
            free(hdma_rx);
    }

    return RES_OK;
}

/** STM32 UART MSP initialization
 * 
 * The function executes GPIO, DMA initialization, enables corresponding clocks
 * 
 * \param[in] type BSP UART type
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __uart_msp_init(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (__HAL_RCC_DMA1_IS_CLK_DISABLED())
        __HAL_RCC_DMA1_CLK_ENABLE();

    /* GPIO, RCC, NVIC initialization */
    switch (type) {
    case BSP_UART_TYPE_CLI:
        if (__HAL_RCC_GPIOA_IS_CLK_DISABLED())
            __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_UART4;

        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_UART4_CLK_ENABLE();

        HAL_NVIC_ClearPendingIRQ(UART4_IRQn);
        HAL_NVIC_SetPriority(UART4_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(UART4_IRQn);

        break;

    case BSP_UART_TYPE_RS232_TX:
        if (__HAL_RCC_GPIOA_IS_CLK_DISABLED())
            __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART2;

        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_USART2_CLK_ENABLE();

        HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
        HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);

        break;

    case BSP_UART_TYPE_RS232_RX:
        if (__HAL_RCC_GPIOC_IS_CLK_DISABLED())
            __HAL_RCC_GPIOC_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_5;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;

        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        __HAL_RCC_USART3_CLK_ENABLE();

        HAL_NVIC_ClearPendingIRQ(USART3_IRQn);
        HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);

        break;

    default:
        break;
    }

    /* If initialization was successfull 
       do DMA UART initialization */
    if (res == RES_OK)
        res = __uart_dma_init(type);

    return RES_OK;
}

/** Callback by data reception
 * 
 * The function is called by STM32 HAL UART by idle detection if data was received  
 * The function operates with write position of \ref uart_ctx::rx_buff, set overflow flag  
 * if appropriate event is occured
 * 
 * \param[in] huart STM32 HAL UART instance
 * \param[in] pos current write position of \ref uart_ctx::rx_buff
*/
static void __uart_rx_callback(UART_HandleTypeDef *huart, uint16_t pos)
{
    if (!huart)
        return;

    enum uart_type type = __uart_type_get(huart->Instance);

    if (type != BSP_UART_TYPE_MAX) {
        if (uart_obj[type].ctx && uart_obj[type].ctx->rx_buff) {
            uint16_t idx_set = uart_obj[type].ctx->rx_idx_set;
            uint16_t idx_get = uart_obj[type].ctx->rx_idx_get;
            uint32_t rx_size = uart_obj[type].ctx->init.rx_size;
            bool overflow = false;

            pos = (pos == rx_size) ? 0 : pos;

            if (idx_set == pos)
                return;

            if (pos < idx_set)
                overflow = (idx_get > idx_set) || (idx_get <= pos);
            else
                overflow = (idx_get > idx_set) && (idx_get <= pos);

            if (overflow && uart_obj[type].ctx->init.overflow_isr_cb)
                uart_obj[type].ctx->init.overflow_isr_cb(type, uart_obj[type].ctx->init.params);

            uart_obj[type].ctx->rx_idx_set = pos;
        }
    }
}

/** Callback by BSP UART error
 * 
 * The function is called from \ref __uart_irq_handler when BSP UART error occured  
 * The function is the wrapper over user callback \ref uart_init_ctx::error_isr_cb  
 * 
 * \param[in] type BSP UART type
 * \param[in] error mask of occured BSP UART errors
*/
static void __uart_error_callback(enum uart_type type, uint32_t error)
{
    if (!UART_TYPE_VALID(type))
        return;

    if (type != BSP_UART_TYPE_MAX) {
        if (uart_obj[type].ctx && uart_obj[type].ctx->init.error_isr_cb)
            uart_obj[type].ctx->init.error_isr_cb(type, error, uart_obj[type].ctx->init.params);
    }
}

/** UART data mask
 * 
 * The function executes masking of UART data according to UART settings
 * 
 * \param[in] type BSP UART type
 * \param[in,out] data masked data
 * \param[in] len size of \p data
 */
static void __uart_data_mask(enum uart_type type, uint16_t *data, uint16_t len)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx || !len)
        return;

    uint16_t data_mask = 0;
    struct uart_init_ctx *params = &uart_obj[type].ctx->init;

    if (params->wordlen == BSP_UART_WORDLEN_9)
        data_mask = (params->parity == BSP_UART_PARITY_NONE) ? 0x1FF : 0xFF;
    else
        data_mask = (params->parity == BSP_UART_PARITY_NONE) ? 0xFF : 0x7F;

    for (uint16_t i = 0; i < len; i++)
        data[i] &= data_mask;
}

/* BSP UART instance start, see header file for details */
uint8_t bsp_uart_start(enum uart_type type)
{
    if (!UART_TYPE_VALID(type))
        return RES_INVALID_PAR;

    if (uart_obj[type].ctx && uart_obj[type].ctx->rx_buff) {
        uart_obj[type].ctx->rx_idx_get = 0;
        uart_obj[type].ctx->rx_idx_set = 0;
        uart_obj[type].ctx->frame_error = false;

        if (HAL_UARTEx_ReceiveToIdle_DMA(&uart_obj[type].uart, uart_obj[type].ctx->rx_buff, uart_obj[type].ctx->init.rx_size) != HAL_OK)
            return RES_NOK;

        if (uart_obj[type].ctx->init.lin_enabled)
            __HAL_UART_ENABLE_IT(&uart_obj[type].uart, UART_IT_LBD);
    }

    return RES_OK;
}

/* BSP UART instance stop, see header file for details */
uint8_t bsp_uart_stop(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    HAL_StatusTypeDef hal_res = HAL_UART_DMAStop(&uart_obj[type].uart);

    return (hal_res == HAL_OK) ? RES_OK : RES_NOK;
}

/* Flag whether BSP UART instance is started, see header file for details */
bool bsp_uart_is_started(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx || !uart_obj[type].ctx->rx_buff)
        return false;

    bool status = (READ_BIT(uart_obj[type].uart.Instance->CR3, USART_CR3_DMAR) != 0) ||
                  (READ_BIT(uart_obj[type].uart.Instance->CR3, USART_CR3_DMAT) != 0);

    return status;
}

/* Send BSP UART data, see header file for details */
uint8_t bsp_uart_write(enum uart_type type, void *data, uint16_t len, uint32_t tmt_ms)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx || !uart_obj[type].ctx->tx_buff)
        return RES_INVALID_PAR;

    if (!data || !len)
        return RES_INVALID_PAR;

    if (len > uart_obj[type].ctx->init.tx_size)
        return RES_INVALID_PAR;

    /* Wait when previous DMA UART sending is completed */
    uint8_t res = RES_TIMEOUT;
    uint32_t start_time = HAL_GetTick();
    while((HAL_GetTick() - start_time) < tmt_ms) {
        if (!READ_BIT(uart_obj[type].uart.Instance->CR3, USART_CR3_DMAT) && 
            READ_BIT(uart_obj[type].uart.Instance->SR, USART_SR_TC)) {
            res = RES_OK;
            break;
        }
    }

    if (res != RES_OK)
        HAL_UART_AbortTransmit(&uart_obj[type].uart);

    memcpy(uart_obj[type].ctx->tx_buff, data, len);

    HAL_StatusTypeDef hal_res = HAL_UART_Transmit_DMA(&uart_obj[type].uart, uart_obj[type].ctx->tx_buff, len);

    if (hal_res != HAL_OK)
        return RES_NOK;

    return RES_OK;
}

/* Flag whether received buffer is empty, see header file for details */
bool bsp_uart_rx_buffer_is_empty(enum uart_type type)
{
    if (!UART_TYPE_VALID(type))
        return true;

    return (uart_obj[type].ctx->rx_idx_set == uart_obj[type].ctx->rx_idx_get);
}

/* Receive BSP UART data, see header file for details */
uint8_t bsp_uart_read(enum uart_type type, void *data, uint16_t *len, uint32_t tmt_ms)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx || !uart_obj[type].ctx->rx_buff)
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    uint32_t start_time = HAL_GetTick();
    uint16_t idx_get = uart_obj[type].ctx->rx_idx_get;

    uint8_t data_size = 0;

    if (type == BSP_UART_TYPE_CLI)
        data_size = sizeof(uint8_t);
    else
        data_size = sizeof(uint16_t);

    while(true) {
        uint16_t idx_set = uart_obj[type].ctx->rx_idx_set;
        if (idx_get != idx_set) {
            uint16_t __len = 0;
            uint32_t rx_size = uart_obj[type].ctx->init.rx_size;

            if (idx_set > idx_get) {
                if (data)
                    memcpy(data, (uint8_t*)uart_obj[type].ctx->rx_buff + idx_get * data_size, (idx_set - idx_get) * data_size);
                __len = idx_set - idx_get;
            } else {
                if (data) {
                    memcpy(data, (uint8_t*)uart_obj[type].ctx->rx_buff + idx_get * data_size, (rx_size - idx_get) * data_size);
                    memcpy((uint8_t*)data + data_size * (rx_size - idx_get), uart_obj[type].ctx->rx_buff, idx_set * data_size);
                }
                __len = rx_size - idx_get + idx_set;
            }

            if (len)
                *len = __len;

            if (data && type != BSP_UART_TYPE_CLI)
                __uart_data_mask(type, (uint16_t*)data, __len);

            uart_obj[type].ctx->rx_idx_get = idx_set;

            break;
        }

        if ((HAL_GetTick() - start_time) >= tmt_ms) {
            res = RES_TIMEOUT;
            break;
        }
    }

    return res;
}

/* Initialization of BSP UART instance, see header file for details */
uint8_t bsp_uart_init(enum uart_type type, struct uart_init_ctx *init)
{
    if (!UART_TYPE_VALID(type) || !init)
        return RES_INVALID_PAR;

    if (!UART_WORDLEN_VALID(init->wordlen) || !UART_PARITY_VALID(init->parity))
        return RES_INVALID_PAR;

    if (!UART_STOPBITS_VALID(init->stopbits))
        return RES_INVALID_PAR;

    if (!init->baudrate)
        return RES_INVALID_PAR;

    if (type == BSP_UART_TYPE_CLI && init->lin_enabled)
        return RES_NOT_SUPPORTED;

    uint8_t rx_data_size = 0;

    if (type == BSP_UART_TYPE_CLI)
        rx_data_size = sizeof(uint8_t);
    else
        rx_data_size = sizeof(uint16_t);

    uint8_t res = RES_OK;
    HAL_StatusTypeDef hal_res = HAL_OK;

    do {
        if (!uart_obj[type].ctx) {
            uart_obj[type].ctx = (struct uart_ctx*)malloc(sizeof(struct uart_ctx));

            if (!uart_obj[type].ctx) {
                res = RES_MEMORY_ERR;
                break;
            }

            memset(uart_obj[type].ctx, 0, sizeof(struct uart_ctx));
        } else {
            res = bsp_uart_stop(type);

            if (res != RES_OK)
                return res;
        }

        uint32_t rx_size = init->rx_size;
        if (uart_obj[type].ctx->rx_buff && uart_obj[type].ctx->init.rx_size != rx_size) {
            free(uart_obj[type].ctx->rx_buff);
            uart_obj[type].ctx->rx_buff = NULL;
        }

        uint32_t tx_size = init->tx_size;
        if (uart_obj[type].ctx->tx_buff && uart_obj[type].ctx->init.tx_size != tx_size) {
            free(uart_obj[type].ctx->tx_buff);
            uart_obj[type].ctx->tx_buff = NULL;
        }

        if (!uart_obj[type].ctx->rx_buff && rx_size) {
            uart_obj[type].ctx->rx_buff = malloc(rx_size * rx_data_size);

            if (!uart_obj[type].ctx->rx_buff) {
                res = RES_MEMORY_ERR;
                break;
            }
            memset(uart_obj[type].ctx->rx_buff, 0, rx_size * rx_data_size);
        }

        if (!uart_obj[type].ctx->tx_buff && tx_size) {
            uart_obj[type].ctx->tx_buff = malloc(tx_size * sizeof(uint8_t));

            if (!uart_obj[type].ctx->tx_buff) {
                res = RES_MEMORY_ERR;
                break;
            }
            memset(uart_obj[type].ctx->tx_buff, 0, tx_size * sizeof(uint8_t));
        }

        uart_obj[type].ctx->init = *init;

        if (uart_obj[type].uart.gState == HAL_UART_STATE_RESET) {
            res = __uart_msp_init(type);

            if (res != RES_OK)
                break;
        }

        uart_obj[type].uart.Init.BaudRate       = uart_obj[type].ctx->init.baudrate;
        uart_obj[type].uart.Init.WordLength     = HAL_UART_WORDLEN_TO(uart_obj[type].ctx->init.wordlen);
        uart_obj[type].uart.Init.StopBits       = HAL_UART_STOPBITS_TO(uart_obj[type].ctx->init.stopbits);
        uart_obj[type].uart.Init.Parity         = HAL_UART_PARITY_TO(uart_obj[type].ctx->init.parity);
        uart_obj[type].uart.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
        uart_obj[type].uart.Init.Mode           = (type == BSP_UART_TYPE_CLI) ? UART_MODE_TX_RX : UART_MODE_RX;
        uart_obj[type].uart.Init.OverSampling   = UART_OVERSAMPLING_16;

        if (!uart_obj[type].ctx->init.lin_enabled)
            hal_res = HAL_UART_Init(&uart_obj[type].uart);
        else
            hal_res = HAL_LIN_Init(&uart_obj[type].uart, UART_LINBREAKDETECTLENGTH_11B);

        if (hal_res != HAL_OK) {
            res = RES_NOK;
            break;
        }

        hal_res = HAL_UART_RegisterRxEventCallback(&uart_obj[type].uart, __uart_rx_callback);

        if (hal_res != HAL_OK) {
            res = RES_NOK;
            break;
        }

        res = bsp_uart_start(type);

        if (res != RES_OK)
            break;

    } while(0);

    if (res != RES_OK) {
        bsp_uart_deinit(type);
    }

    return res;
}

/* Deinitialization of BSP UART instance, see header file for details */
uint8_t bsp_uart_deinit(enum uart_type type)
{
    if (!UART_TYPE_VALID(type))
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    HAL_StatusTypeDef hal_res = HAL_OK;

    if (uart_obj[type].ctx) {
        hal_res = HAL_UART_DMAStop(&uart_obj[type].uart);

        if (hal_res != HAL_OK)
            return RES_NOK;

        hal_res = HAL_UART_DeInit(&uart_obj[type].uart);

        if (hal_res != HAL_OK)
            return RES_NOK;

        res = __uart_msp_deinit(type);

        if (res != RES_OK)
            return res;

        if (uart_obj[type].ctx->rx_buff)
            free(uart_obj[type].ctx->rx_buff);

        free(uart_obj[type].ctx);
        uart_obj[type].ctx = NULL;
    }

    return res;
}

/** UART IRQ handler
 * 
 * The function is called from NVIC UART interrupts, processes receiption,  
 * errors and LIN break detection
 * 
 * \param[in] type BSP UART type
 */
static void __uart_irq_handler(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return;

    uint32_t error = 0;
    USART_TypeDef *instance = uart_obj[type].uart.Instance;

    /* Workaround if UART error occurred before DMA receiver is enabled (see UART_Start_Receive_DMA) */
    if (!HAL_IS_BIT_SET(instance->CR3, USART_CR3_DMAR)) {
        // ORE flag is proccesed separately in HAL_UART_IRQHandler
        if (!(instance->SR & USART_SR_ORE)) {
            if (instance->SR & (USART_SR_PE | USART_SR_FE | USART_SR_NE)) {
                // Clear mentioned errors
                __HAL_UART_CLEAR_PEFLAG(&uart_obj[type].uart);
                return;
            }
        }
    }

    /* Process LIN break detection if enabled */
    if (LL_USART_IsEnabledLIN(instance) && LL_USART_IsEnabledIT_LBD(instance)) {
        if (LL_USART_IsActiveFlag_LBD(instance)) {
            uart_obj[type].ctx->frame_error = false;
            LL_USART_ClearFlag_LBD(instance);

            if (uart_obj[type].ctx->init.lin_break_isr_cb)
                uart_obj[type].ctx->init.lin_break_isr_cb(type, uart_obj[type].ctx->init.params);
        }

        /* If after occured frame error LIN break is not detected 
           generates frame error */
        if (uart_obj[type].ctx->frame_error) {
            error = BSP_UART_ERROR_FE;
            uart_obj[type].ctx->frame_error = false;
        } else {
            if (LL_USART_IsActiveFlag_FE(instance)) {
                if (LL_USART_IsActiveFlag_RXNE(instance)) {
                    return;
                } else {
                    uart_obj[type].ctx->frame_error = true;
                    LL_USART_ClearFlag_FE(instance);
                }
            }
        }
    }

    HAL_UART_IRQHandler(&uart_obj[type].uart);

    if ((uart_obj[type].uart.ErrorCode & BSP_UART_ERRORS_ALL) | error)
        __uart_error_callback(type, (uart_obj[type].uart.ErrorCode & BSP_UART_ERRORS_ALL) | error);
}

/** NVIC UART4 IRQ handler */
void UART4_IRQHandler(void)
{
    __uart_irq_handler(BSP_UART_TYPE_CLI);
}

/** NVIC USART2 IRQ handler */
void USART2_IRQHandler(void)
{
    __uart_irq_handler(BSP_UART_TYPE_RS232_TX);
}

/** NVIC USART3 IRQ handler */
void USART3_IRQHandler(void)
{
    __uart_irq_handler(BSP_UART_TYPE_RS232_RX);
}

/** NVIC DMA1 (Stream 1) IRQ handler */
void DMA1_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_RS232_RX].uart.hdmarx);
}

/** NVIC DMA1 (Stream 2) IRQ handler */
void DMA1_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_CLI].uart.hdmarx);
}

/** NVIC DMA1 (Stream 4) IRQ handler */
void DMA1_Stream4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_CLI].uart.hdmatx);
}

/** NVIC DMA1 (Stream 5) IRQ handler */
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_RS232_TX].uart.hdmarx);
}

/** @} */