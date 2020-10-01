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

#ifndef __CAFE_TOGGLE_ACTION_H__
#define __CAFE_TOGGLE_ACTION_H__

#include "cafeaction.h"

G_BEGIN_DECLS

#define CAFE_TYPE_TOGGLE_ACTION            (cafe_toggle_action_get_type ())
#define CAFE_TOGGLE_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_TOGGLE_ACTION, CafeToggleAction))
#define CAFE_TOGGLE_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAFE_TYPE_TOGGLE_ACTION, CafeToggleActionClass))
#define CAFE_IS_TOGGLE_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_TOGGLE_ACTION))
#define CAFE_IS_TOGGLE_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAFE_TYPE_TOGGLE_ACTION))
#define CAFE_TOGGLE_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAFE_TYPE_TOGGLE_ACTION, CafeToggleActionClass))

typedef struct _CafeToggleAction        CafeToggleAction;
typedef struct _CafeToggleActionPrivate CafeToggleActionPrivate;
typedef struct _CafeToggleActionClass   CafeToggleActionClass;

struct _CafeToggleAction
{
  CafeAction parent;

  /*< private >*/
  CafeToggleActionPrivate *private_data;
};

struct _CafeToggleActionClass
{
  CafeActionClass parent_class;

  void (* toggled) (CafeToggleAction *action);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType            cafe_toggle_action_get_type          (void) G_GNUC_CONST;
CafeToggleAction *cafe_toggle_action_new               (const gchar     *name,
                                                      const gchar     *label,
                                                      const gchar     *tooltip,
                                                      const gchar     *stock_id);
void             cafe_toggle_action_toggled           (CafeToggleAction *action);
void             cafe_toggle_action_set_active        (CafeToggleAction *action,
                                                      gboolean         is_active);
gboolean         cafe_toggle_action_get_active        (CafeToggleAction *action);
void             cafe_toggle_action_set_draw_as_radio (CafeToggleAction *action,
                                                      gboolean         draw_as_radio);
gboolean         cafe_toggle_action_get_draw_as_radio (CafeToggleAction *action);

/* private */
void             _cafe_toggle_action_set_active       (CafeToggleAction *toggle_action,
                                                      gboolean         is_active);


G_END_DECLS

#endif  /* __CAFE_TOGGLE_ACTION_H__ */

