/* File: ad5522_core.c
 * Author: Torsten Coym
 * Created: 25.08.2014
 *
 * Basic routines to communicate with the AD5522 via SPI
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
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "ad5522.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* MODE Settings, defines which register to set */

#define AD5522_REG_X1(ch, addr)   (((ch) << 8) | ((addr)) | (3 << 6))
#define AD5522_REG_C(ch, addr)    (((ch) << 8) | ((addr)) | (2 << 6))	
#define AD5522_REG_M(ch, addr)    (((ch) << 8) | ((addr)) | (1 << 6))
#define AD5522_FIN_DAC(range)   (((range) | 8) & 0xf)

#define VREF_MICROVOLT 5000000
typedef struct _spidev_t spidev_t;

/*
 * TODO: factor out spi specific data, so that a bus handle can be created
 * rather than hardcoded spidev0.0 is used
 */
struct _ad5522_t {
    spidev_t *ad5522_dev; /* PMU device specific settings */
};

struct _spidev_t {
    int fd; /* file descriptor associated with the device file name */
    uint8_t mode;
    uint8_t bits;
    uint32_t speed;
};

/* precalculated current gain settings to convert output level in micro amp to raw DAC levels */
const unsigned int curr_gain_tbl[5] = {97734366, 24433591, 2443359, 244335, 48867};
/* precalculated current gain correction factors/dividers */
const unsigned int curr_gain_scale_tbl[5] = {24, 24, 24, 24, 24};
/* current range DAC addresses */
const unsigned int curr_dac_x1_addr_tbl[5] = {
    DAC_FIN_I_5_MICROAMP,  
    DAC_FIN_I_20_MICROAMP,  
    DAC_FIN_I_200_MICROAMP, 
    DAC_FIN_I_2000_MICROAMP,
    DAC_FIN_I_EXT,
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
void spidev_destroy(spidev_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        spidev_t *self = *self_p;
        close(self->fd);
        free(self);
        *self_p = NULL;
    }
}

