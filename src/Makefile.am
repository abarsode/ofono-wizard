bin_PROGRAMS = ofono-wizard

dbus_built_sources =	ofono-manager.h ofono-manager.c  \
			ofono-modem.h ofono-modem.c

ofono-manager.c: ofono-manager.h
ofono-manager.h: Makefile.am ofono-manager.xml
	gdbus-codegen 						\
		--interface-prefix org.ofono.			\
		--generate-c-code ofono-manager			\
		$(srcdir)/ofono-manager.xml

ofono-modem.c: ofono-modem.h
ofono-modem.h: Makefile.am ofono-modem.xml
	gdbus-codegen 						\
		--interface-prefix org.ofono.			\
		--generate-c-code ofono-modem			\
		$(srcdir)/ofono-modem.xml

ofono_wizard_SOURCES =       \
			$(dbus_built_sources) \
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

CLEANFILES = $(dbus_built_sources)
EXTRA_DIST = ofono-manager.xml ofono-modem.xml

-include $(top_srcdir)/git.mk
