/* File: pca9632_core.c
 * Author: Torsten Coym / Oliver Langguth
 * Created: 25.08.2014
 *
 * Basic routines to communicate with PCA9632 LED driver via I2C
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
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <linux/i2c-dev-user.h>
#include "pca9632.h"
#include "i2cbusses.h"

//Mode register 1
#define PCA9632_MODE1_REG					0x00
//Mode register 2
#define PCA9632_MODE2_REG					0x01
//Brightness reg LED0
#define PCA9632_PWM0_REG					0x02
//Brightness reg LED1
#define PCA9632_PWM1_REG					0x03
//Brightness reg LED2
#define PCA9632_PWM2_REG					0x04
//Brightness reg LED3
#define PCA9632_PWM3_REG					0x05
//Blink duty cycle control
#define PCA9632_GRPPWM_REG					0x06
//Blink frequency control
#define PCA9632_GRPFREQ_REG					0x07
//LED output state
#define PCA9632_LEDOUT_REG					0x08
//I2C subaddress 1
#define PCA9632_SUBADR1_REG					0x09
//I2C subaddress 2
#define PCA9632_SUBADR2_REG					0x0A
//I2C subaddress 3
#define PCA9632_SUBADR3_REG					0x0B
//LED All call address
#define PCA9632_ALLCALLADR_REG				0x0C

#define PCA9632_AUTOINC_ENABLED				0x80
#define PCA9632_AUTOINC_DISABLED			0x00
#define PCA9632_AUTOINC_MODE0				0x00
#define PCA9632_AUTOINC_MODE1				0x20
#define PCA9632_AUTOINC_MODE2				0x40
#define PCA9632_AUTOINC_MODE3				0x60
#define PCA9632_SLEEP_MODE					0x10
#define PCA9632_ACTIVE_MODE					0x00

#define PCA9632_RESPOND_SUBADR1				0x08
#define PCA9632_RESPOND_SUBADR2				0x04
#define PCA9632_RESPOND_SUBADR3				0x02
#define PCA9632_RESPOND_ALLCALL				0x01

#define PCA9632_GROUPCTRL_DIMMING			0x00
#define PCA9632_GROUPCTRL_BLINKING			0x20
#define PCA9632_OUTPUT_INVERT				0x10
#define PCA9632_OUTPUT_NORMAL				0x00
#define PCA9632_OUTPUT_ON_ACK				0x08
#define PCA9632_OUTPUT_ON_STOP				0x00
#define PCA9632_OUTPUT_PUSHPULL				0x04
#define PCA9632_OUTPUT_OPENDRAIN			0x00
#define PCA9632_OUTNE						0x02

#define PCA9632_OUTPUT_MASK					0x03
#define PCA9632_OUTPUT_OFF					0x00
#define PCA9632_OUTPUT_FULL_ON				0x01
#define PCA9632_OUTPUT_INDIVIDUAL			0x02
#define PCA9632_OUTPUT_INDIVIDUAL_AND_GROUP	0x03
#define PCA9632_OUTPUT_FULLMODE				0x0100
#define PCA9632_OUTPUT_FULLSHIFT			0x08

#define CHANNEL0_SHIFT						0x00
#define CHANNEL1_SHIFT						0x02
#define CHANNEL2_SHIFT						0x04
#define CHANNEL3_SHIFT						0x06


pca9632_t *pca9632_create(int i2cbus, int address,
        unsigned int polarity_inverted, unsigned int output_mode_pushpull)
{
    pca9632_t *self = (pca9632_t *) calloc(1, (sizeof (pca9632_t)));
    int ret = 0;
    int force = 0;

    /* open i2c device and provide i2c bus specific settings */
    if (!self)
        return NULL;
    self->dev_i2cbus = i2cbus;
    self->dev_address = address;
    self->dev_file = open_i2c_dev(self->dev_i2cbus, 
            self->dev_filename, sizeof(self->dev_filename), 0);
    if ((self->dev_file < 0) || set_slave_addr(self->dev_file, self->dev_address, force)) {
        fprintf(stderr, "Error: opening i2c device failed\n");
        free(self);
        return NULL;
    }
    uint8_t outmode = (PCA9632_GROUPCTRL_DIMMING \ 
            | PCA9632_OUTPUT_ON_STOP \
            | PCA9632_OUTNE);
    outmode |= (polarity_inverted != 0) ? PCA9632_OUTPUT_INVERT : PCA9632_OUTPUT_NORMAL;
    outmode |= (output_mode_pushpull != 0) ? PCA9632_OUTPUT_PUSHPULL : PCA9632_OUTPUT_OPENDRAIN;

    ret = i2c_smbus_write_byte_data(self->dev_file, 
            (uint8_t) PCA9632_MODE1_REG, 
            PCA9632_AUTOINC_DISABLED \
            | PCA9632_AUTOINC_MODE0 \
            | PCA9632_ACTIVE_MODE);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 mode1 register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_MODE2_REG, outmode);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 mode2 register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_PWM0_REG, 0);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 PWM0 register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_PWM1_REG, 0);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 PWM1 register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_PWM2_REG, 0);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 PWM2 register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_PWM3_REG, 0);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 PWM3 register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_GRPPWM_REG, 0xFF);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 GRPPWM register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_GRPFREQ_REG, 0x00);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 GRPFREQ register failed\n");
        close(self->dev_file);
        return NULL;
    }
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_LEDOUT_REG, 0x00);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to PCA9632 LEDOUT register failed\n");
        close(self->dev_file);
        return NULL;
    }

    return self;
}

