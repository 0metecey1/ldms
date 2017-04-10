ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = @MYSQL_CFLAGS@ -Wall -std=gnu99 -Werror -Wno-unused-but-set-variable
AM_LDFLAGS = @MYSQL_LDFLAGS@
AM_LIBS = @MYSQL_LIBS@


ldmsdir = @datarootdir@/$(PACKAGE)
dist_ldms_DATA = exit.lua init.lua

bin_PROGRAMS = ldms
ldms_SOURCES = ldms.c tracks.c engine.c 
ldms_SOURCES += i2cbusses.c mcdc04_core.c mcdc04_lua.c
ldms_SOURCES += ad5522_core.c ad5522_lua.c
ldms_SOURCES += tlc5948a_core.c tlc5948a_lua.c
ldms_SOURCES += id_lua.c aux_lua.c db_lua.c
ldms_SOURCES += dib_lua.c 
ldms_LDADD = $(LUA_LIB) ${MYSQL_LDFLAGS}
ldms_CFLAGS = $(LUA_INCLUDE) ${MYSQL_CFLAGS}
