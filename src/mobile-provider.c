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
 * (C) Copyright 2009 Novell, Inc.
 * (C) Copyright 2012 Lanedo GmbH.
 * (C) Copyright 2013 Intel, Inc.
 *
 * Authors : Dan Williams <dcbw@redhat.com>
 * Tambet Ingo <tambet@gmail.com>
 * Alok Barsode <alok.barsode@intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <glib/gi18n.h>

#include <stdio.h>
#include <glib.h>

/* Fixit: Determine the PREFIX from configure */
#ifndef MOBILE_BROADBAND_PROVIDER_INFO
#define MOBILE_BROADBAND_PROVIDER_INFO "/usr/share/mobile-broadband-provider-info/serviceproviders.xml"
#endif

#define ISO_3166_COUNTRY_CODES "/usr/share/xml/iso-codes/iso_3166.xml"

typedef enum {
  PARSER_TOPLEVEL = 0,
  PARSER_COUNTRY,
  PARSER_PROVIDER,
  PARSER_METHOD_GSM,
  PARSER_METHOD_GSM_APN,
  PARSER_METHOD_CDMA,
  PARSER_ERROR
} MobileContextState;

MobileContextState servicexml_state = PARSER_TOPLEVEL;

static char *servicexml_text_buffer = NULL;

static char *servicexml_current_country_code = NULL;

static char *servicexml_current_provider_name = NULL;

static char *servicexml_current_apn = NULL;
static char *servicexml_current_plan_name = NULL;
static char *servicexml_current_username = NULL;
static char *servicexml_current_password = NULL;

typedef struct _PlanInfo
{
  char *apn;
  char *username;
  char *password;
}PlanInfo;

/* Country Code (key) <--> Hash of Providers (value)	: country_info
 * Provider Name(key) <--> Hash of Plans (value)	: provider_info
 * Plan Name	(key) <--> Hash of Plan Info (value)	: plan_info
*/
static GHashTable *plan_info = NULL;
static GHashTable *provider_info = NULL;
static GHashTable *country_info = NULL;

/* Country Name (key) <--> Country codes (value)	: country_codes */
static GHashTable *country_codes = NULL;

static mobile_provider_table_destroy (gpointer data)
{
	GHashTable *a = data;

	if (a == NULL)
		return;

	g_hash_table_destroy (a);
}

static void
servicexml_toplevel_start (const char *name,
			   const char **attribute_names,
			   const char **attribute_values)
{
	int i;

	if (!strcmp (name, "serviceproviders")) {
		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "format")) {
				if (strcmp (attribute_values[i], "2.0")) {
					g_warning ("%s: mobile broadband provider database format '%s'"
					           " not supported.", __func__, attribute_values[i]);
					servicexml_state = PARSER_ERROR;
					break;
				}
			}
		}
	} else if (!strcmp (name, "country")) {
		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "code")) {
				char *country_code;
				/* Directly use servicexml_current_country_code? */
				country_code = g_ascii_strup (attribute_values[i], -1);
				servicexml_current_country_code = country_code;

				servicexml_state = PARSER_COUNTRY;
				break;
			}
		}
	}
}

static void
servicexml_country_start (const char *name,
			  const char **attribute_names,
			  const char **attribute_values)
{
	if (!strcmp (name, "provider")) {
		servicexml_state = PARSER_PROVIDER;
	}
}

static void
servicexml_provider_start (const char *name,
			   const char **attribute_names,
			   const char **attribute_values)
{
	if (!strcmp (name, "gsm"))
		servicexml_state = PARSER_METHOD_GSM;
	else if (!strcmp (name, "cdma")) {
		servicexml_state = PARSER_METHOD_CDMA;
	}
}

static void
servicexml_gsm_start (const char *name,
		      const char **attribute_names,
		      const char **attribute_values)
{
	if (!strcmp (name, "apn")) {
		int i;

		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "value")) {
				servicexml_state = PARSER_METHOD_GSM_APN;
				servicexml_current_apn = g_strstrip (g_strdup (attribute_values[i]));
				break;
			}
		}
	}
}

static void
servicexml_cdma_start (const char *name,
                   const char **attribute_names,
                   const char **attribute_values)
{
	return;
}

void servicexml_start_element (GMarkupParseContext *context,
			const gchar         *element_name,
			const gchar        **attribute_names,
			const gchar        **attribute_values,
			gpointer             user_data,
			GError             **error)
{
	switch (servicexml_state) {
	case PARSER_TOPLEVEL:
		servicexml_toplevel_start (element_name, attribute_names, attribute_values);
		break;
	case PARSER_COUNTRY:
		servicexml_country_start (element_name, attribute_names, attribute_values);
		break;
	case PARSER_PROVIDER:
		servicexml_provider_start (element_name, attribute_names, attribute_values);
		break;
	case PARSER_METHOD_GSM:
		servicexml_gsm_start (element_name, attribute_names, attribute_values);
		break;
	case PARSER_METHOD_CDMA:
		servicexml_cdma_start (element_name, attribute_names, attribute_values);
		break;
	default:
		break;
	}
}

