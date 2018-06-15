/* File: se97_core.c
 * Author: Torsten Coym / Oliver Langguth
 * Created: 04.06.2018
 *
 * Basic routines to communicate with the NXP SE97B via I2C
 * Written in a way to be easily migrated to an industrial I/O (IIO) 
 * kernel framework driver
 */

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/i2c-dev-user.h>
#include "se97.h"
#include "i2cbusses.h"

/* interval for updating the device in msec */
#define SE97_HZ_MS 10

#define SE97B_CONFIG_MODE_SHUTDOWN		0x0100
#define SE97B_CONFIG_MODE_NORMAL		0x0000
#define SE97B_CONFIG_REG				0x01
#define SE97B_TEMPERATURE_REG			0x05
#define EEPROM_ID_START		0x080
#define EEPROM_ID_LENGTH	0x008

/* JC42 registers. All registers are 16 bit. */
#define JC42_REG_CAP		0x00
#define JC42_REG_CONFIG		0x01
#define JC42_REG_TEMP_UPPER	0x02
#define JC42_REG_TEMP_LOWER	0x03
#define JC42_REG_TEMP_CRITICAL	0x04
#define JC42_REG_TEMP		0x05
#define JC42_REG_MANID		0x06
#define JC42_REG_DEVICEID	0x07

/* Configuration register defines */
#define JC42_CFG_CRIT_ONLY	(1 << 2)
#define JC42_CFG_TCRIT_LOCK	(1 << 6)
#define JC42_CFG_EVENT_LOCK	(1 << 7)
#define JC42_CFG_SHUTDOWN	(1 << 8)
#define JC42_CFG_HYST_SHIFT	9
#define JC42_CFG_HYST_MASK	(0x03 << 9)
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

enum temp_index {
	t_input = 0,
	t_crit,
	t_min,
	t_max,
	t_num_temp
};

static const uint8_t temp_regs[t_num_temp] = {
	[t_input] = JC42_REG_TEMP,
	[t_crit] = JC42_REG_TEMP_CRITICAL,
	[t_min] = JC42_REG_TEMP_LOWER,
	[t_max] = JC42_REG_TEMP_UPPER,
};

struct _se97_t {
    int dev_i2cbus;
    int dev_temp_address;
    int dev_temp_file;
    char dev_temp_filename[128];
    int dev_eeprom_address;
    int dev_eeprom_file;
    char dev_eeprom_filename[128];
    bool extended;	/* true if extended range supported */
    bool data_valid;
    struct timespec last_updated;	/* In in seconds/nanoseconds */
    uint16_t orig_config;	/* original configuration */
    uint16_t config;		/* current configuration */
    uint16_t temp[t_num_temp];/* Temperatures */
};

se97_t *se97_create(int i2cbus, int address)
{
    se97_t *self = (se97_t *) calloc(1, (sizeof (se97_t)));
    int ret = 0;
    int force = 0;
    int16_t config, config_swp;
    int32_t val;

    /* open i2c device and provide i2c bus specific settings */
    assert(self);
    self->dev_i2cbus = i2cbus;
    self->dev_temp_address = address;
    self->dev_eeprom_address = address+0x38;
    self->dev_temp_file = open_i2c_dev(self->dev_i2cbus, self->dev_temp_filename, 
            sizeof(self->dev_temp_filename), 0);
    if ((self->dev_temp_file < 0) || set_slave_addr(self->dev_temp_file, self->dev_temp_address, force)) {
        fprintf(stderr, "Error: opening i2c SE97 temperature device failed\n");
    }
    self->dev_eeprom_file = open_i2c_dev(self->dev_i2cbus, self->dev_eeprom_filename, 
            sizeof(self->dev_eeprom_filename), 0);
    if ((self->dev_eeprom_file < 0) || set_slave_addr(self->dev_eeprom_file, self->dev_eeprom_address, force)) {
        fprintf(stderr, "Error: opening i2c SE97 eeprom device failed\n");
    }

    val = i2c_smbus_read_word_data(self->dev_temp_file, SE97B_CONFIG_REG);
    if (val < 0) {
        config = SE97B_CONFIG_MODE_NORMAL;
    }
    /* Swap order of low bytes, drop high bytes*/
    config = (((val & 0x00ffU) << 8) | ((val & 0xff00U) >> 8) \
            & 0x0000ffffU);

    self->orig_config = config;
	if (config & JC42_CFG_SHUTDOWN) {
		config &= ~JC42_CFG_SHUTDOWN;
        /* Swap order of low bytes, drop high bytes*/
        config_swp = (((config & 0x00ffU) << 8) | ((config & 0xff00U) >> 8) \
                & 0x0000ffffU);
        ret = i2c_smbus_write_word_data(self->dev_temp_file, JC42_REG_CONFIG, config_swp);
    }
    self->config = config;

    if (ret < 0) {
        fprintf(stderr, "Error: write to SE97B configuration register failed\n");
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
        int16_t config, config_swp;
        se97_t *self = *self_p;
        /* Restore original configuration except hysteresis */
        if ((self->config & ~JC42_CFG_HYST_MASK) !=
                (self->orig_config & ~JC42_CFG_HYST_MASK)) {

            config = (self->orig_config & ~JC42_CFG_HYST_MASK)
                | (self->config & JC42_CFG_HYST_MASK);
            /* Swap order of low bytes, drop high bytes*/
            config_swp = (((config & 0x00ffU) << 8) | ((config & 0xff00U) >> 8) \
                    & 0x0000ffffU);
            i2c_smbus_write_word_data(self->dev_temp_file, JC42_REG_CONFIG, config_swp);
        }
        close(self->dev_temp_file);
        close(self->dev_eeprom_file);
        free(self);
        *self_p = NULL;
    }
}

/*
 * Write eeprom data to se97
 */
int se97_write_eeprom(se97_t *self, const char *buf)
{
    int32_t ret;
    ret = i2c_smbus_write_i2c_block_data(self->dev_eeprom_file, 
            EEPROM_ID_START, EEPROM_ID_LENGTH, buf);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

/*
 * Read eeprom data from se97
 */
int se97_read_eeprom(se97_t *self, char *buf)
{
    int32_t ret;
    if ((ret = i2c_smbus_read_i2c_block_data(self->dev_eeprom_file, 
                    EEPROM_ID_START, EEPROM_ID_LENGTH, buf)) < 0) {
        return ret;
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
 * Updates the temperatures from the chip and stores the results in memory
 */
static int se97_update_device(se97_t *self)
{
    int i;
    int32_t val;
    struct timespec current;
    long elapsed_ms;

    clock_gettime( CLOCK_MONOTONIC_RAW, &current);
    elapsed_ms = (current.tv_sec - self->last_updated.tv_sec) * 1000 \
                 + (current.tv_nsec - self->last_updated.tv_nsec) / 1000000; 

    if (elapsed_ms > SE97_HZ_MS || !self->data_valid) {
        for (i = 0; i < t_num_temp; i++) {
            val = i2c_smbus_read_word_data(self->dev_temp_file, temp_regs[i]);
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

int se97_read_temp(se97_t *self, int index, int *val)
{
    int ret = se97_update_device(self);
    *val = jc42_temp_from_reg(self->temp[index]);
    return ret;
}
