/**
 ********************************************************************************
 * @file    ${file_name}
 * @author  ${user}
 * @date    ${date}
 * @brief   
 ********************************************************************************
 */

/************************************
 * INCLUDES
 ************************************/
#include "serial_log.h"
#include "debug.h"
#include "tx_api.h"

#include "stm32g4xx_hal.h"
#include "stm32g4xx_nucleo.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/************************************
 * EXTERN VARIABLES
 ************************************/


/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/
// Should be continous 1's, e.g. 0xFF, 0x1FF, 0x3FF, etc.
// Size is dependent of how fast the os task is executed, and the expected data rate.
#define RX_BUFFER_MASK      (0x3F)
#define MAX_FRAME_SIZE      (0x1F)
#define MAX_NR_OF_FRAMES    (0x04)

#define TX_ACK          (0x06)
#define TX_NACK         (0x15)
#define TX_EOT          (0x04)
#define TX_STX          (0x02)
#define TX_ETX          (0x03)

#define SERIAL_DGB      (false)
#define LOG_PREFIX      "SerialLog: "
#define DEBUG_LOG(fmt, ...) \
    do { if (SERIAL_DGB) printf(LOG_PREFIX fmt, ##__VA_ARGS__); } while(0)


/************************************
 * PRIVATE TYPEDEFS
 ************************************/
typedef struct {
    uint32_t *address;
    uint32_t size; // size is only 1 byte, but use a 32b to pad to 32b
} SerialLog_FrameVariable;

typedef struct {
    uint32_t id;
    uint32_t variables_in_frame;
    SerialLog_FrameVariable variables[MAX_FRAME_SIZE];
} SerialLog_Frame;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/
void insert_byte(uint8_t byte);
void RunCommandHandler(uint8_t byte);

void EXTI0_IRQHandler();

bool CH_Invalid(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size);
//!bool CH_Toggle_Led(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size);
bool CH_Start_Log(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size);
bool CH_Stop_Log(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size);
static bool CH_Setup_Frame_2(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size);

/************************************
 * PRIVATE VARIABLES
 ************************************/
static uint8_t  rx_data = 0;
static uint8_t  rx_buffer[RX_BUFFER_MASK+1]; // +1 to allow for wrapping around
static int      rx_handle_idx = 0;
// Points to the next index where data will be written, 
// i.e. rx_head-1 is the last recieved byte
static int      rx_head = 0;
static uint8_t  last_byte = 0x00;
static bool     log_started = false;
static uint8_t  nr_of_frames = 0;
static uint32_t log_time    = 0;
uint8_t tick_size_10us = 100; // [10*us]

uint8_t  SerialLog_tx_frame     = 0;

volatile SerialLog_Frame frames[MAX_NR_OF_FRAMES];
/*
 *   ...           ...                             ...            
 * frames[1]   variables[0]                 |            size|
 * frames[1]   variables[0]                 |----address-----|
 * frames[1]   variables_in_frame           |            vars|
 * frames[1]   id                           |            -id-|
 * 
 * frames[0]   variables[MAX_FRAME_SIZE]    |            size|
 * frames[0]   variables[MAX_FRAME_SIZE]    |----address-----|
 *   ...           ...                             ...            
 * frames[0]   variables[1]                 |            size|
 * frames[0]   variables[1]                 |----address-----|
 * frames[0]   variables[0]                 |            size|
 * frames[0]   variables[0]                 |----address-----|
 * frames[0]   variables_in_frame           |            vars|
 * frames[0]   id                           |            -id-| <=== frames (pointer)
 *          
 *                                          31b          4b  0b
 */

static bool (*command_handlers[])(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size) = {
    CH_Invalid,         // 0x00
    CH_Invalid,         // 0x01
    CH_Setup_Frame_2,   // 0x02
    CH_Invalid,         // 0x03
    CH_Start_Log,       // 0x04
    CH_Stop_Log,        // 0x05
};
const int nr_of_ch = sizeof(command_handlers) / sizeof(command_handlers[0]);

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
void SerialLog_Init() {
    SYSCFG->EXTICR[0] = SYSCFG_EXTICR1_EXTI0_PA;
    EXTI->RTSR1 |= (1 << 0);
    EXTI->IMR1 |= (1 << 0);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    HAL_UART_Receive_IT(&huart3, &rx_data, 1);
}

void SerialLog_RxIrq() {
    insert_byte(rx_data);
    HAL_UART_Receive_IT(&huart3, &rx_data, 1);
}

void SerialLog_OsTask() {
    while (rx_handle_idx != rx_head) {
        // Process the received byte
        uint8_t byte = rx_buffer[rx_handle_idx];
        rx_handle_idx++;
        rx_handle_idx &= RX_BUFFER_MASK; // Wrap around if needed

        last_byte = byte;

        RunCommandHandler(byte);
    }
}

void SerialLog_IncrementTick() {
    log_time += tick_size_10us;
}

void EXTI0_IRQHandler() {
    uint8_t tx_frame;

    // Detect SWI
    bool is_swi = (EXTI->SWIER1 & (1<<0)) != 0;

    // Save the frame id before clearing interrupt
    tx_frame = SerialLog_tx_frame;

    // Clear interrupt
    EXTI->PR1 = GPIO_PIN_0;

    if (is_swi)
    {
        SerialLog_TransmittFrame(log_time, tx_frame, 10);
    }
}

/*
 * Transmitts one frame.
 * Is called from OS tasks. Note: Should not be called from IRQ. Rather store what is to be logged from IRQ into a frame and set a flag to transmit.
 * 
 * The caller has to provide the time, since if it is called from TIM irq some calculations might have to be done to get the time.
 * 
 * This function uses OS utility of mutex, since the point is that it can be called from mutilple threads. If using other OS, the mutex handling has to be changed but most OS have similair handling. The threadx priority inheritance must be specified, while in freertos it is on by default.
 * The caller specifies a max wait time for the mutex, aga
 */
void SerialLog_TransmittFrame(uint32_t time, uint16_t frame_id, uint32_t wait_option) {
    uint8_t big_endian_copy[4];
    uint8_t preamble[5];

    // With a 10 us time size, we can log (2**32-1)/(1e5)/60 = 715 minutes.

    if (log_started == false) {
        return;
    }

    __disable_irq();

    if (frames[frame_id].variables_in_frame > 0) {
        preamble[0] = frame_id;
        preamble[1] = (log_time >> 24) & 0xFF;
        preamble[2] = (log_time >> 16) & 0xFF;
        preamble[3] = (log_time >> 8)  & 0xFF;
        preamble[4] =  log_time        & 0xFF;
        HAL_UART_Transmit(&huart3, preamble, 5, 500);
    }

    for (int i=0; i<frames[frame_id].variables_in_frame; i++) {
        //uint8_t *src = (uint8_t *)frame_vars_2[0][i].address;
        uint8_t *src = (uint8_t *)frames[frame_id].variables[i].address;
        uint32_t variable_size = frames[frame_id].variables[i].size;
        for (size_t j = 0; j < variable_size; j++) {
            big_endian_copy[j] = src[variable_size - 1 - j];
        }

        HAL_UART_Transmit(&huart3, big_endian_copy, variable_size, 500);
    }

    __enable_irq();
}


/************************************
 * PRIVATE FUNCTIONS
 ************************************/
void insert_byte(uint8_t byte) {
    rx_buffer[rx_head] = byte;
    rx_head++;
    rx_head &= RX_BUFFER_MASK; // Wrap around if needed
}

void RunCommandHandler(uint8_t byte) {
    uint8_t  tx_data[0xF+1] = {TX_NACK};
    uint16_t tx_size        = 0;
    static int handler      = -1;

    if (handler == -1) {
        handler = byte;
    }

    if (command_handlers[handler](byte, tx_data, &tx_size)) {
        handler = -1;
    }

    HAL_UART_Transmit(&huart3, tx_data, tx_size, 500);
}

bool CH_Invalid(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size) {
    UNUSED(byte);

    tx_data[0] = TX_NACK;
    *tx_size   = 1;

    return true;
}

//!bool CH_Toggle_Led(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size) {
//!    UNUSED(byte);

//!    BSP_LED_Toggle(LED_GREEN);

//!    tx_data[0] = TX_ACK;
//!    *tx_size   = 1;

//!    return true;
//!}

bool CH_Start_Log(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size) {
        UNUSED(byte);
        log_started = true;
        tx_data[0] = TX_ACK;
        tx_data[1] = 0xFF;
        *tx_size = 2;
        return true;
}

bool CH_Stop_Log(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size) {
        UNUSED(byte);
        log_started = false;
        tx_data[0] = TX_ACK;
        *tx_size = 1;
        return true;
}

static bool CH_Setup_Frame_2(uint8_t byte, uint8_t tx_data[], uint16_t * tx_size) {
    static uint16_t frame_idx    = 0;
    static  int16_t rx_cnt       = -1;
    
    uint16_t frame_variables = 0;


    if (rx_cnt == -1) {
        memset((void*)frames, 0, sizeof(frames));
        rx_cnt++;
        return false;
    }

    if ( (rx_cnt==0) && (byte==0xFF) ) {
        rx_cnt       = -1;
        frame_idx    = 0;
        nr_of_frames = 0;

        tx_data[0] = TX_ACK;
        *tx_size   = 1;
        return true;
    }

    // Pointer to the base of the current frame
    uint8_t * frame_ptr = (uint8_t*)&(frames[frame_idx]);
    // Pointer to the current byte in the current frame
    uint8_t * byte_ptr  = frame_ptr+rx_cnt;
    *(byte_ptr) |= byte;

    // Variables in frame is recieved first, so it will always exist here.
    frame_variables = frames[frame_idx].variables_in_frame;

    rx_cnt++;
    if (rx_cnt == 8+4*2*frame_variables) {
        // Address and size has been written
        rx_cnt = 0;
        frame_idx++;
        nr_of_frames++;
    }

    return false;
}


/************************************
 * END OF FILE
 ************************************/