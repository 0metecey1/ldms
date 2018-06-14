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
//#include <string.h>
//#include <assert.h>
//#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
//#include <linux/types.h>
#include <time.h>
//#include <errno.h>
#include <linux/i2c-dev-user.h>
#include "se97.h"
#include "i2cbusses.h"


#define SE97B_CONFIG_MODE_SHUTDOWN		0x0100
#define SE97B_CONFIG_MODE_NORMAL		0x0000
#define SE97B_CONFIG_REG				0x01
#define SE97B_TEMPERATURE_REG			0x05
#define EEPROM_ID_START		0x080
#define EEPROM_ID_LENGTH	0x008

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

se97_t *se97_create(int i2cbus, int address)
{
    se97_t *self = (se97_t *) calloc(1, (sizeof (se97_t)));
    int ret = 0;
    int force = 0;

    /* open i2c device and provide i2c bus specific settings */
    if (!self)
        return NULL;
    self->dev_i2cbus = i2cbus;
    self->dev_temp_address = address;
    self->dev_eeprom_address = address+0x38;
    self->dev_temp_file = open_i2c_dev(self->dev_i2cbus, self->dev_temp_filename, 
            sizeof(self->dev_temp_filename), 0);
    if ((self->dev_temp_file < 0) || set_slave_addr(self->dev_temp_file, self->dev_temp_address, force)) {
        fprintf(stderr, "Error: opening i2c SE97 temperature device failed\n");
        free(self);
        return NULL;
    }
    self->dev_eeprom_file = open_i2c_dev(self->dev_i2cbus, self->dev_eeprom_filename, 
            sizeof(self->dev_eeprom_filename), 0);
    if ((self->dev_eeprom_file < 0) || set_slave_addr(self->dev_eeprom_file, self->dev_eeprom_address, force)) {
        fprintf(stderr, "Error: opening i2c SE97 eeprom device failed\n");
        free(self);
        return NULL;
    }

    ret = i2c_smbus_write_word_data(self->dev_temp_file, SE97B_CONFIG_REG, SE97B_CONFIG_MODE_NORMAL);
    if (ret < 0) {
        fprintf(stderr, "Error: write to SE97B configuration register failed\n");
        close(self->dev_temp_file);
        return NULL;
    }
    return self;
}

/*
 * Destructor
 */
void se97_destroy(se97_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        se97_t *self = *self_p;
        close(self->dev_temp_file);
        close(self->dev_eeprom_file);
        free(self);
        *self_p = NULL;
    }
}


/*
 * Write eeprom data to se97
 */
int se97_write_eeprom(se97_t *self)
{
    int32_t ret;
    ret = i2c_smbus_write_i2c_block_data(self->dev_eeprom_file, 
            EEPROM_ID_START, EEPROM_ID_LENGTH, self->eeprom_data);
    if (ret < 0) {
        fprintf(stderr, "Error: Writing EEPROM from SE97B on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_eeprom_address);
        return -1;
    }
}

/*
 * Read eeprom data from se97
 */
int se97_read_eeprom(se97_t *self)
{
    int32_t ret;
    if ((ret=i2c_smbus_read_i2c_block_data(self->dev_eeprom_file, 
                    EEPROM_ID_START, EEPROM_ID_LENGTH, self->eeprom_data))<0) {
        fprintf(stderr, "Error: Reading EEPROM from SE97B on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_eeprom_address);
        return -1;
    }
    return 0;
}


/*
 * Returns the temperature as 16 bit value.
 */
int se97_read_temperature(se97_t *self)
{
    /* Output registers are 16 bit wide -> use word data functions */
    int32_t ret;
    if ((ret=i2c_smbus_read_word_data(self->dev_temp_file, SE97B_TEMPERATURE_REG))<0) {
        fprintf(stderr, "Error: Reading temperature from SE97B on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_eeprom_address);
        return -1;
    } else {
        self->last_temperature=((float)((int16_t)((ret<<3) & 0x0000FFFF)))*0.0078125;
    }
    return 0;
}

#define JC42_TEMP_MIN_EXTENDED	(-40000)
#define JC42_TEMP_MIN		0
#define JC42_TEMP_MAX		125000
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

static uint16_t jc42_temp_to_reg(long temp, bool extended)
{
	int ntemp = clamp_val(temp,
			      extended ? JC42_TEMP_MIN_EXTENDED :
			      JC42_TEMP_MIN, JC42_TEMP_MAX);

	/* convert from 0.001 to 0.0625 resolution */
	return (ntemp * 2 / 125) & 0x1fff;
}

static int jc42_temp_from_reg(int16_t reg)
{
	reg = sign_extend32(reg, 12);

	/* convert from 0.0625 to 0.001 resolution */
	return reg * 125 / 2;
}

/*
 * Read temperature from the chip and stores in memory result as 16 bit value.
 */
static int se97_update_temp(se97_t *self)
{
    /* Output registers are 16 bit wide -> use word data functions */
    int32_t ret  = i2c_smbus_read_word_data(self->dev_temp_file, 
            SE97B_TEMPERATURE_REG);
    if (ret < 0) {
        fprintf(stderr, "Error: Reading temperature from SE97B on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus, self->dev_temp_address);
        return -1;
    } else {
        self->last_temperature=((float)((int16_t)((ret<<3) & 0x0000FFFF)))*0.0078125;
    }
    return 0;
}

int se97_read_temp_raw (se97_t *self, int *val)
{
    *val = 0;
    return 0;
}
