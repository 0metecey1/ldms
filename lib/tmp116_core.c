/* File: tmp116_core.c
 * Author: Torsten Coym / Oliver Langguth
 * Created: 25.08.2014
 *
 * Basic routines to communicate with the MCDC04 via I2C
 * Written in a way to be easily migrated to an industrial I/O (IIO) 
 * kernel framework driver
 */

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <errno.h>
#include <linux/i2c-dev-user.h>
#include "tmp116.h"
#include "i2cbusses.h"

#define TMP116_HZ_MS 10

#define TMP116_HIGH_ALERT_FLAG					0x8000
#define TMP116_HIGH_ALERT_HIGH_LIMIT_REACHED	0x8000
#define TMP116_HIGH_ALERT_HIGH_LIM_NOT_REACHED	0x0000
#define TMP116_HIGH_ALERT_THERM_LIMIT_REACHED	0x8000
#define TMP116_HIGH_ALERT_THERM_LIM_NOT_REACHED	0x0000

#define TMP116_LOW_ALERT_FLAG					0x4000
#define TMP116_LOW_ALERT_LOW_LIMIT_REACHED		0x4000
#define TMP116_LOW_ALERT_LOW_LIM_NOT_REACHED	0x0000

#define TMP116_DATA_READY_FLAG					0x2000
#define TMP116_DATA_READY_CONVERSION_COMPLETE	0x2000
#define TMP116_DATA_READY_CONVERSION_PENDING	0x0000

#define TMP116_EEPROM_BUSY_FLAG					0x1000
#define TMP116_EEPROM_IS_BUSY					0x1000
#define TMP116_EEPROM_IS_READY					0x0000


#define TMP116_MODE_MASK						0x0C00
#define TMP116_MODE_ONESHOT						0x0C00
#define TMP116_MODE_CONTINUOUS_CONV2			0x0800
#define TMP116_MODE_SHUTDOWN					0x0400
#define TMP116_MODE_CONTINUOUS_CONV				0x0000

#define TMP116_CONV_CYCLE_MASK					0x0380
#define TMP116_CONV_CYCLE_000					0x0000
#define TMP116_CONV_CYCLE_001					0x0080
#define TMP116_CONV_CYCLE_010					0x0100
#define TMP116_CONV_CYCLE_011					0x0180
#define TMP116_CONV_CYCLE_100					0x0200
#define TMP116_CONV_CYCLE_101					0x0280
#define TMP116_CONV_CYCLE_110					0x0300
#define TMP116_CONV_CYCLE_111					0x0380

/*
   TMP116_CONV_CYCLE_MASK	CONV[2:0]	AVG[1:0]= 00	AVG[1:0]= 01	AVG[1:0]= 10	AVG[1:0]= 11
   0x0000					000			15.5 ms			125 ms			500 ms			1 s
   0x0080					001			125 ms			125 ms			500 ms			1 s
   0x0100					010			250 ms			250 ms			500 ms			1 s
   0x0180					011			500 ms			500 ms			500 ms			1 s
   0x0200					100			1 s				1 s				1 s				1 s
   0x0280					101			4 s				4 s				4 s				4 s
   0x0300					110			8 s				8 s				8 s				8 s
   0x0380					111			16 s			16 s			16 s			16 s
 */

#define TMP116_AVG_MODE							0x0060
//AVG[1:0]= 00
#define TMP116_AVG_MODE_NO_AVGERAGES			0x0000
//AVG[1:0]= 01
#define TMP116_AVG_MODE_8_AVERAGES				0x0020
//AVG[1:0]= 10
#define TMP116_AVG_MODE_32_AVERAGES				0x0040
//AVG[1:0]= 11
#define TMP116_AVG_MODE_64_AVERAGES				0x0060


#define TMP116_THERM_ALERT_MODE_MASK			0x0010
#define TMP116_THERM_ALERT_MODE_THERM			0x0010
#define TMP116_THERM_ALERT_MODE_ALERT			0x0000

#define TMP116_ALERT_POLARITY_MASK				0x0008
#define TMP116_ALERT_POLARITY_ACTIVEHIGH		0x0008
#define TMP116_ALERT_POLARITY_ACTIVELOW			0x0000


