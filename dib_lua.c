/*
 * Provide auxillary functions to present board id and box id to Lua interpreter
 */

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
#include <dirent.h>

#define BOX_TEMP_SIZE 7
#define BOX_ID_SIZE 8
#define BOARD_ID_SIZE 6

typedef struct {
    char w1_path[128];
} ldib_userdata_t;

static int ldib_new(lua_State *L)
{
    ldib_userdata_t *su;
    const char *w1_path;

    w1_path = luaL_checkstring(L, 1);

    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su = (ldib_userdata_t *)lua_newuserdata(L, sizeof(*su));
    strcpy(su->w1_path, w1_path);

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Lid");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    return 1;
}

static int ldib_destroy(lua_State *L)
{
    return 0;
}

static int ldib_get_id(lua_State *L)
{
    char id_str[20] = {' '};
    struct dirent *d_entp;
    DIR *dp;
    FILE *fp;
    int path_length;
    char path[PATH_MAX];
    ldib_userdata_t *su;
    su = (ldib_userdata_t *)luaL_checkudata(L, 1, "Lid");

    dp = opendir(su->w1_path);
    if (dp == NULL) {
        lua_pushstring(L, " ");
        return 1;
    }

    /* Traverse directory, take address from first sub-directory matching family code 23 */
    while((d_entp = readdir(dp)) != NULL)
    {
        if(strncmp(d_entp->d_name, "3B.", 3) == 0)
        {
            path_length = snprintf (path, PATH_MAX,
                    "%s/%s/%s", su->w1_path, d_entp->d_name, "address");
            if (path_length >= PATH_MAX) {
                lua_pushstring(L, " ");
                return 1;
            }
            fp = fopen(path, "r");
            if (fgets(id_str, 2 * BOX_ID_SIZE + 1, fp) == NULL) {
                lua_pushstring(L, " ");
                return 1;
            }
            fclose(fp);
            break;
        }
    }
    closedir(dp);
    lua_pushstring(L, id_str);
    return 1;
}

static int ldib_get_temperature(lua_State *L)
{
    char temp_str[20] = {' '};
    struct dirent *d_entp;
    DIR *dp;
    FILE *fp;
    int path_length;
    char path[PATH_MAX];
    ldib_userdata_t *su;
    su = (ldib_userdata_t *)luaL_checkudata(L, 1, "Lid");

    dp = opendir(su->w1_path);
    if (dp == NULL) {
        lua_pushstring(L, "-1000.0");
        return 1;
    }

    /* Traverse directory, take address from first sub-directory matching family code 3B */
    while((d_entp = readdir(dp)) != NULL)
    {
        if(strncmp(d_entp->d_name, "3B.", 3) == 0)
        {
            path_length = snprintf (path, PATH_MAX,
                    "%s/%s/%s", su->w1_path, d_entp->d_name, "temperature");
            if (path_length >= PATH_MAX) {
                lua_pushstring(L, "-1000.0");
                return 1;
            }
            fp = fopen(path, "r");
            if (fgets(temp_str, 2 * BOX_TEMP_SIZE + 1, fp) == NULL) {
                lua_pushstring(L, "-1000.0");
                return 1;
            }
            fclose(fp);
            break;
        }
    }
    closedir(dp);
    lua_pushstring(L, temp_str);
    return 1;
}

static const luaL_Reg ldib_methods[] = {
    {"get_id", ldib_get_id},
    {"get_temperature", ldib_get_temperature},
    {"__gc", ldib_destroy},
    {NULL, NULL}
};

static const luaL_Reg ldib_functions[] = {
    {"new", ldib_new},
    {NULL, NULL}
};

int luaopen_dib(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Lid");
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
    luaL_setfuncs(L, ldib_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, ldib_functions);

    return 1;
}
