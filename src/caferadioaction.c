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

#include "config.h"

#include <gtk/gtk.h>

#include "caferadioaction.h"

/**
 * SECTION:gtkradioaction
 * @Short_description: An action of which only one in a group can be active
 * @Title: CafeRadioAction
 *
 * A #CafeRadioAction is similar to #GtkRadioMenuItem. A number of radio
 * actions can be linked together so that only one may be active at any
 * one time.
 */


struct _CafeRadioActionPrivate 
{
  GSList *group;
  gint    value;
};

enum 
{
  CHANGED,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_VALUE,
  PROP_GROUP,
  PROP_CURRENT_VALUE
};

static void cafe_radio_action_finalize     (GObject *object);
static void cafe_radio_action_set_property (GObject         *object,
				           guint            prop_id,
				           const GValue    *value,
				           GParamSpec      *pspec);
static void cafe_radio_action_get_property (GObject         *object,
				           guint            prop_id,
				           GValue          *value,
				           GParamSpec      *pspec);
static void cafe_radio_action_activate     (CafeAction *action);
static GtkWidget *create_menu_item        (CafeAction *action);


G_DEFINE_TYPE_WITH_PRIVATE (CafeRadioAction, cafe_radio_action, CAFE_TYPE_TOGGLE_ACTION)

static guint         radio_action_signals[LAST_SIGNAL] = { 0 };

static void
cafe_radio_action_class_init (CafeRadioActionClass *klass)
{
  GObjectClass *gobject_class;
  CafeActionClass *action_class;

  gobject_class = G_OBJECT_CLASS (klass);
  action_class = CAFE_ACTION_CLASS (klass);

  gobject_class->finalize = cafe_radio_action_finalize;
  gobject_class->set_property = cafe_radio_action_set_property;
  gobject_class->get_property = cafe_radio_action_get_property;

  action_class->activate = cafe_radio_action_activate;

  action_class->create_menu_item = create_menu_item;

  /**
   * CafeRadioAction:value:
   *
   * The value is an arbitrary integer which can be used as a
   * convenient way to determine which action in the group is 
   * currently active in an ::activate or ::changed signal handler.
   * See cafe_radio_action_get_current_value() and #CafeRadioActionEntry
   * for convenient ways to get and set this property.
   *
   * Since: 2.4
   *
   * Deprecated: 3.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_VALUE,
				   g_param_spec_int ("value",
						     "The value",
						     "The value returned by cafe_radio_action_get_current_value() when this action is the current action of its group.",
						     G_MININT,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE));

  /**
   * CafeRadioAction:group:
   *
   * Sets a new group for a radio action.
   *
   * Since: 2.4
   *
   * Deprecated: 3.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							"Group",
							"The radio action whose group this action belongs to.",
							CAFE_TYPE_RADIO_ACTION,
							G_PARAM_WRITABLE));

  /**
   * CafeRadioAction:current-value:
   *
   * The value property of the currently active member of the group to which
   * this action belongs. 
   *
   * Since: 2.10
   *
   * Deprecated: 3.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_CURRENT_VALUE,
                                   g_param_spec_int ("current-value",
						     "The current value",
						     "The value property of the currently active member of the group to which this action belongs.",
						     G_MININT,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE));

  /**
   * CafeRadioAction::changed:
   * @action: the action on which the signal is emitted
   * @current: the member of @action's group which has just been activated
   *
   * The ::changed signal is emitted on every member of a radio group when the
   * active member is changed. The signal gets emitted after the ::activate signals
   * for the previous and current active members.
   *
   * Since: 2.4
   *
   * Deprecated: 3.10
   */
  radio_action_signals[CHANGED] =
    g_signal_new ("changed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (CafeRadioActionClass, changed),  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1, CAFE_TYPE_RADIO_ACTION);
}

static void
cafe_radio_action_init (CafeRadioAction *action)
{
  action->private_data = cafe_radio_action_get_instance_private (action);
  action->private_data->group = g_slist_prepend (NULL, action);
  action->private_data->value = 0;

  cafe_toggle_action_set_draw_as_radio (CAFE_TOGGLE_ACTION (action), TRUE);
}

/**
 * cafe_radio_action_new:
 * @name: A unique name for the action
 * @label: (allow-none): The label displayed in menu items and on buttons,
 *   or %NULL
 * @tooltip: (allow-none): A tooltip for this action, or %NULL
 * @stock_id: (allow-none): The stock icon to display in widgets representing
 *   this action, or %NULL
 * @value: The value which cafe_radio_action_get_current_value() should
 *   return if this action is selected.
 *
 * Creates a new #CafeRadioAction object. To add the action to
 * a #CafeActionGroup and set the accelerator for the action,
 * call cafe_action_group_add_action_with_accel().
 *
 * Returns: a new #CafeRadioAction
 *
 * Since: 2.4
 *
 * Deprecated: 3.10
 */
CafeRadioAction *
cafe_radio_action_new (const gchar *name,
		      const gchar *label,
		      const gchar *tooltip,
		      const gchar *stock_id,
		      gint value)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (CAFE_TYPE_RADIO_ACTION,
                       "name", name,
                       "label", label,
                       "tooltip", tooltip,
                       "stock-id", stock_id,
                       "value", value,
                       NULL);
}

