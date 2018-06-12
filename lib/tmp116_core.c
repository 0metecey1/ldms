/* File: tmp116_core.c
 * Author: Torsten Coym / Oliver Langguth
 * Created: 25.08.2014
 *
 * Basic routines to communicate with the MCDC04 via I2C
 * Written in a way to be easily migrated to an industrial I/O (IIO) 
 * kernel framework driver
 */

#include <stdint.h>
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





tmp116_t *tmp116_create(int i2cbus, int address)
{
    tmp116_t *self = (tmp116_t *) calloc(1, (sizeof (tmp116_t)));
    int ret = 0;
    int force = 0;

    /* open i2c device and provide i2c bus specific settings */
    if (!self)
        return NULL;
    self->dev_i2cbus = i2cbus;
    self->dev_address = address;
    self->dev_file = open_i2c_dev(self->dev_i2cbus, self->dev_filename,
            sizeof(self->dev_filename), 0);
    if ((self->dev_file < 0) || set_slave_addr(self->dev_file, 
                self->dev_address, force)) {
        fprintf(stderr, "Error: opening i2c device failed\n");
        free(self);
        return NULL;
    }

    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_CONFIGURATION_REG, \
            TMP116_MODE_CONTINUOUS_CONV \
            | TMP116_CONV_CYCLE_011 \
            | TMP116_AVG_MODE_8_AVERAGES \
            | TMP116_THERM_ALERT_MODE_ALERT \
            | TMP116_ALERT_POLARITY_ACTIVELOW \
            | TMP116_ALERT_PIN_SELECT_ALERTFLG);
    if (ret < 0) {
        fprintf(stderr, "Error: write to TMP116 configuration register failed\n");
        close(self->dev_file);
        return NULL;
    }
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
int tmp116_write_eeprom(tmp116_t *self)
{
    int ret;
    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_EEPROM_UNLOCK_REG, TMP116_EEPROM_UNLOCK);
    if (ret < 0) {
        fprintf(stderr, "Error: Unlocking EEPROM from TMP116 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_EEPROM_REG1, self->eeprom_data[0]);
    if (ret < 0) {
        fprintf(stderr, "Error: write to TMP116 EEPROM1 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_EEPROM_REG2, self->eeprom_data[1]);
    if (ret < 0) {
        fprintf(stderr, "Error: write to TMP116 EEPROM1 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_EEPROM_REG3, self->eeprom_data[2]);
    if (ret < 0) {
        fprintf(stderr, "Error: write to TMP116 EEPROM1 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_EEPROM_REG4, self->eeprom_data[3]);
    if (ret < 0) {
        fprintf(stderr, "Error: write to TMP116 EEPROM1 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
    ret = i2c_smbus_write_word_data(self->dev_file, TMP116_EEPROM_UNLOCK_REG, 
            TMP116_EEPROM_LOCK);
    if (ret < 0) {
        fprintf(stderr, "Error: Locking EEPROM from TMP116 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
}

/*
 * Read eeprom data from tmp116
 */
int tmp116_read_eeprom(tmp116_t *self)
{
    long ret;
    if ((ret=i2c_smbus_read_word_data(self->dev_file, TMP116_EEPROM_REG1))<0) {
        fprintf(stderr, "Error: Reading EEPROM1 from TMP116 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    } else self->eeprom_data[0]=(unsigned int)(ret & 0x0000FFFF);
    if ((ret=i2c_smbus_read_word_data(self->dev_file, TMP116_EEPROM_REG2))<0) {
        fprintf(stderr, "Error: Reading EEPROM2 from TMP116 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    } else self->eeprom_data[1]=(unsigned int)(ret & 0x0000FFFF);
    if ((ret=i2c_smbus_read_word_data(self->dev_file, TMP116_EEPROM_REG3))<0) {
        fprintf(stderr, "Error: Reading EEPROM3 from TMP116 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    } else self->eeprom_data[2]=(unsigned int)(ret & 0x0000FFFF);
    if ((ret=i2c_smbus_read_word_data(self->dev_file, TMP116_EEPROM_REG4))<0) {
        fprintf(stderr, "Error: Reading EEPROM4 from TMP116 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    } else self->eeprom_data[3]=(unsigned int)(ret & 0x0000FFFF);
    return 0;
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
