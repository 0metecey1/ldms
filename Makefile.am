ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = @MYSQL_CFLAGS@ -Wall -std=gnu99 -Werror -Wno-unused-but-set-variable
AM_LDFLAGS = @MYSQL_LDFLAGS@
AM_LIBS = @MYSQL_LIBS@

EXTRA_DIST = LICENSE

ldmsdir = @datarootdir@/$(PACKAGE)
dist_ldms_DATA = exit.lua init.lua

bin_PROGRAMS = ldms
ldms_SOURCES = ldms.c ldms_init.h 
ldms_SOURCES += tracks.c tracks.h engine.c engine.h waitsupport.h
ldms_SOURCES += i2cbusses.c i2cbusses.h
ldms_SOURCES += mcdc04_core.c mcdc04_lua.c mcdc04.h
ldms_SOURCES += ad5522_core.c ad5522_lua.c ad5522.h
ldms_SOURCES += tlc5948a_core.c tlc5948a_lua.c tlc5948a.h
ldms_SOURCES += id_lua.c id.h aux_lua.c aux.h db_lua.c db.h
ldms_SOURCES += dib_lua.c dib.h 
ldms_LDADD = ${MYSQL_LDFLAGS} $(LUA_LIBS) ${CZMQ_LIBS} ${ZMQ_LIBS}
ldms_LDADD += ${JANSSON_LIBS}  
ldms_CFLAGS = ${MYSQL_CFLAGS} $(LUA_CFLAGS) ${CZMQ_CFLAGS} ${ZMQ_CFLAGS}
ldms_CFLAGS += ${JANSSON_CFLAGS}  

