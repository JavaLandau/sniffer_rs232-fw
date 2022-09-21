#include "common.h"
#include "bsp_uart.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define HAL_UART_WORDLEN_TO(X)     (((X) == BSP_UART_WORDLEN_8) ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B)
#define HAL_UART_STOPBITS_TO(X)    (((X) == BSP_UART_STOPBITS_1) ? UART_STOPBITS_1 : UART_STOPBITS_2)
#define HAL_UART_PARITY_TO(X)      (((X) == BSP_UART_PARITY_NONE) ? UART_PARITY_NONE : \
                                    (((X) == BSP_UART_PARITY_EVEN) ? UART_PARITY_EVEN : UART_PARITY_ODD))

struct uart_ctx {
    struct uart_init_ctx init;
    uint8_t *tx_buff;
    uint8_t *rx_buff;
    uint16_t rx_idx_get;
    uint16_t rx_idx_set;
};

static struct {
    UART_HandleTypeDef uart;
    struct uart_ctx *ctx;
} uart_obj[BSP_UART_TYPE_MAX] = {
    {.uart = {.Instance = UART4}, .ctx = NULL},
    {.uart = {.Instance = USART2}, .ctx = NULL},
    {.uart = {.Instance = USART3}, .ctx = NULL}
};

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

static uint8_t __uart_dma_init(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    if (__HAL_RCC_DMA1_IS_CLK_DISABLED())
        __HAL_RCC_DMA1_CLK_ENABLE();

    DMA_HandleTypeDef *hdma_tx = uart_obj[type].uart.hdmatx;
    DMA_HandleTypeDef *hdma_rx = uart_obj[type].uart.hdmarx;

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

        irq_type = (uart_obj[type].uart.Instance == USART2) ? DMA1_Stream5_IRQn : DMA1_Stream1_IRQn;

        HAL_NVIC_ClearPendingIRQ(irq_type);
        HAL_NVIC_SetPriority(irq_type, 5, 0);
        HAL_NVIC_EnableIRQ(irq_type);

        break;

    default:
        break;
    }

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

static uint8_t __uart_msp_init(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (__HAL_RCC_DMA1_IS_CLK_DISABLED())
        __HAL_RCC_DMA1_CLK_ENABLE();

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

    if (res == RES_OK)
        res = __uart_dma_init(type);

    return RES_OK;
}

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

static void __uart_error_callback(enum uart_type type)
{
    if (!UART_TYPE_VALID(type))
        return;

    if (type != BSP_UART_TYPE_MAX) {
        if (uart_obj[type].ctx && uart_obj[type].ctx->init.error_isr_cb)
            uart_obj[type].ctx->init.error_isr_cb(type, uart_obj[type].uart.ErrorCode, uart_obj[type].ctx->init.params);
    }
}

uint8_t bsp_uart_start(enum uart_type type)
{
    if (!UART_TYPE_VALID(type))
        return RES_INVALID_PAR;

    if (uart_obj[type].ctx && uart_obj[type].ctx->rx_buff) {
        if (HAL_UARTEx_ReceiveToIdle_DMA(&uart_obj[type].uart, uart_obj[type].ctx->rx_buff, uart_obj[type].ctx->init.rx_size) != HAL_OK)
            return RES_NOK;
    }

    return RES_OK;
}

uint8_t bsp_uart_stop(enum uart_type type)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx)
        return RES_INVALID_PAR;

    HAL_StatusTypeDef hal_res = HAL_UART_DMAStop(&uart_obj[type].uart);

    return (hal_res == HAL_OK) ? RES_OK : RES_NOK;
}

uint8_t bsp_uart_write(enum uart_type type, uint8_t *data, uint16_t len, uint32_t tmt_ms)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx || !uart_obj[type].ctx->tx_buff)
        return RES_INVALID_PAR;

    if (!data || !len)
        return RES_INVALID_PAR;

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

    HAL_StatusTypeDef hal_res = HAL_UART_Transmit_DMA(&uart_obj[type].uart, data, len);

    if (hal_res != HAL_OK)
        return RES_NOK;

    return RES_OK;
}