static int ad5522_spi_write(int fd, const void *buf, size_t buf_len)
{
    struct spi_ioc_transfer tr = { .tx_buf = (unsigned long)buf, .len = buf_len, };
    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

static int ad5522_spi_write_then_read(int fd, 
        const void *txbuf, size_t txbuf_len, 
        void *rxbuf, size_t rxbuf_len)
{
    int ret;
    char buf[3] = {0xff, 0xff, 0xff};

    struct spi_ioc_transfer tr = { .tx_buf = (unsigned long)txbuf, .len = txbuf_len, };
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0) {
        return ret;
    }
    tr.tx_buf = (unsigned long)buf;
    tr.rx_buf = (unsigned long)rxbuf;
    tr.len = rxbuf_len;
    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

static void ad5522_format_10_22_write(void *buf,
				    unsigned int reg, unsigned int val)
{
	char *out = buf;

	out[3] = val;
	out[2] = val >> 8;
	out[1] = (val >> 16) | (reg << 6);
	out[0] = reg >> 2;
}

static void ad5522_format_16_16_write(void *buf, unsigned int reg,
        unsigned int val)
{
	char *out = buf;

	out[3] = val;
	out[2] = val >> 8;
	out[1] = reg;
	out[0] = reg >> 8;
}

static unsigned int ad5522_parse_22(const void *buf)
{
    const char *b = buf;
    unsigned int ret = b[2];
	ret |= ((unsigned int)b[1]) << 8;
	ret |= ((unsigned int)b[0] & 0x3f) << 16;

	return ret;
}

static unsigned int ad5522_parse_16(const void *buf)
{
    const char *b = buf;
    unsigned int ret = b[2];
	ret |= ((unsigned int)b[1]) << 8;

	return ret;
}

static void ad5522_write_dac_reg(int fd, unsigned int reg, unsigned int val)
{
    /* 
     * The input shift register is 29 bits wide. It accepts 32 bits as long as
     * the data ist positioned in the 29 LSB.
     * The first bit (B28, MSB) signals whether a write (0) or read (1) operation
     * should take place. B27...B24 select the PMU channel to operate on
     * B23...B22 select the mode, B21...B16 are the DAC address, B15...B0 the DAC
     * value.
     * Use the DAC_ADDR_* macros to obtain the correct reg data.
     */
    char work_buf[4];
    int ret;

    ad5522_format_16_16_write(work_buf, reg, val);
    ret = ad5522_spi_write(fd, work_buf, 4);
    if (ret < 0) {
        perror("SPI write error\n");
        return;
    }
}

static int ad5522_read_dac_reg(int fd, unsigned int reg, unsigned int *val)
{

    /* 
     * A regiser readback is performed by first sending the read request command to with 
     * the register address bits correctly set and all data bits set to '0' and then
     * sending a second message with 24 bits set to '1'.
     */
    char txbuf[4], rxbuf[3];
    int ret;

    ad5522_format_16_16_write(txbuf, DAC_RD_NOTWR | reg, 0);
    ret = ad5522_spi_write_then_read(fd, txbuf, ARRAY_SIZE(txbuf), rxbuf, ARRAY_SIZE(rxbuf));
    if (ret < 0) {
        perror("SPI write error\n");
        return ret;
    }
    *val = ad5522_parse_16(rxbuf);

    return ret;
}

static void ad5522_write_sys_reg(int fd, unsigned int reg, unsigned int val)
{

    /* 
     * The input shift register is 29 bits wide. It accepts 32 bits as long as
     * the data ist positioned in the 29 LSB.
     * The first bit (B28, MSB) signals whether a write (0) or read (1) operation
     * should take place. B27...B24 select the PMU channel to operate on
     * B23...B22 select the mode, B21...B0 are register specific bits.
     */
    char work_buf[4];
    int ret;

    ad5522_format_10_22_write(work_buf, reg, val);
    ret = ad5522_spi_write(fd, work_buf, 4);
    if (ret < 0) {
        perror("SPI write error\n");
        return;
    }
}

static int ad5522_read_sys_reg(int fd, unsigned int reg, unsigned int *val)
{

    /* 
     * A regiser readback is performed by first sending the read request command to with 
     * the register address bits correctly set and all data bits set to '0' and then
     * sending a second message with 24 bits set to '1'.
     */
    char txbuf[4], rxbuf[3];
    int ret;

    ad5522_format_10_22_write(txbuf, RD_NOTWR | reg, 0);
    ret = ad5522_spi_write_then_read(fd, txbuf, ARRAY_SIZE(txbuf), rxbuf, ARRAY_SIZE(rxbuf));
    if (ret < 0) {
        perror("SPI write error\n");
        return ret;
    }
    *val = ad5522_parse_22(rxbuf);

    return ret;
}

ad5522_t * ad5522_create(const char *ad5522_path)
{
    ad5522_t *self = (ad5522_t *) calloc(1, (sizeof (ad5522_t)));

    if (!self)
        return NULL;
    self->ad5522_dev = spidev_create(ad5522_path, SPI_MODE_1, 8, 400000);

    if (self->ad5522_dev < 0) {
        perror("can't create ad5522 spi device");
        free(self);
        return NULL;
    }
    return self;
}

void ad5522_configure(ad5522_t *self, unsigned int *sysval, unsigned int *pmuval)
{
    uint32_t val, rdval;
    const struct timespec wr_delay = {.tv_sec = 0, .tv_nsec = 15000000};
    if (sysval != NULL)
        ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_SYSCTRL, *sysval);
    else {
        /* set initial system configuration */
        ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_SYSCTRL, &rdval);
        /* limit word to 22 bits */
        rdval &= 0x3ffffc;
        val = rdval
            | SYS_CTRL_DUTGND 
            | SYS_CTRL_GUARDALM 
            | SYS_CTRL_CLAMPALM 
            | SYS_CTRL_MEASOUT_GAIN_200_MILLI 
            | SYS_CTRL_I_GAIN_10 
            | SYS_CTRL_TMP_100; 
        nanosleep(&wr_delay, NULL);
        ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_SYSCTRL, val);
    }
    /* set pmu channel specific defaults */
    nanosleep(&wr_delay, NULL);
    if (pmuval != NULL)
        ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(PMU0 | PMU1 | PMU2 | PMU3), *pmuval);
    else {
        ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(PMU0), &rdval);
        /* limit word to 22 bits, mask out lower 7 bits */
        rdval &= 0x3fff80;
        val = rdval | PMU_HIZ_I | PMU_I_2000_MICROAMP | PMU_MEAS_HIZ; 
        nanosleep(&wr_delay, NULL);
        /* initialize all pmu registers */
        ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(PMU0 | PMU1 | PMU2 | PMU3), val);
        /* CAUTION: client needs to ensure that supply voltage/bias voltage is set appropriately */
        /* set the offset DAC to the most positive output range */
    }
}

/*
 * Destructor
 */
void ad5522_destroy(ad5522_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        ad5522_t *self = *self_p;
        spidev_destroy(&(self->ad5522_dev));
        free(self);
        *self_p = NULL;
    }
}


/*
 * Sets the mode to either replicate a current source (force current, measure voltage - FIMV)
 * or voltage source (force voltage, measure current).
 * Follows a read-modify-write policy.
 */
