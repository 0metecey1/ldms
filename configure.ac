# Initialize autoconf.
AC_INIT([ldms], [2.4.1], [bug-automake@gnu.org])

AC_PREREQ([2.68]) # Force autoconf to be at least this version number
AC_CONFIG_AUX_DIR([build-aux]) # Put autotools junk in 'build-aux' subdir
AC_CONFIG_MACRO_DIR([m4]) # Automatically load scripts from 'm4' subdir

AC_CONFIG_SRCDIR([lib/mcdc04_core.c]) # Safety check - list a source file that wouldn't be in other directories
AC_CONFIG_HEADERS([src/config.h]) # Put configuration results here, (should be `#include`d)

# Enable "automake"
AM_INIT_AUTOMAKE([subdir-objects foreign -Wall])
AC_CONFIG_FILES([Makefile])

# Enable libtool
m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) # see https://lists.gnu.org/archive/html/automake/2012-05/msg00014.html
AC_DISABLE_STATIC # change static libraries to not build by default
LT_INIT

# Check for header files
AC_CHECK_HEADER([linux/i2c-dev-user.h])

# Check for programs
AC_PROG_CC
AX_PROG_LUA([5.3], [5.4])

# Check for libraries
AX_LUA_LIBS

# Check for MySQL/MariaDB client libraries
WITH_MYSQL()
MYSQL_USE_CLIENT_API()
MYSQL_SUBST()

# Check for header files
# AX_LUA_HEADERS

# Check for typedefs, structures, and compiler characteristics

# Check for library functions
PKG_CHECK_MODULES([JANSSON], [jansson >= 2])
PKG_CHECK_MODULES([ZMQ], [libzmq >= 4.1])
PKG_CHECK_MODULES([CZMQ], [libczmq >= 4.1])

AC_OUTPUT # Do final output
