INCLUDES = -I$(top_srcdir) -I$(includedir) \
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps"\" \
         -DG_LOG_DOMAIN=\"grip\" -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
        -I../intl -I$(top_srcdir)/intl \
	-DPREFIX=\""$(prefix)"\" \
        -DSYSCONFDIR=\""$(sysconfdir)"\" \
        -DDATADIR=\""$(datadir)"\" \
        -DLIBDIR=\""$(libdir)"\"



bin_PROGRAMS = grip

grip_SOURCES =         \
	cppmain.cc \
        main.c \
	common.h \
        grip.c \
	grip.h \
	cddev.c	\
	cddev.h \
	cdplay.c \
	cdplay.h \
	discedit.c \
	discedit.h \
	rip.c \
	rip.h \
	cdpar.c \
	cdpar.h \
	id3.c \
	grip_id3.h \
	discdb.c \
	discdb.h \
	uihelper.c \
	status_window.c \
	status_window.h \
	xpm.h \
	dialog.c \
	dialog.h \
	gripcfg.c \
	gripcfg.h \
	parsecfg.c \
	parsecfg.h \
	launch.c \
	launch.h \
	gain_analysis.c \
	gain_analysis.h \
	eggtrayicon.c \
	eggtrayicon.h \
	tray.c \
	tray.h 

grip_LDADD = @TERMINAL_WIDGET_LIBS@ $(CDPAR_LIBS) $(ID3_LIBS)
