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
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "ofono-wizard.h"

struct _OfonoWizardPrivate {
	gchar	*modem_path;
	GtkWidget *assistant;
	const gchar *country_by_locale;
	gchar *selected_country;

	/* Country page */
	guint32 country_idx;
	GtkWidget *country_page;
	GtkWidget *country_view;
	GtkListStore *country_store;
	guint32 country_focus_id;
};

#define OFONO_WIZARD_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), OFONO_TYPE_WIZARD, OfonoWizardPrivate))

static void          ofono_wizard_init                   (OfonoWizard		*ofono_wizard);
static void          ofono_wizard_class_init             (OfonoWizardClass	*klass);
static void          ofono_wizard_finalize               (GObject		*object);


G_DEFINE_TYPE (OfonoWizard, ofono_wizard, G_TYPE_OBJECT)

static char *
get_country_from_locale (void)
{
	char *p, *m, *cc, *lang;

	lang = getenv ("LC_ALL");
	if (!lang)
		lang = getenv ("LANG");
	if (!lang)
		return NULL;

	p = strchr (lang, '_');
	if (!p || !strlen (p)) {
		g_free (p);
		return NULL;
	}

	p = cc = g_strdup (++p);
	m = strchr (cc, '.');
	if (m)
		*m = '\0';

	while (*p) {
		*p = g_ascii_toupper (*p);
		p++;
	}

	return cc;
}

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

/**********************************************************/
/* Country page */
/**********************************************************/

#define COUNTRIES_COL_NAME 0

static gboolean
country_search_func (GtkTreeModel *model,
                     gint column,
                     const char *key,
                     GtkTreeIter *iter,
                     gpointer search_data)
{
	gboolean unmatched = TRUE;
	char *country = NULL;

	if (!key)
		return TRUE;

	gtk_tree_model_get (model, iter, column, &country, -1);
	if (!country)
		return TRUE;

	unmatched = !!g_ascii_strncasecmp (country, key, strlen (key));
	g_free (country);
	return unmatched;
}

static void
add_country (gpointer data, gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;
	gchar *country = data;
	GtkTreeIter country_iter;
	GtkTreePath *country_path;

	g_assert (data);

	gtk_list_store_append (GTK_LIST_STORE (priv->country_store), &country_iter);

	gtk_list_store_set (GTK_LIST_STORE (priv->country_store),
	                    &country_iter,
	                    COUNTRIES_COL_NAME,
	                    country,
	                    -1);

	/* If this country is the same country as the user's current locale,
	 * select it by default.
	 */

	if (strcmp (priv->country_by_locale, country) != 0)
		return;

	country_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->country_store), &country_iter);
	if (!country_path)
		return;

	if (country_path) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->country_view));
		g_assert (selection);

		gtk_tree_selection_select_path (selection, country_path);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->country_view),
		                              country_path, NULL, TRUE, 0, 0);
	}

	gtk_tree_path_free (country_path);
}

static const gchar *
get_selected_country (OfonoWizardPrivate *priv)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *country = NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->country_view));
	g_assert (selection);

	if (!gtk_tree_selection_get_selected (GTK_TREE_SELECTION (selection), &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COUNTRIES_COL_NAME, &country, -1);
	return country;
}

static void
country_update_complete (OfonoWizardPrivate *priv)
{
	gboolean complete = FALSE;

	priv->selected_country = get_selected_country (priv);
	if (priv->selected_country) {
		complete = TRUE;
		g_printerr ("\nSelected country is %s\n", priv->selected_country);
	}

	gtk_assistant_set_page_complete (GTK_ASSISTANT (priv->assistant),
	                                 priv->country_page,
	                                 complete);
}

static void
country_setup (OfonoWizardPrivate *priv)
{
	GtkWidget *vbox, *label, *scroll, *alignment;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeIter unlisted_iter;
	GList *countries;

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	label = gtk_label_new (_("Country or Region List:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

	priv->country_store = gtk_list_store_new (1, G_TYPE_STRING);

	priv->country_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->country_store));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Country or region"),
	                                                   renderer,
	                                                   "text", COUNTRIES_COL_NAME,
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->country_view), column);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Add the providers */
	countries = mobile_provider_get_country_list ();
	if (countries)
		g_list_foreach (countries, add_country, priv);

	/* My country is not listed... */
	gtk_list_store_append (GTK_LIST_STORE (priv->country_store), &unlisted_iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->country_store), &unlisted_iter,
	                    COUNTRIES_COL_NAME, _("My country is not listed"),
	                    -1);

	g_object_set (G_OBJECT (priv->country_view), "enable-search", TRUE, NULL);

	/* If no row has focus yet, focus the first row so that the user can start
	 * incremental search without clicking.
	 */

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->country_view));
	g_assert (selection);
	if (!gtk_tree_selection_count_selected_rows (selection)) {
		GtkTreeIter first_iter;
		GtkTreePath *first_path;

		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->country_store), &first_iter)) {
			first_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->country_store), &first_iter);
			if (first_path) {
				gtk_tree_selection_select_path (selection, first_path);
				gtk_tree_path_free (first_path);
			}
		}
	}

	g_signal_connect_swapped (selection, "changed", G_CALLBACK (country_update_complete), priv);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), priv->country_view);

	alignment = gtk_alignment_new (0, 0, 1, 1);
	gtk_container_add (GTK_CONTAINER (alignment), scroll);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 6);

	priv->country_idx = gtk_assistant_append_page (GTK_ASSISTANT (priv->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (priv->assistant), vbox, _("Choose your Provider's Country or Region"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (priv->assistant), vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (GTK_ASSISTANT (priv->assistant), vbox, TRUE);
	gtk_widget_show_all (vbox);

	priv->country_page = vbox;

	/* Initial completeness state */
	country_update_complete (priv);
}

static gboolean
focus_country_view (gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;

	priv->country_focus_id = 0;
	gtk_widget_grab_focus (priv->country_view);
	return FALSE;
}

static void
country_prepare (OfonoWizardPrivate *priv)
{
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->country_view), COUNTRIES_COL_NAME);
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (priv->country_view), country_search_func, priv, NULL);

	if (!priv->country_focus_id)
		priv->country_focus_id = g_idle_add (focus_country_view, priv);

	country_update_complete (priv);
}

static void
assistant_cancel (GtkButton *button, gpointer user_data)
{
	gtk_main_quit ();
}

static void
remove_country_focus_idle (OfonoWizardPrivate *priv)
{
	if (priv->country_focus_id) {
		g_source_remove (priv->country_focus_id);
		priv->country_focus_id = 0;
	}
}

static void
assistant_prepare (GtkAssistant *assistant, GtkWidget *page, gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;

	if (page != priv->country_page)
		remove_country_focus_idle (priv);

	if (page == priv->country_page)
		country_prepare (priv);
}

void
ofono_wizard_setup_assistant(OfonoWizard *ofono_wizard)
{
	OfonoWizardPrivate *priv;
	gchar *country_code_by_locale;

	priv = OFONO_WIZARD_GET_PRIVATE (ofono_wizard);

	priv->assistant = gtk_assistant_new ();

	country_code_by_locale = get_country_from_locale ();
	priv->country_by_locale = mobile_provider_get_country_from_code (country_code_by_locale);

	gtk_window_set_title (GTK_WINDOW (priv->assistant), _("Mobile Broadband Connection Setup"));
	gtk_window_set_position (GTK_WINDOW (priv->assistant), GTK_WIN_POS_CENTER_ALWAYS);

	intro_setup (priv);
	country_setup (priv);

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