static void
servicexml_country_end (const char *name)
{
	if (!strcmp (name, "country")) {
		g_free (servicexml_text_buffer);
		servicexml_text_buffer = NULL;

		if (country_info == NULL) {
			country_info = g_hash_table_new_full (g_str_hash, g_str_equal,
							       (GDestroyNotify) g_free,
							       (GDestroyNotify) mobile_provider_table_destroy);
		}

		g_hash_table_insert (country_info, servicexml_current_country_code, provider_info);

		servicexml_current_country_code = NULL;
		provider_info = NULL;

		servicexml_state = PARSER_TOPLEVEL;
	}
}

static void
servicexml_provider_end (const char *name)
{
	if (!strcmp (name, "name")) {
		servicexml_current_provider_name = servicexml_text_buffer;

		servicexml_text_buffer = NULL;
	} else if (!strcmp (name, "provider")) {
		g_free (servicexml_text_buffer);
		servicexml_text_buffer = NULL;

		if (provider_info == NULL) {
			provider_info = g_hash_table_new_full (g_str_hash, g_str_equal,
							       (GDestroyNotify) g_free,
							       (GDestroyNotify) mobile_provider_table_destroy);
		}

		g_hash_table_insert (provider_info, servicexml_current_provider_name, plan_info);

		servicexml_current_provider_name = NULL;
		plan_info = NULL;

		servicexml_state = PARSER_COUNTRY;
	}
}

static void
servicexml_gsm_end (const char *name)
{
	if (!strcmp (name, "gsm")) {
		g_free (servicexml_text_buffer);
		servicexml_text_buffer = NULL;
		servicexml_state = PARSER_PROVIDER;
	}
}

static void
servicexml_plan_info_free (gpointer user_data)
{
	PlanInfo *info = user_data;
	g_free (info->apn);
	g_free (info->username);
	g_free (info->password);

	g_slice_free (PlanInfo, info);
}

static void
servicexml_gsm_apn_end (const char *name)
{
	if (!strcmp (name, "name")) {
		servicexml_current_plan_name = servicexml_text_buffer;
		servicexml_text_buffer = NULL;
	} else if (!strcmp (name, "username")) {
		servicexml_current_username = servicexml_text_buffer;
		servicexml_text_buffer = NULL;
	} else if (!strcmp (name, "password")) {
		servicexml_current_password = servicexml_text_buffer;
		servicexml_text_buffer = NULL;
	} else if (!strcmp (name, "apn")) {
		if (plan_info == NULL) {
			plan_info = g_hash_table_new_full (g_str_hash, g_str_equal,
							   (GDestroyNotify) g_free,
							   (GDestroyNotify) servicexml_plan_info_free);
		}

		PlanInfo *info = g_slice_new (PlanInfo);
		info->apn	= servicexml_current_apn;
		info->username	= servicexml_current_username;
		info->password	= servicexml_current_password;

		if (servicexml_current_plan_name == NULL)
			servicexml_current_plan_name = g_strdup ("Default");

		g_hash_table_insert (plan_info, servicexml_current_plan_name, info);

		/*Create a apn table*/
		g_free (servicexml_text_buffer);
		servicexml_text_buffer		= NULL;

		servicexml_current_plan_name	= NULL;

		servicexml_current_apn		= NULL;
		servicexml_current_username	= NULL;
		servicexml_current_password	= NULL;

		servicexml_state = PARSER_METHOD_GSM;
	}
}

static void
servicexml_cdma_end (const char *name)
{
	if (!strcmp (name, "username")) {
	} else if (!strcmp (name, "password")) {
	} else if (!strcmp (name, "dns")) {
	} else if (!strcmp (name, "gateway")) {
	} else if (!strcmp (name, "cdma")) {
		g_free (servicexml_text_buffer);
		servicexml_text_buffer = NULL;
		servicexml_state = PARSER_PROVIDER;
	}
}

void servicexml_end_element (GMarkupParseContext *context,
			const gchar         *element_name,
			gpointer             user_data,
			GError             **error)
{

	switch (servicexml_state) {
	case PARSER_COUNTRY:
		servicexml_country_end (element_name);
		break;
	case PARSER_PROVIDER:
		servicexml_provider_end (element_name);
		break;
	case PARSER_METHOD_GSM:
		servicexml_gsm_end (element_name);
		break;
	case PARSER_METHOD_GSM_APN:
		servicexml_gsm_apn_end (element_name);
		break;
	case PARSER_METHOD_CDMA:
		servicexml_cdma_end (element_name);
		break;
	default:
		break;
	}
}

