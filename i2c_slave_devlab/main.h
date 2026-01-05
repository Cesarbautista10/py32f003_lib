#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "py32f0xx_hal.h"
#include "py32f0xx_hal_conf.h"
#include "py32f0xx_it.h"

/* Private includes ----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
#ifndef HAL_OK
  #define HAL_OK       0x00U
  #define HAL_ERROR    0x01U
  #define HAL_BUSY     0x02U
  #define HAL_TIMEOUT  0x03U
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
void APP_ErrorHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
