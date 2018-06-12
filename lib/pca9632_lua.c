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
#include "pca9632.h"



typedef struct {
    pca9632_t *s;
} lpca9632_userdata_t;



static int lpca9632_new(lua_State *L)
{
    lpca9632_userdata_t *su;
    int i2cbus, address;
	unsigned char polarity_inverted=0, output_mode_pushpull=0;

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
	polarity_inverted = luaL_checkinteger(L, 3);
    if ((polarity_inverted < 0) || (polarity_inverted > 1))
	{
        luaL_error(L, "No valid polarity_inverted given, allowed: 0..1");
		return 0;
	}
	output_mode_pushpull = luaL_checkinteger(L, 4);
    if ((output_mode_pushpull < 0) || (output_mode_pushpull > 1))
	{
        luaL_error(L, "No valid output_mode_pushpull value, allowed: 0..1");
		return 0;
	}
    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su       = (lpca9632_userdata_t *)lua_newuserdata(L, sizeof(*su));
    su->s    = NULL;

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Lpca9632");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    /* Create the data that comprises the userdata (the pca9632 state). */
    su->s    = pca9632_create(i2cbus, address, polarity_inverted, output_mode_pushpull);

    return 1;
}

static int lpca9632_destroy(lua_State *L)
{
    lpca9632_userdata_t *su;

    su = (lpca9632_userdata_t *)luaL_checkudata(L, 1, "Lpca9632");

    if (su->s != NULL)
        pca9632_destroy(&(su->s));
    su->s = NULL;

    return 0;
}





static int lpca9632_set_channel_output(lua_State *L)
{
    int ret, ptr = 0;
    lpca9632_userdata_t *su;
	
    su = (lpca9632_userdata_t *)luaL_checkudata(L, 1, "Lpca9632");
	unsigned int channel = luaL_checkinteger(L, 2);
    if ((channel < 0) || (channel > 3))
	{
        luaL_error(L, "No valid channel value, allowed: 0..3");
		return 0;
	}
	unsigned int output = luaL_checkinteger(L, 3);
    if ((output < 0) || (output > 0x0100))
	{
        luaL_error(L, "No valid output value, allowed: 0..256");
		return 0;
	}
	pca9632_set_channel_output(su->s,channel,output);
    return 1;
}



static int lpca9632_set_channel_mode(lua_State *L)
{
    int ret, ptr = 0;
    lpca9632_userdata_t *su;
	
    su = (lpca9632_userdata_t *)luaL_checkudata(L, 1, "Lpca9632");
	unsigned int channel = luaL_checkinteger(L, 2);
    if ((channel < 0) || (channel > 3))
	{
        luaL_error(L, "No valid channel value, allowed: 0..3");
		return 0;
	}
	unsigned int mode = luaL_checkinteger(L, 3);
    if ((mode < 0) || (mode > 2))
	{
        luaL_error(L, "No valid mode value, allowed: 0..2");
		return 0;
	}
	pca9632_set_channel_mode(su->s,channel,mode);
    return 1;
}

static int lpca9632_all_off(lua_State *L)
{
    int ret, ptr = 0;
    lpca9632_userdata_t *su;
	
    su = (lpca9632_userdata_t *)luaL_checkudata(L, 1, "Lpca9632");
	pca9632_switch_off_all_channels(su->s);
    return 1;
}

static const luaL_Reg lpca9632_methods[] = {
    {"set_channel_output", lpca9632_set_channel_output},
    {"set_channel_mode", lpca9632_set_channel_mode},
	{"all_off", lpca9632_all_off},
    {"__gc", lpca9632_destroy},
    {NULL, NULL}
};

static const luaL_Reg lpca9632_functions[] = {
    {"new", lpca9632_new},
    {NULL, NULL}
};

int luaopen_pca9632(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Lpca9632");
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
    luaL_setfuncs(L, lpca9632_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, lpca9632_functions);

    return 1;
}
