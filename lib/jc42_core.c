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

/* JC42 registers. All registers are 16 bit. */
#define JC42_REG_CAP		0x00
#define JC42_REG_CONFIG		0x01
#define JC42_REG_TEMP_UPPER	0x02
#define JC42_REG_TEMP_LOWER	0x03
#define JC42_REG_TEMP_CRITICAL	0x04
#define JC42_REG_TEMP		0x05
#define JC42_REG_MANID		0x06
#define JC42_REG_DEVICEID	0x07

/* Status bits in temperature register */
#define JC42_ALARM_CRIT_BIT	15
#define JC42_ALARM_MAX_BIT	14
#define JC42_ALARM_MIN_BIT	13

/* Configuration register defines */
#define JC42_CFG_CRIT_ONLY	(1 << 2)
#define JC42_CFG_TCRIT_LOCK	(1 << 6)
#define JC42_CFG_EVENT_LOCK	(1 << 7)
#define JC42_CFG_SHUTDOWN	(1 << 8)
#define JC42_CFG_HYST_SHIFT	9
#define JC42_CFG_HYST_MASK	(0x03 << 9)

/* Capabilities */
#define JC42_CAP_RANGE		(1 << 2)

/* Manufacturer IDs */
#define ADT_MANID		0x11d4  /* Analog Devices */
#define ATMEL_MANID		0x001f  /* Atmel */
#define ATMEL_MANID2		0x1114	/* Atmel */
#define MAX_MANID		0x004d  /* Maxim */
#define IDT_MANID		0x00b3  /* IDT */
#define MCP_MANID		0x0054  /* Microchip */
#define NXP_MANID		0x1131  /* NXP Semiconductors */
#define ONS_MANID		0x1b09  /* ON Semiconductor */
#define STM_MANID		0x104a  /* ST Microelectronics */

/* Supported chips */
/* NXP */
#define SE97_DEVID		0xa200
#define SE97_DEVID_MASK		0xfffc

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

/* Each client has this additional data */
struct jc42_data {
    struct i2c_client *client;
    struct mutex	update_lock;	/* protect register access */
    bool		extended;	/* true if extended range supported */
    bool		valid;
    unsigned long	last_updated;	/* In jiffies */
    u16		orig_config;	/* original configuration */
    u16		config;		/* current configuration */
    u16		temp[t_num_temp];/* Temperatures */
};

#define JC42_TEMP_MIN_EXTENDED	(-40000)
#define JC42_TEMP_MIN		0
#define JC42_TEMP_MAX		125000

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



