/* GTK - The GIMP Toolkit
 * Copyright (C) Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __CAFE_IMAGE_MENU_ITEM_H__
#define __CAFE_IMAGE_MENU_ITEM_H__


#include <gtk/gtk.h>


G_BEGIN_DECLS

#define CAFE_TYPE_IMAGE_MENU_ITEM            (cafe_image_menu_item_get_type ())
#define CAFE_IMAGE_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_IMAGE_MENU_ITEM, CafeImageMenuItem))
#define CAFE_IMAGE_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAFE_TYPE_IMAGE_MENU_ITEM, CafeImageMenuItemClass))
#define CAFE_IS_IMAGE_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_IMAGE_MENU_ITEM))
#define CAFE_IS_IMAGE_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAFE_TYPE_IMAGE_MENU_ITEM))
#define CAFE_IMAGE_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAFE_TYPE_IMAGE_MENU_ITEM, CafeImageMenuItemClass))


typedef struct _CafeImageMenuItem              CafeImageMenuItem;
typedef struct _CafeImageMenuItemPrivate       CafeImageMenuItemPrivate;
typedef struct _CafeImageMenuItemClass         CafeImageMenuItemClass;

struct _CafeImageMenuItem
{
  GtkMenuItem menu_item;

  /*< private >*/
  CafeImageMenuItemPrivate *priv;
};

/**
 * CafeImageMenuItemClass:
 * @parent_class: The parent class.
 */
struct _CafeImageMenuItemClass
{
  GtkMenuItemClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType	   cafe_image_menu_item_get_type          (void) G_GNUC_CONST;
GtkWidget* cafe_image_menu_item_new               (void);
GtkWidget* cafe_image_menu_item_new_with_label    (const gchar      *label);
GtkWidget* cafe_image_menu_item_new_with_mnemonic (const gchar      *label);
GtkWidget* cafe_image_menu_item_new_from_stock    (const gchar      *stock_id,
                                                  GtkAccelGroup    *accel_group);
void       cafe_image_menu_item_set_always_show_image (CafeImageMenuItem *image_menu_item,
                                                      gboolean          always_show);
gboolean   cafe_image_menu_item_get_always_show_image (CafeImageMenuItem *image_menu_item);
void       cafe_image_menu_item_set_image         (CafeImageMenuItem *image_menu_item,
                                                  GtkWidget        *image);
GtkWidget* cafe_image_menu_item_get_image         (CafeImageMenuItem *image_menu_item);
void       cafe_image_menu_item_set_use_stock     (CafeImageMenuItem *image_menu_item,
						  gboolean          use_stock);
gboolean   cafe_image_menu_item_get_use_stock     (CafeImageMenuItem *image_menu_item);
void       cafe_image_menu_item_set_accel_group   (CafeImageMenuItem *image_menu_item, 
						  GtkAccelGroup    *accel_group);

G_END_DECLS

#endif /* __CAFE_IMAGE_MENU_ITEM_H__ */

