#define LUA_LIB
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include "ad5522.h"

#define VREF 5.0
#define AD5522_CHANNEL_NUM 4

#define MI    0
#define MV    1
#define MTEMP 2
#define MHIZ  3

#define FV    0
#define FI    1
#define FHIZV 2
#define FHIZI 3

#define SUP_LO_NAME "/sys/class/gpio/gpio98/value"
#define SUP_MID_NAME "/sys/class/gpio/gpio5/value"
#define SUP_HI_NAME "/sys/class/gpio/gpio103/value"
#define SUP_LDO_EN_NAME "/sys/class/gpio/gpio63/value"
#define SUP_DCDC_EN_NAME "/sys/class/gpio/gpio96/value"

#define PMU_RST_NAME "/sys/class/gpio/gpio88/value"
#define PMU_TMP_NAME "/sys/class/gpio/gpio127/value"
#define PMU_CG_NAME "/sys/class/gpio/gpio108/value"
#define PMU_BUSY_NAME "/sys/class/gpio/gpio119/value"

#define SUP_OFF 0
#define SUP_LO_RANGE 1
#define SUP_MID_RANGE 2
#define SUP_HI_RANGE 3

typedef struct {
    ad5522_t *s;
    char *spi_name;
    char *iio_name; /* name of the iio sysfs interface file for the adc */
    unsigned int channel_mapping[AD5522_CHANNEL_NUM]; /* logical to physical driver channel mapping */
} lad5522_userdata_t;

/* 
 * Analog board power ranges:
 * Range          VSS       VDD      Vout @    Vout @             
 *                                   DAC# 0    DAC# 65535
 * 1, low:        -19.5V    +11.5V   -16.25V   + 6.25V
 * 2, mid:        -16.5V    +16.5V   -11.25V   +11.25V
 * 3, hi :        -11.5V    +19.5V   - 5.25V   +17.25V
 */
static const unsigned int voltage_range_offset_dac_tbl[] = 
{42130, 60855, 42130, 19660}; /* ad5522 internal offset dac 16 bit wide, cf. pp. 36 data sheet */
/* maximum absolute values for current ranges */
const int rsense_ohm_tbl[] = {200000, 50000, 5000, 500, 100};
const int current_range_max_na_tbl[] = {5000, 20000, 200000, 2000000, 10000000};
const int voltage_range_max_uv_tbl[] = {0, 6250000, 11250000, 17250000};
const int voltage_range_min_uv_tbl[] = {0, -16250000, -11250000, -5250000};

static void reset (void)
{
    int fd;
    const struct timespec tv = {.tv_sec = 0, .tv_nsec = 3000};

    /* Pull reset line */
    fd = open(PMU_RST_NAME, O_WRONLY);
    if (fd < 0) {
        perror("can't open reset gpio device");
        return;
    }
    if (write(fd, "0", 1) < 0) {
        perror("can't write to reset gpio device");
        close(fd);
        return;
    }
    close(fd);
    /* hold rst line low for at least tv_nsec nano seconds > 1500 */
    nanosleep(&tv, NULL);
    /* Release reset line */
    fd = open(PMU_RST_NAME, O_WRONLY);
    if (fd < 0) {
        perror("can't open reset gpio device");
        return;
    }
    if (write(fd, "1", 1) < 0) {
        perror("can't write to reset gpio device");
        close(fd);
        return;
    }
    close(fd);
    /* hold rst line low for at least tv_nsec nano seconds > 1500 */
    nanosleep(&tv, NULL);
    return; 
}

static int adc_read_raw (const char *iio_dev, int *val)
{
    int fd;
    char buf[6];
    char fname[128]; 
    const struct timespec tv = {.tv_sec = 0, .tv_nsec = 10000};

    strcpy(fname, iio_dev);
    strcat(fname, "/in_voltage0_raw");
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        perror("can't open iio device");
        return -1;
    }
    if (read(fd, buf, 6) < 0) {
        perror("can't read from iio device");
        close(fd);
        return -1;
    }
    close(fd);
    /* Need to read two times, because reading triggers the sampling, 
     * so the first result is not valid */
    buf[0] = '\0';
    nanosleep(&tv, NULL); /* wait some time before reading again */
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        perror("can't open iio device");
        return -1;
    }
    if (read(fd, buf, 6) < 0) {
        perror("can't read from iio device");
        close(fd);
        return -1;
    }
    close(fd);
    *val = atoi(buf);
    return 0; 
}

