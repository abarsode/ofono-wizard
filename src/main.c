/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Authors: Alok Barsode <alok.barsode@intel.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "ofono-wizard.h"

gint
main (gint argc, gchar **argv)
{
	OfonoWizard *wizard;
	GOptionContext *context;
	GError *error = NULL;
	gchar *path = NULL;
	gboolean success;

	GOptionEntry entries[] = {
		{ "path", 'p', 0, G_OPTION_ARG_STRING, &path, "Object path for the modem", "PATH" },
		{ NULL }
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("Setup Context Details (APN) for modem's Context");
	g_option_context_add_main_entries (context, entries, NULL);
	success = g_option_context_parse (context, &argc, &argv, &error);

	if (!success) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		return 1;
	}

	if (path == NULL) {
		g_warning (_("Provide a modem path.\n"));
		exit (0);
	}

	mobile_provider_init ();

	wizard = ofono_wizard_new ();
	ofono_wizard_setup_modem (wizard, path);

	gtk_main ();

	mobile_provider_exit ();

	return 0;
}
