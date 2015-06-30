/**
 * \file /tlc5948a/tlc5948a_core.c
 * \brief core functionality of the TI TLC5948a LED driver
 *
 * Basic functions to set intensity of LED channels, 
 * write and read back control registers, turn on/off individual LED
 *
 * Written in a style that would allow migration to LED kernel driver
 * framework
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
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "tlc5948a.h"

#define REGISTER_SIZE 33 /* space for 257 bits */
#define CHANNEL_COUNT 16

typedef struct _spidev_t spidev_t;
/*
 * TODO: factor out spi specific data, so that a bus handle can be created
 * rather than hardcoded spidev0.0 is used
 */
struct _tlc5948a_t {
    spidev_t *tlc5948a_dev; /* PMU device specific settings */
    size_t register_size;
    uint8_t gs_reg[REGISTER_SIZE]; 
    /* 
     * holds 16-bit PWM values for each constant current output
     * internally 2 latches, both 256 bits wide 
     */ 
    uint8_t ctrl_reg[REGISTER_SIZE];
    /* 
     * two latches, 1st: 137 bits, 2nd 119 bits, 
     * the first latch contains dot correction (DC) data, global brightness control (BC) data,
     * and function control (FC) data.
     * the second latch contains DC data and global BC data.
     */
    uint16_t on_set_brightness[CHANNEL_COUNT]; /* grayscale level used for turn on */
};

struct _spidev_t {
    int fd; /* file descriptor associated with the device file name */
    uint8_t mode;
    uint8_t bits;
    uint32_t speed;
};

static spidev_t *spidev_create(const char *devname, char mode, char bits, int speed)
{
    int ret;
    spidev_t *self = (spidev_t *) calloc(1, (sizeof (spidev_t)));

    self->mode = mode;
    self->speed = speed;
    self->bits = bits;

    self->fd = open(devname, O_RDWR);
    if (self->fd < 0) {
        perror("can't open device");
        free(self);
        return NULL;
    }
    /*
     *   * spi mode
     *       */
    ret = ioctl(self->fd, SPI_IOC_WR_MODE, &self->mode);
    if (ret == -1) {
        perror("can't set spi mode");
        free(self);
        return NULL;
    }

    /*
     *   * bits per word
     *       */
    ret = ioctl(self->fd, SPI_IOC_WR_BITS_PER_WORD, &self->bits);
    if (ret == -1) {
        perror("can't set BITs per word");
        free(self);
        return NULL;
    }
    /*
     *   * max speed hz
     *       */
    ret = ioctl(self->fd, SPI_IOC_WR_MAX_SPEED_HZ, &self->speed);
    if (ret == -1) {
        perror("can't set max speed hz");
        free(self);
        return NULL;
    }
    return self;
}

/*
 * Destructor
 */
static void spidev_destroy(spidev_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        spidev_t *self = *self_p;
        close(self->fd);
        free(self);
        *self_p = NULL;
    }
}

/*
 * Sends the control register data to the TLC5948A
 */
static void tlc5948a_update_ctrl_reg(tlc5948a_t *self)
{
	int ret;
	uint8_t rx[REGISTER_SIZE] = {0, };
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)(self->ctrl_reg),
		.rx_buf = (unsigned long)rx,
        .len = self->register_size,
        .speed_hz = self->tlc5948a_dev->speed,
        .bits_per_word = self->tlc5948a_dev->bits,
    };
    ret = ioctl(self->tlc5948a_dev->fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		perror("can't send spi message");
}

/*
 * Sends the grayscale register data to the TLC5948A
 */
static void tlc5948a_update_gs_reg(tlc5948a_t *self)
{
	int ret;
	uint8_t rx[REGISTER_SIZE] = {0, };
    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)(self->gs_reg),
		.rx_buf = (unsigned long)rx,
        .len = self->register_size,
        .speed_hz = self->tlc5948a_dev->speed,
        .bits_per_word = self->tlc5948a_dev->bits,
    };
    ret = ioctl(self->tlc5948a_dev->fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		perror("can't send spi message");
}

/**
 * Sets the gray scale level for the given LED in the grey scale register.
 * Does not send anything to the TLC5948A !
 *
 * @param ch number of the led (0..15)
 * @param level desired grey scale level
 */
static void tlc5948a_set_grayscale_level(tlc5948a_t *self, unsigned int ch, unsigned int level)
{
    // there are just 16 LEDs (starting with 0)
    if (ch > 15)
        return;

    self->gs_reg[1 + (15 - ch) * 2] = (level >> 8);
    self->gs_reg[2 + ((15 - ch) * 2)] = (level & 0xFF);
}

/*
 * Constructor
 */
tlc5948a_t * tlc5948a_create(const char *tlc5948a_path)
{
    /* Default options */
    const uint8_t buf[] = {
        0x01,                                           /* control reg select bit */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* bits 192..255 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* bits 128..191 */
        0x85, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* bits  64..127 */
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* bits   0..63  */
    };
    const struct timespec wr_delay = {.tv_sec = 0, .tv_nsec = 1000000};
    tlc5948a_t *self = (tlc5948a_t *) calloc(1, (sizeof (tlc5948a_t)));

    if (!self)
        return NULL;
    self->tlc5948a_dev = spidev_create(tlc5948a_path, SPI_MODE_3, 8, 400000);

    if (self->tlc5948a_dev < 0) {
        perror("can't create tlc5948a spi device");
        free(self);
        return NULL;
    }
    /* Set default brightness to maximum value */
    memset(self->on_set_brightness, -1, sizeof (self->on_set_brightness));

    self->register_size = REGISTER_SIZE;
    /* MSB set to 1, to latch shift register to control latch */
    memcpy(self->ctrl_reg, buf, self->register_size);
    /* MSB set to 0, to latch shift register to grayscale latch */
    tlc5948a_update_ctrl_reg(self);
    nanosleep(&wr_delay, NULL);
    tlc5948a_turn_all_off(self);
    return self;
}

/*
 * Destructor
 */
void tlc5948a_destroy(tlc5948a_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        tlc5948a_t *self = *self_p;
        spidev_destroy(&(self->tlc5948a_dev));
        free(self);
        *self_p = NULL;
    }
}

void tlc5948a_set_brightness(tlc5948a_t *self, unsigned int ch, unsigned int level)
{
    if (ch > CHANNEL_COUNT - 1) return;
    self->on_set_brightness[ch] = level;
}

void tlc5948a_turn_on(tlc5948a_t *self, unsigned int ch)
{
    tlc5948a_set_grayscale_level(self, ch, self->on_set_brightness[ch]);
    tlc5948a_update_gs_reg(self);
}

void tlc5948a_turn_off(tlc5948a_t *self, unsigned int ch)
{
    tlc5948a_set_grayscale_level(self, ch, 0);
    tlc5948a_update_gs_reg(self);
}

void tlc5948a_turn_all_off(tlc5948a_t *self)
{
    /* setting all bits in gs_reg turns all channels off */
    memset(self->gs_reg, 0, self->register_size);
    tlc5948a_update_gs_reg(self);
}
/**
 * Sets options in control register
 * Does not send anything to the TLC5948A
 *
 * @param bitpos position of the lsb in the data word
 * @param numberofbits width of the option word
 * @param val value of the option
 */
void tlc5948a_set_ctrl_reg(tlc5948a_t *self, unsigned int bit, 
        unsigned int numberofbits, unsigned int val)
{
    ;
}

int tlc5948a_is_on(tlc5948a_t *self, unsigned int ch)
{
    return 0;
}
