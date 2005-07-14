if BUILD_DEMO_PLUGIN
demo_dir = demo
endif

if BUILD_SPAMASSASSIN_PLUGIN
spamassasssin_dir = spamassassin
endif

if BUILD_MATHML_VIEWER_PLUGIN
mathml_viewer_dir = mathml_viewer
endif

if BUILD_DILLO_VIEWER_PLUGIN
dillo_viewer_dir = dillo_viewer
endif

if BUILD_TRAYICON_PLUGIN
trayicon_dir = trayicon
endif

if BUILD_CLAMAV_PLUGIN
clamav_dir = clamav
endif

if BUILD_PGPCORE_PLUGIN
pgpcore_dir = pgpcore
if BUILD_PGPMIME_PLUGIN
pgpmime_dir = pgpmime
endif
endif

SUBDIRS = $(demo_dir) \
	$(spamassasssin_dir) \
	$(mathml_viewer_dir) \
	$(dillo_viewer_dir) \
	$(trayicon_dir) \
	$(clamav_dir) \
	$(pgpcore_dir) \
	$(pgpmime_dir)