void servicexml_text (GMarkupParseContext *context,
			const gchar         *text,
			gsize                text_len,
			gpointer             user_data,
			GError             **error)
{
	if (servicexml_text_buffer)
	  g_free (servicexml_text_buffer);

	servicexml_text_buffer = g_strdup (text);
}

static GMarkupParser servicexmlparser = {
	servicexml_start_element,
	servicexml_end_element,
	servicexml_text,
	NULL,
	NULL
};

/***** parse iso3166.xml *******/
static void
iso3166_start_element (GMarkupParseContext *context,
                               const gchar *element_name,
                               const gchar **attribute_names,
                               const gchar **attribute_values,
                               gpointer data,
                               GError **error)
{
	int i;
	const char *country_code = NULL;
	const char *common_name = NULL;
	const char *name = NULL;
	const char *country_name = NULL;

	if (!strcmp (element_name, "iso_3166_entry")) {
		for (i = 0; attribute_names && attribute_names[i]; i++) {
			if (!strcmp (attribute_names[i], "alpha_2_code"))
				country_code = attribute_values[i];
			else if (!strcmp (attribute_names[i], "common_name"))
				common_name = attribute_values[i];
			else if (!strcmp (attribute_names[i], "name"))
				name = attribute_values[i];
		}
		if (!country_code) {
			g_warning ("%s: missing mandatory 'alpha_2_code' atribute in '%s'"
			           " element.", __func__, element_name);
			return;
		}
		if (!name) {
			g_warning ("%s: missing mandatory 'name' atribute in '%s'"
			           " element.", __func__, element_name);
			return;
		}

		if (country_codes == NULL) {
			country_codes = g_hash_table_new_full (g_str_hash, g_str_equal,
							   (GDestroyNotify) g_free,
							   (GDestroyNotify) g_free);
		}

		country_name = dgettext ("iso_3166", common_name ? common_name : name);

		g_hash_table_insert (country_codes, g_strdup(country_name), g_strdup(country_code));
	}
}

static GMarkupParser iso3166parser = {
	iso3166_start_element,
	NULL,
	NULL,
	NULL,
	NULL
};

/***end of parser***/

gint
mobile_provider_init ()
{
	gchar *contents;
	gsize length;
	GError *error;
	GMarkupParseContext *servicexmlcontext;
	GMarkupParseContext *iso3166context;

	error = NULL;
	if (!g_file_get_contents (MOBILE_BROADBAND_PROVIDER_INFO,
				  &contents,
				  &length,
				  &error)) {
		fprintf (stderr, "%s\n", error->message);
		g_error_free (error);
		return 1;
	}

	servicexmlcontext = g_markup_parse_context_new (&servicexmlparser, 0, NULL, NULL);

	if (!g_markup_parse_context_parse (servicexmlcontext, contents, length, NULL)) {
		g_markup_parse_context_free (servicexmlcontext);
		return 1;
	}

	if (!g_markup_parse_context_end_parse (servicexmlcontext, NULL)) {
		g_markup_parse_context_free (servicexmlcontext);
		return 1;
	}

	g_markup_parse_context_free (servicexmlcontext);
	g_free (contents);

	/* parse iso3166 for the country names */
	if (!g_file_get_contents (ISO_3166_COUNTRY_CODES,
				  &contents,
				  &length,
				  &error)) {
		fprintf (stderr, "%s\n", error->message);
		g_error_free (error);
		return 1;
	}

	iso3166context = g_markup_parse_context_new (&iso3166parser, 0, NULL, NULL);

	if (!g_markup_parse_context_parse (iso3166context, contents, length, NULL)) {
		g_markup_parse_context_free (iso3166context);
		return 1;
	}

	if (!g_markup_parse_context_end_parse (iso3166context, NULL)) {
		g_markup_parse_context_free (iso3166context);
		return 1;
	}

	g_markup_parse_context_free (iso3166context);
	g_free (contents);

	return 0;
}

gint
mobile_provider_exit ()
{
	g_hash_table_destroy (country_codes);
	g_hash_table_destroy (country_info);
}

/************ ELPER FUNCTIONS FOR MOBILE PROVIDER *********/

GList *mobile_provider_get_country_list ()
{
	GList *country_list = NULL;

	country_list = g_hash_table_get_keys (country_codes);
	if (country_list == NULL)
		return NULL;

	country_list = g_list_sort (country_list, (GCompareFunc) strcmp);

	return country_list;
}

