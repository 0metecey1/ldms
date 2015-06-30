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
#include "tlc5948a.h"

typedef struct {
    tlc5948a_t *s;
    char *spi_name;
} ltlc5948a_userdata_t;

static int ltlc5948a_new(lua_State *L)
{
    ltlc5948a_userdata_t *su;
    const char *spi_name;

    /* Check the arguments are valid. */
    spi_name  = luaL_checkstring(L, 1);
    if (spi_name == NULL)
        luaL_error(L, "spi_name cannot be empty");

    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su       = (ltlc5948a_userdata_t *)lua_newuserdata(L, sizeof(*su));
    su->s    = NULL;
    su->spi_name = NULL;

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Ltlc5948a");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    /* Create the data that comprises the userdata (the tlc5948a state). */
    su->s    = tlc5948a_create(spi_name);
    su->spi_name = strdup(spi_name);

    return 1;
}

static int ltlc5948a_destroy(lua_State *L)
{
    ltlc5948a_userdata_t *su;

    su = (ltlc5948a_userdata_t *)luaL_checkudata(L, 1, "Ltlc5948a");

    if (su->s != NULL)
        tlc5948a_destroy(&(su->s));
    su->s = NULL;

    if (su->spi_name != NULL)
        free(su->spi_name);
    su->spi_name = NULL;

    return 0;
}

static int ltlc5948a_set_brightness(lua_State *L)
{
    ltlc5948a_userdata_t *su;
    unsigned int ch, level;

    su = (ltlc5948a_userdata_t *)luaL_checkudata(L, 1, "Ltlc5948a");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    level = luaL_checkinteger(L, 3);
    tlc5948a_set_brightness(su->s, ch - 1, level); /* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int ltlc5948a_turn_on(lua_State *L)
{
    ltlc5948a_userdata_t *su;
    unsigned int ch;

    su = (ltlc5948a_userdata_t *)luaL_checkudata(L, 1, "Ltlc5948a");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    tlc5948a_turn_on(su->s, ch - 1); /* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int ltlc5948a_turn_off(lua_State *L)
{
    ltlc5948a_userdata_t *su;
    unsigned int ch;

    su = (ltlc5948a_userdata_t *)luaL_checkudata(L, 1, "Ltlc5948a");
    /* Check the arguments are valid. */
    ch = luaL_checkinteger(L, 2);
    tlc5948a_turn_off(su->s, ch - 1);/* use 1..4 indexing in Lua, but 0..3 in C */
    return 0;
}

static int ltlc5948a_turn_all_off(lua_State *L)
{
    ltlc5948a_userdata_t *su;

    su = (ltlc5948a_userdata_t *)luaL_checkudata(L, 1, "Ltlc5948a");
    tlc5948a_turn_all_off(su->s);
    return 0;
}

static const luaL_Reg ltlc5948a_methods[] = {
    {"set_brightness", ltlc5948a_set_brightness},
    {"turn_on", ltlc5948a_turn_on},
    {"turn_off", ltlc5948a_turn_off},
    {"turn_all_off", ltlc5948a_turn_all_off},
    {"__gc", ltlc5948a_destroy},
    {NULL, NULL}
};

static const luaL_Reg ltlc5948a_functions[] = {
    {"new", ltlc5948a_new},
    {NULL, NULL}
};

int luaopen_tlc5948a(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Ltlc5948a");
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
    luaL_setfuncs(L, ltlc5948a_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, ltlc5948a_functions);

    return 1;
}
