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

#include "ofono-manager.h"
#include "ofono-modem.h"
#include "ofono-connman.h"
#include "ofono-context.h"

#include "ofono-wizard.h"
#include "mobile-provider.h"

struct _OfonoWizardPrivate {
	Manager *manager;
	Modem	*modem;
	gchar	*name;
	gchar	*context_path;
	gchar	*modem_path;
	ConnectionManager *ConnectionManager;
	ConnectionContext *context;
	gboolean active;

	GtkWidget *assistant;
	const gchar *country_by_locale;
	gchar *selected_country;
	gchar *selected_provider;
	gchar *selected_plan;
	gchar *selected_apn;
	gchar *selected_username;
	gchar *selected_password;

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

	/* Plan page */
	guint32 plan_idx;
	GtkWidget *plan_page;
	GtkWidget *plan_combo;
	GtkListStore *plan_store;
	guint32 plan_focus_id;

	GtkWidget *plan_unlisted_entry;

	/* Confirm page */
	GtkWidget *confirm_page;
	GtkWidget *confirm_provider;
	GtkWidget *confirm_plan;
	GtkWidget *confirm_apn;
	GtkWidget *confirm_plan_label;
	GtkWidget *confirm_device;
	GtkWidget *confirm_device_label;
	guint32 confirm_idx;
};

#define OFONO_WIZARD_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), OFONO_TYPE_WIZARD, OfonoWizardPrivate))

static void          ofono_wizard_init                   (OfonoWizard		*ofono_wizard);
static void          ofono_wizard_class_init             (OfonoWizardClass	*klass);
static void          ofono_wizard_finalize               (GObject		*object);


G_DEFINE_TYPE (OfonoWizard, ofono_wizard, G_TYPE_OBJECT)

static void
ofono_wizard_setup_context (OfonoWizard *ofono_wizard, gchar *apn, gchar *username, gchar *password);
static void
connection_context_set_apn (GObject *source_object, GAsyncResult *res, gpointer user_data);

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

/**********************************************************/
/* Confirm page */
/**********************************************************/

static void
confirm_setup (OfonoWizardPrivate *priv)
{
	GtkWidget *vbox, *label, *alignment, *pbox;
	GtkWidget *device_label;

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	label = gtk_label_new (_("Your mobile broadband connection is configured with the following settings:"));
	gtk_widget_set_size_request (label, 500, -1);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 6);

	/* Device */
	label = gtk_label_new (_("Your Device:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 25, 0);
	device_label = gtk_label_new (priv->name);
	gtk_container_add (GTK_CONTAINER (alignment), device_label);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	/* Provider */
	label = gtk_label_new (_("Your Provider:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 25, 0);
	priv->confirm_provider = gtk_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (alignment), priv->confirm_provider);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	/* Plan and APN */
	priv->confirm_plan_label = gtk_label_new (_("Your Plan:"));
	gtk_misc_set_alignment (GTK_MISC (priv->confirm_plan_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), priv->confirm_plan_label, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 25, 0);

        pbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	gtk_container_add (GTK_CONTAINER (alignment), pbox);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	priv->confirm_plan = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->confirm_plan), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (pbox), priv->confirm_plan, FALSE, FALSE, 0);

	priv->confirm_apn = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->confirm_apn), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (priv->confirm_apn), 0, 6);
	gtk_box_pack_start (GTK_BOX (pbox), priv->confirm_apn, FALSE, FALSE, 0);

	if (TRUE) {
		alignment = gtk_alignment_new (0, 0.5, 1, 0);
		label = gtk_label_new (_("You have to enable this connection to connect to your mobile broadband provider using the settings you selected.  If the connection fails or you cannot access network resources, double-check your settings.  To modify your mobile broadband connection settings, choose \"APN settings \" from the System >> Network >> Cellular menu."));
		gtk_widget_set_size_request (label, 500, -1);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
		gtk_misc_set_padding (GTK_MISC (label), 0, 6);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_container_add (GTK_CONTAINER (alignment), label);
		gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 6);
	}

	gtk_widget_show_all (vbox);
	priv->confirm_idx = gtk_assistant_append_page (GTK_ASSISTANT (priv->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (priv->assistant),
	                              vbox, _("Confirm Mobile Broadband Settings"));

	gtk_assistant_set_page_complete (GTK_ASSISTANT (priv->assistant), vbox, TRUE);
	gtk_assistant_set_page_type (GTK_ASSISTANT (priv->assistant), vbox, GTK_ASSISTANT_PAGE_CONFIRM);

	priv->confirm_page = vbox;
}

