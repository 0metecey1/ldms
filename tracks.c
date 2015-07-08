/*
 * A state-based scripting engine implemented as a CZMQ actor using Lua
 */

#include <czmq.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <jansson.h>
#include "engine.h"
#include "tlc5948a.h"
#include "ad5522.h"
#include "mcdc04.h"
#include "id.h"

#include "waitsupport.h"
#include "ldms_init.h"

#define LMU_I2C_BUS 1
#define LMU_I2C_ADDRESS 0x74
#define LED_SPI_BUS "/dev/spidev2.0"
#define HW_BOARD_ID_PATH "/sys/bus/i2c/devices/0-0050/eeprom"
//#define HW_BOX_ID_PATH "/var/lib/w1/bus.0/bus.0"
#define HW_BOX_ID_PATH "/var/lib/w1/bus.0"

/* print an error message */
#define luai_writestring(s,l)	fwrite((s), sizeof(char), (l), stdout)
#define luai_writeline()	(luai_writestring("\n", 1), fflush(stdout))
#define luai_writestringerror(s,p) \
            (fprintf(stderr, (s), (p)), fflush(stderr))
//  --------------------------------------------------------------------------
//  The self_t structure holds the state for one actor instance

typedef struct {
    zsock_t *pipe;              //  Actor command pipe
    zsock_t *responder;         //  Responder socket for replies
    zpoller_t *poller;          //  Poller for API and REP socket
    zmsg_t *reply;              //  Reply send back via REP socket
    json_t *root;               //  JSON object holding the reply
    lua_State *L;               //  Lua state
    const char *lchunk;               //  Chunk of Lua code to be run
    int port_nbr;               //  TCP port number to work on
    int64_t currtime;           //  Current execution time 
    int64_t nexttime;           //  Next execution time
    int64_t interval;           //  Lua track execution interval in ms
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    bool concurrent;            //  Can this chunk be executed as a track?
} self_t;

static int lua_status_encode(json_t *object, const char *status, const char *errmsg)
{
    json_object_set(object, "errormsg", json_string(errmsg));
    return json_object_set(object, "status", json_string(status));
}

static void
s_self_destroy (self_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        self_t *self = *self_p;
        zsock_destroy(&self->responder);
        lua_close(self->L);
        free (self);
        *self_p = NULL;
    }
}

