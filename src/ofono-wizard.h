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

#ifndef OFONO_WIZARD_H
#define OFONO_WIZARD_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _OfonoWizard        OfonoWizard;
typedef struct _OfonoWizardClass   OfonoWizardClass;
typedef struct _OfonoWizardPrivate OfonoWizardPrivate;

#define OFONO_TYPE_WIZARD               (ofono_wizard_get_type())
#define OFONO_WIZARD(object)            (G_TYPE_CHECK_INSTANCE_CAST((object), OFONO_TYPE_WIZARD, OfonoWizard))
#define OFONO_IS_WIZARD(object)         (G_TYPE_CHECK_INSTANCE_TYPE((object), OFONO_TYPE_WIZARD))
#define OFONO_WIZARD_CLASS(klass)     	(G_TYPE_CHACK_CLASS_CAST((klass), OFONO_TYPE_WIZARD, OfonoWizard))
#define OFONO_IS_WIZARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass), OFONO_TYPE_WIZARD))
#define OFONO_WIZARD_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS((object), OFONO_TYPE_WIZARD, OfonoWizard))

struct _OfonoWizard {
	GObject      parent;
	OfonoWizardPrivate *priv;
};

struct _OfonoWizardClass {
	GObjectClass  parent_class;
};

GType ofono_wizard_get_type (void) G_GNUC_CONST;

OfonoWizard  *ofono_wizard_new (void);

G_END_DECLS

#endif /* !OFONO_WIZARD_H */