static void
confirm_prepare (OfonoWizardPrivate *priv)
{
	gboolean manual = FALSE;
	GString *str;

	/* Provider */
	str = g_string_new (NULL);
	g_string_append (str, priv->selected_provider);
	g_string_append_printf (str, ", %s", priv->selected_country);

	gtk_label_set_text (GTK_LABEL (priv->confirm_provider), str->str);
	g_string_free (str, TRUE);

	/* Plan */
	gtk_widget_show (priv->confirm_plan_label);
	gtk_widget_show (priv->confirm_plan);
	gtk_widget_show (priv->confirm_apn);

	if (strcmp (priv->selected_plan, _("My plan is not listed...")))
		gtk_label_set_text (GTK_LABEL (priv->confirm_plan), priv->selected_plan);
	else
		gtk_label_set_text (GTK_LABEL (priv->confirm_plan), _("Unlisted"));


	str = g_string_new (NULL);
	g_string_append_printf (str, "<span color=\"#999999\">APN: %s</span>", priv->selected_apn);
	gtk_label_set_markup (GTK_LABEL (priv->confirm_apn), str->str);
	g_string_free (str, TRUE);
}

static void
intro_setup (OfonoWizardPrivate *priv)
{
	GtkWidget *vbox, *label, *alignment, *info_vbox;
	GtkCellRenderer *renderer;
	char *s, *markup;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	label = gtk_label_new (NULL);
	markup = g_markup_printf_escaped (_("This assistant helps you easily set up a mobile broadband connection to a cellular (3G) network using <b>%s</b>."), priv->name);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free (markup);
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
	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	gtk_widget_hide (priv->assistant);

	gtk_widget_destroy (priv->assistant);

	ofono_wizard_setup_context (wizard, priv->selected_apn, priv->selected_username, priv->selected_password);
}

/**********************************************************/
/* Plan page */
/**********************************************************/

#define PLAN_COL_NAME 0
#define PLAN_COL_MANUAL 1

static PlanInfo *
get_selected_plan_info (OfonoWizardPrivate *priv)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *plan;
	PlanInfo *info;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->plan_combo), &iter))
		return NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->plan_combo));
	if (!model)
		return NULL;

	gtk_tree_model_get (model, &iter,
	                    PLAN_COL_NAME, &plan,
	                    -1);

	priv->selected_plan = plan;
	info = mobile_provider_get_plan_info (priv->selected_country, priv->selected_provider, plan);

	return info;
}

static void
plan_update_complete (OfonoWizardPrivate *priv)
{
	GtkAssistant *assistant = GTK_ASSISTANT (priv->assistant);
	PlanInfo *info;

	info = get_selected_plan_info (priv);
	if (info) {
		priv->selected_apn = info->apn;

		if (info->username)
			priv->selected_username = info->username;

		if (info->password)
			priv->selected_password = info->password;

		gtk_assistant_set_page_complete (assistant, priv->plan_page, TRUE);
	} else {
		priv->selected_apn = (gchar *)gtk_entry_get_text (GTK_ENTRY (priv->plan_unlisted_entry));
		priv->selected_username = NULL;
		priv->selected_password = NULL;

		gtk_assistant_set_page_complete (assistant, priv->plan_page,
		                                 (priv->selected_apn && strlen (priv->selected_apn)));
	}

	/* For Debug only */
	/* g_printerr ("\nselected APN is %s\n", priv->selected_apn); */
	/* g_printerr ("selected  Username is %s\n", priv->selected_username); */
	/* g_printerr ("selected Password is %s\n", priv->selected_password); */
}

static void
plan_combo_changed (OfonoWizardPrivate *priv)
{
	PlanInfo *info;

	info = get_selected_plan_info (priv);
	if (info) {
		gtk_entry_set_text (GTK_ENTRY (priv->plan_unlisted_entry), info->apn);
		gtk_widget_set_sensitive (priv->plan_unlisted_entry, FALSE);
	} else {
		gtk_entry_set_text (GTK_ENTRY (priv->plan_unlisted_entry), "");
		gtk_widget_set_sensitive (priv->plan_unlisted_entry, TRUE);
		gtk_widget_grab_focus (priv->plan_unlisted_entry);
	}

	plan_update_complete (priv);
}

