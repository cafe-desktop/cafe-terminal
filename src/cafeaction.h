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

#ifndef __CAFE_ACTION_H__
#define __CAFE_ACTION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACTION            (gtk_action_get_type ())
#define CAFE_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ACTION, CafeAction))
#define CAFE_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ACTION, CafeActionClass))
#define GTK_IS_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ACTION))
#define GTK_IS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ACTION))
#define CAFE_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_ACTION, CafeActionClass))

typedef struct _CafeAction      CafeAction;
typedef struct _CafeActionClass CafeActionClass;
typedef struct _CafeActionPrivate CafeActionPrivate;

struct _CafeAction
{
  GObject object;

  /*< private >*/
  CafeActionPrivate *private_data;
};

/**
 * CafeActionClass:
 * @parent_class: The parent class.
 * @activate: Signal emitted when the action is activated.
 */
struct _CafeActionClass
{
  GObjectClass parent_class;

  /*< public >*/

  /* activation signal */
  void       (* activate)           (CafeAction    *action);

  /*< private >*/

  GType      menu_item_type;
  GType      toolbar_item_type;

  /* widget creation routines (not signals) */
  GtkWidget *(* create_menu_item)   (CafeAction *action);
  GtkWidget *(* create_tool_item)   (CafeAction *action);
  void       (* connect_proxy)      (CafeAction *action,
				     GtkWidget *proxy);
  void       (* disconnect_proxy)   (CafeAction *action,
				     GtkWidget *proxy);

  GtkWidget *(* create_menu)        (CafeAction *action);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GDK_DEPRECATED_IN_3_10
GType        gtk_action_get_type               (void) G_GNUC_CONST;
GDK_DEPRECATED_IN_3_10
CafeAction   *gtk_action_new                    (const gchar *name,
						const gchar *label,
						const gchar *tooltip,
						const gchar *stock_id);
GDK_DEPRECATED_IN_3_10
const gchar* gtk_action_get_name               (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
gboolean     gtk_action_is_sensitive           (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
gboolean     gtk_action_get_sensitive          (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
void         gtk_action_set_sensitive          (CafeAction     *action,
						gboolean       sensitive);
GDK_DEPRECATED_IN_3_10
gboolean     gtk_action_is_visible             (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
gboolean     gtk_action_get_visible            (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
void         gtk_action_set_visible            (CafeAction     *action,
						gboolean       visible);
GDK_DEPRECATED_IN_3_10
void         gtk_action_activate               (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
GtkWidget *  gtk_action_create_icon            (CafeAction     *action,
						GtkIconSize    icon_size);
GDK_DEPRECATED_IN_3_10
GtkWidget *  gtk_action_create_menu_item       (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
GtkWidget *  gtk_action_create_tool_item       (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
GtkWidget *  gtk_action_create_menu            (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
GSList *     gtk_action_get_proxies            (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
void         gtk_action_connect_accelerator    (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
void         gtk_action_disconnect_accelerator (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
const gchar *gtk_action_get_accel_path         (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
GClosure    *gtk_action_get_accel_closure      (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
void         gtk_action_block_activate         (CafeAction     *action);
GDK_DEPRECATED_IN_3_10
void         gtk_action_unblock_activate       (CafeAction     *action);

void         _gtk_action_add_to_proxy_list     (CafeAction     *action,
						GtkWidget     *proxy);
void         _gtk_action_remove_from_proxy_list(CafeAction     *action,
						GtkWidget     *proxy);

/* protected ... for use by child actions */
void         _gtk_action_emit_activate         (CafeAction     *action);

/* protected ... for use by action groups */
GDK_DEPRECATED_IN_3_10
void         gtk_action_set_accel_path         (CafeAction     *action,
						const gchar   *accel_path);
GDK_DEPRECATED_IN_3_10
void         gtk_action_set_accel_group        (CafeAction     *action,
						GtkAccelGroup *accel_group);
void         _gtk_action_sync_menu_visible     (CafeAction     *action,
						GtkWidget     *proxy,
						gboolean       empty);

GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_label              (CafeAction   *action,
                                                         const gchar *label);
GDK_DEPRECATED_IN_3_10
const gchar *         gtk_action_get_label              (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_short_label        (CafeAction   *action,
                                                         const gchar *short_label);
GDK_DEPRECATED_IN_3_10
const gchar *         gtk_action_get_short_label        (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_tooltip            (CafeAction   *action,
                                                         const gchar *tooltip);
GDK_DEPRECATED_IN_3_10
const gchar *         gtk_action_get_tooltip            (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_stock_id           (CafeAction   *action,
                                                         const gchar *stock_id);
GDK_DEPRECATED_IN_3_10
const gchar *         gtk_action_get_stock_id           (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_gicon              (CafeAction   *action,
                                                         GIcon       *icon);
GDK_DEPRECATED_IN_3_10
GIcon                *gtk_action_get_gicon              (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_icon_name          (CafeAction   *action,
                                                         const gchar *icon_name);
GDK_DEPRECATED_IN_3_10
const gchar *         gtk_action_get_icon_name          (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_visible_horizontal (CafeAction   *action,
                                                         gboolean     visible_horizontal);
GDK_DEPRECATED_IN_3_10
gboolean              gtk_action_get_visible_horizontal (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_visible_vertical   (CafeAction   *action,
                                                         gboolean     visible_vertical);
GDK_DEPRECATED_IN_3_10
gboolean              gtk_action_get_visible_vertical   (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_is_important       (CafeAction   *action,
                                                         gboolean     is_important);
GDK_DEPRECATED_IN_3_10
gboolean              gtk_action_get_is_important       (CafeAction   *action);
GDK_DEPRECATED_IN_3_10
void                  gtk_action_set_always_show_image  (CafeAction   *action,
                                                         gboolean     always_show);
GDK_DEPRECATED_IN_3_10
gboolean              gtk_action_get_always_show_image  (CafeAction   *action);


G_END_DECLS

#endif  /* __CAFE_ACTION_H__ */

