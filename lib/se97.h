#ifndef _SE97_H_
#define _SE97_H_
#include <stdint.h>
#include <lua.h>
//  version macros for compile-time API detection

#define ID_VERSION_MAJOR 3
#define ID_VERSION_MINOR 0
#define ID_VERSION_PATCH 0

#define ID_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define ID_VERSION \
    ID_MAKE_VERSION(ID_VERSION_MAJOR, ID_VERSION_MINOR, ID_VERSION_PATCH)

//  Opaque class structures to allow forward references
typedef struct _se97_t se97_t;

se97_t *se97_create(int i2cbus, int address);
void se97_destroy(se97_t **self_p);
int se97_write_eeprom(se97_t *self, const char *buf);
int se97_read_eeprom(se97_t *self, char *buf);
int se97_read_temp(se97_t *self, int index, int *val);
#endif