static gboolean
plan_row_separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean is_manual = FALSE;

	gtk_tree_model_get (model, iter,
	                    PLAN_COL_MANUAL, &is_manual,
	                    -1);
	if (!is_manual)
		return TRUE;

	return FALSE;
}

static void
apn_filter_cb (GtkEntry *   entry,
               const gchar *text,
               gint         length,
               gint *       position,
               gpointer     user_data)
{
	GtkEditable *editable = GTK_EDITABLE (entry);
	int i, count = 0;
	gchar *result = g_new0 (gchar, length);

	for (i = 0; i < length; i++) {
		if (   isalnum (text[i])
		    || (text[i] == '.')
		    || (text[i] == '_')
		    || (text[i] == '-'))
			result[count++] = text[i];
	}

	if (count > 0) {
		g_signal_handlers_block_by_func (G_OBJECT (editable),
		                                 G_CALLBACK (apn_filter_cb),
		                                 user_data);
		gtk_editable_insert_text (editable, result, count, position);
		g_signal_handlers_unblock_by_func (G_OBJECT (editable),
		                                   G_CALLBACK (apn_filter_cb),
		                                   user_data);
	}

	g_signal_stop_emission_by_name (G_OBJECT (editable), "insert-text");
	g_free (result);
}

static void
plan_setup (OfonoWizardPrivate *priv)
{
	GtkWidget *vbox, *label, *alignment, *hbox, *image;
	GtkCellRenderer *renderer;

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	label = gtk_label_new_with_mnemonic (_("_Select your plan:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	priv->plan_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);

	priv->plan_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (priv->plan_store));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->plan_combo);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (priv->plan_combo),
	                                      plan_row_separator_func,
	                                      NULL,
	                                      NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->plan_combo), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->plan_combo), renderer, "text", PLAN_COL_NAME);

	g_signal_connect_swapped (priv->plan_combo, "changed", G_CALLBACK (plan_combo_changed), priv);

	alignment = gtk_alignment_new (0, 0.5, 0.5, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 12, 0, 0);
	gtk_container_add (GTK_CONTAINER (alignment), priv->plan_combo);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic (_("Selected plan _APN (Access Point Name):"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	priv->plan_unlisted_entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->plan_unlisted_entry);
	gtk_entry_set_max_length (GTK_ENTRY (priv->plan_unlisted_entry), 64);
	g_signal_connect (priv->plan_unlisted_entry, "insert-text", G_CALLBACK (apn_filter_cb), priv);
	g_signal_connect_swapped (priv->plan_unlisted_entry, "changed", G_CALLBACK (plan_update_complete), priv);

	alignment = gtk_alignment_new (0, 0.5, 0.5, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 24, 0, 0);
	gtk_container_add (GTK_CONTAINER (alignment), priv->plan_unlisted_entry);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	if (priv->active) {
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

		image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
		gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

		label = gtk_label_new (_("Warning: You seem to have a active data connection.\nApplying these new settings will disconnect you from the active data connection.\n\n"));
		gtk_widget_set_size_request (label, 500, -1);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	}

	priv->plan_idx = gtk_assistant_append_page (GTK_ASSISTANT (priv->assistant), vbox);
	gtk_assistant_set_page_title (GTK_ASSISTANT (priv->assistant), vbox, _("Choose your Billing Plan"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (priv->assistant), vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_widget_show_all (vbox);

	priv->plan_page = vbox;
}

static void
add_plan (gpointer data, gpointer user_data)
{
	OfonoWizardPrivate *priv = user_data;
	gchar *plan = data;
	GtkTreeIter plan_iter;

	g_assert (data);

	gtk_list_store_append (GTK_LIST_STORE (priv->plan_store), &plan_iter);

	gtk_list_store_set (GTK_LIST_STORE (priv->plan_store),
	                    &plan_iter,
	                    PLAN_COL_NAME,
	                    plan,
	                    PLAN_COL_MANUAL,
	                    TRUE,
	                    -1);
}

static void
plan_prepare (OfonoWizardPrivate *priv)
{
	GtkTreeIter method_iter;
	GList *plans;

	if (priv->plan_store)
		gtk_list_store_clear (priv->plan_store);

	plans = mobile_provider_get_plan_list (priv->selected_country, priv->selected_provider);
	if (plans)
		g_list_foreach (plans, add_plan, priv);

	/* Draw the separator */
	if (plans)
		gtk_list_store_append (GTK_LIST_STORE (priv->plan_store), &method_iter);

	/* Add the "My plan is not listed..." item */
	gtk_list_store_append (GTK_LIST_STORE (priv->plan_store), &method_iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->plan_store),
	                    &method_iter,
	                    PLAN_COL_NAME,
	                    _("My plan is not listed..."),
	                    PLAN_COL_MANUAL,
			    TRUE,
	                    -1);

	/* Select the first item by default if nothing is yet selected */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->plan_combo)) < 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->plan_combo), 0);

	plan_combo_changed (priv);
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
	if (priv->selected_country)
		complete = TRUE;

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
	else if (page == priv->plan_page)
		plan_prepare (priv);
	else if (page == priv->confirm_page)
		confirm_prepare (priv);
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
	plan_setup (priv);
	confirm_setup (priv);

	g_signal_connect (priv->assistant, "close", G_CALLBACK (assistant_closed), ofono_wizard);
	g_signal_connect (priv->assistant, "cancel", G_CALLBACK (assistant_cancel), priv);
	g_signal_connect (priv->assistant, "prepare", G_CALLBACK (assistant_prepare), priv);

	gtk_window_present (GTK_WINDOW (priv->assistant));
}

