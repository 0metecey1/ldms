#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ(2.60)
AC_INIT([ldms], [2.1.0], [torsten.coym@novaled.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([ldms.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

# Require GNU toolchain
AC_GNU_SOURCE

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
PKG_CHECK_MODULES([JANSSON], [jansson >= 2])
PKG_CHECK_MODULES([ZMQ], [libzmq >= 4.1])
PKG_CHECK_MODULES([CZMQ], [libczmq >= 4.1])
PKG_CHECK_MODULES([LUA], [lua >= 5.3])

# AC_SUBST([LUA_CFLAGS])
# AC_SUBST([LUA_LIBS])

WITH_MYSQL()
MYSQL_USE_CLIENT_API()
MYSQL_SUBST()

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
