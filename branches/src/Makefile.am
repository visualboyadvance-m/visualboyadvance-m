CORE_SUBDIRS = gb

EXTRA_SUBDIRS = i386 prof sdl gtk

SUBDIRS = $(CORE_SUBDIRS) @VBA_SRC_EXTRA@

DIST_SUBDIRS = $(CORE_SUBDIRS) $(EXTRA_SUBDIRS)

dist_sysconf_DATA = VisualBoyAdvance.cfg

EXTRA_DIST = \
	win32			\
	expr.l			\
	expr.y			\
	CheatSearch.cpp		\
	CheatSearch.h

dist-hook:
	rm -rf `find $(distdir)/win32 -name CVS`
