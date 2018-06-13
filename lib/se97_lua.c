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
#include "se97.h"

typedef struct {
    se97_t *s;
} lse97_userdata_t;

static int lse97_new(lua_State *L)
{
    lse97_userdata_t *su;
    int i2cbus, address;

    i2cbus = luaL_checkinteger(L, 1);
    if (i2cbus < 0)
        luaL_error(L, "i2cbus cannot be a negative number");

    address = luaL_checkinteger(L, 2);
    if ((address < 0x08) || (address > 0x77))
        luaL_error(L, "No valid i2c 7-bit address");

    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su       = (lse97_userdata_t *)lua_newuserdata(L, sizeof(*su));
    su->s    = NULL;

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Lse97");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    /* Create the data that comprises the userdata (the se97 state). */
    su->s    = se97_create(i2cbus, address);

    return 1;
}

static int lse97_destroy(lua_State *L)
{
    lse97_userdata_t *su;

    su = (lse97_userdata_t *)luaL_checkudata(L, 1, "Lse97");

    if (su->s != NULL)
        se97_destroy(&(su->s));
    su->s = NULL;

    return 0;
}

#define BOARD_ID_SIZE 8
static int lse97_get_board_id(lua_State *L)
{
    char board_id_str[2 * BOARD_ID_SIZE+1];
    int ret, ptr = 0;
    lse97_userdata_t *su;
    su = (lse97_userdata_t *)luaL_checkudata(L, 1, "Lse97");

	if (se97_read_eeprom(su->s)<0)
	{
		lua_pushstring(L, "XXXXXXXXXXXXXXXX");
		return 1;
	}
	char *board_id_buf=(char*)(su->s->eeprom_data);
    /* Bytewise convert number to hexadecimal ASCII representation */
    for (ret = 0; ret < BOARD_ID_SIZE; ret++) {
        ptr += snprintf(board_id_str + ptr, sizeof board_id_str - ptr, "%.2X", board_id_buf[ret]);
    }
    lua_pushstring(L, board_id_str);
    return 1;
}

static int lse97_get_temperature(lua_State *L)
{
    char temp_str[20] = {' '};
    lse97_userdata_t *su;
    su = (lse97_userdata_t *)luaL_checkudata(L, 1, "Lse97");

	if (se97_read_temperature(su->s)>=0) {
		snprintf(temp_str,20,"%3.3f",su->s->last_temperature);
		lua_pushstring(L, temp_str);
	} else {
		lua_pushstring(L, "-1000.0");
	}
    return 1;
}

static const luaL_Reg lse97_methods[] = {
    {"get_id", lse97_get_board_id},
    {"get_temperature", lse97_get_temperature},
    {"__gc", lse97_destroy},
    {NULL, NULL}
};

static const luaL_Reg lse97_functions[] = {
    {"new", lse97_new},
    {NULL, NULL}
};

int luaopen_se97(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Lse97");
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
    luaL_setfuncs(L, lse97_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, lse97_functions);

    return 1;
}
