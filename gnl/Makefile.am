built_header_make = gnlmarshal.h
built_source_make = gnlmarshal.c
plugin_LTLIBRARIES = libgnl.la

EXTRA_libgnl_la_SOURCES = gnlmarshal.list 

libgnl_la_SOURCES =		\
	gnl.c			\
	gnlobject.c		\
	gnlcomposition.c	\
	gnlghostpad.c		\
	gnlmarshal.c		\
	gnloperation.c		\
	gnlsource.c		\
	gnlurisource.c		\
	gnlfilesource.c
libgnl_la_CFLAGS = $(GST_CFLAGS)
libgnl_la_LIBADD = $(GST_LIBS)
libgnl_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgnl_la_LIBTOOLFLAGS = --tag=disable-static

gnl_headers =			\
	gnl.h			\
	gnlobject.h		\
	gnlcomposition.h	\
	gnltypes.h		\
	gnlghostpad.h		\
	gnloperation.h		\
	gnlsource.h		\
	gnltypes.h		\
	gnlurisource.h		\
	gnlfilesource.h

DISTCLEANFILE = $(CLEANFILES)

#files built on make all/check/instal
BUILT_SOURCES =			\
	$(built_header_configure)	\
	$(built_header_make)		\
	$(built_source_make)

CLEANFILES = $(built_header_make) $(built_source_make)

noinst_HEADERS = $(gnl_headers) $(built_header_make) $(built_header_configure)

gnlmarshal.h: gnlmarshal.list
	$(AM_V_GEN)glib-genmarshal --header --prefix=gnl_marshal $^ > gnlmarshal.h.tmp && \
	mv gnlmarshal.h.tmp gnlmarshal.h

gnlmarshal.c: gnlmarshal.list
	$(AM_V_GEN)echo "#include \"glib-object.h\"" >gnlmarshal.c.tmp && \
	echo "#include \"gnlmarshal.h\"" >> gnlmarshal.c.tmp && \
	glib-genmarshal --body --prefix=gnl_marshal $^ >> gnlmarshal.c.tmp && \
	mv gnlmarshal.c.tmp gnlmarshal.c


Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgnl -:SHARED libgnl \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgnl_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgnl_la_CFLAGS) \
	 -:LDFLAGS $(libgnl_la_LDFLAGS) \
	           $(libgnl_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