static void
ofono_wizard_init (OfonoWizard *ofono_wizard)
{
	GError *error = NULL;
	OfonoWizardPrivate *priv;

	ofono_wizard->priv = OFONO_WIZARD_GET_PRIVATE (ofono_wizard);
	priv = ofono_wizard->priv;

	priv->manager = manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							G_DBUS_PROXY_FLAGS_NONE,
							"org.ofono",
							"/",
							NULL,
							&error);

	if (priv->manager == NULL) {
		g_warning ("Unable to get oFono proxy:%s", error->message);
		g_error_free (error);
		exit (0);
	}
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

/**********************************************************/
/* oFono functions */
/**********************************************************/
static void
connection_manager_get_contexts_cb (GObject      *source_object,
				    GAsyncResult *res,
				    gpointer      user_data)
{
	GError *error = NULL;
	GVariant *value = NULL;
	gboolean ret, found = FALSE;
	GVariant *result, *array_value, *tuple_value, *properties;
	GVariantIter array_iter, tuple_iter;
	gchar *path, *type;

	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	ret = connection_manager_call_get_contexts_finish (priv->ConnectionManager, &result, res, &error);
	if (!ret) {
		g_warning ("Unable to get Modem Proxy: %s", error->message);
		g_error_free (error);
		exit (0);
	}

	/* Result is  (a(oa{sv}))*/
	g_variant_iter_init (&array_iter, result);

	while ((array_value = g_variant_iter_next_value (&array_iter)) != NULL) {
		/* tuple_iter is oa{sv} */
		g_variant_iter_init (&tuple_iter, array_value);

		/* get the object path */
		tuple_value = g_variant_iter_next_value (&tuple_iter);
		g_variant_get (tuple_value, "o", &path);

		/* get the Properties */
		properties = g_variant_iter_next_value (&tuple_iter);
		g_variant_lookup (properties, "Type", "s", &type);
		if (!strcmp (type, "internet")) {
			/* Found the 1st internet context */
			priv->context_path = g_strdup (path);
			ret = g_variant_lookup (properties, "Active", "b", &priv->active);
			/* Check if the context if active */
			if (!ret)
				priv->active = TRUE;

			found = TRUE;
			break;
		}

		g_variant_unref (array_value);
		g_variant_unref (tuple_value);
		g_variant_unref (properties);
	}

	g_variant_unref (array_value);
	g_variant_unref (tuple_value);
	g_variant_unref (properties);
	g_variant_unref (result);

	if (!found) {
		g_warning ("Unable to get Internet context");
		exit (0);
		/* Create a 'internet' context here? */
	}

	ofono_wizard_setup_assistant (wizard);
}

