/*
 * Provide auxillary functions to present board id and box id to Lua interpreter
 */

#define LUA_LIB
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <my_global.h>
#include <mysql.h>

#define IP_ADDR_BUFSIZE 15
#define MAX_ROW_SIZE 1024
#define TEXT_BUFSIZE 256

typedef struct {
    int closed;
    MYSQL mysql, *con;
    char ip_addr[IP_ADDR_BUFSIZE];
    char *host;
    char *user;
    char *password;
    char *database;
} ldb_userdata_t;

static void finish_with_error(MYSQL *con)
{
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);        
}

static int ldb_new(lua_State *L)
{
    MYSQL_RES *res;
    ldb_userdata_t *su;
    int i;
    const char *host, *user, *password, *database;
    const char *query = "SELECT SUBSTRING_INDEX(host,':',1) AS 'ip'\n"
        "FROM information_schema.processlist\n"
        "WHERE ID= CONNECTION_ID();";


    /* Create the user data pushing it onto the stack. We also pre-initialize
     * the member of the userdata in case initialization fails in some way. If
     * that happens we want the userdata to be in a consistent state for __gc. */
    su = (ldb_userdata_t *)lua_newuserdata(L, sizeof(*su));

    host = luaL_checkstring(L, 1);
    user = luaL_checkstring(L, 2);
    password = luaL_checkstring(L, 3);
    database = luaL_checkstring(L, 4);
    su->host = strdup(host);
    su->user = strdup(user);
    su->password = strdup(password);
    su->database = strdup(database);
    mysql_init(&su->mysql);
    /* Connect to database */
    su->con = mysql_real_connect(&su->mysql, su->host,
                su->user, su->password, su->database, 0, NULL, 0);
    if (!su->con) {
        luaL_error(L, "%s", mysql_error(su->con));
    }
    su->closed = 0;
    /* Add the metatable to the stack. */
    luaL_getmetatable(L, "Ldb");
    /* Set the metatable on the userdata. */
    lua_setmetatable(L, -2);
    if (mysql_query(su->con, query)) 
    {
        finish_with_error(su->con);
    }

    res = mysql_store_result(su->con);
    int num_fields = mysql_num_fields(res);

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) 
    { 
        for(i = 0; i < num_fields; i++) 
        { 
            snprintf(su->ip_addr, IP_ADDR_BUFSIZE, "%s ", row[i] ? row[i] : "NULL"); 
        } 
    }
    mysql_free_result(res);
    if (su->con != NULL && !(su->closed)) {
        mysql_close(su->con);
        su->closed = 1;
    }
    return 1;
}

static int ldb_push_results(lua_State *L)
{
    ldb_userdata_t *su;
    int ch;
    const char *data_str;
    char query[MAX_ROW_SIZE];

    su = (ldb_userdata_t *)luaL_checkudata(L, 1, "Ldb");
    ch = luaL_checkinteger(L, 2);
    data_str  = luaL_checkstring(L, 3);
    if (data_str == NULL) 
        luaL_error(L, "data cannot be empty");
    snprintf(query, MAX_ROW_SIZE, 
            "update tblData as D, tblPorts as P set D.LTData=if(D.LTData is NULL, "
            "'%s', concat(D.LTData,'%s')) "
            "where D.ID_Sample=P.ID_Sample and P.IPAddress ='%s'"
            " and D.DriverNo=%d", 
            data_str, data_str, su->ip_addr, ch);

    /* send query */
    if (mysql_query(su->con, query)) 
    {
        luaL_error(L, "%s", mysql_error(su->con));
    }

    return 0;
}

static int ldb_pull_calibration(lua_State *L)
{
    ldb_userdata_t *su;
    su = (ldb_userdata_t *)luaL_checkudata(L, 1, "Ldb");
    return 0;
}

static int ldb_close(lua_State *L)
{
    ldb_userdata_t *su;
    su = (ldb_userdata_t *)luaL_checkudata(L, 1, "Ldb");
    /* close connection to database */
    if (su->con != NULL && !(su->closed)) {
        mysql_close(su->con);
        su->closed = 1;
    }

    return 0;
}

static int ldb_open(lua_State *L)
{
    ldb_userdata_t *su;
    su = (ldb_userdata_t *)luaL_checkudata(L, 1, "Ldb");
    /* Connect to database */
    su->con = mysql_real_connect(&su->mysql, su->host,
                su->user, su->password, su->database, 0, NULL, 0);
    if (!su->con) {
        luaL_error(L, "%s", mysql_error(su->con));
    }
    su->closed = 0;

    return 0;
}

static int ldb_get_own_ip(lua_State *L)
{
    ldb_userdata_t *su;
    su = (ldb_userdata_t *)luaL_checkudata(L, 1, "Ldb");

    lua_pushstring(L, su->ip_addr);
    return 1;
}

static const luaL_Reg ldb_methods[] = {
    {"open", ldb_open},
    {"close", ldb_close},
    {"push_results", ldb_push_results},
    {"pull_calibration", ldb_pull_calibration},
    {"get_ip", ldb_get_own_ip},
    {"__gc", ldb_close},
    {NULL, NULL}
};

static const luaL_Reg ldb_functions[] = {
    {"new", ldb_new},
    {NULL, NULL}
};

int luaopen_db(lua_State *L){
    /* Create the metatable and put it on the stack. */
    luaL_newmetatable(L, "Ldb");
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
    luaL_setfuncs(L, ldb_methods, 0);

    /* Register the object.func functions into the table that is at the top of the
     *      * stack. */
    luaL_newlib(L, ldb_functions);

    return 1;
}
