#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ(2.60)
AC_INIT(ldms, 1.1, torsten.coym@novaled.com)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([ldms.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

AM_PROG_LIBTOOL

# Set shared libraries
AC_DISABLE_STATIC
AC_ENABLE_SHARED
# Checks for library functions.
AC_CHECK_LIB([zmq], [zmq_ctx_new])
AC_CHECK_LIB([czmq], [zactor_new])
AC_CHECK_LIB([jansson], [json_object])

# Check if lua is present
AS_IF([test "x$with_lua" != 'xno'],
      [ AX_PROG_LUA([5.2], [], [lua_interp='yes'], [lua_interp='no'])

      AS_IF([test "x$lua_interp" = 'xyes'],
            [ AX_LUA_HEADERS([lua_headers='yes'], [lua_headers='no'])
            AX_LUA_LIBS([lua_libs='yes'], [lua_libs='no'])
            ])

      AS_IF([test "x$lua_interp" = 'xyes' &&
             test "x$lua_headers" = 'xyes' &&
             test "x$lua_libs" = 'xyes'],
      [with_lua='yes'],
      [with_lua='no'])
      ])

AM_CONDITIONAL(HAVE_LUA, test "x$with_lua" = "xyes")

WITH_MYSQL()
MYSQL_USE_CLIENT_API()
MYSQL_SUBST()

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
