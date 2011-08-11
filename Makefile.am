AM_CFLAGS = -Wall
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libwinpthread.la
lib_LIBRARIES =

if EXPORT_ALL_SYMBOLS
EXPORT_DLL_ALL_DEBUG = -DWINPTHREAD_EXPORT_ALL_DEBUG
EXPORT_DLL_ALL = --export-all-symbols
else
EXPORT_DLL_ALL_DEBUG =
EXPORT_DLL_ALL =
endif

libwinpthread_la_CPPFLAGS = -I$(srcdir)/include -DIN_WINPTHREAD $(EXPORT_DLL_ALL_DEBUG)
libwinpthread_la_LDFLAGS = -no-undefined -version-info 1:0:0 $(EXPORT_DLL_ALL)
# -export-symbols-regex '^(pthread|sem|sched)_' ?

libwinpthread_la_SOURCES = \
  src/barrier.h  src/cond.h  src/misc.h  src/mutex.h  src/rwlock.h  src/spinlock.h  src/thread.h  src/ref.h  src/sem.h \
  src/barrier.c  src/cond.c  src/misc.c  src/mutex.c  src/rwlock.c  src/spinlock.c  src/thread.c  src/ref.c  src/sem.c  src/sched.c \
  src/version.rc

include_HEADERS = include/pthread.h include/sched.h include/semaphore.h

DISTCHECK_CONFIGURE_FLAGS = --host=$(host_triplet)

.rc.lo:
	$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --tag=CC --mode=compile $(RC) $(RCFLAGS) -i $< -o $@

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

if COPY_SHARED
lib_LIBRARIES += libpthread.dll.a
libpthread.dll.a: $(LT_OBJDIR)/libwinpthread.dll.a
	cp -p $< $@
	touch $@
endif

if COPY_STATIC
lib_LIBRARIES += libpthread.a
libpthread.a: $(LT_OBJDIR)/libwinpthread.a
	cp -p $< $@
	touch $@
endif
