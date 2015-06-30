/* File: mcdc04_core.c
 * Author: Torsten Coym
 * Created: 25.08.2014
 *
 * Basic routines to communicate with the MCDC04 via SPI
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
#include <linux/i2c-dev.h>
#include "mcdc04.h"
#include "i2cbusses.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
/* register address table: config state */
#define MCDC04_ADDR_OSR    0x0
#define MCDC04_ADDR_CREGL  0x6
#define MCDC04_ADDR_CREGH  0x7
#define MCDC04_ADDR_OPTREG 0x8
#define MCDC04_ADDR_BREAK  0x9
#define MCDC04_ADDR_EDGES  0xa

/* register address table: measurement  state */
#define MCDC04_ADDR_OUT0   0x0
#define MCDC04_ADDR_OUT1   0x1
#define MCDC04_ADDR_OUT2   0x2
#define MCDC04_ADDR_OUT3   0x3
#define MCDC04_ADDR_OUTINT 0x4

/* input current direction */
#define MCDC04_DIR_OUT (0x0 << 7)
#define MCDC04_DIR_IN  (0x1 << 7) /* power-on default */
/* ADC reference current settings */
#define MCDC04_IREF_20_NAMP   (0x0 << 4)
#define MCDC04_IREF_80_NAMP   (0x1 << 4)
#define MCDC04_IREF_320_NAMP  (0x2 << 4)
#define MCDC04_IREF_1280_NAMP (0x3 << 4) /* power-on default */
#define MCDC04_IREF_5120_NAMP (0x4 << 4)

/* integration time settings, internal fclk = 1.024MHz */
#define MCDC04_TINT_1_MSEC    0x0
#define MCDC04_TINT_2_MSEC    0x1
#define MCDC04_TINT_4_MSEC    0x2
#define MCDC04_TINT_8_MSEC    0x3
#define MCDC04_TINT_16_MSEC   0x4
#define MCDC04_TINT_32_MSEC   0x5
#define MCDC04_TINT_64_MSEC   0x6 /* power-on default */
#define MCDC04_TINT_128_MSEC  0x7
#define MCDC04_TINT_256_MSEC  0x8
#define MCDC04_TINT_512_MSEC  0x9
#define MCDC04_TINT_1024_MSEC 0xa

/* divider for digital downscaling */
#define MCDC04_DIV_2  (0x0 << 1) /* power-on default */
#define MCDC04_DIV_4  (0x1 << 1)
#define MCDC04_DIV_8  (0x2 << 1)
#define MCDC04_DIV_16 (0x3 << 1)
#define MCDC04_ENDIV_DI 0x0 /* power-on default */
#define MCDC04_ENDIV_EN 0x1

/* measurement modes */
#define MCDC04_MODE_CONT (0x0 << 3)
#define MCDC04_MODE_CMD  (0x1 << 3) /* power-on default */
#define MCDC04_MODE_SYNS (0x2 << 3)
#define MCDC04_MODE_SYND (0x3 << 3)

/* start or stop measurement */
#define MCDC04_SS_STOP     (0x0 << 7) /* power-on default */
#define MCDC04_SS_START    (0x1 << 7)
/* enable or disable power down mode */
#define MCDC04_PD_DI       (0x0 << 6)
#define MCDC04_PD_EN       (0x1 << 6) /* power-on default */
/* select the device operational state */
#define MCDC04_DOS_CONFIG   0x2 /* power-on default */
#define MCDC04_DOS_MEASURE  0x3

/* register bitmasks */
#define MCDC04_MASK_OSR_DOS 0x07
#define MCDC04_MASK_OSR_SS  0x80
#define MCDC04_MASK_OSR_PD  0x40
#define MCDC04_MASK_CREGH_MODE  0x18
#define MCDC04_MASK_CREGL_R  0x70
#define MCDC04_MASK_CREGL_T  0x0f

struct light_t {
    unsigned int ciex;
    unsigned int ciey;
    unsigned int ciez;
};

/*
 * TODO: factor out spi specific data, so that a bus handle can be created
 * rather than hardcoded spidev0.0 is used
 */
