#ifndef _PCA9536_H_
#define _PCA9536_H_
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
struct _pca9536_t {
    int dev_i2cbus;
    int dev_address;
    int dev_file;
    char dev_filename[128];
};

typedef struct _pca9536_t pca9536_t;

pca9536_t * pca9536_create(int i2cbus, int address, unsigned int direction, unsigned int output);
void pca9536_destroy(pca9536_t **self_p);
int pca9536_output(pca9536_t *self, unsigned int output);
int pca9536_input(pca9536_t *self, unsigned int * input);
int luaopen_pca9536(lua_State *L);
#endif

