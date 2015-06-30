
#ifndef _AD5522_H_
#define _AD5522_H_
#include <lua.h>
//  version macros for compile-time API detection

#define AD5522_VERSION_MAJOR 3
#define AD5522_VERSION_MINOR 0
#define AD5522_VERSION_PATCH 0

#define AD5522_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define AD5522_VERSION \
    AD5522_MAKE_VERSION(AD5522_VERSION_MAJOR, AD5522_VERSION_MINOR, AD5522_VERSION_PATCH)
#define PMU_MAX_CHANNEL 3

#define AD5522_NUM_CHANNELS 4

/* We restrict the available PMU source/measure modes to the most commen cases */
#define PMU_MODE_FVMI 0
#define PMU_MODE_FIMV 1
#define PMU_MODE_HIZ  2
#define PMU_MODE_BITMASK  0x186000 /* B20..B19 | B14..B13 */

#define PMU_FORCE_MODE_FV 0
#define PMU_FORCE_MODE_FI 1
#define PMU_FORCE_MODE_HIZV  2
#define PMU_FORCE_MODE_HIZI  3
#define PMU_FORCE_MODE_BITMASK 0x180000

#define PMU_MEASURE_MODE_MI 0
#define PMU_MEASURE_MODE_MV 1
#define PMU_MEASURE_MODE_MT  2
#define PMU_MEASURE_MODE_HIZ  3
#define PMU_MEASURE_MODE_BITMASK 0x6000

#define PMU_RANGE_5_MICROAMP     0
#define PMU_RANGE_20_MICROAMP    1
#define PMU_RANGE_200_MICROAMP   2
#define PMU_RANGE_2000_MICROAMP  3
#define PMU_RANGE_EXT            4
#define PMU_RANGE_BITMASK  0x38000 /* B17..B15 */

#define PMU_CHANNEL_OFF        0
#define PMU_CHANNEL_ON         1
#define PMU_ENABLE_BITMASK  0x201000 /* B21 | B16 */

/* PMU settings, defines which PMU is active, or if global settings are provided */
#define AD5522_REG_SYSCTRL 0x00
#define AD5522_REG_COMP    0x01
#define AD5522_REG_ALARM   0x03

#define AD5522_REG_PMU(ch) ((ch) << 2)
#define PMU0               0x01
#define PMU1               0x02
#define PMU2               0x04
#define PMU3               0x08
#define RD_NOTWR           0x40
#define DAC_RD_NOTWR       RD_NOTWR << 6 

/* System control register specific settings */
#define SYS_CTRL_TMP_100                   0x7 << 3
#define SYS_CTRL_TMP_110                   0x6 << 3
#define SYS_CTRL_TMP_120                   0x5 << 3
#define SYS_CTRL_TMP_130                   0x4 << 3
#define SYS_CTRL_TMP_DISABLE               0 << 5
#define SYS_CTRL_I_GAIN_10                 0 << 6
#define SYS_CTRL_I_GAIN_5                  1 << 6
#define SYS_CTRL_MEASOUT_GAIN_1000_MILLI   0 << 7
#define SYS_CTRL_MEASOUT_GAIN_200_MILLI    1 << 7
#define SYS_CTRL_GUARDEN                   1 << 8
#define SYS_CTRL_INT10K                    1 << 9
#define SYS_CTRL_CLAMPALM                  1 << 10
#define SYS_CTRL_GUARDALM                  1 << 11
#define SYS_CTRL_DUTGND                    1 << 12
#define SYS_CTRL_CPBIASEN                  1 << 13
#define SYS_CTRL_CPOLH0                    1 << 14
#define SYS_CTRL_CPOLH1                    1 << 15
#define SYS_CTRL_CPOLH2                    1 << 16
#define SYS_CTRL_CPOLH3                    1 << 17
#define SYS_CTRL_CL0                       1 << 18
#define SYS_CTRL_CL1                       1 << 19
#define SYS_CTRL_CL2                       1 << 20
#define SYS_CTRL_CL3                       1 << 21
#define SYS_CTRL_GAIN_BITMASK              0xc0 /* bits 6 and 7 */

/* Alarm status register */
#define ALARM_TMPALM         1 << 20 
#define ALARM_LTMPALM        1 << 21 
#define ALARM_TMPALM_BITMASK 0x300000 /* bits 20 and 21 */

