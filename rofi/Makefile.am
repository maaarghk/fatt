ACLOCAL_AMFLAGS=-I m4
plugindir=${rofi_PLUGIN_INSTALL_DIR}/

plugin_LTLIBRARIES = fatt.la

fatt_la_SOURCES=\
		src/external_process.c \
		src/fatt.c

fatt_la_CFLAGS= @glib_CFLAGS@ @rofi_CFLAGS@
fatt_la_LIBADD= @glib_LIBS@ @rofi_LIBS@
fatt_la_LDFLAGS= -module -avoid-version
