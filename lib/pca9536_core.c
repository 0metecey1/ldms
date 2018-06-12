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
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>
#include <linux/i2c-dev-user.h>
#include "pca9536.h"
#include "i2cbusses.h"

//Current pin status
#define PCA9536_INPUT_PORT_REG					0x00
//Output buffer
#define PCA9536_OUTPUT_PORT_REG					0x01
//set bits invert corresponding output
#define PCA9536_POLARITY_INVERSION_REG			0x02
//Direction register 1=input, 0=output
#define PCA9536_CONFIGURATION_REG				0x03

#define PCA9536_PIN0_OUTPUT						0x00
#define PCA9536_PIN1_OUTPUT						0x00
#define PCA9536_PIN2_OUTPUT						0x00
#define PCA9536_PIN3_OUTPUT						0x00
#define PCA9536_PIN0_INPUT						0x01
#define PCA9536_PIN1_INPUT						0x02
#define PCA9536_PIN2_INPUT						0x04
#define PCA9536_PIN3_INPUT						0x08
#define PCA9536_PIN0_MASK						0x01
#define PCA9536_PIN1_MASK						0x02
#define PCA9536_PIN2_MASK						0x04
#define PCA9536_PIN3_MASK						0x08





pca9536_t * pca9536_create(int i2cbus, int address, unsigned int direction, unsigned int output)
{
    pca9536_t *self = (pca9536_t *) calloc(1, (sizeof (pca9536_t)));
    int ret = 0;
    int force = 0;

    /* open i2c device and provide i2c bus specific settings */
    if (!self)
        return NULL;
    self->dev_i2cbus = i2cbus;
    self->dev_address = address;
    self->dev_file = open_i2c_dev(self->dev_i2cbus, self->dev_filename, sizeof(self->dev_filename), 0);
    if ((self->dev_file < 0) || set_slave_addr(self->dev_file, self->dev_address, force)) {
        fprintf(stderr, "Error: opening i2c device failed\n");
        free(self);
        return NULL;
    }

    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9536_OUTPUT_PORT_REG, output);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9536 output register failed\n");
        close(self->dev_file);
        return NULL;
    }

    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9536_CONFIGURATION_REG, direction);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9536 configuration register failed\n");
        close(self->dev_file);
        return NULL;
    }
    return self;
}

/*
 * Destructor
 */
void pca9536_destroy(pca9536_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        pca9536_t *self = *self_p;
        close(self->dev_file);
        free(self);
        *self_p = NULL;
    }
}


/*
 * Write output data to pca9536
 */
int pca9536_output(pca9536_t *self, unsigned int output)
{
    int ret;
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9536_OUTPUT_PORT_REG, output);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9536 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
}

/*
 * Read eeprom data from pca9536
 */
int pca9536_input(pca9536_t *self, unsigned int * input)
{
    long ret;
    if ((ret=i2c_smbus_read_byte_data(self->dev_file, (uint8_t) PCA9536_INPUT_PORT_REG))<0) {
        fprintf(stderr, "Error: Reading Input data from PCA9536 on I2C %d ADR 0x%x failed\n",
                self->dev_i2cbus,self->dev_address);
        return -1;
    } else *input=(uint8_t)(ret & 0x000000FF);
    return 0;
}