static void
ofono_wizard_get_modem_context (OfonoWizard *ofono_wizard)
{
	GError *error = NULL;

	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (ofono_wizard);
	
	priv->ConnectionManager = connection_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
									     G_DBUS_PROXY_FLAGS_NONE,
									     "org.ofono",
									     priv->modem_path,
									     NULL,
									     &error);

	if (priv->ConnectionManager == NULL) {
		g_warning ("Unable to get Modem connection Manager: %s", error->message);
		g_error_free (error);
		exit (0);
	}

	connection_manager_call_get_contexts (priv->ConnectionManager, NULL, connection_manager_get_contexts_cb ,ofono_wizard);
}

static void
modem_get_properties_cb (GObject      *source_object,
			 GAsyncResult *res,
			 gpointer      user_data)
{
	GError *error = NULL;
	GVariant *result = NULL;
	GVariant *value = NULL;
	gboolean ret;
	const gchar *name, *manufacturer, *model, *type;
	gchar *interface_list;
	gchar **interfaces;

	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	ret = modem_call_get_properties_finish (priv->modem, &result, res, &error);
	if (!ret) {
		g_warning ("Unable to get Modem Proxy: %s", error->message);
		g_error_free (error);
		exit (0);
	}

	/* Test for Type=Hardware here */
	ret = g_variant_lookup (result, "Type", "s", &type);
	if (!ret) {
		g_warning ("Unable to determine modem type");
		g_variant_unref (result);
		exit (0);
	}

	if (strcmp (type, "hardware")) {
		g_warning ("Not a real hardware modem");
		g_variant_unref (result);
		exit (0);
	}

	ret = g_variant_lookup (result, "Name", "s", &name);
	if (!ret) {
		g_variant_lookup (result, "Manufacturer", "s", &manufacturer);
		g_variant_lookup (result, "Model", "s", &model);

		if (manufacturer && model)
			priv->name = g_strdup_printf ("%s-%s", manufacturer, model);
		else if (manufacturer)
			priv->name = g_strdup (manufacturer);
		else
			priv->name = g_strdup ("Modem");
	} else
		priv->name = g_strdup (name);

	ret = g_variant_lookup (result, "Interfaces", "^as", &interfaces);
	if (!ret) {
		g_warning ("Unable to get modem interfaces");
		g_variant_unref (result);
		exit (0);
	}

	if (g_strv_length (interfaces) == 0) {
		g_warning ("No modem interfaces found");
		g_variant_unref (result);
		exit (0);
	}

	/* Find if ConnectionManager interface is available */
	interface_list = g_strjoinv (" ", interfaces);
	gchar *s = g_strrstr (interface_list, "org.ofono.ConnectionManager");
	if (s == NULL) {
		g_warning ("No Contexts interface found");
		g_free (interface_list);
		g_strfreev (interfaces);
		g_variant_unref (result);
		exit (0);
	}

	g_free (interface_list);
	g_strfreev (interfaces);
	g_variant_unref (result);

	ofono_wizard_get_modem_context (wizard);
}

void
ofono_wizard_setup_modem (OfonoWizard *ofono_wizard, gchar *modem_path)
{
	GError *error = NULL;

	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (ofono_wizard);
	priv->modem_path = g_strdup (modem_path);

	priv->modem = modem_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						    G_DBUS_PROXY_FLAGS_NONE,
						    "org.ofono",
						    modem_path,
						    NULL,
						    &error);

	if (priv->modem == NULL) {
		g_warning ("Unable to get Modem Proxy: %s", error->message);
		g_error_free (error);
		exit (0);
	}

	modem_call_get_properties (priv->modem, NULL, modem_get_properties_cb, ofono_wizard);
}

static void
connection_context_set_active (GObject      *source_object,
			       GAsyncResult *res,
			       gpointer      user_data)
{
	GError *error = NULL;
	gboolean ret;

	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	ret = connection_context_call_set_property_finish (priv->context, res, &error);
	if (!ret) {
		g_warning ("Unable to set Context Property:Active : %s", error->message);
		g_error_free (error);
		if (error->code != 36)
			gtk_main_quit ();
	}

	connection_context_call_set_property (priv->context,
					      "AccessPointName",
					      g_variant_new_variant (g_variant_new_string (priv->selected_apn)),
					      NULL,
					      connection_context_set_apn,
					      wizard);
}

