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
#include "pca9536.h"



typedef struct {
    pca9536_t *s;
} lpca9536_userdata_t;



static int lpca9536_new(lua_State *L)
{
    lpca9536_userdata_t *su;
    int i2cbus, address;
	unsigned char direction=255, output=255;

    i2cbus = luaL_checkinteger(L, 1);
    if (i2cbus < 0)
	{
        luaL_error(L, "i2cbus cannot be a negative number");
		return 0;
	}
    address = luaL_checkinteger(L, 2);
    if ((address < 0x08) || (address > 0x77))
	{
        luaL_error(L, "No valid i2c 7-bit address");
		return 0;
	}
	direction = luaL_checkinteger(L, 3);
    if ((direction < 0) || (direction > 0xFF))
	{
        luaL_error(L, "No valid direction configuration, allowed: 0..255");
		return 0;
	}
	output = luaL_checkinteger(L, 4);
    if ((output < 0) || (output > 0xFF))
	{
        luaL_error(L, "No valid output value, allowed: 0..255");
		return 0;
	}
    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su       = (lpca9536_userdata_t *)lua_newuserdata(L, sizeof(*su));
    su->s    = NULL;

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Lpca9536");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    /* Create the data that comprises the userdata (the pca9536 state). */
    su->s    = pca9536_create(i2cbus, address, direction, output);

    return 1;
}

static int lpca9536_destroy(lua_State *L)
{
    lpca9536_userdata_t *su;

    su = (lpca9536_userdata_t *)luaL_checkudata(L, 1, "Lpca9536");

    if (su->s != NULL)
        pca9536_destroy(&(su->s));
    su->s = NULL;

    return 0;
}





static int lpca9536_output(lua_State *L)
{
    int ret, ptr = 0;
    lpca9536_userdata_t *su;
    su = (lpca9536_userdata_t *)luaL_checkudata(L, 1, "Lpca9536");

	unsigned int output = luaL_checkinteger(L, 2);
    if ((output < 0) || (output > 0xFF))
	{
        luaL_error(L, "No valid output value, allowed: 0..255");
		return 0;
	}
	pca9536_output(su->s,output);
    return 1;
}

static int lpca9536_input(lua_State *L)
{
    char temp_str[20] = {' '};
    lpca9536_userdata_t *su;
	unsigned char input_value;
    su = (lpca9536_userdata_t *)luaL_checkudata(L, 1, "Lpca9536");

	if (pca9536_input(su->s, &input_value)>=0) {
		lua_pushnumber(L, input_value);
	} else {
		lua_pushnumber(L, -1);
	}
    return 1;
}

static const luaL_Reg lpca9536_methods[] = {
    {"output", lpca9536_output},
    {"input", lpca9536_input},
    {"__gc", lpca9536_destroy},
    {NULL, NULL}
};

static const luaL_Reg lpca9536_functions[] = {
    {"new", lpca9536_new},
    {NULL, NULL}
};

int luaopen_pca9536(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Lpca9536");
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
    luaL_setfuncs(L, lpca9536_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, lpca9536_functions);

    return 1;
}
