#ifndef _DB_H_
#define _DB_H_
#include <lua.h>
//  version macros for compile-time API detection

#define DB_VERSION_MAJOR 3
#define DB_VERSION_MINOR 0
#define DB_VERSION_PATCH 0

#define DB_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define DB_VERSION \
    DB_MAKE_VERSION(DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH)
int luaopen_db(lua_State *L);
#endif
