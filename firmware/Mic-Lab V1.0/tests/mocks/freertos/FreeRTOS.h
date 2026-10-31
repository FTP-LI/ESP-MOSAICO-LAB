#pragma once
#include <assert.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
extern int test_critical;
#define portENTER_CRITICAL(lock) do { (void)(lock);assert(test_critical==0);test_critical=1; } while(0)
#define portEXIT_CRITICAL(lock) do { (void)(lock);assert(test_critical==1);test_critical=0; } while(0)
