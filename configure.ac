AC_INIT(ofono-wizard, 3.6.4)
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz foreign])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE
GNOME_MAINTAINER_MODE_DEFINES
AC_CONFIG_HEADERS([config.h])

AM_PROG_CC_C_O

dnl ###########################################################################
dnl Dependencies
dnl ###########################################################################

GTK_REQUIRED=3.6.4
OFONO_REQUIRED_VERSION=1.9
GLIB_REQUIRED_VERSION=2.31.0
DBUS_REQUIRED_VERSION=1.4

PKG_CHECK_MODULES(OFONO_WIZARD,
  gtk+-3.0 >= $GTK_REQUIRED
  glib-2.0 >= $GLIB_REQUIRED_VERSION
  dbus-1 >= $DBUS_REQUIRED_VERSION
)
AC_SUBST(OFONO_WIZARD_CFLAGS)
AC_SUBST(OFONO_WIZARD_LIBS)

PKG_CHECK_MODULES(OFONO, ofono >= OFONO_REQUIRED_VERSION)

dnl ###########################################################################
dnl Internationalization
dnl ###########################################################################

IT_PROG_INTLTOOL([0.35.0])
AC_SUBST(GETTEXT_PACKAGE, ofono-wizard)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The gettext package name])

dnl ###########################################################################
dnl Files to generate
dnl ###########################################################################

AC_OUTPUT([
Makefile
po/Makefile.in
src/Makefile
])
