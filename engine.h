#ifndef __ENGINE_INCLUDE_H__
#define __ENGINE_INCLUDE_H__
int engine_dofile (lua_State *L, const char *name, char *errbuf);
int engine_dostring (lua_State *L, const char *s, const char *name, char *errbuf, int concurrent); 
#endif
