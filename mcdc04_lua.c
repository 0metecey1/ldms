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
#include "mcdc04.h"

#define CIEX 3
#define CIEY 1
#define CIEZ 2
#define TRD  0

#define ARRAYSIZE(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
    mcdc04_t *s;
    double **m; /* 3-by-3 calibration matrix */
    /* Calibrated color coordinate according to CIE 1931, 2 observer */
} lmcdc04_userdata_t;

const int iref_tbl[] = {2, 2, 1, 1, 0, 0, 0, 0, 0};
const int tint_tbl[] = {6, 7, 6, 7, 6, 7, 8, 9, 10};

static void matrix_init(double ***A, int r, int c)
{
    int i, j;
    *A = (double **)malloc(sizeof(double *)*r);
    for( i = 0; i< r; i++) {
        (*A)[i] = (double *)malloc(sizeof(double) *c);
        for( j = 0; j < c; j++) {
            if (i == j)
                (*A)[i][j] = 1.0;
            else
                (*A)[i][j] = 0.0;
        }
    }
}

static int lmcdc04_new(lua_State *L)
{
    lmcdc04_userdata_t *su;
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
    su       = (lmcdc04_userdata_t *)lua_newuserdata(L, sizeof(*su));
    su->s    = NULL;

    matrix_init(&(su->m), 3, 3);

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Lmcdc04");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    /* Create the data that comprises the userdata (the mcdc04 state). */
    su->s    = mcdc04_create(i2cbus, address);

    return 1;
}

static int lmcdc04_destroy(lua_State *L)
{
    lmcdc04_userdata_t *su;

    su = (lmcdc04_userdata_t *)luaL_checkudata(L, 1, "Lmcdc04");

    if (su->s != NULL)
        mcdc04_destroy(&(su->s));
    su->s = NULL;

    return 0;
}

static int lmcdc04_set_measure_mode(lua_State *L)
{
    lmcdc04_userdata_t *su;
    const char *mode_str;
    int mode;

    su = (lmcdc04_userdata_t *)luaL_checkudata(L, 1, "Lmcdc04");
    mode_str  = luaL_checkstring(L, 2);
    if (mode_str == NULL) 
        luaL_error(L, "mode cannot be empty");

    if ((strcmp(mode_str, "cont") == 0) || (strcmp(mode_str, "CONT") == 0)) {
        mode = 0;
    } else if ((strcmp(mode_str, "cmd") == 0) || (strcmp(mode_str, "CMD") == 0)) {
        mode = 1;
    } else if ((strcmp(mode_str, "syns") == 0) || (strcmp(mode_str, "SYNS") == 0)) {
        mode = 2;
    } else if ((strcmp(mode_str, "synd") == 0) || (strcmp(mode_str, "SYND") == 0)) {
        mode = 3;
    } else {
        luaL_error(L, "mode must be any of cont, cmd, syns or synd");
    }

    mcdc04_set_measure_mode(su->s, mode);

    return 0;
}

/*
 * Sets the ADC reference current, argument is iref in nano amp.
 * Will be mapped to nearest range value, lower than the val.
 */
static int lmcdc04_set_gain(lua_State *L)
{
    int gain_idx;
    lmcdc04_userdata_t *su;

    su = (lmcdc04_userdata_t *)luaL_checkudata(L, 1, "Lmcdc04");
    gain_idx = luaL_checknumber(L, 2); /* gain index, higher number mean higher gain */
    mcdc04_set_iref(su->s, iref_tbl[gain_idx]);
    mcdc04_set_tint(su->s, tint_tbl[gain_idx]);
    return 0;
}

static int lmcdc04_get_max_gain(lua_State *L)
{
    lua_pushinteger(L, ARRAYSIZE(iref_tbl) - 1);
    return 1;
}

/*
 * Sets the ADC reference current, argument is iref in nano amp.
 * Will be mapped to nearest range value, lower than the val.
 */
