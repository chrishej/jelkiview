#ifndef SERIAL_LOG_H_
#define SERIAL_LOG_H_

/************************************
 * INCLUDES
 ************************************/
#include "stm32g4xx_hal.h"

/************************************
 * EXTERN VARIABLES
 ************************************/
extern UART_HandleTypeDef huart3;

/************************************
 * MACROS AND DEFINES
 ************************************/

/************************************
 * TYPEDEFS
 ************************************/

/************************************
 * EXPORTED VARIABLES
 ************************************/
extern uint8_t SerialLog_tx_frame;

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
void SerialLog_Init();
void SerialLog_RxIrq();
void SerialLog_OsTask();
void SerialLog_TransmittFrame(uint32_t time, uint16_t frame_id, uint32_t wait_option);
void SerialLog_IncrementTick();

#endif // SERIAL_LOG_H_