/* PMU specific functions - only valid when writing PMU register */
#define PMU_CLEAR             1 << 6
#define PMU_COMP_VI           1 << 7
#define PMU_CPOLH             1 << 8
#define PMU_CL                1 << 9
#define PMU_SF_HIZ_SS_HIZ     0 << 10
#define PMU_SF_HIZ_SS_MEASVH  1 << 10
#define PMU_SF_FOH_SS_HIZ     2 << 10
#define PMU_SF_FOH_SS_MEASVH  3 << 10
#define PMU_FIN               1 << 12
#define PMU_MEAS_I            0 << 13
#define PMU_MEAS_V            1 << 13
#define PMU_MEAS_T            2 << 13
#define PMU_MEAS_HIZ          3 << 13
#define PMU_I_5_MICROAMP        0 << 15
#define PMU_I_20_MICROAMP       1 << 15
#define PMU_I_200_MICROAMP      2 << 15
#define PMU_I_2000_MICROAMP     3 << 15
#define PMU_I_EXT             4 << 15
#define PMU_I_DI_EXT_BUF      5 << 15
#define PMU_I_EN_EXT_BUF      6 << 15
#define PMU_FV                0 << 19
#define PMU_FI                1 << 19
#define PMU_HIZ_V             2 << 19
#define PMU_HIZ_I             3 << 19
#define PMU_CH_EN             1 << 21

/* DAC register addresses - only valid when writing DAC register */
/* Choose M, C, X1 register with MODE0/MODE1, 
 * i.e. DAC_M_REG, DAC_C_REG, DAC_X1_REG */
#define DAC_OFFSET_X             0x00 /* only X1 register available */
#define DAC_FIN_I_5_MICROAMP     0x08
#define DAC_FIN_I_20_MICROAMP    0x09
#define DAC_FIN_I_200_MICROAMP   0x0a
#define DAC_FIN_I_2000_MICROAMP  0x0b
#define DAC_FIN_I_EXT            0x0c
#define DAC_FIN_V                0x0d
#define DAC_CLL_I                0x14
#define DAC_CLL_V                0x15
#define DAC_CLH_I                0x1c
#define DAC_CLH_V                0x1d
#define DAC_CPL_I_5_MICROAMP     0x20
#define DAC_CPL_I_20_MICROAMP    0x21
#define DAC_CPL_I_200_MICROAMP   0x22
#define DAC_CPL_I_2000_MICROAMP  0x23
#define DAC_CPL_I_EXT            0x24
#define DAC_CPL_V                0x25
#define DAC_CPH_I_5_MICROAMP     0x28
#define DAC_CPH_I_20_MICROAMP    0x29
#define DAC_CPH_I_200_MICROAMP   0x2a
#define DAC_CPH_I_2000_MICROAMP  0x2b
#define DAC_CPH_I_EXT            0x2c
#define DAC_CPH_V                0x2d

//  Opaque class structures to allow forward references
typedef struct _ad5522_t ad5522_t;

ad5522_t * ad5522_create(const char *ad5522_path);
void ad5522_destroy(ad5522_t **self_p);
void ad5522_set_force_mode(ad5522_t *self, unsigned int ch, unsigned int mode);
void ad5522_set_measure_mode(ad5522_t *self, unsigned int ch, unsigned int mode);
void ad5522_set_range(ad5522_t *self, unsigned int ch, unsigned int range);
void ad5522_get_range(ad5522_t *self, unsigned int ch, unsigned int *range);
void ad5522_set_offset(ad5522_t *self, unsigned int level);
void ad5522_set_voltage(ad5522_t *self, unsigned int ch, int level);
void ad5522_set_current(ad5522_t *self, unsigned int ch, int level);
void ad5522_set_compliance(ad5522_t *self, unsigned int ch, int level);
void ad5522_set_output_state(ad5522_t *self, unsigned int ch, unsigned int state);
void ad5522_set_all_output_state(ad5522_t *self, unsigned int state);
void ad5522_read_pmu_reg(ad5522_t *self, unsigned int ch, unsigned int *val);
void ad5522_read_sysctrl_reg(ad5522_t *self, unsigned int *val);
void ad5522_read_alarm_reg(ad5522_t *self, unsigned int *val);
void ad5522_read_comp_reg(ad5522_t *self, unsigned int *val);
void ad5522_read_fin_dac_x1(ad5522_t *self, unsigned int ch, unsigned int range, unsigned int *val);
void ad5522_set_gain(ad5522_t *self, int gain);
void ad5522_get_gain(ad5522_t *self, int *gain);
void ad5522_configure(ad5522_t *self, unsigned int *sysval, unsigned int *pmuval);
void ad5522_get_alarm_flag(ad5522_t *self, int *flag);
void ad5522_clear_alarm_flag(ad5522_t *self);
int luaopen_ad5522(lua_State *L);
#endif
