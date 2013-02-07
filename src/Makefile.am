bin_PROGRAMS = ofono-wizard

ofono_wizard_SOURCES =       \
			ofono-wizard.h \
			ofono-wizard.c \
			mobile-provider.h \
			mobile-provider.c \
			main.c

ofono_wizard_CPPFLAGS = \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS)

ofono_wizard_CFLAGS = \
	-DDATA_DIRECTORY=\"$(datadir)/ofono-wizard\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DDATADIR=\""$(datadir)"\" \
	$(OFONO_WIZARD_CFLAGS)

ofono_wizard_LDADD = $(OFONO_WIZARD_LIBS)

-include $(top_srcdir)/git.mk