static int get_supply_rail(void)
{
    int fd, range_id, sup = 0;
    char buf[] = "0";
    /* Is it dcdc on? */
    fd = open(SUP_DCDC_EN_NAME, O_RDONLY);
    if (fd < 0) {
        perror("can't open sup_dcdc_en gpio device");
        return -1;
    }
    if (read(fd, buf, 1) < 0) {
        perror("can't read from sup_dcdc_en gpio device");
        close(fd);
        return -1;
    }
    buf[1] = '\0';
    close(fd);
    if (atoi(buf) == 1)
        sup |= 1 << 4;

    /* Is it dcdc on? */
    fd = open(SUP_LDO_EN_NAME, O_RDONLY);
    if (fd < 0) {
        perror("can't open sup_ldo_en gpio device");
        return -1;
    }
    if (read(fd, buf, 1) < 0) {
        perror("can't read from sup_ldo_en gpio device");
        close(fd);
        return -1;
    }
    buf[1] = '\0';
    close(fd);
    if (atoi(buf) == 1)
        sup |= 1 << 3;

    /* Is it lo range? */
    fd = open(SUP_LO_NAME, O_RDONLY);
    if (fd < 0) {
        perror("can't open sup_lo gpio device");
        return -1;
    }
    if (read(fd, buf, 1) < 0) {
        perror("can't read from sup_lo gpio device");
        close(fd);
        return -1;
    }
    buf[1] = '\0';
    close(fd);
    if (atoi(buf) == 1)
        sup |= 1 << 2;

    /* Is it mid range? */
    fd = open(SUP_MID_NAME, O_RDONLY);
    if (fd < 0) {
        perror("can't open sup_mid gpio device");
        return -1;
    }
    if (read(fd, buf, 1) < 0) {
        perror("can't read from sup_mid gpio device");
        close(fd);
        return -1;
    }
    buf[1] = '\0';
    close(fd);
    if (atoi(buf) == 1)
        sup |= 1 << 1;

    /* Is it hi range? */
    fd = open(SUP_HI_NAME, O_RDONLY);
    if (fd < 0) {
        perror("can't open sup_hi gpio device");
        return -1;
    }
    if (read(fd, buf, 1) < 0) {
        perror("can't read from sup_hi gpio device");
        close(fd);
        return -1;
    }
    buf[1] = '\0';
    close(fd);
    if (atoi(buf) == 1)
        sup |= 1;

    switch(sup) {
        case 0x1c: range_id = 1; break;
        case 0x1a: range_id = 2; break;
        case 0x19: range_id = 3; break;
        default: range_id = 0;break; /* off-state */
    }
    return range_id;
}

static int lad5522_reset (lua_State *L)
{
    lad5522_userdata_t *su;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");

    reset();
    return 0;
}

static int lad5522_configure(lua_State *L)
{
    unsigned int *sysval_p = NULL, *pmuval_p = NULL;
    lad5522_userdata_t *su;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    ad5522_configure(su->s, sysval_p, pmuval_p);
    return 0;
}

static int lad5522_set_force_mode(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch, md;
    const char *mode;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    mode  = luaL_checkstring(L, 3);
    if (mode == NULL)
        luaL_error(L, "mode cannot be empty");

    if (strcmp(mode, "fv") == 0) 
    {
        md = 0;
    } 
    else if (strcmp(mode, "fi") == 0)
    {
        md = 1;
    }
    else if (strcmp(mode, "hizv") == 0)
    {
        md = 2;
    }
    else /* default: "hizi" */
    {
        md = 3;
    }
    ad5522_set_force_mode(su->s, ch - 1, md); /* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int lad5522_set_measure_mode(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch, md;
    const char *mode;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    mode  = luaL_checkstring(L, 3);
    if (mode == NULL)
        luaL_error(L, "mode cannot be empty");

    if (strcmp(mode, "mv") == 0) 
    {
        md = 0;
    } 
    else if (strcmp(mode, "mi") == 0)
    {
        md = 1;
    }
    else if (strcmp(mode, "mt") == 0)
    {
        md = 2;
    }
    else /* default: "hiz" */
    {
        md = 3;
    }
    ad5522_set_measure_mode(su->s, ch - 1, md); /* use 1..4 indexing in Lua, but 0..3 in C */

    return 0;
}

