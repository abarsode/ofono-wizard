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
 *  Authors : Alok Barsode <alok.barsode@intel.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "ofono-wizard.h"

struct _OfonoWizardPrivate {
	gchar	*modem_path;
};

#define OFONO_WIZARD_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), OFONO_TYPE_WIZARD, OfonoWizardPrivate))

static void          ofono_wizard_init                   (OfonoWizard		*ofono_wizard);
static void          ofono_wizard_class_init             (OfonoWizardClass	*klass);
static void          ofono_wizard_finalize               (GObject		*object);


G_DEFINE_TYPE (OfonoWizard, ofono_wizard, G_TYPE_OBJECT)

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

