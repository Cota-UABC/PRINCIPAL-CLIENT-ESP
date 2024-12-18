#ifndef MACROS
#define MACROS

#include "freertos/semphr.h"

#define LOCK_MUTEX(mutex) if (xSemaphoreTake(mutex, portMAX_DELAY)) {
#define UNLOCK_MUTEX(mutex) xSemaphoreGive(mutex); }

#endif