static int lad5522_turn_on(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    ad5522_set_output_state(su->s, ch - 1, PMU_CHANNEL_ON); /* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int lad5522_turn_off(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    ad5522_set_output_state(su->s, ch - 1, PMU_CHANNEL_OFF);/* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int lad5522_turn_all_on(lua_State *L)
{
    lad5522_userdata_t *su;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    ad5522_set_all_output_state(su->s, PMU_CHANNEL_ON);
    return 0;
}

static int lad5522_turn_all_off(lua_State *L)
{
    lad5522_userdata_t *su;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    ad5522_set_all_output_state(su->s, PMU_CHANNEL_OFF);
    return 0;
}

static int lad5522_set_current_range(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch, r;
    double appr_mag;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    r  = luaL_checkinteger(L, 3);

    if ((r < 0) || (r > 4)) 
    {
        r = 127;
    }
    ad5522_set_range(su->s, ch - 1, r);/* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int lad5522_get_current_range(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch, r;
    double appr_mag;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);

    if ((r < 0) || (r > 4)) 
    {
        r = 127;
    }
    ad5522_get_range(su->s, ch - 1, &r);/* use 1..4 indexing in Lua, but 0..3 in C */
    lua_pushinteger(L, r);
    return 1;
}

static int lad5522_set_voltage(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch;
    int raw_lvl;
    double lvl;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    lvl = luaL_checknumber(L, 3);
    /* convert from number to integer in micro volt */
    raw_lvl = 1000000 * lvl;
    ad5522_set_voltage(su->s, ch - 1, raw_lvl);/* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int lad5522_set_current(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch;
    int raw_lvl;
    double lvl;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    lvl = luaL_checknumber(L, 3);
    /* convert from number to integer value in nano amp */
    raw_lvl = 1000000000 * lvl;
    ad5522_set_current(su->s, ch - 1, raw_lvl);/* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}


static int lad5522_set_voltage_range(lua_State *L)
{
    lad5522_userdata_t *su;
    int range;
    char buf[4];

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */

    range = luaL_checknumber(L, 2);

    /* set offset dac level */
    ad5522_set_offset(su->s, voltage_range_offset_dac_tbl[range]);

    return 0;
}

static int lad5522_read_pmu_reg(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch, val;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    ad5522_read_pmu_reg(su->s, ch - 1, &val);/* use 1..4 indexing in Lua, but 0..3 in C */
    lua_pushinteger(L, val);
    return 1;
}

static int lad5522_read_sys_reg(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int val;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ad5522_read_sysctrl_reg(su->s, &val);
    lua_pushinteger(L, val);
    return 1;
}

static int lad5522_read_alarm_reg(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int val;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ad5522_read_alarm_reg(su->s, &val);
    lua_pushinteger(L, val);
    return 1;
}

static int lad5522_read_comp_reg(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int val;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ad5522_read_comp_reg(su->s, &val);
    lua_pushinteger(L, val);
    return 1;
}

static int lad5522_read_dac_x1(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch, val, range;
    const char *dacname;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    dacname = luaL_checkstring(L, 3);
    if (dacname == NULL)
        luaL_error(L, "dacname cannot be empty");
    range = luaL_checkinteger(L, 4);

    if (strcmp(dacname, "fin") == 0) 
    {
        ad5522_read_fin_dac_x1(su->s, ch - 1, range, &val);
    } 
    else /* default: not implemented */
    {
        luaL_error(L, "dacname %s not supported", dacname);
        val = 0;
    }
    lua_pushinteger(L, val);
    return 1;
}

static int clamp(int val, int minval, int maxval)
{
    return (val < minval) ? minval : (val > maxval) ? maxval : val;
}

/** measure
 * \brief: sets the output mode and level for a given channel
 * \param ch the channel number
 * \param mode the force mode to be used
 * \param level the levelue to be set as a floating point number representing SI units,
 * i.e. ampere when mode is 'i' or  'hizi' or volts, otherwise
 */
