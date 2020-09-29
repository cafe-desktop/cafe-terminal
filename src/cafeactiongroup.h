/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __CAFE_ACTION_GROUP_H__
#define __CAFE_ACTION_GROUP_H__

#include "cafeaction.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CAFE_TYPE_ACTION_GROUP              (cafe_action_group_get_type ())
#define CAFE_ACTION_GROUP(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_ACTION_GROUP, CafeActionGroup))
#define CAFE_ACTION_GROUP_CLASS(vtable)     (G_TYPE_CHECK_CLASS_CAST ((vtable), CAFE_TYPE_ACTION_GROUP, CafeActionGroupClass))
#define CAFE_IS_ACTION_GROUP(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_ACTION_GROUP))
#define CAFE_IS_ACTION_GROUP_CLASS(vtable)  (G_TYPE_CHECK_CLASS_TYPE ((vtable), CAFE_TYPE_ACTION_GROUP))
#define CAFE_ACTION_GROUP_GET_CLASS(inst)   (G_TYPE_INSTANCE_GET_CLASS ((inst), CAFE_TYPE_ACTION_GROUP, CafeActionGroupClass))

typedef struct _CafeActionGroup        CafeActionGroup;
typedef struct _CafeActionGroupPrivate CafeActionGroupPrivate;
typedef struct _CafeActionGroupClass   CafeActionGroupClass;
typedef struct _CafeActionEntry        CafeActionEntry;
typedef struct _GtkToggleActionEntry  GtkToggleActionEntry;
typedef struct _GtkRadioActionEntry   GtkRadioActionEntry;

struct _CafeActionGroup
{
  GObject parent;

  /*< private >*/
  CafeActionGroupPrivate *priv;
};

/**
 * CafeActionGroupClass:
 * @parent_class: The parent class.
 * @get_action: Looks up an action in the action group by name.
 */
struct _CafeActionGroupClass
{
  GObjectClass parent_class;

  CafeAction *(* get_action) (CafeActionGroup *action_group,
                             const gchar    *action_name);

