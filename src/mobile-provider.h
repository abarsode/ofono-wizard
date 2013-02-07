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

#ifndef MOBILE_PROVIDER_H
#define MOBILE_PROVIDER_H

typedef struct _PlanInfo
{
  char *apn;
  char *username;
  char *password;
} PlanInfo;

GList *mobile_provider_get_country_list ();
GList *mobile_provider_get_provider_list (const gchar *country_name);
GList *mobile_provider_get_plan_list (const gchar *country_name, const gchar *provider_name);

PlanInfo *mobile_provider_get_plan_info (const gchar *country_name,
					 const gchar *provider_name,
					 const gchar *plan_name);

gchar *mobile_provider_get_country_from_code (gchar *code);

#endif /* MOBILE_PROVIDER_H*/
