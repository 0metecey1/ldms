# Boiler plate
ACLOCAL_AMFLAGS = -I m4 --install

# General settings
AM_LDFLAGS = -export-symbols-regex '^luaopen_' -module -avoid-version

# binaries to create
bin_PROGRAMS = ldms

# per-binary settings
# ldms_SOURCES = src/main.c
ldms_SOURCES = src/ldms.c
ldms_CFLAGS = $(LUA_INCLUDE) ${CZMQ_CFLAGS} ${ZMQ_CFLAGS}
ldms_LDFLAGS = $(LUA_LIB) ${CZMQ_LIBS} ${ZMQ_LIBS}

# Shared objects to create
luaexec_LTLIBRARIES = lcounter.la mcdc04.la ad5522.la tlc5948a.la 
# luaexec_LTLIBRARIES += pca9632.la tmp116.la se97.la
luaexec_LTLIBRARIES += se97.la

# per-shared object settings
lcounter_la_SOURCES = lib/counter.c lib/counter.h lib/wrap.c
lcounter_la_CFLAGS = @LUA_INCLUDE@
mcdc04_la_SOURCES = lib/i2cbusses.c lib/i2cbusses.h 
mcdc04_la_SOURCES += lib/mcdc04_core.c lib/mcdc04_lua.c lib/mcdc04.h
mcdc04_la_CFLAGS = @LUA_INCLUDE@
ad5522_la_SOURCES = lib/ad5522_core.c lib/ad5522_lua.c lib/ad5522.h
ad5522_la_CFLAGS = @LUA_INCLUDE@
tlc5948a_la_SOURCES = lib/tlc5948a_core.c lib/tlc5948a_lua.c lib/tlc5948a.h
tlc5948a_la_CFLAGS = @LUA_INCLUDE@
# pca9536_la_SOURCES = lib/pca9536_core.c lib/pca9536_lua.c lib/pca9536.h
# pca9536_la_CFLAGS = @LUA_INCLUDE@
# pca9632_la_SOURCES = lib/pca9632_core.c lib/pca9632_lua.c lib/pca9632.h
# pca9632_la_CFLAGS = @LUA_INCLUDE@
# tmp116_la_SOURCES = lib/tmp116_core.c lib/tmp116_lua.c lib/tmp116.h
# tmp116_la_CFLAGS = @LUA_INCLUDE@
se97_la_SOURCES = lib/se97_core.c lib/se97_lua.c lib/se97.h
se97_la_CFLAGS = @LUA_INCLUDE@
