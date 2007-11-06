VBA_WM_IMAGES = \
	stock-vba-wm-16.png	\
	stock-vba-wm-32.png	\
	stock-vba-wm-48.png	\
	stock-vba-wm-64.png

VBA_WM_VARIABLES = \
	stock_vba_wm_16  $(srcdir)/stock-vba-wm-16.png	\
	stock_vba_wm_32  $(srcdir)/stock-vba-wm-32.png	\
	stock_vba_wm_48  $(srcdir)/stock-vba-wm-48.png	\
	stock_vba_wm_64  $(srcdir)/stock-vba-wm-64.png

EXTRA_DIST = $(VBA_WM_IMAGES)

noinst_DATA = vba-wm-pixbufs.h

dist_pkgdata_DATA = vba-64.png

CLEANFILES = $(noinst_DATA)

$(srcdir)/vba-wm-pixbufs.h: $(VBA_WM_IMAGES) Makefile.am
	gdk-pixbuf-csource --raw --build-list $(VBA_WM_VARIABLES) > $(@F)