/*
 * Destructor
 */
void pca9632_destroy(pca9632_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        pca9632_t *self = *self_p;
        close(self->dev_file);
        free(self);
        *self_p = NULL;
    }
}


/*
 * Set LED channel to output value/mode
 */
int pca9632_set_channel_output(pca9632_t *self, unsigned int channel, unsigned int output)
{
    int ret;
    if (channel>3) {
        fprintf(stderr, "Error: Channel out of range. Allowed: 0..3\n");
        return -1;
    }
    if (output>256) {
        fprintf(stderr, "Error: Output value out of range. Allowed: 0..256\n");
        return -1;
    }
    uint8_t mode_set; 
    if ((output!=0)&&(output<256)) 
        mode_set=PCA9632_OUTPUT_INDIVIDUAL;
    else
        mode_set=((output&PCA9632_OUTPUT_FULLMODE)>>PCA9632_OUTPUT_FULLSHIFT);
    mode_set=mode_set<<(channel<<1);
    uint8_t mode_mask=PCA9632_OUTPUT_MASK<<(channel<<1);
    uint8_t outval=(uint8_t)(output);
    ret = i2c_smbus_read_byte_data(self->dev_file, (uint8_t) PCA9632_LEDOUT_REG);
    if (ret<0) {
        fprintf(stderr, "Error: Reading LEDOUT register from PCA9632 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
    uint8_t ledout_prev=(uint8_t)(ret&0x000000FF);
    ledout_prev&=~mode_mask;
    ledout_prev|=mode_set;
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_PWM0_REG+channel,outval);
    if (ret<0) {
        fprintf(stderr, "Error: Write PWM%d register of PCA9632 on I2C %d ADR 0x%x failed\n", 
                channel,self->dev_i2cbus,self->dev_address);
        return -1;
    }
    //write channel mode to LEDOUT register
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_LEDOUT_REG, ledout_prev);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to LEDOUT register of PCA9536 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
}

/*
 * Set LED channel to output mode, leave previous PWM setting
 * mode=0 OFF
 * mode=1 FULL ON
 * mode=2 PWM
 */
int pca9632_set_channel_mode(pca9632_t *self, unsigned int channel, unsigned int mode)
{
    int ret;
    if (channel>3) {
        fprintf(stderr, "Error: Channel out of range. Allowed: 0..3\n");
        return -1;
    }
    if (mode>2) {
        fprintf(stderr, "Error: Mode value out of range. Allowed: 0..2\n");
        return -1;
    }
    mode=mode<<(channel<<1);
    uint8_t mode_mask=PCA9632_OUTPUT_MASK<<(channel<<1);
    ret = i2c_smbus_read_byte_data(self->dev_file, (uint8_t) PCA9632_LEDOUT_REG);
    if (ret<0) {
        fprintf(stderr, "Error: Reading LEDOUT register from PCA9632 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
    uint8_t ledout_prev=(uint8_t)(ret&0x000000FF);
    ledout_prev&=~mode_mask;
    ledout_prev|=mode;
    //write channel mode to LEDOUT register
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_LEDOUT_REG, ledout_prev);
    if (ret < 0) {
        fprintf(stderr, "Error: Write to LEDOUT register of PCA9536 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
}

/*
 * Switch off all LEDs, but keep PWM settings
 */
int pca9632_switch_off_all_channels(pca9632_t *self)
{
    int ret;
    uint8_t mode_set; 
    mode_set = (PCA9632_OUTPUT_OFF<<CHANNEL0_SHIFT) \
               | (PCA9632_OUTPUT_OFF<<CHANNEL1_SHIFT) \
               | (PCA9632_OUTPUT_OFF<<CHANNEL2_SHIFT) \
               | (PCA9632_OUTPUT_OFF<<CHANNEL3_SHIFT);
    ret = i2c_smbus_write_byte_data(self->dev_file, (uint8_t) PCA9632_LEDOUT_REG, mode_set);
    if (ret<0) {
        fprintf(stderr, "Error: Write to LEDOUT register of PCA9536 on I2C %d ADR 0x%x failed\n", 
                self->dev_i2cbus,self->dev_address);
        return -1;
    }
}
