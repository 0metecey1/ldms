#ifndef _TMP116_H_
#define _TMP116_H_
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
typedef struct _tmp116_t tmp116_t;

tmp116_t *tmp116_create(int i2cbus, int address);
void tmp116_destroy(tmp116_t **self_p);
int tmp116_write_eeprom(tmp116_t *self, const char *buf);
int tmp116_read_eeprom(tmp116_t *self, char *buf);
int tmp116_read_temp(tmp116_t *self, int index, int *val);
int luaopen_tmp116(lua_State *L);
#endif