static int lad5522_measure(lua_State *L)
{
    lad5522_userdata_t *su;
    int ch, range_id;
    int raw_level;
    double level;
    const char *mode;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are levelid. */
    ch = luaL_checkinteger(L, 2);
    mode  = luaL_checkstring(L, 3);
    if (mode == NULL)
        return luaL_error(L, "mode cannot be empty");

    if (strcmp(mode, "i") == 0)
    {
        /* MEASOUT Gain 0.2, current gain 10 */
        ad5522_set_gain(su->s,  2);
        ad5522_set_measure_mode(su->s, ch - 1, MI); 
        ad5522_get_range(su->s, ch - 1, &range_id);/* use 1..4 indexing in Lua, but 0..3 in C */
        adc_read_raw(su->iio_name, &raw_level);
        /* Convert dac raw level to amps, c.f. table 11, p.33, data sheet */
        level = VREF * raw_level/65536.0 - VREF * 0.45;
        level /= rsense_ohm_tbl[range_id] * 10.0 * 0.2;

    }
    else if (strcmp(mode, "v") == 0) 
    {
        ad5522_set_gain(su->s,  2);
        ad5522_set_measure_mode(su->s, ch - 1, MV); 
        range_id = get_supply_rail();
        adc_read_raw(su->iio_name, &raw_level);
        /* Convert dac raw level to volts, c.f. table 11, p.33, data sheet */
        /* Wrong formula in Rev. D and Rev. E! */
        level = raw_level * VREF / 65536.0 * 5.0;
        level += -3.5 * VREF * voltage_range_offset_dac_tbl[range_id] / 65536.0;
    } 
    else if (strcmp(mode, "temp") == 0)
    {
        fprintf(stderr, "measuring temperature is not yet implemented\n");
        level = 100.0;
    }
    else 
    {
        return luaL_error(L, "unknown mode %s", mode);
    }
    ad5522_set_measure_mode(su->s, ch - 1, MHIZ); 
    lua_pushnumber(L, level);
    return 1;
}

/** set_output
 * \brief: sets the output mode and level for a given channel
 * \param ch the channel number
 * \param mode the force mode to be used
 * \param level the levelue to be set as a floating point number representing SI units,
 * i.e. ampere when mode is 'i' or  'hizi' or volts, otherwise
 */
static int lad5522_set_output(lua_State *L)
{
    lad5522_userdata_t *su;
    unsigned int ch, md;
    int raw_level, range_id;
    double level;
    const char *mode;
    char buf[2];

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    /* Check the arguments are levelid. */
    ch = luaL_checkinteger(L, 2);
    mode  = luaL_checkstring(L, 3);
    if (mode == NULL)
        return luaL_error(L, "mode cannot be empty");

    if (strcmp(mode, "v") == 0) 
    {
        md = FV;
    } 
    else if (strcmp(mode, "i") == 0)
    {
        md = FI;
    }
    else if (strcmp(mode, "off") == 0)
    {
        ad5522_set_output_state(su->s, ch - 1, PMU_CHANNEL_OFF); 
        return 0;
    }
    else /* default: do nothing */
    {
        /* Error handling??? */
        return luaL_error(L, "unknown mode %s", mode);
    }
    level = luaL_checknumber(L, 4);
    if ((md == 0) || (md == 2)) /* level represents a voltage */
    {
        /* which supply range? */
        range_id = get_supply_rail();
        /* supply rail limits the voltage output, so select from look-up table */

        /* convert from floating point number in volt to integer in micro volt */
        /* clamp voltage to allowed region according to selected supply voltage range */
        raw_level = clamp((int)(1000000 * level), voltage_range_min_uv_tbl[range_id], 
                voltage_range_max_uv_tbl[range_id]);
        /* set 'hizv' force mode */
        ad5522_set_force_mode(su->s, ch - 1, FHIZV); 
        /* pre-load new DAC value and let internal circuitry settle */
        ad5522_set_voltage(su->s, ch - 1, raw_level);/* use 1..4 indexing in Lua, but 0..3 in C */
    }
    else /* level represents a current */
    {
        /* Which current range are we in? */
        ad5522_get_range(su->s, ch - 1, &range_id);/* use 1..4 indexing in Lua, but 0..3 in C */
        /* convert from floating point number in volt to integer in micro volt */
        /* clamp voltage to allowed region according to selected current range */
        raw_level = clamp((int)(1000000000 * level), -1 * current_range_max_na_tbl[range_id], 
                current_range_max_na_tbl[range_id]);
        /* set 'hizi' force mode */
        ad5522_set_force_mode(su->s, ch - 1, FHIZI); 
        /* pre-load new DAC value and let internal circuitry settle */
        ad5522_set_current(su->s, ch - 1, raw_level);/* use 1..4 indexing in Lua, but 0..3 in C */
    }
    /* change force mode as the user requested */
    ad5522_set_force_mode(su->s, ch - 1, md); 
    ad5522_set_output_state(su->s, ch - 1, PMU_CHANNEL_ON); 
    return 0;
}

