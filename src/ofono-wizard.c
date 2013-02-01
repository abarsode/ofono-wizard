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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 * (C) Copyright 2013 Intel, Inc.
 *
 * Authors : Dan Williams <dcbw@redhat.com>
 * Alok Barsode <alok.barsode@intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "ofono-wizard.h"

struct _OfonoWizardPrivate {
	gchar	*modem_path;
	GtkWidget *assistant;

};

#define OFONO_WIZARD_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), OFONO_TYPE_WIZARD, OfonoWizardPrivate))

static void          ofono_wizard_init                   (OfonoWizard		*ofono_wizard);
static void          ofono_wizard_class_init             (OfonoWizardClass	*klass);
static void          ofono_wizard_finalize               (GObject		*object);


G_DEFINE_TYPE (OfonoWizard, ofono_wizard, G_TYPE_OBJECT)

static void
intro_setup (OfonoWizardPrivate *priv)
{
	GtkWidget *vbox, *label, *alignment, *info_vbox;
	GtkCellRenderer *renderer;
	char *s;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	label = gtk_label_new (_("This assistant helps you easily set up a mobile broadband connection to a cellular (3G) network."));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 6);

	label = gtk_label_new (_("You will need the following information:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 6);

	alignment = gtk_alignment_new (0, 0, 1, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 25, 25, 0);

	info_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	gtk_container_add (GTK_CONTAINER (alignment), info_vbox);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 6);

	s = g_strdup_printf ("• %s", _("Your broadband provider's name"));
	label = gtk_label_new (s);
	g_free (s);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (info_vbox), label, FALSE, TRUE, 0);

	s = g_strdup_printf ("• %s", _("Your broadband billing plan name"));
	label = gtk_label_new (s);
	g_free (s);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (info_vbox), label, FALSE, TRUE, 0);

	s = g_strdup_printf ("• %s", _("(in some cases) Your broadband billing plan APN (Access Point Name)"));
	label = gtk_label_new (s);
	g_free (s);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (info_vbox), label, FALSE, TRUE, 0);

	gtk_widget_show_all (vbox);
	gtk_assistant_append_page (GTK_ASSISTANT (priv->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (priv->assistant),
	                              vbox, _("Set up a Mobile Broadband Connection"));

	gtk_assistant_set_page_complete (GTK_ASSISTANT (priv->assistant), vbox, TRUE);
	gtk_assistant_set_page_type (GTK_ASSISTANT (priv->assistant), vbox, GTK_ASSISTANT_PAGE_INTRO);
}

static void
assistant_closed (GtkButton *button, gpointer user_data)
{
}

static void
assistant_cancel (GtkButton *button, gpointer user_data)
{
	gtk_main_quit ();
}

static void
assistant_prepare (GtkButton *button, gpointer user_data)
{
}

void
ofono_wizard_setup_assistant(OfonoWizard *ofono_wizard)
{
	OfonoWizardPrivate *priv;

	priv = OFONO_WIZARD_GET_PRIVATE (ofono_wizard);

	priv->assistant = gtk_assistant_new ();

	gtk_window_set_title (GTK_WINDOW (priv->assistant), _("Mobile Broadband Connection Setup"));
	gtk_window_set_position (GTK_WINDOW (priv->assistant), GTK_WIN_POS_CENTER_ALWAYS);

	intro_setup (priv);

	g_signal_connect (priv->assistant, "close", G_CALLBACK (assistant_closed), priv);
	g_signal_connect (priv->assistant, "cancel", G_CALLBACK (assistant_cancel), priv);
	g_signal_connect (priv->assistant, "prepare", G_CALLBACK (assistant_prepare), priv);

	gtk_window_present (GTK_WINDOW (priv->assistant));
}

static void
ofono_wizard_init (OfonoWizard *ofono_wizard)
{
	ofono_wizard->priv = OFONO_WIZARD_GET_PRIVATE (ofono_wizard);
}

static void
ofono_wizard_class_init (OfonoWizardClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (OfonoWizardPrivate));

	object_class->finalize = ofono_wizard_finalize;
}

static void
ofono_wizard_finalize (GObject *object)
{

	OfonoWizard *ofono_wizard = OFONO_WIZARD (object);

	if (G_OBJECT_CLASS (ofono_wizard_parent_class)->finalize)
		(* G_OBJECT_CLASS (ofono_wizard_parent_class)->finalize) (object);
}

OfonoWizard *
ofono_wizard_new (void)
{
	return g_object_new (OFONO_TYPE_WIZARD, NULL);
}

