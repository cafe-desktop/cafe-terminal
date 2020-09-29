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

#define CAFE_TYPE_ACTION            (cafe_action_get_type ())
#define CAFE_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_ACTION, CafeAction))
#define CAFE_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAFE_TYPE_ACTION, CafeActionClass))
#define CAFE_IS_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_ACTION))
#define CAFE_IS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAFE_TYPE_ACTION))
#define CAFE_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAFE_TYPE_ACTION, CafeActionClass))

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

GType        cafe_action_get_type               (void) G_GNUC_CONST;
CafeAction   *cafe_action_new                    (const gchar *name,
						const gchar *label,
						const gchar *tooltip,
						const gchar *stock_id);
const gchar* cafe_action_get_name               (CafeAction     *action);
gboolean     cafe_action_is_sensitive           (CafeAction     *action);
gboolean     cafe_action_get_sensitive          (CafeAction     *action);
void         cafe_action_set_sensitive          (CafeAction     *action,
						gboolean       sensitive);
gboolean     cafe_action_is_visible             (CafeAction     *action);
gboolean     cafe_action_get_visible            (CafeAction     *action);
void         cafe_action_set_visible            (CafeAction     *action,
						gboolean       visible);
void         cafe_action_activate               (CafeAction     *action);
GtkWidget *  cafe_action_create_icon            (CafeAction     *action,
						GtkIconSize    icon_size);
GtkWidget *  cafe_action_create_menu_item       (CafeAction     *action);
GtkWidget *  cafe_action_create_tool_item       (CafeAction     *action);
GtkWidget *  cafe_action_create_menu            (CafeAction     *action);
GSList *     cafe_action_get_proxies            (CafeAction     *action);
void         cafe_action_connect_accelerator    (CafeAction     *action);
void         cafe_action_disconnect_accelerator (CafeAction     *action);
const gchar *cafe_action_get_accel_path         (CafeAction     *action);
GClosure    *cafe_action_get_accel_closure      (CafeAction     *action);
void         cafe_action_block_activate         (CafeAction     *action);
void         cafe_action_unblock_activate       (CafeAction     *action);

void         _cafe_action_add_to_proxy_list     (CafeAction     *action,
						GtkWidget     *proxy);
void         _cafe_action_remove_from_proxy_list(CafeAction     *action,
						GtkWidget     *proxy);

/* protected ... for use by child actions */
void         _cafe_action_emit_activate         (CafeAction     *action);

/* protected ... for use by action groups */
void         cafe_action_set_accel_path         (CafeAction     *action,
						const gchar   *accel_path);
void         cafe_action_set_accel_group        (CafeAction     *action,
						GtkAccelGroup *accel_group);
void         _cafe_action_sync_menu_visible     (CafeAction     *action,
						GtkWidget     *proxy,
						gboolean       empty);

void                  cafe_action_set_label              (CafeAction   *action,
                                                         const gchar *label);
const gchar *         cafe_action_get_label              (CafeAction   *action);
void                  cafe_action_set_short_label        (CafeAction   *action,
                                                         const gchar *short_label);
const gchar *         cafe_action_get_short_label        (CafeAction   *action);
void                  cafe_action_set_tooltip            (CafeAction   *action,
                                                         const gchar *tooltip);
const gchar *         cafe_action_get_tooltip            (CafeAction   *action);
void                  cafe_action_set_stock_id           (CafeAction   *action,
                                                         const gchar *stock_id);
const gchar *         cafe_action_get_stock_id           (CafeAction   *action);
void                  cafe_action_set_gicon              (CafeAction   *action,
                                                         GIcon       *icon);
GIcon                *cafe_action_get_gicon              (CafeAction   *action);
void                  cafe_action_set_icon_name          (CafeAction   *action,
                                                         const gchar *icon_name);
const gchar *         cafe_action_get_icon_name          (CafeAction   *action);
void                  cafe_action_set_visible_horizontal (CafeAction   *action,
                                                         gboolean     visible_horizontal);
gboolean              cafe_action_get_visible_horizontal (CafeAction   *action);
void                  cafe_action_set_visible_vertical   (CafeAction   *action,
                                                         gboolean     visible_vertical);
gboolean              cafe_action_get_visible_vertical   (CafeAction   *action);
void                  cafe_action_set_is_important       (CafeAction   *action,
                                                         gboolean     is_important);
gboolean              cafe_action_get_is_important       (CafeAction   *action);
void                  cafe_action_set_always_show_image  (CafeAction   *action,
                                                         gboolean     always_show);
gboolean              cafe_action_get_always_show_image  (CafeAction   *action);


G_END_DECLS

#endif  /* __CAFE_ACTION_H__ */