static int lad5522_get_channel_count(lua_State *L)
{
    lad5522_userdata_t *su;
    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    lua_pushinteger(L, AD5522_CHANNEL_NUM);
    return 1;
}

static int set_supply_rail(int range_id)
{
    int fd, sup, range;

    sup = 0;
    switch(range_id) {
        case 1: sup = 0x1c; range = 1; break; /* -19.5V...+11.5V, enable dcdc + ldo */
        case 2: sup = 0x1a; range = 2;break; /* -16.5V...+16.5V, enable dcdc + ldo */
        case 3: sup = 0x19; range = 3;break; /* -11.5V...+19.5V, enable dcdc + ldo */
        default: sup = 0; range = 0; break;   /* off, disable dcdc + ldo */
    }
    /* Write to dcdc pin */
    fd = open(SUP_DCDC_EN_NAME, O_WRONLY);
    if (fd < 0) {
        perror("can't open sup_dcdc_en gpio device");
        return -1;
    }
    if (write(fd, (sup & 0x10) ? "1" : "0", 1) < 0) {
        perror("can't write to sup_dcdc_en gpio device");
        close(fd);
        return -1;
    }
    close(fd);
    /* Write to ldo pin */
    fd = open(SUP_LDO_EN_NAME, O_WRONLY);
    if (fd < 0) {
        perror("can't open sup_ldo_en gpio device");
        return -1;
    }
    if (write(fd, (sup & 0x8) ? "1" : "0", 1) < 0) {
        perror("can't write to sup_ldo_en gpio device");
        close(fd);
        return -1;
    }
    close(fd);
    /* Is it lo range? */
    fd = open(SUP_LO_NAME, O_WRONLY);
    if (fd < 0) {
        perror("can't open sup_lo gpio device");
        return -1;
    }
    if (write(fd, (sup & 0x4) ? "1" : "0", 1) < 0) {
        perror("can't write to sup_lo gpio device");
        close(fd);
        return -1;
    }
    close(fd);
    /* Is it mid range? */
    fd = open(SUP_MID_NAME, O_WRONLY);
    if (fd < 0) {
        perror("can't open sup_mid gpio device");
        return -1;
    }
    if (write(fd, (sup & 0x2) ? "1" : "0", 1) < 0) {
        perror("can't write to sup_mid gpio device");
        close(fd);
        return -1;
    }
    close(fd);
    /* Is it hi range? */
    fd = open(SUP_HI_NAME, O_WRONLY);
    if (fd < 0) {
        perror("can't open sup_hi gpio device");
        return -1;
    }
    if (write(fd, (sup & 0x1) ? "1" : "0", 1) < 0) {
        perror("can't write to sup_hi gpio device");
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}

static int lad5522_set_supply_rail(lua_State *L)
{
    int range_id;

    lad5522_userdata_t *su;
    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    range_id = luaL_checkinteger(L, 2);
    if (set_supply_rail(range_id) < 0) {
        perror("can't set supply rails");
        return 0;
    }
    /* set offset dac level */
    ad5522_set_offset(su->s, voltage_range_offset_dac_tbl[range_id]);

    return 0;
}

static int lad5522_get_supply_rail(lua_State *L)
{
    lad5522_userdata_t *su;
    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");

    lua_pushinteger(L, get_supply_rail());
    return 1;
}

static int lad5522_set_gain(lua_State *L)
{
    int gain;
    lad5522_userdata_t *su;
    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");
    gain = luaL_checkinteger(L, 2);

    ad5522_set_gain(su->s,  gain);
    return 0;
}

static int lad5522_get_gain(lua_State *L)
{
    int gain;
    lad5522_userdata_t *su;
    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");

    ad5522_get_gain(su->s, &gain);

    lua_pushinteger(L, gain);
    return 1;
}

static int lad5522_get_min_voltage(lua_State *L)
{
    lad5522_userdata_t *su;
    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");

    lua_pushnumber(L, voltage_range_min_uv_tbl[get_supply_rail()]/1.0e6);
    return 1;
}

static int lad5522_get_max_voltage(lua_State *L)
{
    lad5522_userdata_t *su;
    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");

    lua_pushnumber(L, voltage_range_max_uv_tbl[get_supply_rail()]/1.0e6);
    return 1;
}

static int lad5522_new(lua_State *L)
{
    lad5522_userdata_t *su;
    int spi_dev_num, spi_cs_num, iio_dev_num, gpio_rst_num;
    char *spi_dev_name, *iio_dev_name, *gpio_rst_dev_name;

    /* Check the arguments are valid. */
    spi_dev_num  = luaL_checkinteger(L, 1);
    spi_cs_num  = luaL_checkinteger(L, 2);
    iio_dev_num  = luaL_checkinteger(L, 3);


    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su       = (lad5522_userdata_t *)lua_newuserdata(L, sizeof(*su));
    su->s    = NULL;
    su->spi_name = NULL;

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Lad5522");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    /* Create the data that comprises the userdata (the ad5522 state). */
    asprintf(&spi_dev_name, "/dev/spidev%d.%d", spi_dev_num, spi_cs_num);
    asprintf(&iio_dev_name, "/sys/bus/iio/devices/iio:device%d", iio_dev_num);
    su->spi_name = strdup(spi_dev_name);
    su->iio_name = strdup(iio_dev_name);
    /* turn on supply rails for the device */
    set_supply_rail(SUP_MID_RANGE);
    /* reset the device */
    reset();

    su->s    = ad5522_create(spi_dev_name);
    return 1;
}

static int lad5522_destroy(lua_State *L)
{
    lad5522_userdata_t *su;

    su = (lad5522_userdata_t *)luaL_checkudata(L, 1, "Lad5522");

    ad5522_set_all_output_state(su->s, PMU_CHANNEL_OFF);
    /* turn off supply rails for the device */
    set_supply_rail(SUP_OFF);
    if (su->s != NULL)
        ad5522_destroy(&(su->s));
    su->s = NULL;

    if (su->spi_name != NULL)
        free(su->spi_name);
    su->spi_name = NULL;

    return 0;
}

static const luaL_Reg lad5522_methods[] = {
    {"set_voltage_range", lad5522_set_voltage_range},
    {"set_current_range", lad5522_set_current_range},
    {"get_current_range", lad5522_get_current_range},
    {"set_output", lad5522_set_output},
    {"measure", lad5522_measure},
    {"set_voltage", lad5522_set_voltage},
    {"set_current", lad5522_set_current},
    {"turn_on", lad5522_turn_on},
    {"turn_off", lad5522_turn_off},
    {"turn_all_on", lad5522_turn_all_on},
    {"turn_all_off", lad5522_turn_all_off},
    {"set_force_mode", lad5522_set_force_mode},
    {"set_measure_mode", lad5522_set_measure_mode},
    {"get_channel_count", lad5522_get_channel_count},
    {"get_min_voltage", lad5522_get_min_voltage},
    {"get_max_voltage", lad5522_get_max_voltage},
    {"get_supply_rail", lad5522_get_supply_rail},
    {"set_supply_rail", lad5522_set_supply_rail},
    {"get_gain", lad5522_get_gain},
    {"set_gain", lad5522_set_gain},
    {"read_sys_reg", lad5522_read_sys_reg},
    {"read_pmu_reg", lad5522_read_pmu_reg},
    {"read_alarm_reg", lad5522_read_alarm_reg},
    {"read_comp_reg", lad5522_read_comp_reg},
    {"read_dac_x1", lad5522_read_dac_x1},
    {"reset", lad5522_reset},
    {"configure", lad5522_configure},
    {"__gc", lad5522_destroy},
    {NULL, NULL}
};

static const luaL_Reg lad5522_functions[] = {
    {"new", lad5522_new},
    {NULL, NULL}
};

int luaopen_ad5522(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Lad5522");
    /* Duplicate the metatable on the stack (We know have 2). */
    lua_pushvalue(L, -1);
    /* Pop the first metatable off the stack and assign it to __index
     * of the second one. We set the metatable for the table to itself.
     * This is equivalent to the following in lua:
     * metatable = {}
     * metatable.__index = metatable
     */
    lua_setfield(L, -2, "__index");

    /* Set the methods to the metatable that should be accessed via object:func */
    luaL_setfuncs(L, lad5522_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, lad5522_functions);

    return 1;
}