#define TMP116_ALERT_PIN_SELECT_MASK			0x0004
#define TMP116_ALERT_PIN_SELECT_DATAREADYFLG	0x0004
#define TMP116_ALERT_PIN_SELECT_ALERTFLG		0x0000

#define TMP116_TEMPERATURE_REG					0x00
#define TMP116_CONFIGURATION_REG				0x01
#define TMP116_HIGHLIMIT_REG					0x02
#define TMP116_LOWLIMIT_REG						0x03
//EEPROM write unlock register
#define TMP116_EEPROM_UNLOCK_REG				0x04
#define TMP116_EEPROM_UNLOCK					0x8000
#define TMP116_EEPROM_LOCK						0x0000
//EEPROM data 1
#define TMP116_EEPROM_REG1						0x05
//EEPROM data 2
#define TMP116_EEPROM_REG2						0x06
//EEPROM data 3
#define TMP116_EEPROM_REG3						0x07
//EEPROM data 4
#define TMP116_EEPROM_REG4						0x08
//Device ID = 0xa116  a=not used
#define TMP116_DEVICE_ID_REG					0x0F

#define EEPROM_ID_START		0x05
#define EEPROM_ID_LENGTH	0x04

enum temp_index {
	t_input = 0,
	t_min,
	t_max,
	t_num_temp
};

static const uint8_t temp_regs[t_num_temp] = {
	[t_input] = TMP116_TEMPERATURE_REG,
	[t_min] = TMP116_HIGHLIMIT_REG,
	[t_max] = TMP116_LOWLIMIT_REG	
};

struct _tmp116_t {
    int dev_i2cbus;
    int dev_address;
    int dev_file;
    char dev_filename[128];
    float last_temperature;
    bool data_valid;
    struct timespec last_updated;	/* In in seconds/nanoseconds */
    uint16_t orig_config;	/* original configuration */
    uint16_t config;		/* current configuration */
    uint16_t temp[t_num_temp];/* Temperatures */
};

tmp116_t *tmp116_create(int i2cbus, int address)
{
    tmp116_t *self = (tmp116_t *) calloc(1, (sizeof (tmp116_t)));
    int ret = 0;
    int force = 0;
    int16_t config, config_swp;
    int32_t val;

    /* open i2c device and provide i2c bus specific settings */
    assert(self);
    self->dev_i2cbus = i2cbus;
    self->dev_address = address;
    self->dev_file = open_i2c_dev(self->dev_i2cbus, self->dev_filename,
            sizeof(self->dev_filename), 0);
    if ((self->dev_file < 0) || set_slave_addr(self->dev_file, 
                self->dev_address, force)) {
        fprintf(stderr, "Error: opening i2c device failed\n");
    }

    /* Read original configuration */
    val = i2c_smbus_read_word_data(self->dev_file, TMP116_CONFIGURATION_REG);
    if (val < 0) {
        fprintf(stderr, "Error: read from i2c device failed\n");
        return self;
    }
    /* Swap order of low bytes, drop high bytes*/
    config = (((val & 0x00ffU) << 8) | ((val & 0xff00U) >> 8) \
            & 0x0000ffffU);

    self->orig_config = config;
    config = TMP116_MODE_CONTINUOUS_CONV \
             | TMP116_CONV_CYCLE_011 \
             | TMP116_AVG_MODE_8_AVERAGES \
             | TMP116_THERM_ALERT_MODE_ALERT \
             | TMP116_ALERT_POLARITY_ACTIVELOW \
             | TMP116_ALERT_PIN_SELECT_ALERTFLG;

    config_swp = (((config & 0x00ffU) << 8) | ((config & 0xff00U) >> 8) \
            & 0x0000ffffU);
    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_CONFIGURATION_REG, 
            config_swp);
    if (ret < 0) {
        fprintf(stderr, "Error: write to TMP116 configuration register failed\n");
    }
    self->config = config;
    return self;
}

/*
 * Destructor
 */
void tmp116_destroy(tmp116_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        tmp116_t *self = *self_p;
        close(self->dev_file);
        free(self);
        *self_p = NULL;
    }
}

/*
 * Write eeprom data to tmp116
 */
