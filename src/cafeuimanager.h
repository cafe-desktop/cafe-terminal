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

#ifndef __CAFE_UI_MANAGER_H__
#define __CAFE_UI_MANAGER_H__

#include <gtk/gtk.h>
#include "cafeaction.h"
#include "cafeactiongroup.h"

G_BEGIN_DECLS

#define CAFE_TYPE_UI_MANAGER            (cafe_ui_manager_get_type ())
#define CAFE_UI_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_UI_MANAGER, CafeUIManager))
#define CAFE_UI_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAFE_TYPE_UI_MANAGER, CafeUIManagerClass))
#define CAFE_IS_UI_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_UI_MANAGER))
#define CAFE_IS_UI_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAFE_TYPE_UI_MANAGER))
#define CAFE_UI_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAFE_TYPE_UI_MANAGER, CafeUIManagerClass))

typedef struct _CafeUIManager      CafeUIManager;
typedef struct _CafeUIManagerClass CafeUIManagerClass;
typedef struct _CafeUIManagerPrivate CafeUIManagerPrivate;


struct _CafeUIManager {
  GObject parent;

  /*< private >*/
  CafeUIManagerPrivate *private_data;
};

struct _CafeUIManagerClass {
  GObjectClass parent_class;

  /* Signals */
  void (* add_widget)       (CafeUIManager *manager,
                             GtkWidget    *widget);
  void (* actions_changed)  (CafeUIManager *manager);
  void (* connect_proxy)    (CafeUIManager *manager,
			     CafeAction    *action,
			     GtkWidget    *proxy);
  void (* disconnect_proxy) (CafeUIManager *manager,
			     CafeAction    *action,
			     GtkWidget    *proxy);
  void (* pre_activate)     (CafeUIManager *manager,
			     CafeAction    *action);
  void (* post_activate)    (CafeUIManager *manager,
			     CafeAction    *action);

  /* Virtual functions */
  GtkWidget * (* get_widget) (CafeUIManager *manager,
                              const gchar  *path);
  CafeAction * (* get_action) (CafeUIManager *manager,
                              const gchar  *path);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

/**
 * CafeUIManagerItemType:
 * @CAFE_UI_MANAGER_AUTO: Pick the type of the UI element according to context.
 * @CAFE_UI_MANAGER_MENUBAR: Create a menubar.
 * @CAFE_UI_MANAGER_MENU: Create a menu.
 * @CAFE_UI_MANAGER_TOOLBAR: Create a toolbar.
 * @CAFE_UI_MANAGER_PLACEHOLDER: Insert a placeholder.
 * @CAFE_UI_MANAGER_POPUP: Create a popup menu.
 * @CAFE_UI_MANAGER_MENUITEM: Create a menuitem.
 * @CAFE_UI_MANAGER_TOOLITEM: Create a toolitem.
 * @CAFE_UI_MANAGER_SEPARATOR: Create a separator.
 * @CAFE_UI_MANAGER_ACCELERATOR: Install an accelerator.
 * @CAFE_UI_MANAGER_POPUP_WITH_ACCELS: Same as %CAFE_UI_MANAGER_POPUP, but the
 *   actions’ accelerators are shown.
 *
 * These enumeration values are used by cafe_ui_manager_add_ui() to determine
 * what UI element to create.
 *
 * Deprecated: 3.10
 */
typedef enum {
  CAFE_UI_MANAGER_AUTO              = 0,
  CAFE_UI_MANAGER_MENUBAR           = 1 << 0,
  CAFE_UI_MANAGER_MENU              = 1 << 1,
  CAFE_UI_MANAGER_TOOLBAR           = 1 << 2,
  CAFE_UI_MANAGER_PLACEHOLDER       = 1 << 3,
  CAFE_UI_MANAGER_POPUP             = 1 << 4,
  CAFE_UI_MANAGER_MENUITEM          = 1 << 5,
  CAFE_UI_MANAGER_TOOLITEM          = 1 << 6,
  CAFE_UI_MANAGER_SEPARATOR         = 1 << 7,
  CAFE_UI_MANAGER_ACCELERATOR       = 1 << 8,
  CAFE_UI_MANAGER_POPUP_WITH_ACCELS = 1 << 9
} CafeUIManagerItemType;

GType          cafe_ui_manager_get_type            (void) G_GNUC_CONST;
CafeUIManager  *cafe_ui_manager_new                 (void);
GDK_DEPRECATED_IN_3_4
void           cafe_ui_manager_set_add_tearoffs    (CafeUIManager          *manager,
                                                   gboolean               add_tearoffs);
GDK_DEPRECATED_IN_3_4
gboolean       cafe_ui_manager_get_add_tearoffs    (CafeUIManager          *manager);

void           cafe_ui_manager_insert_action_group (CafeUIManager          *manager,
						   CafeActionGroup        *action_group,
						   gint                   pos);
void           cafe_ui_manager_remove_action_group (CafeUIManager          *manager,
						   CafeActionGroup        *action_group);
GList         *cafe_ui_manager_get_action_groups   (CafeUIManager          *manager);
GtkAccelGroup *cafe_ui_manager_get_accel_group     (CafeUIManager          *manager);
GtkWidget     *cafe_ui_manager_get_widget          (CafeUIManager          *manager,
						   const gchar           *path);
GSList        *cafe_ui_manager_get_toplevels       (CafeUIManager          *manager,
						   CafeUIManagerItemType   types);
CafeAction     *cafe_ui_manager_get_action          (CafeUIManager          *manager,
						   const gchar           *path);
guint          cafe_ui_manager_add_ui_from_string  (CafeUIManager          *manager,
						   const gchar           *buffer,
						   gssize                 length,
						   GError               **error);
guint          cafe_ui_manager_add_ui_from_file    (CafeUIManager          *manager,
						   const gchar           *filename,
						   GError               **error);
guint          cafe_ui_manager_add_ui_from_resource(CafeUIManager          *manager,
						   const gchar           *resource_path,
						   GError               **error);
void           cafe_ui_manager_add_ui              (CafeUIManager          *manager,
						   guint                  merge_id,
						   const gchar           *path,
						   const gchar           *name,
						   const gchar           *action,
						   CafeUIManagerItemType   type,
						   gboolean               top);
void           cafe_ui_manager_remove_ui           (CafeUIManager          *manager,
						   guint                  merge_id);
gchar         *cafe_ui_manager_get_ui              (CafeUIManager          *manager);
void           cafe_ui_manager_ensure_update       (CafeUIManager          *manager);
guint          cafe_ui_manager_new_merge_id        (CafeUIManager          *manager);

G_END_DECLS

#endif /* __CAFE_UI_MANAGER_H__ */