static void
cafe_radio_action_finalize (GObject *object)
{
  CafeRadioAction *action;
  GSList *tmp_list;

  action = CAFE_RADIO_ACTION (object);

  action->private_data->group = g_slist_remove (action->private_data->group, action);

  tmp_list = action->private_data->group;

  while (tmp_list)
    {
      CafeRadioAction *tmp_action = tmp_list->data;

      tmp_list = tmp_list->next;
      tmp_action->private_data->group = action->private_data->group;
    }

  G_OBJECT_CLASS (cafe_radio_action_parent_class)->finalize (object);
}

static void
cafe_radio_action_set_property (GObject         *object,
			       guint            prop_id,
			       const GValue    *value,
			       GParamSpec      *pspec)
{
  CafeRadioAction *radio_action;
  
  radio_action = CAFE_RADIO_ACTION (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      radio_action->private_data->value = g_value_get_int (value);
      break;
    case PROP_GROUP: 
      {
	CafeRadioAction *arg;
	GSList *slist = NULL;
	
	if (G_VALUE_HOLDS_OBJECT (value)) 
	  {
	    arg = CAFE_RADIO_ACTION (g_value_get_object (value));
	    if (arg)
	      slist = cafe_radio_action_get_group (arg);
	    cafe_radio_action_set_group (radio_action, slist);
	  }
      }
      break;
    case PROP_CURRENT_VALUE:
      cafe_radio_action_set_current_value (radio_action,
                                          g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cafe_radio_action_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  CafeRadioAction *radio_action;

  radio_action = CAFE_RADIO_ACTION (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_int (value, radio_action->private_data->value);
      break;
    case PROP_CURRENT_VALUE:
      g_value_set_int (value,
                       cafe_radio_action_get_current_value (radio_action));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cafe_radio_action_activate (CafeAction *action)
{
  CafeRadioAction *radio_action;
  CafeToggleAction *toggle_action;
  CafeToggleAction *tmp_action;
  GSList *tmp_list;
  gboolean active;

  radio_action = CAFE_RADIO_ACTION (action);
  toggle_action = CAFE_TOGGLE_ACTION (action);

  active = cafe_toggle_action_get_active (toggle_action);
  if (active)
    {
      tmp_list = radio_action->private_data->group;

      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (cafe_toggle_action_get_active (tmp_action) &&
              (tmp_action != toggle_action))
	    {
              _cafe_toggle_action_set_active (toggle_action, !active);

	      break;
	    }
	}
      g_object_notify (G_OBJECT (action), "active");
    }
  else
    {
      _cafe_toggle_action_set_active (toggle_action, !active);
      g_object_notify (G_OBJECT (action), "active");

      tmp_list = radio_action->private_data->group;
      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;

          if (cafe_toggle_action_get_active (tmp_action) &&
              (tmp_action != toggle_action))
	    {
	      _cafe_action_emit_activate (CAFE_ACTION (tmp_action));
	      break;
	    }
	}

      tmp_list = radio_action->private_data->group;
      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;
	  
          g_object_notify (G_OBJECT (tmp_action), "current-value");

	  g_signal_emit (tmp_action, radio_action_signals[CHANGED], 0, radio_action);
	}
    }

  cafe_toggle_action_toggled (toggle_action);
}

static GtkWidget *
create_menu_item (CafeAction *action)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, 
		       "draw-as-radio", TRUE,
		       NULL);
}

/**
 * cafe_radio_action_get_group:
 * @action: the action object
 *
 * Returns the list representing the radio group for this object.
 * Note that the returned list is only valid until the next change
 * to the group. 
 *
 * A common way to set up a group of radio group is the following:
 * |[<!-- language="C" -->
 *   GSList *group = NULL;
 *   CafeRadioAction *action;
 *  
 *   while ( ...more actions to add... /)
 *     {
 *        action = cafe_radio_action_new (...);
 *        
 *        cafe_radio_action_set_group (action, group);
 *        group = cafe_radio_action_get_group (action);
 *     }
 * ]|
 *
 * Returns:  (element-type CafeRadioAction) (transfer none): the list representing the radio group for this object
 *
 * Since: 2.4
 *
 * Deprecated: 3.10
 */