uint8_t bsp_uart_read(enum uart_type type, uint8_t *data, uint16_t *len, uint32_t tmt_ms)
{
    if (!UART_TYPE_VALID(type) || !uart_obj[type].ctx || !uart_obj[type].ctx->rx_buff)
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    uint32_t start_time = HAL_GetTick();
    uint16_t idx_get = uart_obj[type].ctx->rx_idx_get;

    while(true) {
        uint16_t idx_set = uart_obj[type].ctx->rx_idx_set;
        if (idx_get != idx_set) {
            uint16_t __len = 0;
            uint32_t rx_size = uart_obj[type].ctx->init.rx_size;

            if (idx_set > idx_get) {
                if (data)
                    memcpy(data, &uart_obj[type].ctx->rx_buff[idx_get], idx_set - idx_get);
                __len = idx_set - idx_get;
            } else {
                if (data) {
                    memcpy(data, &uart_obj[type].ctx->rx_buff[idx_get], rx_size - idx_get);
                    memcpy(&data[rx_size - idx_get], uart_obj[type].ctx->rx_buff, idx_set);
                }
                __len = rx_size - idx_get + idx_set;
            }

            if (len)
                *len = __len;

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

            uart_obj[type].ctx->rx_idx_get = 0;
            uart_obj[type].ctx->rx_idx_set = 0;
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
            uart_obj[type].ctx->rx_buff = (uint8_t*)malloc(rx_size);

            if (!uart_obj[type].ctx->rx_buff) {
                res = RES_MEMORY_ERR;
                break;
            }
            memset(uart_obj[type].ctx->rx_buff, 0, rx_size);
        }

        if (!uart_obj[type].ctx->tx_buff && tx_size) {
            uart_obj[type].ctx->tx_buff = (uint8_t*)malloc(tx_size);

            if (!uart_obj[type].ctx->tx_buff) {
                res = RES_MEMORY_ERR;
                break;
            }
            memset(uart_obj[type].ctx->tx_buff, 0, tx_size);
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

        hal_res = HAL_UART_Init(&uart_obj[type].uart);

        if (hal_res != HAL_OK) {
            res = RES_NOK;
            break;
        }

        hal_res = HAL_UART_RegisterRxEventCallback(&uart_obj[type].uart, __uart_rx_callback);

        if (hal_res != HAL_OK) {
            res = RES_NOK;
            break;
        }

        if (uart_obj[type].ctx->rx_buff) {
            hal_res = HAL_UARTEx_ReceiveToIdle_DMA(&uart_obj[type].uart, uart_obj[type].ctx->rx_buff, uart_obj[type].ctx->init.rx_size);

            if (hal_res != HAL_OK) {
                res = RES_NOK;
                break;
            }
        }
    } while(0);

    if (res != RES_OK) {
        bsp_uart_deinit(type);
    }

    return res;
}

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

void UART4_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart_obj[BSP_UART_TYPE_CLI].uart);

    if (uart_obj[BSP_UART_TYPE_CLI].uart.ErrorCode & BSP_UART_ERRORS_ALL)
        __uart_error_callback(BSP_UART_TYPE_CLI);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart_obj[BSP_UART_TYPE_RS232_TX].uart);

    if (uart_obj[BSP_UART_TYPE_RS232_TX].uart.ErrorCode & BSP_UART_ERRORS_ALL)
        __uart_error_callback(BSP_UART_TYPE_RS232_TX);
}

void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart_obj[BSP_UART_TYPE_RS232_RX].uart);

    if (uart_obj[BSP_UART_TYPE_RS232_RX].uart.ErrorCode & BSP_UART_ERRORS_ALL)
        __uart_error_callback(BSP_UART_TYPE_RS232_RX);
}

void DMA1_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_RS232_RX].uart.hdmarx);
}

void DMA1_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_CLI].uart.hdmarx);
}

void DMA1_Stream4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_CLI].uart.hdmatx);
}

void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uart_obj[BSP_UART_TYPE_RS232_TX].uart.hdmarx);
}