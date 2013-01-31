bin_PROGRAMS = ofono-wizard

ofono_wizard_SOURCES =       \
			main.c

ofono_wizard_CPPFLAGS = \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS)

ofono_wizard_CFLAGS = \
	-DDATA_DIRECTORY=\"$(datadir)/ofono-wizard\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	$(OFONO_WIZARD_CFLAGS)

ofono_wizard_LDADD = $(OFONO_WIZARD_LIBS)

-include $(top_srcdir)/git.mk