GList *mobile_provider_get_provider_list (const gchar *country_name)
{
	GList *provider_list = NULL;
	GHashTable *providers = NULL;
	gchar *country_code = NULL;

	country_code = g_hash_table_lookup (country_codes, country_name);

	providers = (GHashTable *) g_hash_table_lookup (country_info, country_code);
	if (providers == NULL)
		return NULL;

	provider_list = g_hash_table_get_keys (providers);
	if (provider_list == NULL)
		return NULL;

	return provider_list;
}

GList *mobile_provider_get_plan_list (const gchar *country_name, const gchar *provider_name)
{
	GList *plan_list = NULL;
	GHashTable *providers = NULL;
	GHashTable *plans = NULL;
	gchar *country_code = NULL;

	country_code = g_hash_table_lookup (country_codes, country_name);

	providers = (GHashTable *) g_hash_table_lookup (country_info, country_code);
	if (providers == NULL)
		return NULL;

	plans = (GHashTable *) g_hash_table_lookup (providers, provider_name);
	if (providers == NULL)
		return NULL;

	plan_list = g_hash_table_get_keys (plans);
	if (plan_list == NULL)
		return NULL;

	return plan_list;
}

PlanInfo *mobile_provider_get_plan_info (const gchar *country_name,
					 const gchar *provider_name,
					 const gchar *plan_name)
{
	PlanInfo *plan_info = NULL;

	GHashTable *providers = NULL;
	GHashTable *plans = NULL;
	gchar *country_code = NULL;

	country_code = g_hash_table_lookup (country_codes, country_name);

	providers = (GHashTable *) g_hash_table_lookup (country_info, country_code);
	if (providers == NULL)
		return NULL;

	plans = (GHashTable *) g_hash_table_lookup (providers, provider_name);
	if (providers == NULL)
		return NULL;


	plan_info = g_hash_table_lookup (plans, plan_name);
	if (plan_info == NULL)
		return NULL;

	return plan_info;
}

/************** TEST THE SERVICEXML TABLES & COUNTRY CODES ****************/
void for_each_plan (gpointer plan_name, gpointer plan_info, gpointer user_data)
{
	PlanInfo *info = plan_info;

	g_printerr ("\t\tPlan:%s\n", (gchar *) plan_name);

	if (plan_info) {
		g_printerr ("\t\t\tAPN:%s\n", info->apn);
		g_printerr ("\t\t\tUsername:%s\n", info->username);
		g_printerr ("\t\t\tPassword:%s\n\n", info->password);
	}
}

void for_each_provider (gpointer provider, gpointer plan_info, gpointer user_data)
{
	g_printerr ("\tProvider:%s\n", (gchar *) provider);

	if (plan_info)
		g_hash_table_foreach (plan_info, for_each_plan, NULL);
}

void for_each_country (gpointer country_code, gpointer provider_info, gpointer user_data)
{

	g_printerr ("\n\nCode:%s\n", (gchar *) country_code);

	if (provider_info)
	g_hash_table_foreach (provider_info, for_each_provider, NULL);
}

void print_country_codes (gpointer country_name, gpointer user_data)
{
  g_printerr ("%s : %s\n", (gchar *) country_name,  (gchar *) g_hash_table_lookup (country_codes, (gchar *) country_name));
}

void print_test (gpointer data, gpointer user_data)
{
  g_printerr ("%s\n", (gchar *) data);
}

void
mobile_provider_test()
{
	GList *country_code_list = NULL;
	GList *test = NULL;
	PlanInfo *data;
	country_code_list = g_hash_table_get_keys (country_codes);

	country_code_list = g_list_sort (country_code_list, (GCompareFunc) strcmp);

	g_printerr ("****** DATABASE OF COUNTRY CODE *******\n");

	g_list_foreach (country_code_list, print_country_codes, NULL);

	g_printerr ("****** DATABASE OF SERVICE PROVIDERS *******\n");
	g_hash_table_foreach (country_info, for_each_country, NULL);

	g_list_free_full (country_code_list, g_free);

	g_printerr ("****** TEST *******\n");

	test = mobile_provider_get_provider_list ("United Kingdom");
	g_list_foreach (test, print_test, NULL);
	g_list_free (test);

	g_printerr ("\n");
	test = mobile_provider_get_plan_list ("United Kingdom", "O2");
	g_list_foreach (test, print_test, NULL);
	g_list_free (test);

	g_printerr ("\n");
	data = mobile_provider_get_plan_info ("United Kingdom", "O2", "Pay and Go (Prepaid)");
	g_printerr ("APN:%s\n", data->apn);
	g_printerr ("UserName:%s\n", data->username);
	g_printerr ("Password:%s\n", data->password);
}
