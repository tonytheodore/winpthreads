#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([mingw-w64-libpthread], [1.0], [mingw-w64-public@lists.sf.net])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([src/spinlock.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE

AC_CANONICAL_HOST

# Checks for programs.
AC_NO_EXECUTABLES
AC_PROG_CC
# Libtool 2.4 should check them correctly
# AC_CHECK_TOOLS([AR],[ar],[:])
AC_CHECK_TOOLS([RC],[windres],[:])

# Libtool
LT_INIT([win32-dll])
AC_SUBST([LIBTOOL_DEPS])
AC_SUBST([LT_OBJDIR],$lt_cv_objdir)

AM_CONDITIONAL( [COPY_SHARED], [test "x$enable_shared" = xyes] )
AM_CONDITIONAL( [COPY_STATIC], [test "x$enable_static" = xyes] )

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lpthread':
#AC_CHECK_LIB([pthread], [main])

# Checks for header files.
AC_CHECK_HEADERS([limits.h sys/timeb.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
# mingw-w64 should already have them
#AC_CHECK_FUNCS([ftime gettimeofday memset])

AC_SUBST([RCFLAGS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
