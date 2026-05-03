#pragma once

#include_next <mcuconf.h>

// Use RP2040 I2C0 peripheral for pins labeled SDA0/SCL0.
#undef RP_I2C_USE_I2C0
#define RP_I2C_USE_I2C0 TRUE

#undef RP_I2C_USE_I2C1
#define RP_I2C_USE_I2C1 FALSE

#undef RP_PWM_USE_PWM2
#define RP_PWM_USE_PWM2 TRUE
