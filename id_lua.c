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

typedef struct {
    char box_id_path[128];
    char board_id_path[128];
} lid_userdata_t;

static int lid_new(lua_State *L)
{
    lid_userdata_t *su;
    const char *box_id_path, *board_id_path;

    board_id_path = luaL_checkstring(L, 1);
    box_id_path = luaL_checkstring(L, 2);

    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su = (lid_userdata_t *)lua_newuserdata(L, sizeof(*su));
    strcpy(su->box_id_path, box_id_path);
    strcpy(su->board_id_path, board_id_path);

    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Lid");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);

    return 1;
}

static int lid_destroy(lua_State *L)
{
    return 0;
}

static int lid_get_board_id(lua_State *L)
{
    FILE *fp;
    char board_id[6];
    char board_id_str[13];
    lid_userdata_t *su;
    su = (lid_userdata_t *)luaL_checkudata(L, 1, "Lid");

    // fp = fopen("/sys/class/net/eth0/address", "r");
    fprintf(stderr, "%s", su->board_id_path);
    fp = fopen(su->board_id_path, "r");

    if (!fseek(fp, -6, SEEK_END))
        snprintf(board_id_str, 12, "            ");
    fread(&board_id, sizeof(board_id), 1, fp);
    fclose(fp);
    snprintf(board_id_str, 12, "%X%X%X%X%X%X", board_id[0], board_id[1], board_id[2],
            board_id[3], board_id[4], board_id[5]);
    lua_pushstring(L, board_id_str);
    return 1;
}

static int lid_get_box_id(lua_State *L)
{
    static char box_id_str[20] = {' '};
    // const char *dir_name = "/var/lib/w1/bus.0/bus.0";
    struct dirent *d_entp;
    DIR *dp;
    FILE *fp;
    int path_length;
    char path[PATH_MAX];
    lid_userdata_t *su;
    su = (lid_userdata_t *)luaL_checkudata(L, 1, "Lid");

    dp = opendir(su->box_id_path);
    if (dp == NULL) {
        lua_pushstring(L, " ");
        return 1;
    }

    /* Traverse directory, take address from first sub-directory matching family code 23 */
    while((d_entp = readdir(dp)) != NULL)
    {
        if(strncmp(d_entp->d_name, "23.", 3) == 0)
        {
            path_length = snprintf (path, PATH_MAX,
                    "%s/%s/%s", su->box_id_path, d_entp->d_name, "address");
            if (path_length >= PATH_MAX) {
                lua_pushstring(L, " ");
                return 1;
            }
            fp = fopen(path, "r");
            if (fgets(box_id_str, 17, fp) == NULL) {
                lua_pushstring(L, " ");
                return 1;
            }
            fclose(fp);
            break;
        }
    }
    closedir(dp);
    lua_pushstring(L, box_id_str);
    return 1;
}

static const luaL_Reg lid_methods[] = {
    {"get_board_id", lid_get_board_id},
    {"get_box_id", lid_get_box_id},
    {"__gc", lid_destroy},
    {NULL, NULL}
};

static const luaL_Reg lid_functions[] = {
    {"new", lid_new},
    {NULL, NULL}
};

int luaopen_id(lua_State *L){
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
    luaL_setfuncs(L, lid_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, lid_functions);

    return 1;
}
