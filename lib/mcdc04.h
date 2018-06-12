#ifndef _MCDC04_H_
#define _MCDC04_H_
#include <lua.h>
//  version macros for compile-time API detection

#define MCDC04_VERSION_MAJOR 3
#define MCDC04_VERSION_MINOR 0
#define MCDC04_VERSION_PATCH 0

#define MCDC04_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define MCDC04_VERSION \
    MCDC04_MAKE_VERSION(MCDC04_VERSION_MAJOR, MCDC04_VERSION_MINOR, MCDC04_VERSION_PATCH)
#define PMU_MAX_CHANNEL 3
//  Opaque class structures to allow forward references
typedef struct _mcdc04_t mcdc04_t;

mcdc04_t * mcdc04_create(int i2cbus, int address);
void mcdc04_destroy(mcdc04_t **self_p);
void mcdc04_set_measure_mode(mcdc04_t *self, int mode);
void mcdc04_set_iref(mcdc04_t *self, int val);
void mcdc04_set_tint(mcdc04_t *self, int val);
void mcdc04_read_raw(mcdc04_t *self, unsigned int ch, unsigned int *val);
void mcdc04_trigger(mcdc04_t *self);
int luaopen_mcdc04(lua_State *L);
#endif