GSList *
cafe_radio_action_get_group (CafeRadioAction *action)
{
  g_return_val_if_fail (CAFE_IS_RADIO_ACTION (action), NULL);

  return action->private_data->group;
}

/**
 * cafe_radio_action_set_group:
 * @action: the action object
 * @group: (element-type CafeRadioAction) (allow-none): a list representing a radio group, or %NULL
 *
 * Sets the radio group for the radio action object.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10
 */
void
cafe_radio_action_set_group (CafeRadioAction *action, 
			    GSList         *group)
{
  g_return_if_fail (CAFE_IS_RADIO_ACTION (action));
  g_return_if_fail (!g_slist_find (group, action));

  if (action->private_data->group)
    {
      GSList *slist;

      action->private_data->group = g_slist_remove (action->private_data->group, action);

      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  CafeRadioAction *tmp_action = slist->data;

	  tmp_action->private_data->group = action->private_data->group;
	}
    }

  action->private_data->group = g_slist_prepend (group, action);

  if (group)
    {
      GSList *slist;

      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  CafeRadioAction *tmp_action = slist->data;

	  tmp_action->private_data->group = action->private_data->group;
	}
    }
  else
    {
      cafe_toggle_action_set_active (CAFE_TOGGLE_ACTION (action), TRUE);
    }
}

/**
 * cafe_radio_action_join_group:
 * @action: the action object
 * @group_source: (allow-none): a radio action object whos group we are 
 *   joining, or %NULL to remove the radio action from its group
 *
 * Joins a radio action object to the group of another radio action object.
 *
 * Use this in language bindings instead of the cafe_radio_action_get_group() 
 * and cafe_radio_action_set_group() methods
 *
 * A common way to set up a group of radio actions is the following:
 * |[<!-- language="C" -->
 *   CafeRadioAction *action;
 *   CafeRadioAction *last_action;
 *  
 *   while ( ...more actions to add... /)
 *     {
 *        action = cafe_radio_action_new (...);
 *        
 *        cafe_radio_action_join_group (action, last_action);
 *        last_action = action;
 *     }
 * ]|
 * 
 * Since: 3.0
 *
 * Deprecated: 3.10
 */
void
cafe_radio_action_join_group (CafeRadioAction *action, 
			     CafeRadioAction *group_source)
{
  g_return_if_fail (CAFE_IS_RADIO_ACTION (action));
  g_return_if_fail (group_source == NULL || CAFE_IS_RADIO_ACTION (group_source));  

  if (group_source)
    {
      GSList *group;
      group = cafe_radio_action_get_group (group_source);
      
      if (!group)
        {
          /* if we are not already part of a group we need to set up a new one
             and then get the newly created group */  
          cafe_radio_action_set_group (group_source, NULL);
          group = cafe_radio_action_get_group (group_source);
        }

      cafe_radio_action_set_group (action, group);
    }
  else
    {
      cafe_radio_action_set_group (action, NULL);
    }
}

/**
 * cafe_radio_action_get_current_value:
 * @action: a #CafeRadioAction
 * 
 * Obtains the value property of the currently active member of 
 * the group to which @action belongs.
 * 
 * Returns: The value of the currently active group member
 *
 * Since: 2.4
 *
 * Deprecated: 3.10
 **/
gint
cafe_radio_action_get_current_value (CafeRadioAction *action)
{
  GSList *slist;

  g_return_val_if_fail (CAFE_IS_RADIO_ACTION (action), 0);

  if (action->private_data->group)
    {
      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  CafeToggleAction *toggle_action = slist->data;

	  if (cafe_toggle_action_get_active (toggle_action))
	    return CAFE_RADIO_ACTION (toggle_action)->private_data->value;
	}
    }

  return action->private_data->value;
}

/**
 * cafe_radio_action_set_current_value:
 * @action: a #CafeRadioAction
 * @current_value: the new value
 * 
 * Sets the currently active group member to the member with value
 * property @current_value.
 *
 * Since: 2.10
 *
 * Deprecated: 3.10
 **/
void
cafe_radio_action_set_current_value (CafeRadioAction *action,
                                    gint            current_value)
{
  GSList *slist;

  g_return_if_fail (CAFE_IS_RADIO_ACTION (action));

  if (action->private_data->group)
    {
      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  CafeRadioAction *radio_action = slist->data;

	  if (radio_action->private_data->value == current_value)
            {
              cafe_toggle_action_set_active (CAFE_TOGGLE_ACTION (radio_action),
                                            TRUE);
              return;
            }
	}
    }

  if (action->private_data->value == current_value)
    cafe_toggle_action_set_active (CAFE_TOGGLE_ACTION (action), TRUE);
  else
    g_warning ("Radio group does not contain an action with value '%d'",
	       current_value);
}

