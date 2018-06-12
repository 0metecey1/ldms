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
#include "se97.h"
#include "i2cbusses.h"


#define SE97B_CONFIG_MODE_SHUTDOWN		0x0100
#define SE97B_CONFIG_MODE_NORMAL		0x0000
#define SE97B_CONFIG_REG				0x01
#define SE97B_TEMPERATURE_REG			0x05
#define EEPROM_ID_START		0x080
#define EEPROM_ID_LENGTH	0x008



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
 * Read temperature result as 16 bit value.
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



