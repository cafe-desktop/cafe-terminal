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

#ifndef __CAFE_RADIO_ACTION_H__
#define __CAFE_RADIO_ACTION_H__

#include "cafetoggleaction.h"

G_BEGIN_DECLS

#define CAFE_TYPE_RADIO_ACTION            (cafe_radio_action_get_type ())
#define CAFE_RADIO_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_RADIO_ACTION, CafeRadioAction))
#define CAFE_RADIO_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAFE_TYPE_RADIO_ACTION, CafeRadioActionClass))
#define CAFE_IS_RADIO_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_RADIO_ACTION))
#define CAFE_IS_RADIO_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAFE_TYPE_RADIO_ACTION))
#define CAFE_RADIO_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAFE_TYPE_RADIO_ACTION, CafeRadioActionClass))

typedef struct _CafeRadioAction        CafeRadioAction;
typedef struct _CafeRadioActionPrivate CafeRadioActionPrivate;
typedef struct _CafeRadioActionClass   CafeRadioActionClass;

struct _CafeRadioAction
{
  CafeToggleAction parent;

  /*< private >*/
  CafeRadioActionPrivate *private_data;
};

struct _CafeRadioActionClass
{
  CafeToggleActionClass parent_class;

  void       (* changed) (CafeRadioAction *action, CafeRadioAction *current);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType           cafe_radio_action_get_type          (void) G_GNUC_CONST;
CafeRadioAction *cafe_radio_action_new               (const gchar           *name,
                                                    const gchar           *label,
                                                    const gchar           *tooltip,
                                                    const gchar           *stock_id,
                                                    gint                   value);
GSList         *cafe_radio_action_get_group         (CafeRadioAction        *action);
void            cafe_radio_action_set_group         (CafeRadioAction        *action,
                                                    GSList                *group);
void            cafe_radio_action_join_group        (CafeRadioAction        *action,
                                                    CafeRadioAction        *group_source);
gint            cafe_radio_action_get_current_value (CafeRadioAction        *action);
void            cafe_radio_action_set_current_value (CafeRadioAction        *action,
                                                    gint                   current_value);

G_END_DECLS

#endif  /* __CAFE_RADIO_ACTION_H__ */

