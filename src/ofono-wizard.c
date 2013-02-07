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
#include "mobile-provider.h"

struct _OfonoWizardPrivate {
	gchar	*modem_path;
	GtkWidget *assistant;
	const gchar *country_by_locale;
	gchar *selected_country;
	gchar *selected_provider;

	/* Country page */
	guint32 country_idx;
	GtkWidget *country_page;
	GtkWidget *country_view;
	GtkListStore *country_store;
	guint32 country_focus_id;

	/* Providers page */
	guint32 providers_idx;
	GtkWidget *providers_page;
	GtkWidget *providers_view;
	GtkListStore *providers_store;
	guint32 providers_focus_id;
	GtkWidget *providers_view_radio;

	GtkWidget *provider_unlisted_radio;
	GtkWidget *provider_unlisted_entry;
	GtkWidget *provider_unlisted_type_combo;

	gboolean provider_only_cdma;
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
/* Providers page */
/**********************************************************/

#define PROVIDER_COL_NAME 0

static gchar *
get_selected_provider (OfonoWizardPrivate *priv)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *provider = NULL;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->providers_view_radio)))
		return NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->providers_view));
	g_assert (selection);

	if (!gtk_tree_selection_get_selected (GTK_TREE_SELECTION (selection), &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, PROVIDER_COL_NAME, &provider, -1);
	return provider;
}

static void
providers_update_complete (OfonoWizardPrivate *priv)
{
	GtkAssistant *assistant = GTK_ASSISTANT (priv->assistant);
	gboolean use_view;
	gboolean complete = FALSE;

	use_view = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->providers_view_radio));
	if (use_view) {
		priv->selected_provider = get_selected_provider (priv);
		if (priv->selected_provider)
			complete = TRUE;

		gtk_assistant_set_page_complete (assistant, priv->providers_page, complete);
	} else {
		priv->selected_provider = (gchar *) gtk_entry_get_text (GTK_ENTRY (priv->provider_unlisted_entry));
		gtk_assistant_set_page_complete (assistant, priv->providers_page,
		                                 (priv->selected_provider && strlen (priv->selected_provider)));
	}

	g_printerr ("\n selected provider:%s\n", priv->selected_provider);
}

static gboolean
focus_providers_view (gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;

	priv->providers_focus_id = 0;
	gtk_widget_grab_focus (priv->providers_view);
	return FALSE;
}

static gboolean
focus_provider_unlisted_entry (gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;

	priv->providers_focus_id = 0;
	gtk_widget_grab_focus (priv->provider_unlisted_entry);
	return FALSE;
}

static void
providers_radio_toggled (GtkToggleButton *button, gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;
	gboolean use_view;

	use_view = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->providers_view_radio));
	if (use_view) {
		if (!priv->providers_focus_id)
			priv->providers_focus_id = g_idle_add (focus_providers_view, priv);
		gtk_widget_set_sensitive (priv->providers_view, TRUE);
		gtk_widget_set_sensitive (priv->provider_unlisted_entry, FALSE);
		gtk_widget_set_sensitive (priv->provider_unlisted_type_combo, FALSE);
	} else {
		if (!priv->providers_focus_id)
			priv->providers_focus_id = g_idle_add (focus_provider_unlisted_entry, priv);
		gtk_widget_set_sensitive (priv->providers_view, FALSE);
		gtk_widget_set_sensitive (priv->provider_unlisted_entry, TRUE);
		gtk_widget_set_sensitive (priv->provider_unlisted_type_combo, TRUE);
	}

	providers_update_complete (priv);
}

static gboolean
providers_search_func (GtkTreeModel *model,
                       gint column,
                       const char *key,
                       GtkTreeIter *iter,
                       gpointer search_data)
{
	gboolean unmatched = TRUE;
	char *provider = NULL;

	if (!key)
		return TRUE;

	gtk_tree_model_get (model, iter, column, &provider, -1);
	if (!provider)
		return TRUE;

	unmatched = !!g_ascii_strncasecmp (provider, key, strlen (key));
	g_free (provider);
	return unmatched;
}


static void
add_provider (gpointer data, gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;
	gchar *provider = data;
	GtkTreeIter provider_iter;

	g_assert (data);

	gtk_list_store_append (GTK_LIST_STORE (priv->providers_store), &provider_iter);

	gtk_list_store_set (GTK_LIST_STORE (priv->providers_store),
	                    &provider_iter,
	                    PROVIDER_COL_NAME,
	                    provider,
	                    -1);
}

