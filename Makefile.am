AM_CFLAGS = -Wall -DWIN32_LEAN_AND_MEAN
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libwinpthread.la
lib_LIBRARIES =
fakelibdir = fakelibs
fakelib = $(fakelibdir)/libgcc.a $(fakelibdir)/libgcc_s.a $(fakelibdir)/libgcc.dll.a $(fakelibdir)/libgcc_s.dll.a \
           $(fakelibdir)/libgcc_eh.a $(fakelibdir)/libpthread.a

clean-local:
	-rm -rf $(fakelibdir)

libwinpthread_la_CPPFLAGS = -I$(srcdir)/include -DIN_WINPTHREAD
libwinpthread_la_LDFLAGS = -no-undefined -version-info 1:0:0 -L$(fakelibdir)

libwinpthread_la_SOURCES = \
  src/barrier.h  src/cond.h  src/misc.h  src/mutex.h  src/rwlock.h  src/spinlock.h  src/thread.h  src/ref.h  src/sem.h \
  src/barrier.c  src/cond.c  src/misc.c  src/mutex.c  src/rwlock.c  src/spinlock.c  src/thread.c  src/ref.c  src/sem.c  src/sched.c \
  src/winpthread_internal.h  src/clock.c src/nanosleep.c src/version.rc

include_HEADERS = include/pthread.h include/sched.h include/semaphore.h include/pthread_unistd.h include/pthread_time.h include/pthread_compat.h

DISTCHECK_CONFIGURE_FLAGS = --host=$(host_triplet)

.rc.lo:
	$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --tag=CC --mode=compile $(RC) @RCFLAGS@ $(RCFLAGS) -i $< -o $@

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

noinst_LTLIBRARIES = libdummy.la

libwinpthread_la_LIBADD = libdummy.la

dll_dependency.o: src/dll_dependency.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

dll_math.o: src/dll_math.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(fakelibdir):
	mkdir -p $@

$(fakelibdir)/libgcc.a: $(fakelibdir) dll_dependency.o dll_math.o
	$(AR) cru $@ dll_dependency.o dll_math.o
	touch $@

$(fakelibdir)/libgcc.dll.a: $(fakelibdir) $(fakelibdir)/libgcc.a
	cp $(fakelibdir)/libgcc.a $@
	touch $@

$(fakelibdir)/libgcc_s.a: $(fakelibdir) $(fakelibdir)/libgcc.a
	$(AR) cru $@
	touch $@

$(fakelibdir)/libgcc_s.dll.a: $(fakelibdir) $(fakelibdir)/libgcc.a
	$(AR) cru $@
	touch $@

$(fakelibdir)/libgcc_eh.a:
	$(AR) cru $@
	touch $@

$(fakelibdir)/libpthread.a:
	$(AR) cru $@
	touch $@

BUILT_SOURCES = dummy.c

libdummy_la_SOURCES = dummy.c

# Force libgcc dummy build
dummy.c: $(fakelib)
	echo "static int __attribute__((unused)) dummy;" > $@

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
