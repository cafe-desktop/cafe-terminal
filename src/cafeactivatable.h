/* GTK - The GIMP Toolkit
 * Copyright (C) 2008 Tristan Van Berkom <tristan.van.berkom@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CAFE_ACTIVATABLE_H__
#define __CAFE_ACTIVATABLE_H__

#include "cafeaction.h"

G_BEGIN_DECLS

#define CAFE_TYPE_ACTIVATABLE            (cafe_activatable_get_type ())
#define CAFE_ACTIVATABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_ACTIVATABLE, CafeActivatable))
#define CAFE_ACTIVATABLE_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST ((obj), CAFE_TYPE_ACTIVATABLE, CafeActivatableIface))
#define CAFE_IS_ACTIVATABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_ACTIVATABLE))
#define CAFE_ACTIVATABLE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAFE_TYPE_ACTIVATABLE, CafeActivatableIface))


typedef struct _CafeActivatable      CafeActivatable; /* Dummy typedef */
typedef struct _CafeActivatableIface CafeActivatableIface;


/**
 * CafeActivatableIface:
 * @update: Called to update the activatable when its related action’s properties change.
 * You must check the #CafeActivatable:use-action-appearance property only apply action
 * properties that are meant to effect the appearance accordingly.
 * @sync_action_properties: Called to update the activatable completely, this is called internally when
 * #CafeActivatable:related-action property is set or unset and by the implementor when
 * #CafeActivatable:use-action-appearance changes.
 *
 * > This method can be called with a %NULL action at times.
 *
 * Since: 2.16
 *
 * Deprecated: 3.10
 */

struct _CafeActivatableIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/

  /* virtual table */
  void   (* update)                   (CafeActivatable *activatable,
		                       CafeAction      *action,
		                       const gchar    *property_name);
  void   (* sync_action_properties)   (CafeActivatable *activatable,
		                       CafeAction      *action);
};


GType      cafe_activatable_get_type                   (void) G_GNUC_CONST;

void       cafe_activatable_sync_action_properties     (CafeActivatable *activatable,
						       CafeAction      *action);

void       cafe_activatable_set_related_action         (CafeActivatable *activatable,
						       CafeAction      *action);
CafeAction *cafe_activatable_get_related_action         (CafeActivatable *activatable);

void       cafe_activatable_set_use_action_appearance  (CafeActivatable *activatable,
						       gboolean        use_appearance);
gboolean   cafe_activatable_get_use_action_appearance  (CafeActivatable *activatable);

/* For use in activatable implementations */
void       cafe_activatable_do_set_related_action      (CafeActivatable *activatable,
						       CafeAction      *action);

G_END_DECLS

#endif /* __CAFE_ACTIVATABLE_H__ */

