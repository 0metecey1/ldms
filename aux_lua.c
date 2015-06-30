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
#include <mysql.h>

static int laux_append_to_db(lua_State *L)
{
    char box_id_str[20] = {'f','o','o'};
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char *server = "192.168.16.15";
    char *user = "root";
    char *password = "V0st!novaled#"; /* set me first */
    char *database = "nlts";
    conn = mysql_init(NULL);
    /* Connect to database */
    if (!mysql_real_connect(conn, server,
                user, password, database, 0, NULL, 0)) {
        luaL_error(L, "%s", mysql_error(conn));
    }

    mysql_close(conn);

    lua_pushstring(L, box_id_str);
    return 1;
}

static const luaL_Reg laux_functions[] = {
    {"append_to_db", laux_append_to_db},
    {NULL, NULL}
};

int luaopen_aux(lua_State *L){
    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, laux_functions);

    return 1;
}