//  Spawn a fresh, clean Lua state
static int
s_self_spawn_lua (self_t *self)
{
    json_object_clear(self->root);
    if (self->L != NULL)
        lua_close(self->L);
    self->L = luaL_newstate();

    if (self->L == NULL) {
        zsys_error ("Not enough memory to create Lua state");
        lua_status_encode(self->root, "error", "Not enough memory to create Lua state");
        return -1;
    } 
    /* Open standard Lua libraries */
    luaL_openlibs(self->L);
    /* Add state-based scripting support */
    if (engine_dostring(self->L, tracks_wait_support_lua_str, "tracks", NULL, false) != LUA_OK) {
        lua_status_encode(self->root, "error", "could not load wait_support.lua");
        return -1;
    }
    luaL_requiref(self->L, "tlc5948a", luaopen_tlc5948a, true);
    /* Initialize LED driver object and make methods available */
    // led = tlc5948a.new("/dev/spidev2.0")
    //Here it is in C:
    lua_getglobal(self->L, "tlc5948a");
    lua_getfield(self->L, -1, "new");        
    lua_remove(self->L, -2);                
    lua_pushstring(self->L, LED_SPI_BUS); // spidev bus number       
    lua_call(self->L, 1, 1);     
    lua_setglobal(self->L, "led");

    // Initialize color sensor object and make methods available
    luaL_requiref(self->L, "mcdc04", luaopen_mcdc04, true);
    // lmu = mcdc04.new(1, 0x74)
    // C equivalent of Lua code
    lua_getglobal(self->L, "mcdc04");
    lua_getfield(self->L, -1, "new");        
    lua_remove(self->L, -2);                
    lua_pushinteger(self->L, LMU_I2C_BUS); // i2c bus number
    lua_pushinteger(self->L, LMU_I2C_ADDRESS); // i2c bus address
    lua_call(self->L, 2, 1);     
    lua_setglobal(self->L, "lmu");

    // Initialize parametric measurement unit object and make methods available
    luaL_requiref(self->L, "ad5522", luaopen_ad5522, true);
    // lmu = mcdc04.new(1, 0x74)
    // C equivalent of Lua code
    lua_getglobal(self->L, "ad5522");
    lua_getfield(self->L, -1, "new");        
    lua_remove(self->L, -2);                
    lua_pushinteger(self->L, 1); // spi bus number
    lua_pushinteger(self->L, 0); //
    lua_pushinteger(self->L, 0); //
    lua_pushinteger(self->L, 126); //
    lua_call(self->L, 4, 1);     
    lua_setglobal(self->L, "pmu");

    luaL_requiref(self->L, "id", luaopen_id, true);
    /* Initialize LED driver object and make methods available */
    // led = tlc5948a.new("/dev/spidev2.0")
    //Here it is in C:
    lua_getglobal(self->L, "id");
    lua_getfield(self->L, -1, "new");        
    lua_remove(self->L, -2);                
    lua_pushstring(self->L, HW_BOARD_ID_PATH); // path to the file containing the board id
    lua_pushstring(self->L, HW_BOX_ID_PATH); // path to the file containing the box id
    lua_call(self->L, 2, 1);     
    lua_setglobal(self->L, "hw");

    /* Initialize auxillary functions and objects */
    // zsys_info(ldms_init_lua_str);
    // if (engine_dostring(self->L, ldms_init_lua_str, "ldms_init", NULL, false) != LUA_OK) {
    //     lua_status_encode(self->root, "error", "could not ldms_init.lua");
    //     return -1;
    // }

    //     if (engine_dofile(self->L, "/usr/bin/wait_support.lua", NULL) != LUA_OK) {
    //         lua_status_encode(self->root, "error", "could not load wait_support.lua");
    //         return -1;
    //     }
    //     if (engine_dofile(self->L, "/usr/bin/ldms_init.lua", NULL) != LUA_OK) {
    //         lua_status_encode(self->root, "error", "could not load wait_support.lua");
    //         return -1;
    //     }
    lua_status_encode(self->root, "ok", "");
    return 0;
}

static self_t *
s_self_new (zsock_t *pipe)
{
    self_t *self = (self_t *) zmalloc (sizeof (self_t));
    if (!self)
        return NULL;
    self->pipe = pipe;
    self->root = json_object();
    assert(self->root);
    self->interval = 5LL;
    //  Set-up poller
    self->poller = zpoller_new (self->pipe, NULL);
    assert (self->poller);
    s_self_spawn_lua(self);
    assert(self->L);
    return self;
}

//  --------------------------------------------------------------------------
//  Prepare responder socket to work on specified TCP port, reply hostname to
//  pipe (or "" if this failed)

static void
s_self_configure (self_t *self, int port_nbr)
{
    assert (port_nbr);
    self->port_nbr = port_nbr;
    self->responder = zsock_new_rep(zsys_sprintf("tcp://*:%d", self->port_nbr));
    assert(self->responder);
    assert (zsock_resolve (self->responder) != self->responder);
    assert (streq (zsock_type_str (self->responder), "REP"));
    zpoller_add(self->poller, self->responder);

    zstr_send (self->pipe, zsock_endpoint(self->responder));
    if (streq (zsock_endpoint(self->responder), ""))
        zsys_error ("No reply interface found, (ZSYS_INTERFACE=%s)", zsys_interface ());
}