static int lmcdc04_auto_adjust_gain(lua_State *L)
{
    unsigned int val, maxval;
    int gain_idx;
    lmcdc04_userdata_t *su;

    su = (lmcdc04_userdata_t *)luaL_checkudata(L, 1, "Lmcdc04");
    /* the search in mid position of the gain */
    gain_idx = ARRAYSIZE(iref_tbl) / 2;

    while ((gain_idx >= 0) && (gain_idx < ARRAYSIZE(iref_tbl)))
    {
        /* set reference current and integration time */
        mcdc04_set_iref(su->s, iref_tbl[gain_idx]);
        mcdc04_set_tint(su->s, tint_tbl[gain_idx]);
        /* do the measurement, reads all values at the same time */
        mcdc04_trigger(su->s);
        /* read back the results one at a time */
        /* find maximum value over all color channels */
        mcdc04_read_raw(su->s, CIEX, &val);
        maxval = val;
        mcdc04_read_raw(su->s, CIEY, &val);
        maxval = (maxval > val) ? maxval : val;
        mcdc04_read_raw(su->s, CIEZ, &val);
        maxval = (maxval > val) ? maxval : val;

        /* check value */
        if (maxval <  (65535 / 3))
            gain_idx++; /* level to small, increase gain */
        else if (maxval < (2 * 65535 / 3))
            break;
        else
            gain_idx--; /* level to big, decrease gain */
    }
    if (gain_idx >= ARRAYSIZE(iref_tbl))
        gain_idx = -gain_idx + 1;

    lua_pushinteger(L, gain_idx);
    return 1;
}

static double vectors_dot_prod(const double *x, const double *y, int n)
{
    double res = 0.0;
    int i;
    for (i = 0; i < n; i++)
    {
        res += x[i] * y[i];
    }
    return res;
}

static void matrix_vector_mult(const double **mat, const double *vec, double *result, int rows, int cols)
{ // in matrix form: result = mat * vec;
    int i;
    for (i = 0; i < rows; i++)
    {
        result[i] = vectors_dot_prod(mat[i], vec, cols);
    }
}

static int lmcdc04_apply_calibration(lua_State *L)
{
    int ch;
    double val, sum, s[3], t[3];
    lmcdc04_userdata_t *su;

    su = (lmcdc04_userdata_t *)luaL_checkudata(L, 1, "Lmcdc04");
    ch = luaL_checkinteger(L, 2); /* channel number, calibration has spatial components */
    val = luaL_checknumber(L, 3); /*  */
    sum = val;
    s[0] = val;
    val = luaL_checknumber(L, 4); /*  */
    sum += val;
    s[1] = val;
    val = luaL_checknumber(L, 5); /*  */
    sum += val;
    s[2] = val;
    matrix_vector_mult((const double **)(su->m), (const double *)s, t, 3, 3);
    lua_pushnumber(L, t[0]);
    lua_pushnumber(L, t[1]);
    lua_pushnumber(L, t[2]);
    lua_pushnumber(L, t[0]/sum);
    lua_pushnumber(L, t[1]/sum);
    lua_pushnumber(L, t[2]/sum);
    return 6;
}
static int lmcdc04_measure(lua_State *L)
{
    unsigned int val;
    double x, y, z, sum = 0.0;
    lmcdc04_userdata_t *su;
    su = (lmcdc04_userdata_t *)luaL_checkudata(L, 1, "Lmcdc04");

    mcdc04_trigger(su->s);
    mcdc04_read_raw(su->s, CIEX, &val);
    x = (double)val;
    sum += x;
    lua_pushinteger(L, val);
    mcdc04_read_raw(su->s, CIEY, &val);
    y = (double)val;
    sum += y;
    lua_pushinteger(L, val);
    mcdc04_read_raw(su->s, CIEZ, &val);
    z = (double)val;
    sum += z;
    lua_pushinteger(L, val);
    lua_pushnumber(L, x/sum);
    lua_pushnumber(L, y/sum);
    lua_pushnumber(L, z/sum);
    return 6;
}

static const luaL_Reg lmcdc04_methods[] = {
    {"set_gain", lmcdc04_set_gain},
    {"set_measure_mode", lmcdc04_set_measure_mode},
    {"get_max_gain", lmcdc04_get_max_gain},
    {"auto_adjust_gain", lmcdc04_auto_adjust_gain},
    {"apply_calibration", lmcdc04_apply_calibration},
    {"measure", lmcdc04_measure},
    {"__gc", lmcdc04_destroy},
    {NULL, NULL}
};

static const luaL_Reg lmcdc04_functions[] = {
    {"new", lmcdc04_new},
    {NULL, NULL}
};

int luaopen_mcdc04(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Lmcdc04");
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
    luaL_setfuncs(L, lmcdc04_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, lmcdc04_functions);

    return 1;
}