int tmp116_write_eeprom(tmp116_t *self, const char *buf)
{
    int32_t ret;
    ret = i2c_smbus_write_i2c_block_data(self->dev_file, 
            EEPROM_ID_START, EEPROM_ID_LENGTH, buf);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

/*
 * Read eeprom data from tmp116
 */
int tmp116_read_eeprom(tmp116_t *self, char *buf)
{
    int32_t ret;
    if ((ret = i2c_smbus_read_i2c_block_data(self->dev_file, 
                    EEPROM_ID_START, EEPROM_ID_LENGTH, buf)) < 0) {
        return ret;
    }
    return 0;
}

#define TMP116_TEMP_MIN_EXTENDED	(-55000)
#define TMP116_TEMP_MIN		0
#define TMP116_TEMP_MAX		125000
/*
 * swap - swap value of @a and @b
 */
#define swap(a, b) \
    do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/*
 * min()/max()/clamp() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)

/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use min/max/clamp at all, of course.
 */
#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1: __max2; })

/**
 * sign_extend32 - sign extend a 32-bit value using specified bit as sign-bit
 * @value: value to sign extend
 * @index: 0 based bit index (0<=index<32) to sign bit
 */
static inline int32_t sign_extend32(uint32_t value, int index)
{
    uint8_t shift = 31 - index;
    return (int32_t)(value << shift) >> shift;
}

/**
 * clamp_t - return a value clamped to a given range using a given type
 * @type: the type of variable to use
 * @val: current value
 * @lo: minimum allowable value
 * @hi: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of type
 * 'type' to make all the comparisons.
 */
#define clamp_t(type, val, lo, hi) min_t(type, max_t(type, val, lo), hi)

/**
 * clamp_val - return a value clamped to a given range using val's type
 * @val: current value
 * @lo: minimum allowable value
 * @hi: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of whatever
 * type the input argument 'val' is.  This is useful when val is an unsigned
 * type and min and max are literals that will otherwise be assigned a signed
 * integer type.
 */
#define clamp_val(val, lo, hi) clamp_t(typeof(val), val, lo, hi)

static uint16_t tmp116_temp_to_reg(long temp, bool extended)
{
    int ntemp = clamp_val(temp,
            extended ? TMP116_TEMP_MIN_EXTENDED :
            TMP116_TEMP_MIN, TMP116_TEMP_MAX);

    /* convert from 0.001 to 0.0078125 resolution */
    return (ntemp * 16 / 125) & 0x1fff;
}

static int tmp116_temp_from_reg(int16_t reg)
{
    reg = sign_extend32(reg, 12);

    /* convert from 0.0078125 to 0.001 resolution */
    return reg * 125 / 16;
}

/*
 * Updates the temperatures from the chip and stores the results in memory
 */
static int tmp116_update_device(tmp116_t *self)
{
    int i;
    int32_t val;
    struct timespec current;
    long elapsed_ms;

    clock_gettime( CLOCK_MONOTONIC_RAW, &current);
    elapsed_ms = (current.tv_sec - self->last_updated.tv_sec) * 1000 \
                 + (current.tv_nsec - self->last_updated.tv_nsec) / 1000000; 

    if (elapsed_ms > TMP116_HZ_MS || !self->data_valid) {
        for (i = 0; i < t_num_temp; i++) {
            val = i2c_smbus_read_word_data(self->dev_file, temp_regs[i]);
            if (val < 0) {
                self->data_valid = false;
                return val;
            }
            /* Swap order of low bytes, drop high bytes*/
            self->temp[i] = (((val & 0x00ffU) << 8) | ((val & 0xff00U) >> 8) \
                    & 0x0000ffffU);
        }
        clock_gettime( CLOCK_MONOTONIC_RAW, &self->last_updated);
        self->data_valid = true;
    }

    return 0;
}

int tmp116_read_temp(tmp116_t *self, int index, int *val)
{
    int ret = tmp116_update_device(self);
    *val = tmp116_temp_from_reg(self->temp[index]);
    return ret;
}

/*
 * Read temperature result as 16 bit value.
 */
int tmp116_read_temperature(tmp116_t *self)
{
    /* Output registers are 16 bit wide -> use word data functions */
    long ret;
    if ((ret=i2c_smbus_read_word_data(self->dev_file, TMP116_TEMPERATURE_REG))<0) {
        fprintf(stderr, "Error: Reading temperature from TMP116 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    } else {
        self->last_temperature=((float)((int)(ret & 0x0000FFFF)))*0.0078125;
    }
    return 0;
}