static json_t* lua_table_encode_json(lua_State *L, int index)
{
    json_t *root = json_object();
    json_t *results = json_object();

    // Push another reference to the table on top of the stack (so we know
    // where it is, and this function can work for negative, positive and
    // pseudo indices
    lua_pushvalue(L, index);
    // stack now contains: -1 => table
    lua_pushnil(L);
    // stack now contains: -1 => nil; -2 => table
    while (lua_next(L, -2))
    {
        // stack now contains: -1 => value; -2 => key; -3 => table
        // copy the key so that lua_tostring does not modify the original
        lua_pushvalue(L, -2);
        // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
        const char *key = lua_tostring(L, -1);
        const char *value = lua_tostring(L, -2);
        json_object_set(results, key,  json_string(value));
        // pop value + copy of key, leaving original key
        lua_pop(L, 2);
        // stack now contains: -1 => key; -2 => table
    }
    // stack now contains: -1 => table (when lua_next returns 0 it pops the key
    // but does not push anything.)
    // Pop table
    lua_pop(L, 1);
    json_object_set(root, "results", results);
    // Stack is now the same as it was on entry to this function
    return root;
}
static void stackDump (lua_State *L) {
    int i;
    int top = lua_gettop(L);
    for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {

            case LUA_TSTRING:  /* strings */
                printf("`%s'", lua_tostring(L, i));
                break;

            case LUA_TBOOLEAN:  /* booleans */
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;

            case LUA_TNUMBER:  /* numbers */
                printf("%g", lua_tonumber(L, i));
                break;

            default:  /* other values */
                printf("%s", lua_typename(L, t));
                break;

        }
        printf("  ");  /* put a separator */
    }
    printf("\n");  /* end the listing */
}

static int
s_self_call_engine(self_t *self)
{
    char errmsg[1024] = "";
    json_object_clear(self->root);

    if (self->verbose)
        zsys_info ("tracks: RUN \n%s", self->lchunk);
    if (engine_dostring(self->L, self->lchunk, "lua_loop_actor", 
                errmsg, self->concurrent) == LUA_OK) 
    {
        if (self->verbose)
            zsys_info ("tracks: RUN successful, get results");
        /* Lua execution was successful */
        lua_getglobal(self->L, "results");
        int type = lua_type(self->L, -1);
        /* we have results in a table */
        if (type == LUA_TTABLE) {
            self->root = lua_table_encode_json(self->L, -1);
            /* Remove variable from top of Lua stack */
        }
        /* no results available */
        else {
            if (self->verbose)
                zsys_info("tracks: RUN successful, but no results");
            json_object_set(self->root, "results", json_object());
        }

        if (self->verbose)
            zsys_info ("tracks: RUN results send back to requestor %s\n", 
                    json_dumps(self->root, 0));
        lua_status_encode(self->root, "ok", "");
        lua_pop(self->L, 1);
    } else {
        /* Lua returned a syntax error */
        zsys_warning( "<error><%s>", errmsg);
        lua_status_encode(self->root, "error", errmsg);
    }
    return 0;
}

static int
s_self_wake_waiting_threads(self_t *self)
{
    assert(self->L);
    // Execute all tracks, that need to be waken up
    lua_getglobal(self->L, "wakeUpWaitingThreads"); /* function to be called */
    lua_pushnumber(self->L, self->interval); /* 1st argument */
    /* do the call (1 argument, 0 results) */
    if (lua_pcall(self->L, 1, 0, 0) != LUA_OK)
        zsys_warning( "error running function `wakeUpWaitingThreads': %s",
                lua_tostring(self->L, -1));
    return 0;
}

//  --------------------------------------------------------------------------
//  Handle a command from calling application

static int
s_self_handle_pipe (self_t *self)
{
    //  Get just the command off the pipe
    char *command = zstr_recv (self->pipe);
    if (!command)
        return -1;                  //  Interrupted

    if (self->verbose)
        zsys_info ("tracks: API command=%s", command);

    if (streq (command, "VERBOSE"))
        self->verbose = true;
    else
    if (streq (command, "CONFIGURE")) {
        int port;
        int rc = zsock_recv (self->pipe, "i", &port);
        assert (rc == 0);
        s_self_configure (self, port);
    }
    else
    if (streq (command, "RECREATE_LUA")) {
        s_self_spawn_lua(self);
    }
    else
    //  All actors must handle $TERM in this way
    if (streq (command, "$TERM"))
        self->terminated = true;
    else {
        zsys_error ("tracks: - invalid command: %s", command);
        assert (false);
    }
    zstr_free (&command);
    return 0;
}

