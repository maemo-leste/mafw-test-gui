#
# Makefile.am for MAFW Test GUI Application.
#
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

if UNINSTALLED
shareddir = @abs_top_builddir@/data
else
shareddir = $(datadir)/mafw-test-gui
endif

AM_CFLAGS = $(HILDON_CFLAGS) $(GTHREAD_CFLAGS) \
	    $(GCONF_CFLAGS) $(MAFW_CFLAGS) \
	    -I$(top_srcdir) -DDATA_DIR='"$(shareddir)"' -DGTK_DISABLE_DEPRECATED

bin_PROGRAMS = mafw-test-gui

mafw_test_gui_SOURCES = main.c \
			gui.c \
			source-treeview.c \
			renderer-combo.c \
			renderer-controls.c \
			playlist-controls.c \
			playlist-treeview.c \
			metadata-view.c \
			fullscreen.c \
			main.h \
			gui.h \
			source-treeview.h \
			renderer-combo.h \
			renderer-controls.h \
			playlist-controls.h \
			metadata-view.h \
			playlist-treeview.h \
			fullscreen.h

mafw_test_gui_LDADD = 	$(HILDON_LIBS) \
			$(GTHREAD_LIBS) \
			$(GCONF_LIBS) \
			$(MAFW_LIBS)

mafw_test_gui_LDFLAGS = -export-dynamic

MAINTAINERCLEANFILES = Makefile.in
