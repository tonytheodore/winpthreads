AM_CFLAGS = -Wall
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libwinpthread.la
lib_LIBRARIES =

libwinpthread_la_CPPFLAGS = -I$(srcdir)/include -DIN_WINPTHREAD
libwinpthread_la_LDFLAGS = -no-undefined -version-info 1:0:0
# -export-symbols-regex '^(pthread|sem|sched)_' ?

libwinpthread_la_SOURCES = \
  src/barrier.h  src/cond.h  src/misc.h  src/mutex.h  src/rwlock.h  src/spinlock.h  src/thread.h  src/ref.h  src/sem.h \
  src/barrier.c  src/cond.c  src/misc.c  src/mutex.c  src/rwlock.c  src/spinlock.c  src/thread.c  src/ref.c  src/sem.c  src/sched.c \
  src/winpthread_internal.h  src/version.rc

include_HEADERS = include/pthread.h include/sched.h include/semaphore.h include/pthread_unistd.h

DISTCHECK_CONFIGURE_FLAGS = --host=$(host_triplet)

.rc.lo:
	$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --tag=CC --mode=compile $(RC) $(RCFLAGS) -i $< -o $@

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

if COPY_SHARED
lib_LIBRARIES += libpthread.dll.a
libpthread.dll.a: libwinpthread.la
	cp -p $(LT_OBJDIR)/libwinpthread.dll.a $@
	touch $@
endif

if COPY_STATIC
lib_LIBRARIES += libpthread.a
libpthread.a: libwinpthread.la
	cp -p $(LT_OBJDIR)/libwinpthread.a $@
	touch $@
endif