static void
providers_setup (OfonoWizardPrivate *priv)
{
	GtkWidget *vbox, *scroll, *alignment, *unlisted_table, *label;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	priv->providers_view_radio = gtk_radio_button_new_with_mnemonic (NULL, _("Select your provider from a _list:"));
	g_signal_connect (priv->providers_view_radio, "toggled", G_CALLBACK (providers_radio_toggled), priv);
	gtk_box_pack_start (GTK_BOX (vbox), priv->providers_view_radio, FALSE, TRUE, 0);

	priv->providers_store = gtk_list_store_new (1, G_TYPE_STRING);

	priv->providers_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->providers_store));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Provider"),
	                                                   renderer,
	                                                   "text", PROVIDER_COL_NAME,
	                                                   NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->providers_view), column);
	gtk_tree_view_column_set_clickable (column, TRUE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->providers_view));
	g_assert (selection);
	g_signal_connect_swapped (selection, "changed", G_CALLBACK (providers_update_complete), priv);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (scroll, -1, 140);
	gtk_container_add (GTK_CONTAINER (scroll), priv->providers_view);

	alignment = gtk_alignment_new (0, 0, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 25, 0);
	gtk_container_add (GTK_CONTAINER (alignment), scroll);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 0);

	priv->provider_unlisted_radio = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (priv->providers_view_radio),
	                                            _("I can't find my provider and I wish to enter it _manually:"));
	g_signal_connect (priv->providers_view_radio, "toggled", G_CALLBACK (providers_radio_toggled), priv);
	gtk_box_pack_start (GTK_BOX (vbox), priv->provider_unlisted_radio, FALSE, TRUE, 0);

	alignment = gtk_alignment_new (0, 0, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 15, 0);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	unlisted_table = GTK_WIDGET (gtk_grid_new ());
	gtk_container_add (GTK_CONTAINER (alignment), unlisted_table);

	label = gtk_label_new (_("Provider:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_grid_attach (GTK_GRID (unlisted_table), label, 0, 1, 1, 1);

	priv->provider_unlisted_entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (priv->provider_unlisted_entry), 40);
	g_signal_connect_swapped (priv->provider_unlisted_entry, "changed", G_CALLBACK (providers_update_complete), priv);

	alignment = gtk_alignment_new (0, 0.5, 1, 0);
	gtk_container_add (GTK_CONTAINER (alignment), priv->provider_unlisted_entry);
	gtk_grid_attach (GTK_GRID (unlisted_table), alignment, 0, 2, 1, 1);

	priv->provider_unlisted_type_combo = gtk_combo_box_text_new ();

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->provider_unlisted_type_combo);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->provider_unlisted_type_combo),
	                           _("My provider uses GSM technology (GPRS, EDGE, UMTS, HSPA)"));

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->provider_unlisted_type_combo),
	                           _("My provider uses CDMA technology (1xRTT, EVDO)"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->provider_unlisted_type_combo), 0);

	gtk_grid_attach (GTK_GRID (unlisted_table), priv->provider_unlisted_type_combo,
			 0, 3, 1, 2);

	/* Only show the CDMA/GSM combo if we don't know the device type */
	//if (priv->family != NMA_MOBILE_FAMILY_UNKNOWN)
	//gtk_widget_hide (priv->provider_unlisted_type_combo);

	priv->providers_idx = gtk_assistant_append_page (GTK_ASSISTANT (priv->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (priv->assistant), vbox, _("Choose your Provider"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (priv->assistant), vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_widget_show_all (vbox);

	priv->providers_page = vbox;
}

static void
providers_prepare (OfonoWizardPrivate *priv)
{
	GtkTreeSelection *selection;
	GList *providers;

	gtk_list_store_clear (priv->providers_store);

	if (!strcmp (priv->selected_country, _("Not Listed"))) {
		/* Unlisted country */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->provider_unlisted_radio), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->providers_view_radio), FALSE);
		goto done;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (priv->providers_view_radio), TRUE);

	providers = mobile_provider_get_provider_list (priv->selected_country);
	if (providers)
		g_list_foreach (providers, add_provider, priv);


	g_object_set (G_OBJECT (priv->providers_view), "enable-search", TRUE, NULL);

	gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->providers_view), PROVIDER_COL_NAME);
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (priv->providers_view),
	                                     providers_search_func, priv, NULL);

	/* If no row has focus yet, focus the first row so that the user can start
	 * incremental search without clicking.
	 */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->providers_view));
	g_assert (selection);
	if (!gtk_tree_selection_count_selected_rows (selection)) {
		GtkTreeIter first_iter;
		GtkTreePath *first_path;

		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->providers_store), &first_iter)) {
			first_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->providers_store), &first_iter);
			if (first_path) {
				gtk_tree_selection_select_path (selection, first_path);
				gtk_tree_path_free (first_path);
			}
		}
	}

done:
	providers_radio_toggled (NULL, priv);

	/* Initial completeness state */
	providers_update_complete (priv);

	/* If there's already a selected device, hide the GSM/CDMA radios */
	/* if (priv->family != NMA_MOBILE_FAMILY_UNKNOWN) */
	/* 	gtk_widget_hide (priv->provider_unlisted_type_combo); */
	/* else */
	/* 	gtk_widget_show (priv->provider_unlisted_type_combo); */
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

static gchar *
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

	if (!strcmp (country, _("My country is not listed")))
		country = g_strdup (_("Not Listed"));

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

	/* Add the Countries */
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
remove_provider_focus_idle (OfonoWizardPrivate *priv)
{
	if (priv->providers_focus_id) {
		g_source_remove (priv->providers_focus_id);
		priv->providers_focus_id = 0;
	}
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
	if (page != priv->providers_page)
		remove_provider_focus_idle (priv);
	if (page != priv->country_page)
		remove_country_focus_idle (priv);

	if (page == priv->country_page)
		country_prepare (priv);
	else if (page == priv->providers_page)
		providers_prepare (priv);
	else
		return;
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
	providers_setup (priv);

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