struct _mcdc04_t {
    unsigned int reg_cregl; /* config register part low */
    unsigned int reg_cregh;  /* config register part high */
    unsigned int reg_optreg;/* options register */
    unsigned int reg_break; /* break register */
    unsigned int reg_edges; /* edges register */
    int dev_i2cbus;
    int dev_address;
    int dev_file;
    char dev_filename[32];
    int adc_dir_state; /* input photo current state MCDC04_DIR_IN, MCDC04_DIR_OUT */
    int adc_iref_state; /* ADC reference current state, any out of 0, 1, 2, 3, 4 */
    int adc_tint_state; /* ADC integration time state, any out of 0..10 */
    struct timespec adc_tconv; /* Waiting time before conversion data are valid */ 
    struct light_t last_val;
};

mcdc04_t *mcdc04_create(int i2cbus, int address)
{
    mcdc04_t *self = (mcdc04_t *) calloc(1, (sizeof (mcdc04_t)));
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
    /* set device specific register settings */
    self->reg_cregl = MCDC04_DIR_IN | MCDC04_IREF_1280_NAMP | MCDC04_TINT_64_MSEC;
    self->reg_cregh = MCDC04_DIV_2 | MCDC04_ENDIV_DI | MCDC04_MODE_CMD;
    self->adc_tconv.tv_sec = 0;
    self->adc_tconv.tv_nsec = 70000000;

    self->adc_tint_state = MCDC04_TINT_64_MSEC;
    self->adc_iref_state = MCDC04_IREF_1280_NAMP;
    self->adc_dir_state = MCDC04_DIR_IN;

    ret = i2c_smbus_write_byte_data(self->dev_file, MCDC04_ADDR_CREGL, self->reg_cregl);
    if (ret < 0) {
        fprintf(stderr, "Error: write to CREGL register failed\n");
        close(self->dev_file);
        return NULL;
    }

    ret = i2c_smbus_write_byte_data(self->dev_file, MCDC04_ADDR_CREGH, self->reg_cregh);
    if (ret < 0) {
        fprintf(stderr, "Error: write to CREGH register failed\n");
        close(self->dev_file);
        return NULL;
    }
    return self;
}

/*
 * Destructor
 */
void mcdc04_destroy(mcdc04_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        mcdc04_t *self = *self_p;
        close(self->dev_file);
        free(self);
        *self_p = NULL;
    }
}

/*
 * sets operational mode to measurement
 */
void mcdc04_set_measure_mode(mcdc04_t *self, int mode)
{
    int oldvalue, ret;

    /* read CREGH register */
    oldvalue = i2c_smbus_read_byte_data(self->dev_file, MCDC04_ADDR_CREGH);
    if (oldvalue < 0) {
        fprintf(stderr, "Error: Failed to read old value\n");
        return;
    }
    /* clear MODE bits */
    self->reg_cregh = oldvalue & ~MCDC04_MASK_CREGH_MODE;
    /* set measurement mode bits */
    self->reg_cregh |= MCDC04_SS_START;
    switch (mode) {
        case 0: self->reg_cregh |= MCDC04_MODE_CONT; break;
        case 1: self->reg_cregh |= MCDC04_MODE_CMD; break;
        case 2: self->reg_cregh |= MCDC04_MODE_SYNS; break;
        case 3: self->reg_cregh |= MCDC04_MODE_SYND; break;
        default: fprintf(stderr, "Error: illegal measurement mode\n");
    }

    ret = i2c_smbus_write_byte_data(self->dev_file, MCDC04_ADDR_CREGH, self->reg_cregh);
    if (ret < 0) {
        fprintf(stderr, "Error: write to CREGH register failed\n");
        return;
    }
}


/*
 * starts a measurement in CONT or CMD measurement mode
 */
static void mcdc04_start_measure(mcdc04_t *self)
{
    int ret;

    ret = i2c_smbus_write_byte_data(self->dev_file, MCDC04_ADDR_OSR, 
            MCDC04_SS_START | MCDC04_DOS_MEASURE);
    if (ret < 0) {
        fprintf(stderr, "Error: write to OSR register failed\n");
        return;
    }
}

/*
 * stops a measurement in CONT or CMD measurement mode
 */
static void mcdc04_stop_measure(mcdc04_t *self)
{
    int ret;

    ret = i2c_smbus_write_byte_data(self->dev_file, MCDC04_ADDR_OSR, 
            MCDC04_SS_STOP | MCDC04_DOS_CONFIG);
    if (ret < 0) {
        fprintf(stderr, "Error: write to OSR register failed\n");
        return;
    }
}

/*
 * waits until conversion is ready
 */
static void mcdc04_wait_for_ready(mcdc04_t *self)
{
    nanosleep(&(self->adc_tconv), NULL);
}

/*
 * Fetches conversion results as 16 bit adc value.
 * Ensures device is still in measurement state
 */
