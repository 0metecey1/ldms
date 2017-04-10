#ifndef _DIB_H_
#define _DIB_H_
#include <lua.h>
//  version macros for compile-time API detection

#define DIB_VERSION_MAJOR 1
#define DIB_VERSION_MINOR 0
#define DIB_VERSION_PATCH 0

#define DIB_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define DIB_VERSION \
    DIB_MAKE_VERSION(DIB_VERSION_MAJOR, DIB_VERSION_MINOR, DIB_VERSION_PATCH)
int luaopen_dib(lua_State *L);
#endif