void ad5522_set_measure_mode(ad5522_t *self, unsigned int ch, unsigned int mode)
{
    int pmu;
    uint32_t val, rdval = 0;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;

    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), &rdval);
    val = rdval;
    /* Clear force mode bits */
    val &= ~PMU_MEASURE_MODE_BITMASK; 
    /* limit word to 22 bits, mask out lower 7 bits */
    val &= 0x3fff80;
    /* Modify only force mode and measure mode bits, keep all others */
    if ((mode < 0) || (mode > 3)) {
        perror("unknown measure mode.");
        return;
    }
    val |= (mode << 13);
    ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), val);
}

void ad5522_set_force_mode(ad5522_t *self, unsigned int ch, unsigned int mode)
{
    int pmu;
    uint32_t val, rdval = 0;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;

    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), &rdval);
    val = rdval;
    /* Clear force mode bits */
    val &= ~PMU_FORCE_MODE_BITMASK; 
    /* limit word to 22 bits, mask out lower 7 bits */
    val &= 0x3fff80;
    /* Modify only force mode and measure mode bits, keep all others */
    if ((mode < 0) || (mode > 3)) {
        perror("unknown force mode.");
        return;
    }
    val |= (mode << 19);
    ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), val);
}

void ad5522_set_gain(ad5522_t *self, int gain)
{
    uint32_t val, rdval = 0;

    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_SYSCTRL, &rdval);
    /* limit word to 24 bits, mask out lowest bit */
    val = rdval & 0xfffffe;
    /* Clear range bits */
    val &= ~SYS_CTRL_GAIN_BITMASK; 
    /* Modify only current range bits, keep all others */
    if ((gain < 0) || (gain > 4)) {
        perror("unknown current range.");
        return;
    }
    val |= (gain << 6);
    ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_SYSCTRL, val);
}

void ad5522_get_gain(ad5522_t *self, int *gain)
{
    uint32_t rdval = 0;

    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_SYSCTRL, &rdval);
    /* limit word to range bits */
    *gain = (rdval & SYS_CTRL_GAIN_BITMASK) >> 6;
   
    return;
}

void ad5522_get_alarm_flag(ad5522_t *self, int *flag)
{
    uint32_t rdval = 0;

    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_ALARM, &rdval);
    /* limit word to range bits */
    *flag = (rdval & ALARM_TMPALM_BITMASK) >> 20;
   
    return;
}

void ad5522_clear_alarm_flag(ad5522_t *self)
{
    uint32_t val, rdval = 0;

    /* Read current register state, only one register can be read back at a time */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(PMU0), &rdval);
    val = rdval;
    /* limit word to 22 bits, mask out lower 7 bits */
    val &= 0x3fff80;
    /* Alarm clear bit is global, so writing to any pmu register is sufficient and clears the bit */
    ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(PMU0),
            val | PMU_CLEAR);
   
    return;
}

void ad5522_set_range(ad5522_t *self, unsigned int ch, unsigned int range)
{
    int pmu;
    uint32_t val, rdval = 0;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;
    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), &rdval);
    /* limit word to 22 bits, mask out lower 7 bits */
    val = rdval & 0x3fff80;
    /* Clear range bits */
    val &= ~PMU_RANGE_BITMASK; 
    /* Modify only current range bits, keep all others */
    if ((range < 0) || (range > 4)) {
        perror("unknown current range.");
        return;
    }
    val |= (range << 15);
    ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), val);
}

void ad5522_get_range(ad5522_t *self, unsigned int ch, unsigned int *range)
{
    int pmu;
    uint32_t rdval = 0;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        range = NULL;
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;
    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), &rdval);
    /* limit word to range bits */
    *range = (rdval & PMU_RANGE_BITMASK) >> 15;
   
    return;
}

/*
 * level is voltage in micro volt
 */
void ad5522_set_voltage(ad5522_t *self, unsigned int ch, int level)
{
    int pmu, addr;
    unsigned int raw_level, rdval;
    int level_mv, level_uv;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;
    addr = DAC_FIN_V; /* voltage output is set, so modify the voltage register */
    /* all voltages are in micro volt */
    level_mv = level / 1000;
    level_uv = level - 1000 * level_mv;
    /* offset term, read from device */
    ad5522_read_dac_reg(self->ad5522_dev->fd, AD5522_REG_X1(PMU0 | PMU1 | PMU2 | PMU3,
                DAC_OFFSET_X), &rdval);
    raw_level = rdval;
    raw_level =  (raw_level * 35) / 45;
    /* milli volt term */
    raw_level += level_mv * 65535 / (4.5 * VREF_MICROVOLT / 1000);
    /* micro volt term */
    raw_level += level_uv * 65535 / (4.5 * VREF_MICROVOLT);
    ad5522_write_dac_reg(self->ad5522_dev->fd, AD5522_REG_X1(pmu, addr), raw_level);
}

/*
 * level is current in nano amp
 */