static void mcdc04_fetch_data(mcdc04_t *self)
{
    /* Output registers are 16 bit wide -> use word data functions */
    
    self->last_val.ciex = i2c_smbus_read_word_data(self->dev_file, MCDC04_ADDR_OUT1);
    self->last_val.ciey = i2c_smbus_read_word_data(self->dev_file, MCDC04_ADDR_OUT3);
    self->last_val.ciez = i2c_smbus_read_word_data(self->dev_file, MCDC04_ADDR_OUT2);
    return;
}

/*
 * Sets the ADC reference current to a fixed value out of 20nA, 80nA, 320nA, 1.28uA, 5.12uA
 */
void mcdc04_set_iref(mcdc04_t *self, int val)
{
    switch (val) {
        case 0: self->adc_iref_state = MCDC04_IREF_20_NAMP; break;
        case 1: self->adc_iref_state = MCDC04_IREF_80_NAMP; break;
        case 2: self->adc_iref_state = MCDC04_IREF_320_NAMP; break;
        case 3: self->adc_iref_state = MCDC04_IREF_1280_NAMP; break;
        case 4: self->adc_iref_state = MCDC04_IREF_5120_NAMP; break;
        default: self->adc_iref_state = MCDC04_IREF_1280_NAMP;
                 fprintf(stderr, "Error: illegal adc reference current index. Must be 0...4\n");
    }
    return;
}

/*
 * Sets the ADC reference current to a fixed value out of 20nA, 80nA, 320nA, 1.28uA, 5.12uA
 */
void mcdc04_set_tint(mcdc04_t *self, int val)
{
    switch (val) {
        case 0: self->adc_tint_state = MCDC04_TINT_1_MSEC; 
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec =10000000; 
                break;
        case 1: self->adc_tint_state = MCDC04_TINT_2_MSEC;
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 10000000; 
                break;
        case 2: self->adc_tint_state = MCDC04_TINT_4_MSEC; 
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 10000000; 
                break;
        case 3: self->adc_tint_state = MCDC04_TINT_8_MSEC; 
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 10000000; 
                break;
        case 4: self->adc_tint_state = MCDC04_TINT_16_MSEC; 
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 25000000; 
                break;
        case 5: self->adc_tint_state = MCDC04_TINT_32_MSEC; 
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 50000000; 
                break;
        case 6: self->adc_tint_state = MCDC04_TINT_64_MSEC;
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 125000000; 
                break;
        case 7: self->adc_tint_state = MCDC04_TINT_128_MSEC;
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 250000000; 
                break;
        case 8: self->adc_tint_state = MCDC04_TINT_256_MSEC;
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 500000000; 
                break;
        case 9: self->adc_tint_state = MCDC04_TINT_512_MSEC;
                self->adc_tconv.tv_sec = 0; 
                self->adc_tconv.tv_nsec = 750000000; 
                break;
        case 10: self->adc_tint_state = MCDC04_TINT_1024_MSEC;
                self->adc_tconv.tv_sec = 1; 
                self->adc_tconv.tv_nsec = 500000000; 
                break;
        default: self->adc_tint_state = MCDC04_TINT_64_MSEC;
                 fprintf(stderr, "Error: illegal adc integration time index. Must be 0...10\n");
    }
}

/*
 * updates the ADC configuration settings to the chip register
 */
static void mcdc04_update_adc_conf(mcdc04_t *self)
{
    int ret;

    /* set integration time, reference current and direction bits */
    self->reg_cregl = self->adc_iref_state | self->adc_tint_state | self->adc_dir_state;
    ret = i2c_smbus_write_byte_data(self->dev_file, MCDC04_ADDR_CREGL, self->reg_cregl);
    if (ret < 0) {
        fprintf(stderr, "Error: write to CREGL register failed\n");
        return;
    }
    return;
}

void mcdc04_trigger(mcdc04_t *self)
{
    mcdc04_update_adc_conf(self);
    mcdc04_start_measure(self);
    mcdc04_wait_for_ready(self);
    mcdc04_fetch_data(self);
    mcdc04_stop_measure(self);
}


void mcdc04_read_raw(mcdc04_t *self, unsigned int ch, unsigned int *val)
{
    switch(ch) {
        case 1: *val = self->last_val.ciex; break;
        case 3: *val = self->last_val.ciey; break;
        case 2: *val = self->last_val.ciez; break;
        default: fprintf(stderr, "Error: channel must be a number 0..3\n");
    }
}
