#ifndef _JC42_H_
#define _JC42_H_
#include <stdint.h>
#include <lua.h>
//  version macros for compile-time API detection

#define ID_VERSION_MAJOR 0
#define ID_VERSION_MINOR 0
#define ID_VERSION_PATCH 0

#define ID_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define ID_VERSION \
    ID_MAKE_VERSION(ID_VERSION_MAJOR, ID_VERSION_MINOR, ID_VERSION_PATCH)
	
//  Opaque class structures to allow forward references
typedef struct _jc42_t jc42_t;

jc42_t *jc42_create(int i2cbus, int address);
void jc42_destroy(jc42_t **self_p);
int jc42_write_eeprom(jc42_t *self);
int jc42_read_eeprom(jc42_t *self);
int jc42_read_temperature(jc42_t *self);
#endif