void ad5522_set_current(ad5522_t *self, unsigned int ch, int level)
{
    int pmu, addr;
    int64_t tmp; /* scaling factors imply the use of 64 bit wide integers */
    unsigned int raw_level, curr_gain, curr_gain_scale, rdval, val;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;
    /*
     * DAC level: X1 = Iout * MI * (Rsense * 2^16)/(4.5 * Vref) 
     * = Iout * MI * curr_gain/curr_gain_cf 
     */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), &rdval);
    /* Mask out all range bits */
    val = rdval & PMU_RANGE_BITMASK;
    /* set current gain and gain scale values according to the range */
    switch (val) {
        case PMU_I_5_MICROAMP:
            addr = DAC_FIN_I_5_MICROAMP;
            curr_gain = curr_gain_tbl[0]; 
            curr_gain_scale = curr_gain_scale_tbl[0]; 
            break;
        case PMU_I_20_MICROAMP:
            addr = DAC_FIN_I_20_MICROAMP;
            curr_gain = curr_gain_tbl[1]; 
            curr_gain_scale = curr_gain_scale_tbl[1]; 
            break;
        case PMU_I_200_MICROAMP:
            addr = DAC_FIN_I_200_MICROAMP;
            curr_gain = curr_gain_tbl[2]; 
            curr_gain_scale = curr_gain_scale_tbl[2]; 
            break;
        case PMU_I_2000_MICROAMP:
            addr = DAC_FIN_I_2000_MICROAMP;
            curr_gain = curr_gain_tbl[3]; 
            curr_gain_scale = curr_gain_scale_tbl[3]; 
            break;
        case PMU_I_EXT:
            addr = DAC_FIN_I_EXT;
            curr_gain = curr_gain_tbl[4]; 
            curr_gain_scale = curr_gain_scale_tbl[4]; 
            break;
        default:
            return;
    }
    tmp = level; /* integer value represents nano amp */
    tmp *= curr_gain;
    tmp = tmp >> curr_gain_scale; 
    raw_level = 32768; /* level can be negative, but not less than -32768 */
    raw_level = (unsigned int)((int)raw_level + (int)tmp); /* always positive by definition */

    ad5522_write_dac_reg(self->ad5522_dev->fd, AD5522_REG_X1(pmu, addr), raw_level);
}

void ad5522_set_offset(ad5522_t *self, unsigned int raw_level)
{
    ad5522_write_dac_reg(self->ad5522_dev->fd, AD5522_REG_X1(PMU0 | PMU1 | PMU2 | PMU3, 
                DAC_OFFSET_X), raw_level);
}

void ad5522_set_voltage_compliance(ad5522_t *self, unsigned int ch, int level)
{
    ;
}

void ad5522_set_current_compliance(ad5522_t *self, unsigned int ch, int level)
{
    ;
}

void ad5522_set_output_state(ad5522_t *self, unsigned int ch, unsigned int state)
{
    int pmu;
    uint32_t val, rdval = 0;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;

    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), &rdval);
    val = rdval;
    /* Clear relevant bits */
    val &= ~PMU_ENABLE_BITMASK; 
    /* limit the register value to be 22 bits wide */
    val &= 0x3fffff;
    /* Modify enable and fin bits, keep all others */
    if (state == PMU_CHANNEL_ON)
            val |= PMU_CH_EN | PMU_FIN;

    ad5522_write_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), val);
}

void ad5522_set_all_output_state(ad5522_t *self, unsigned int state)
{
    int ch;
    for (ch=0; ch <= PMU_MAX_CHANNEL; ch++) {
        ad5522_set_output_state(self, ch, state);
    }
}

void ad5522_read_pmu_reg(ad5522_t *self, unsigned int ch, unsigned int *val)
{
    int pmu;


    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;
    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_PMU(pmu), val);
}

void ad5522_read_sysctrl_reg(ad5522_t *self, unsigned int *val)
{
    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_SYSCTRL, val);
}

void ad5522_read_alarm_reg(ad5522_t *self, unsigned int *val)
{
    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_ALARM, val);
}

void ad5522_read_comp_reg(ad5522_t *self, unsigned int *val)
{
    /* Read current register state */
    ad5522_read_sys_reg(self->ad5522_dev->fd, AD5522_REG_COMP, val);
}

void ad5522_read_fin_dac_x1(ad5522_t *self, unsigned int ch, unsigned int range, unsigned int *val)
{
    int pmu;

    if ((ch > 4) || (ch < 0)) {
        /* not a valid channel number */
        return;
    }
    /* convert channel number to respective bit in PMU register */
    pmu = 1 << ch;
    /* Read current register state */
    ad5522_read_dac_reg(self->ad5522_dev->fd, AD5522_REG_X1(pmu, AD5522_FIN_DAC(range)), val);
}

