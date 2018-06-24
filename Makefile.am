# Boiler plate
ACLOCAL_AMFLAGS = -I m4 --install

# General settings
# AM_CFLAGS = @MYSQL_CFLAGS@
AM_LDFLAGS = @MYSQL_LDFLAGS@ -export-symbols-regex '^luaopen_' -module -avoid-version
# AM_LIBS = @MYSQL_LIBS@

EXTRA_DIST = LICENSE

# binaries to create
bin_PROGRAMS = ldms 
check_PROGRAMS = test_se97

# per-binary settings
ldms_SOURCES = src/ldms.c src/tracks.c src/engine.c
# This links all modules statically in one monolithic application
ldms_SOURCES += lib/db.h lib/db_lua.c 
ldms_SOURCES += lib/i2cbusses.c lib/i2cbusses.h 
ldms_SOURCES += lib/mcdc04_core.c lib/mcdc04_lua.c lib/mcdc04.h
ldms_SOURCES += lib/ad5522_core.c lib/ad5522_lua.c lib/ad5522.h
ldms_SOURCES += lib/tlc5948a_core.c lib/tlc5948a_lua.c lib/tlc5948a.h
ldms_SOURCES += lib/pca9536_core.c lib/pca9536_lua.c lib/pca9536.h
ldms_SOURCES += lib/pca9632_core.c lib/pca9632_lua.c lib/pca9632.h
ldms_SOURCES += lib/tmp116_core.c lib/tmp116_lua.c lib/tmp116.h
ldms_SOURCES += lib/se97_core.c lib/se97_lua.c lib/se97.h
ldms_SOURCES += lib/id_lua.c
ldms_SOURCES += lib/dib_lua.c
# SHALL be removed as soon as possible. Use dynamic linking for Lua modules instead
ldms_CFLAGS = ${MYSQL_CFLAGS} $(LUA_INCLUDE) ${CZMQ_CFLAGS} ${ZMQ_CFLAGS} ${JANSSON_CFLAGS}
ldms_LDFLAGS = ${MYSQL_LDFLAGS} $(LUA_LIB) ${CZMQ_LIBS} ${ZMQ_LIBS} ${JANSSON_LIBS}

test_se97_SOURCES = lib/se97_core.c lib/i2cbusses.c test/test_se97.c ./Unity/src/unity.c
test_se97_CFLAGS = -I./Unity/src -I./lib

# Shared objects to create
luaexec_LTLIBRARIES = lcounter.la mcdc04.la ad5522.la tlc5948a.la 
luaexec_LTLIBRARIES += pca9536.la pca9632.la tmp116.la se97.la id.la dib.la 

# per-shared object settings
lcounter_la_SOURCES = lib/counter.c lib/counter.h lib/wrap.c
lcounter_la_CFLAGS = $(LUA_INCLUDE)
mcdc04_la_SOURCES = lib/i2cbusses.c lib/i2cbusses.h 
mcdc04_la_SOURCES += lib/mcdc04_core.c lib/mcdc04_lua.c lib/mcdc04.h
mcdc04_la_CFLAGS = $(LUA_INCLUDE)
ad5522_la_SOURCES = lib/ad5522_core.c lib/ad5522_lua.c lib/ad5522.h
ad5522_la_CFLAGS = $(LUA_INCLUDE)
tlc5948a_la_SOURCES = lib/tlc5948a_core.c lib/tlc5948a_lua.c lib/tlc5948a.h
tlc5948a_la_CFLAGS = $(LUA_INCLUDE)
pca9536_la_SOURCES = lib/i2cbusses.c lib/i2cbusses.h 
pca9536_la_SOURCES += lib/pca9536_core.c lib/pca9536_lua.c lib/pca9536.h
pca9536_la_CFLAGS = $(LUA_INCLUDE)
pca9632_la_SOURCES = lib/i2cbusses.c lib/i2cbusses.h 
pca9632_la_SOURCES += lib/pca9632_core.c lib/pca9632_lua.c lib/pca9632.h
pca9632_la_CFLAGS = $(LUA_INCLUDE)
tmp116_la_SOURCES = lib/i2cbusses.c lib/i2cbusses.h 
tmp116_la_SOURCES += lib/tmp116_core.c lib/tmp116_lua.c lib/tmp116.h
tmp116_la_CFLAGS = $(LUA_INCLUDE)
se97_la_SOURCES = lib/i2cbusses.c lib/i2cbusses.h 
se97_la_SOURCES += lib/se97_core.c lib/se97_lua.c lib/se97.h
se97_la_CFLAGS = $(LUA_INCLUDE)
id_la_SOURCES = lib/id_lua.c
id_la_CFLAGS = $(LUA_INCLUDE)
dib_la_SOURCES = lib/dib_lua.c
dib_la_CFLAGS = $(LUA_INCLUDE)

TESTS = $(check_PROGRAMS)
