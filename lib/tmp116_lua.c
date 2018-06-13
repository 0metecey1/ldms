#define LUA_LIB
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include "tmp116.h"

typedef struct {
    tmp116_t *s;
} ltmp116_userdata_t;

static int ltmp116_new(lua_State *L)
{
    ltmp116_userdata_t *su;
    int i2cbus, address;

    i2cbus = luaL_checkinteger(L, 1);
    address = luaL_checkinteger(L, 2);

    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su       = (ltmp116_userdata_t *)lua_newuserdata(L, sizeof(*su));
    su->s    = NULL;

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Ltmp116");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    if (i2cbus < 0)
    {
        luaL_error(L, "i2cbus cannot be a negative number");
        return 1;
    }

    if ((address < 0x08) || (address > 0x77))
    {
        luaL_error(L, "No valid i2c 7-bit address");
        return 1;
    }

    /* Create the data that comprises the userdata (the tmp116 state). */
    su->s    = tmp116_create(i2cbus, address);

    return 1;
}

static int ltmp116_destroy(lua_State *L)
{
    ltmp116_userdata_t *su;

    su = (ltmp116_userdata_t *)luaL_checkudata(L, 1, "Ltmp116");

    if (su->s != NULL)
        tmp116_destroy(&(su->s));
    su->s = NULL;

    return 0;
}

#define BOARD_ID_SIZE 8
static int ltmp116_get_board_id(lua_State *L)
{
    char board_id_str[2 * BOARD_ID_SIZE+1];
    int ret, ptr = 0;
    ltmp116_userdata_t *su;
    su = (ltmp116_userdata_t *)luaL_checkudata(L, 1, "Ltmp116");

    if (tmp116_read_eeprom(su->s)<0)
    {
        lua_pushstring(L, "XXXXXXXXXXXXXXXX");
        return 1;
    }
    char *board_id_buf=(char*)(su->s->eeprom_data);
    /* Bytewise convert number to hexadecimal ASCII representation */
    for (ret = 0; ret < BOARD_ID_SIZE; ret++) {
        ptr += snprintf(board_id_str + ptr, sizeof board_id_str - ptr, "%.2X",
                board_id_buf[ret]);
    }
    lua_pushstring(L, board_id_str);
    return 1;
}

static int ltmp116_get_temperature(lua_State *L)
{
    char temp_str[20] = {' '};
    ltmp116_userdata_t *su;
    su = (ltmp116_userdata_t *)luaL_checkudata(L, 1, "Ltmp116");

    if (tmp116_read_temperature(su->s)>=0) {
        snprintf(temp_str,20,"%3.3f",su->s->last_temperature);
        lua_pushstring(L, temp_str);
    } else {
        lua_pushstring(L, "-1000.0");
    }
    return 1;
}

static const luaL_Reg ltmp116_methods[] = {
    {"get_id", ltmp116_get_board_id},
    {"get_temperature", ltmp116_get_temperature},
    {"__gc", ltmp116_destroy},
    {NULL, NULL}
};

static const luaL_Reg ltmp116_functions[] = {
    {"new", ltmp116_new},
    {NULL, NULL}
};

int luaopen_tmp116(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Ltmp116");
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
    luaL_setfuncs(L, ltmp116_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, ltmp116_functions);

    return 1;
}