static void
connection_context_set_plan (GObject      *source_object,
			     GAsyncResult *res,
			     gpointer      user_data)
{
	GError *error = NULL;
	gboolean ret;

	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	ret = connection_context_call_set_property_finish (priv->context, res, &error);
	if (!ret) {
		if (error->code != 36)
			g_warning ("Unable to set Context Property:Plan : %s", error->message);
		g_error_free (error);
	}

	gtk_main_quit ();
}

static void
connection_context_set_password (GObject      *source_object,
				 GAsyncResult *res,
				 gpointer      user_data)
{
	GError *error = NULL;
	gboolean ret;

	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	ret = connection_context_call_set_property_finish (priv->context, res, &error);
	if (!ret) {
		g_warning ("Unable to set Context Property:Password : %s", error->message);
		g_error_free (error);
		if (error->code != 36)
			gtk_main_quit ();
	}

	connection_context_call_set_property (priv->context,
					      "Name",
					      g_variant_new_variant (g_variant_new_string (priv->selected_plan)),
					      NULL,
					      connection_context_set_plan,
					      wizard);
}

static void
connection_context_set_username (GObject      *source_object,
				 GAsyncResult *res,
				 gpointer      user_data)
{
	GError *error = NULL;
	gboolean ret;

	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	ret = connection_context_call_set_property_finish (priv->context, res, &error);
	if (!ret) {
		g_warning ("Unable to set Context Property:Username : %s", error->message);
		g_error_free (error);
		if (error->code != 36)
			gtk_main_quit ();
	}

	if (priv->selected_password) {
		connection_context_call_set_property (priv->context,
						      "Password",
						      g_variant_new_variant (g_variant_new_string (priv->selected_password)),
						      NULL,
						      connection_context_set_password,
						      wizard);
	} else {
		connection_context_call_set_property (priv->context,
						      "Password",
						      g_variant_new_variant (g_variant_new_string ("")),
						      NULL,
						      connection_context_set_password,
						      wizard);
	}
}

static void
connection_context_set_apn (GObject      *source_object,
			    GAsyncResult *res,
			    gpointer      user_data)
{
	GError *error = NULL;
	gboolean ret;

	OfonoWizard *wizard = user_data;
	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (wizard);

	ret = connection_context_call_set_property_finish (priv->context, res, &error);
	if (!ret) {
		g_warning ("Unable to set Context Property:APN : %s", error->message);
		g_error_free (error);
		if (error->code != 36)
			gtk_main_quit ();
	}

	if (priv->selected_username) {
		connection_context_call_set_property (priv->context,
						      "Username",
						      g_variant_new_variant (g_variant_new_string (priv->selected_username)),
						      NULL,
						      connection_context_set_username,
						      wizard);
	} else {
		connection_context_call_set_property (priv->context,
						      "Username",
						      g_variant_new_variant (g_variant_new_string ("")),
						      NULL,
						      connection_context_set_username,
						      wizard);
	}
}

static void
ofono_wizard_setup_context (OfonoWizard *ofono_wizard, gchar *apn, gchar *username, gchar *password)
{
	GError *error = NULL;
	gboolean ret;

	OfonoWizardPrivate *priv = OFONO_WIZARD_GET_PRIVATE (ofono_wizard);

	priv->context = connection_context_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
								   G_DBUS_PROXY_FLAGS_NONE,
								   "org.ofono",
								   priv->context_path,
								   NULL,
								   &error);

	if (priv->context == NULL) {
		g_warning ("Unable to get Modem context: %s", error->message);
		g_error_free (error);
		gtk_main_quit ();
	}

	if (priv->active) {
		connection_context_call_set_property (priv->context,
						      "Active",
						      g_variant_new_variant (g_variant_new_boolean (FALSE)),
						      NULL,
						      connection_context_set_active,
						      ofono_wizard);
	} else {
		connection_context_call_set_property (priv->context,
						      "AccessPointName",
						      g_variant_new_variant (g_variant_new_string (priv->selected_apn)),
						      NULL,
						      connection_context_set_apn,
						      ofono_wizard);
	}

}