  /*< private >*/

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

/**
 * CafeActionEntry:
 * @name: The name of the action.
 * @stock_id: The stock id for the action, or the name of an icon from the
 *  icon theme.
 * @label: The label for the action. This field should typically be marked
 *  for translation, see cafe_action_group_set_translation_domain(). If
 *  @label is %NULL, the label of the stock item with id @stock_id is used.
 * @accelerator: The accelerator for the action, in the format understood by
 *  gtk_accelerator_parse().
 * @tooltip: The tooltip for the action. This field should typically be
 *  marked for translation, see cafe_action_group_set_translation_domain().
 * @callback: The function to call when the action is activated.
 *
 * #CafeActionEntry structs are used with cafe_action_group_add_actions() to
 * construct actions.
 *
 * Deprecated: 3.10
 */
struct _CafeActionEntry 
{
  const gchar     *name;
  const gchar     *stock_id;
  const gchar     *label;
  const gchar     *accelerator;
  const gchar     *tooltip;
  GCallback  callback;
};

/**
 * GtkToggleActionEntry:
 * @name: The name of the action.
 * @stock_id: The stock id for the action, or the name of an icon from the
 *  icon theme.
 * @label: The label for the action. This field should typically be marked
 *  for translation, see cafe_action_group_set_translation_domain().
 * @accelerator: The accelerator for the action, in the format understood by
 *  gtk_accelerator_parse().
 * @tooltip: The tooltip for the action. This field should typically be
 *  marked for translation, see cafe_action_group_set_translation_domain().
 * @callback: The function to call when the action is activated.
 * @is_active: The initial state of the toggle action.
 *
 * #GtkToggleActionEntry structs are used with
 * cafe_action_group_add_toggle_actions() to construct toggle actions.
 *
 * Deprecated: 3.10
 */
struct _GtkToggleActionEntry 
{
  const gchar     *name;
  const gchar     *stock_id;
  const gchar     *label;
  const gchar     *accelerator;
  const gchar     *tooltip;
  GCallback  callback;
  gboolean   is_active;
};

/**
 * GtkRadioActionEntry:
 * @name: The name of the action.
 * @stock_id: The stock id for the action, or the name of an icon from the
 *  icon theme.
 * @label: The label for the action. This field should typically be marked
 *  for translation, see cafe_action_group_set_translation_domain().
 * @accelerator: The accelerator for the action, in the format understood by
 *  gtk_accelerator_parse().
 * @tooltip: The tooltip for the action. This field should typically be
 *  marked for translation, see cafe_action_group_set_translation_domain().
 * @value: The value to set on the radio action. See
 *  gtk_radio_action_get_current_value().
 *
 * #GtkRadioActionEntry structs are used with
 * cafe_action_group_add_radio_actions() to construct groups of radio actions.
 *
 * Deprecated: 3.10
 */
struct _GtkRadioActionEntry 
{
  const gchar *name;
  const gchar *stock_id;
  const gchar *label;
  const gchar *accelerator;
  const gchar *tooltip;
  gint   value; 
};

GDK_DEPRECATED_IN_3_10
GType           cafe_action_group_get_type                (void) G_GNUC_CONST;
GDK_DEPRECATED_IN_3_10
CafeActionGroup *cafe_action_group_new                     (const gchar                *name);
GDK_DEPRECATED_IN_3_10
const gchar    *cafe_action_group_get_name                (CafeActionGroup             *action_group);
GDK_DEPRECATED_IN_3_10
gboolean        cafe_action_group_get_sensitive           (CafeActionGroup             *action_group);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_set_sensitive           (CafeActionGroup             *action_group,
							  gboolean                    sensitive);
GDK_DEPRECATED_IN_3_10
gboolean        cafe_action_group_get_visible             (CafeActionGroup             *action_group);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_set_visible             (CafeActionGroup             *action_group,
							  gboolean                    visible);
GDK_DEPRECATED_IN_3_10
GtkAccelGroup  *cafe_action_group_get_accel_group         (CafeActionGroup             *action_group);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_set_accel_group         (CafeActionGroup             *action_group,
                                                          GtkAccelGroup              *accel_group);

GDK_DEPRECATED_IN_3_10
CafeAction      *cafe_action_group_get_action              (CafeActionGroup             *action_group,
							  const gchar                *action_name);
GDK_DEPRECATED_IN_3_10
GList          *cafe_action_group_list_actions            (CafeActionGroup             *action_group);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_action              (CafeActionGroup             *action_group,
							  CafeAction                  *action);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_action_with_accel   (CafeActionGroup             *action_group,
							  CafeAction                  *action,
							  const gchar                *accelerator);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_remove_action           (CafeActionGroup             *action_group,
							  CafeAction                  *action);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_actions             (CafeActionGroup             *action_group,
							  const CafeActionEntry       *entries,
							  guint                       n_entries,
							  gpointer                    user_data);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_toggle_actions      (CafeActionGroup             *action_group,
							  const GtkToggleActionEntry *entries,
							  guint                       n_entries,
							  gpointer                    user_data);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_radio_actions       (CafeActionGroup             *action_group,
							  const GtkRadioActionEntry  *entries,
							  guint                       n_entries,
							  gint                        value,
							  GCallback                   on_change,
							  gpointer                    user_data);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_actions_full        (CafeActionGroup             *action_group,
							  const CafeActionEntry       *entries,
							  guint                       n_entries,
							  gpointer                    user_data,
							  GDestroyNotify              destroy);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_toggle_actions_full (CafeActionGroup             *action_group,
							  const GtkToggleActionEntry *entries,
							  guint                       n_entries,
							  gpointer                    user_data,
							  GDestroyNotify              destroy);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_add_radio_actions_full  (CafeActionGroup             *action_group,
							  const GtkRadioActionEntry  *entries,
							  guint                       n_entries,
							  gint                        value,
							  GCallback                   on_change,
							  gpointer                    user_data,
							  GDestroyNotify              destroy);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_set_translate_func      (CafeActionGroup             *action_group,
							  GtkTranslateFunc            func,
							  gpointer                    data,
							  GDestroyNotify              notify);
GDK_DEPRECATED_IN_3_10
void            cafe_action_group_set_translation_domain  (CafeActionGroup             *action_group,
							  const gchar                *domain);
GDK_DEPRECATED_IN_3_10
const gchar *   cafe_action_group_translate_string        (CafeActionGroup             *action_group,
  	                                                  const gchar                *string);

/* Protected for use by CafeAction */
void _cafe_action_group_emit_connect_proxy    (CafeActionGroup *action_group,
                                              CafeAction      *action,
                                              GtkWidget      *proxy);
void _cafe_action_group_emit_disconnect_proxy (CafeActionGroup *action_group,
                                              CafeAction      *action,
                                              GtkWidget      *proxy);
void _cafe_action_group_emit_pre_activate     (CafeActionGroup *action_group,
                                              CafeAction      *action);
void _cafe_action_group_emit_post_activate    (CafeActionGroup *action_group,
                                              CafeAction      *action);

G_END_DECLS

#endif  /* __CAFE_ACTION_GROUP_H__ */