//  --------------------------------------------------------------------------
//  Handle a command from calling application

static int
s_self_handle_rep (self_t *self)
{
    //  Get just the command off the REP socket
    const char *command;
    json_t *root = json_object();
    char *request = zstr_recv (self->responder);
    if (!request) {
        return -1;                  //  Interrupted
    }

    // Decode request
    root = json_loads(request, 0, NULL);
    command = json_string_value(json_object_get(root, "VostCmd"));
    if (!command) {
        return -1;
    }

    // Extract chunk of Lua code, if present in the request
    self->lchunk = json_string_value(json_object_get(root, "LuaCode"));

    if (self->verbose)
        zsys_info ("tracks: REP socket command=%s", command);
    // Request to immediately run a single chunk
    if (streq (command, "RUN")) {
        self->concurrent = false;
        s_self_call_engine(self);
    }
    else
    // Request to inject a new Lua track
    if (streq (command, "RUN_COOP")) {
        self->concurrent = true;
        s_self_call_engine(self);
    }
    else
    // Re-spawn a new Lua state
    if (streq (command, "RECREATE_LUA")) {
        s_self_spawn_lua(self);
    }
    else {
        zsys_error ("tracks: - invalid command: %s", command);
        assert (false);
    }
    zstr_free (&request);
    // We use a REQ/REP socket pair, so a reply is mandatory
    self->reply = zmsg_new();
    zmsg_addstr(self->reply, json_dumps(self->root, 0));
    zmsg_send(&(self->reply), self->responder);
    return 0;
}

//  --------------------------------------------------------------------------
//  Actor
//  must call zsock_signal (pipe) when initialized
//  must listen to pipe and exit on $TERM command

void
tracks (zsock_t *pipe, void *args)
{
    self_t *self = s_self_new (pipe);
    assert (self);
    //  Signal successful initialization
    zsock_signal (pipe, 0);

    //  Do a 'frame rate limited game loop' approach
    //  Update time
    self->nexttime = zclock_mono();
    while (!self->terminated) {
        self->currtime = zclock_mono();
        if(self->currtime >= self->nexttime) {
            // Assign the time for the next execution
            self->nexttime += self->interval;
            //  Poll on API pipe and on REQ-REP socket
            long timeout = 1; // Allow a tiny timeout
            zsock_t *which = (zsock_t *) zpoller_wait(self->poller, timeout);
            if (zpoller_terminated(self->poller)) // Handle interrupts
                break;
            if (which == self->pipe)
                s_self_handle_pipe(self);
            if (which == self->responder)
                s_self_handle_rep(self);
            s_self_wake_waiting_threads(self);
        }
        else {
            // calculate sleep time
            int64_t sleeptime = self->nexttime - self->currtime;
            // sanity check
            if (sleeptime > 0) {
                zclock_sleep(sleeptime);
            }
        }
    }
    s_self_destroy(&self);
}
//  --------------------------------------------------------------------------
//  Selftest

void
tracks_test (bool verbose)
{
    /* Daemon-specific initialization goes here */
    zsys_set_logsystem (true);
    /* Ctrl-C and SIGTERM will set zsys_interrupted. */
    zsys_catch_interrupts();
    // Create state-based script engine
    zactor_t *actor = zactor_new (tracks, NULL);
    assert (actor);
    zstr_sendx (actor, "VERBOSE", NULL);
    zsock_send (actor, "si", "CONFIGURE", 5560);
    char *hostname = zstr_recv (actor);
    assert (*hostname);
    free (hostname);

    /* Tear down the state-based script engine */
    zactor_destroy (&actor);
    //  @end
}
