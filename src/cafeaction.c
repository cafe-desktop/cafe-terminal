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

/**
 * SECTION:gtkaction
 * @Short_description: A deprecated action which can be triggered by a menu or toolbar item
 * @Title: CafeAction
 * @See_also: #CafeActionGroup, #GtkUIManager, #GtkActivatable
 *
 * > In GTK+ 3.10, CafeAction has been deprecated. Use #GAction
 * > instead, and associate actions with #CafeActionable widgets. Use
 * > #GMenuModel for creating menus with gtk_menu_new_from_model().
 *
 * Actions represent operations that the user can be perform, along with
 * some information how it should be presented in the interface. Each action
 * provides methods to create icons, menu items and toolbar items
 * representing itself.
 *
 * As well as the callback that is called when the action gets activated,
 * the following also gets associated with the action:
 *
 * - a name (not translated, for path lookup)
 *
 * - a label (translated, for display)
 *
 * - an accelerator
 *
 * - whether label indicates a stock id
 *
 * - a tooltip (optional, translated)
 *
 * - a toolbar label (optional, shorter than label)
 *
 *
 * The action will also have some state information:
 *
 * - visible (shown/hidden)
 *
 * - sensitive (enabled/disabled)
 *
 * Apart from regular actions, there are [toggle actions][GtkToggleAction],
 * which can be toggled between two states and
 * [radio actions][GtkRadioAction], of which only one in a group
 * can be in the “active” state. Other actions can be implemented as #CafeAction
 * subclasses.
 *
 * Each action can have one or more proxy widgets. To act as an action proxy,
 * widget needs to implement #GtkActivatable interface. Proxies mirror the state
 * of the action and should change when the action’s state changes. Properties
 * that are always mirrored by proxies are #CafeAction:sensitive and
 * #CafeAction:visible. #CafeAction:gicon, #CafeAction:icon-name, #CafeAction:label,
 * #CafeAction:short-label and #CafeAction:stock-id properties are only mirorred
 * if proxy widget has #GtkActivatable:use-action-appearance property set to
 * %TRUE.
 *
 * When the proxy is activated, it should activate its action.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "cafeaction.h"


struct _CafeActionPrivate 
{
  const gchar *name; /* interned */
  gchar *label;
  gchar *short_label;
  gchar *tooltip;
  gchar *stock_id; /* stock icon */
  gchar *icon_name; /* themed icon */
  GIcon *gicon;

  guint sensitive          : 1;
  guint visible            : 1;
  guint label_set          : 1; /* these two used so we can set label */
  guint short_label_set    : 1; /* based on stock id */
  guint visible_horizontal : 1;
  guint visible_vertical   : 1;
  guint is_important       : 1;
  guint hide_if_empty      : 1;
  guint visible_overflown  : 1;
  guint always_show_image  : 1;
  guint recursion_guard    : 1;
  guint activate_blocked   : 1;

  /* accelerator */
  guint          accel_count;
  GtkAccelGroup *accel_group;
  GClosure      *accel_closure;
  GQuark         accel_quark;

  CafeActionGroup *action_group;

  /* list of proxy widgets */
  GSList *proxies;
};

enum 
{
  ACTIVATE,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_NAME,
  PROP_LABEL,
  PROP_SHORT_LABEL,
  PROP_TOOLTIP,
  PROP_STOCK_ID,
  PROP_ICON_NAME,
  PROP_GICON,
  PROP_VISIBLE_HORIZONTAL,
  PROP_VISIBLE_VERTICAL,
  PROP_VISIBLE_OVERFLOWN,
  PROP_IS_IMPORTANT,
  PROP_HIDE_IF_EMPTY,
  PROP_SENSITIVE,
  PROP_VISIBLE,
  PROP_ACTION_GROUP,
  PROP_ALWAYS_SHOW_IMAGE
};

/* GtkBuildable */
static void gtk_action_buildable_init             (GtkBuildableIface *iface);
static void gtk_action_buildable_set_name         (GtkBuildable *buildable,
						   const gchar  *name);
static const gchar* gtk_action_buildable_get_name (GtkBuildable *buildable);

G_DEFINE_TYPE_WITH_CODE (CafeAction, gtk_action, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (CafeAction)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_action_buildable_init))

static void gtk_action_finalize     (GObject *object);
static void gtk_action_set_property (GObject         *object,
				     guint            prop_id,
				     const GValue    *value,
				     GParamSpec      *pspec);
static void gtk_action_get_property (GObject         *object,
				     guint            prop_id,
				     GValue          *value,
				     GParamSpec      *pspec);
static void gtk_action_set_action_group (CafeAction	*action,
					 CafeActionGroup *action_group);

static GtkWidget *create_menu_item    (CafeAction *action);
static GtkWidget *create_tool_item    (CafeAction *action);
static void       connect_proxy       (CafeAction *action,
				       GtkWidget *proxy);
static void       disconnect_proxy    (CafeAction *action,
				       GtkWidget *proxy);
 
static void       closure_accel_activate (GClosure     *closure,
					  GValue       *return_value,
					  guint         n_param_values,
					  const GValue *param_values,
					  gpointer      invocation_hint,
					  gpointer      marshal_data);

static guint         action_signals[LAST_SIGNAL] = { 0 };


static void
gtk_action_class_init (CafeActionClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = gtk_action_finalize;
  gobject_class->set_property = gtk_action_set_property;
  gobject_class->get_property = gtk_action_get_property;

  klass->activate = NULL;

  klass->create_menu_item  = create_menu_item;
  klass->create_tool_item  = create_tool_item;
  klass->create_menu       = NULL;
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  klass->menu_item_type    = GTK_TYPE_IMAGE_MENU_ITEM;
  G_GNUC_END_IGNORE_DEPRECATIONS;
  klass->toolbar_item_type = GTK_TYPE_TOOL_BUTTON;
  klass->connect_proxy    = connect_proxy;
  klass->disconnect_proxy = disconnect_proxy;

  /**
   * CafeAction:name:
   *
   * A unique name for the action.
   *
   * Deprecated: 3.10: Use #GAction:name instead
   */
  g_object_class_install_property (gobject_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
							"Name",
							"A unique name for the action.",
							NULL,
							G_PARAM_READWRITE | 
							G_PARAM_CONSTRUCT_ONLY));

  /**
   * CafeAction:label:
   *
   * The label used for menu items and buttons that activate
   * this action. If the label is %NULL, GTK+ uses the stock 
   * label specified via the stock-id property.
   *
   * This is an appearance property and thus only applies if 
   * #GtkActivatable:use-action-appearance is %TRUE.
   *
   * Deprecated: 3.10: Use the "label" attribute on #GMenuItem instead
   */
  g_object_class_install_property (gobject_class,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							"Label",
							"The label used for menu items and buttons "
							"that activate this action.",
							NULL,
							G_PARAM_READWRITE));

  /**
   * CafeAction:short-label:
   *
   * A shorter label that may be used on toolbar buttons.
   *
   * This is an appearance property and thus only applies if 
   * #GtkActivatable:use-action-appearance is %TRUE.
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_SHORT_LABEL,
				   g_param_spec_string ("short-label",
							"Short label",
							"A shorter label that may be used on toolbar buttons.",
							NULL,
							G_PARAM_READWRITE));


  /**
   * CafeAction:tooltip:
   *
   * A tooltip for this action.
   *
   * Deprecated: 3.10: Use gtk_widget_set_tooltip_text() instead
   */
  g_object_class_install_property (gobject_class,
				   PROP_TOOLTIP,
				   g_param_spec_string ("tooltip",
							"Tooltip",
							"A tooltip for this action.",
							NULL,
							G_PARAM_READWRITE));

  /**
   * CafeAction:stock-id:
   *
   * The stock icon displayed in widgets representing this action.
   *
   * This is an appearance property and thus only applies if 
   * #GtkActivatable:use-action-appearance is %TRUE.
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_STOCK_ID,
				   g_param_spec_string ("stock-id",
							"Stock Icon",
							"The stock icon displayed in widgets representing "
							"this action.",
							NULL,
							G_PARAM_READWRITE));
  /**
   * CafeAction:gicon:
   *
   * The #GIcon displayed in the #CafeAction.
   *
   * Note that the stock icon is preferred, if the #CafeAction:stock-id 
   * property holds the id of an existing stock icon.
   *
   * This is an appearance property and thus only applies if 
   * #GtkActivatable:use-action-appearance is %TRUE.
   *
   * Since: 2.16
   *
   * Deprecated: 3.10: Use the "icon" attribute on a #GMenuItem instead
   */
  g_object_class_install_property (gobject_class,
				   PROP_GICON,
				   g_param_spec_object ("gicon",
							"GIcon",
							"The GIcon being displayed",
							G_TYPE_ICON,
 							G_PARAM_READWRITE));							
  /**
   * CafeAction:icon-name:
   *
   * The name of the icon from the icon theme. 
   * 
   * Note that the stock icon is preferred, if the #CafeAction:stock-id 
   * property holds the id of an existing stock icon, and the #GIcon is
   * preferred if the #CafeAction:gicon property is set. 
   *
   * This is an appearance property and thus only applies if 
   * #GtkActivatable:use-action-appearance is %TRUE.
   *
   * Since: 2.10
   *
   * Deprecated: 3.10: Use the "icon" attribute on a #GMenuItem instead
   */
  g_object_class_install_property (gobject_class,
				   PROP_ICON_NAME,
				   g_param_spec_string ("icon-name",
							"Icon Name",
							"The name of the icon from the icon theme",
							NULL,
 							G_PARAM_READWRITE));

  /**
   * CafeAction:visible-horizontal:
   *
   * Whether the toolbar item is visible when the toolbar is in a horizontal orientation.
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE_HORIZONTAL,
				   g_param_spec_boolean ("visible-horizontal",
							 "Visible when horizontal",
							 "Whether the toolbar item is visible when the toolbar "
							 "is in a horizontal orientation.",
							 TRUE,
							 G_PARAM_READWRITE));
  /**
   * CafeAction:visible-overflown:
   *
   * When %TRUE, toolitem proxies for this action are represented in the 
   * toolbar overflow menu.
   *
   * Since: 2.6
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE_OVERFLOWN,
				   g_param_spec_boolean ("visible-overflown",
							 "Visible when overflown",
							 "When TRUE, toolitem proxies for this action "
							 "are represented in the toolbar overflow menu.",
							 TRUE,
							 G_PARAM_READWRITE));

  /**
   * CafeAction:visible-vertical:
   *
   * Whether the toolbar item is visible when the toolbar is in a vertical orientation.
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE_VERTICAL,
				   g_param_spec_boolean ("visible-vertical",
							 "Visible when vertical",
							 "Whether the toolbar item is visible when the toolbar "
							 "is in a vertical orientation.",
							 TRUE,
							 G_PARAM_READWRITE));
  /**
   * CafeAction:is-important:
   *
   * Whether the action is considered important. When TRUE, toolitem
   * proxies for this action show text in GTK_TOOLBAR_BOTH_HORIZ mode.
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_IS_IMPORTANT,
				   g_param_spec_boolean ("is-important",
							 "Is important",
							 "Whether the action is considered important. "
							 "When TRUE, toolitem proxies for this action "
							 "show text in GTK_TOOLBAR_BOTH_HORIZ mode.",
							 FALSE,
							 G_PARAM_READWRITE));
  /**
   * CafeAction:hide-if-empty:
   *
   * When TRUE, empty menu proxies for this action are hidden.
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_HIDE_IF_EMPTY,
				   g_param_spec_boolean ("hide-if-empty",
							 "Hide if empty",
							 "When TRUE, empty menu proxies for this action are hidden.",
							 TRUE,
							 G_PARAM_READWRITE));
  /**
   * CafeAction:sensitive:
   *
   * Whether the action is enabled.
   *
   * Deprecated: 3.10: Use #GAction:enabled and #GSimpleAction:enabled
   * instead
   */
  g_object_class_install_property (gobject_class,
				   PROP_SENSITIVE,
				   g_param_spec_boolean ("sensitive",
							 "Sensitive",
							 "Whether the action is enabled.",
							 TRUE,
							 G_PARAM_READWRITE));
  /**
   * CafeAction:visible:
   *
   * Whether the action is visible.
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   */
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
							 "Visible",
							 "Whether the action is visible.",
							 TRUE,
							 G_PARAM_READWRITE));
  /**
   * CafeAction:action-group:
   *
   * The CafeActionGroup this CafeAction is associated with, or NULL
   * (for internal use).
   *
   * Deprecated: 3.10: Lookup the #GAction using g_action_map_lookup_action()
   * instead
   */
  g_object_class_install_property (gobject_class,
				   PROP_ACTION_GROUP,
				   g_param_spec_object ("action-group",
							 "Action Group",
							 "The CafeActionGroup this CafeAction is associated with, or NULL (for internal use).",
							 CAFE_TYPE_ACTION_GROUP,
							 G_PARAM_READWRITE));

  /**
   * CafeAction:always-show-image:
   *
   * If %TRUE, the action's menu item proxies will ignore the #GtkSettings:gtk-menu-images 
   * setting and always show their image, if available.
   *
   * Use this property if the menu item would be useless or hard to use
   * without their image. 
   *
   * Since: 2.20
   *
   * Deprecated: 3.10: There is no corresponding replacement when using
   * #GAction
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ALWAYS_SHOW_IMAGE,
                                   g_param_spec_boolean ("always-show-image",
                                                         "Always show image",
                                                         "Whether the image will always be shown",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * CafeAction::activate:
   * @action: the #CafeAction
   *
   * The "activate" signal is emitted when the action is activated.
   *
   * Since: 2.4
   *
   * Deprecated: 3.10: Use #GSimpleAction::activate instead
   */
  action_signals[ACTIVATE] =
    g_signal_new ("activate",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (CafeActionClass, activate),  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);
}


static void
gtk_action_init (CafeAction *action)
{
  action->private_data = gtk_action_get_instance_private (action);

  action->private_data->name = NULL;
  action->private_data->label = NULL;
  action->private_data->short_label = NULL;
  action->private_data->tooltip = NULL;
  action->private_data->stock_id = NULL;
  action->private_data->icon_name = NULL;
  action->private_data->visible_horizontal = TRUE;
  action->private_data->visible_vertical   = TRUE;
  action->private_data->visible_overflown  = TRUE;
  action->private_data->is_important = FALSE;
  action->private_data->hide_if_empty = TRUE;
  action->private_data->always_show_image = FALSE;
  action->private_data->activate_blocked = FALSE;

  action->private_data->sensitive = TRUE;
  action->private_data->visible = TRUE;

  action->private_data->label_set = FALSE;
  action->private_data->short_label_set = FALSE;

  action->private_data->accel_count = 0;
  action->private_data->accel_group = NULL;
  action->private_data->accel_quark = 0;
  action->private_data->accel_closure = 
    g_closure_new_object (sizeof (GClosure), G_OBJECT (action));
  g_closure_set_marshal (action->private_data->accel_closure, 
			 closure_accel_activate);
  g_closure_ref (action->private_data->accel_closure);
  g_closure_sink (action->private_data->accel_closure);

  action->private_data->action_group = NULL;

  action->private_data->proxies = NULL;
  action->private_data->gicon = NULL;  
}

static void
gtk_action_buildable_init (GtkBuildableIface *iface)
{
  iface->set_name = gtk_action_buildable_set_name;
  iface->get_name = gtk_action_buildable_get_name;
}

static void
gtk_action_buildable_set_name (GtkBuildable *buildable,
			       const gchar  *name)
{
  CafeAction *action = CAFE_ACTION (buildable);

  action->private_data->name = g_intern_string (name);
}

static const gchar *
gtk_action_buildable_get_name (GtkBuildable *buildable)
{
  CafeAction *action = CAFE_ACTION (buildable);

  return action->private_data->name;
}

/**
 * gtk_action_new:
 * @name: A unique name for the action
 * @label: (allow-none): the label displayed in menu items and on buttons,
 *         or %NULL
 * @tooltip: (allow-none): a tooltip for the action, or %NULL
 * @stock_id: (allow-none): the stock icon to display in widgets representing
 *            the action, or %NULL
 *
 * Creates a new #CafeAction object. To add the action to a
 * #CafeActionGroup and set the accelerator for the action,
 * call gtk_action_group_add_action_with_accel().
 * See the [UI Definition section][XML-UI] for information on allowed action
 * names.
 *
 * Returns: a new #CafeAction
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use #GAction instead, associating it to a widget with
 * #CafeActionable or creating a #GtkMenu with gtk_menu_new_from_model()
 */
CafeAction *
gtk_action_new (const gchar *name,
		const gchar *label,
		const gchar *tooltip,
		const gchar *stock_id)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (CAFE_TYPE_ACTION,
                       "name", name,
		       "label", label,
		       "tooltip", tooltip,
		       "stock-id", stock_id,
		       NULL);
}

static void
gtk_action_finalize (GObject *object)
{
  CafeAction *action;
  action = CAFE_ACTION (object);

  g_free (action->private_data->label);
  g_free (action->private_data->short_label);
  g_free (action->private_data->tooltip);
  g_free (action->private_data->stock_id);
  g_free (action->private_data->icon_name);
  
  if (action->private_data->gicon)
    g_object_unref (action->private_data->gicon);

  g_closure_unref (action->private_data->accel_closure);
  if (action->private_data->accel_group)
    g_object_unref (action->private_data->accel_group);

  G_OBJECT_CLASS (gtk_action_parent_class)->finalize (object);  
}

static void
gtk_action_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  CafeAction *action;
  
  action = CAFE_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      action->private_data->name = g_intern_string (g_value_get_string (value));
      break;
    case PROP_LABEL:
      gtk_action_set_label (action, g_value_get_string (value));
      break;
    case PROP_SHORT_LABEL:
      gtk_action_set_short_label (action, g_value_get_string (value));
      break;
    case PROP_TOOLTIP:
      gtk_action_set_tooltip (action, g_value_get_string (value));
      break;
    case PROP_STOCK_ID:
      gtk_action_set_stock_id (action, g_value_get_string (value));
      break;
    case PROP_GICON:
      gtk_action_set_gicon (action, g_value_get_object (value));
      break;
    case PROP_ICON_NAME:
      gtk_action_set_icon_name (action, g_value_get_string (value));
      break;
    case PROP_VISIBLE_HORIZONTAL:
      gtk_action_set_visible_horizontal (action, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_VERTICAL:
      gtk_action_set_visible_vertical (action, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_OVERFLOWN:
      action->private_data->visible_overflown = g_value_get_boolean (value);
      break;
    case PROP_IS_IMPORTANT:
      gtk_action_set_is_important (action, g_value_get_boolean (value));
      break;
    case PROP_HIDE_IF_EMPTY:
      action->private_data->hide_if_empty = g_value_get_boolean (value);
      break;
    case PROP_SENSITIVE:
      gtk_action_set_sensitive (action, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE:
      gtk_action_set_visible (action, g_value_get_boolean (value));
      break;
    case PROP_ACTION_GROUP:
      gtk_action_set_action_group (action, g_value_get_object (value));
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      gtk_action_set_always_show_image (action, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_action_get_property (GObject    *object,
			 guint       prop_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
  CafeAction *action;

  action = CAFE_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_static_string (value, action->private_data->name);
      break;
    case PROP_LABEL:
      g_value_set_string (value, action->private_data->label);
      break;
    case PROP_SHORT_LABEL:
      g_value_set_string (value, action->private_data->short_label);
      break;
    case PROP_TOOLTIP:
      g_value_set_string (value, action->private_data->tooltip);
      break;
    case PROP_STOCK_ID:
      g_value_set_string (value, action->private_data->stock_id);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, action->private_data->icon_name);
      break;
    case PROP_GICON:
      g_value_set_object (value, action->private_data->gicon);
      break;
    case PROP_VISIBLE_HORIZONTAL:
      g_value_set_boolean (value, action->private_data->visible_horizontal);
      break;
    case PROP_VISIBLE_VERTICAL:
      g_value_set_boolean (value, action->private_data->visible_vertical);
      break;
    case PROP_VISIBLE_OVERFLOWN:
      g_value_set_boolean (value, action->private_data->visible_overflown);
      break;
    case PROP_IS_IMPORTANT:
      g_value_set_boolean (value, action->private_data->is_important);
      break;
    case PROP_HIDE_IF_EMPTY:
      g_value_set_boolean (value, action->private_data->hide_if_empty);
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, action->private_data->sensitive);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, action->private_data->visible);
      break;
    case PROP_ACTION_GROUP:
      g_value_set_object (value, action->private_data->action_group);
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      g_value_set_boolean (value, action->private_data->always_show_image);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkWidget *
create_menu_item (CafeAction *action)
{
  GType menu_item_type;

  menu_item_type = CAFE_ACTION_GET_CLASS (action)->menu_item_type;

  return g_object_new (menu_item_type, NULL);
}

static GtkWidget *
create_tool_item (CafeAction *action)
{
  GType toolbar_item_type;

  toolbar_item_type = CAFE_ACTION_GET_CLASS (action)->toolbar_item_type;

  return g_object_new (toolbar_item_type, NULL);
}

static void
remove_proxy (CafeAction *action,
	      GtkWidget *proxy)
{
  g_object_unref (proxy);
  action->private_data->proxies = g_slist_remove (action->private_data->proxies, proxy);
}

static void
connect_proxy (CafeAction *action,
	       GtkWidget *proxy)
{
  action->private_data->proxies = g_slist_prepend (action->private_data->proxies, proxy);

  g_object_ref_sink (proxy);

  if (action->private_data->action_group)
    _gtk_action_group_emit_connect_proxy (action->private_data->action_group, action, proxy);

}

static void
disconnect_proxy (CafeAction *action,
		  GtkWidget *proxy)
{
  remove_proxy (action, proxy);

  if (action->private_data->action_group)
    _gtk_action_group_emit_disconnect_proxy (action->private_data->action_group, action, proxy);
}

/**
 * _gtk_action_sync_menu_visible:
 * @action: (allow-none): a #CafeAction, or %NULL to determine the action from @proxy
 * @proxy: a proxy menu item
 * @empty: whether the submenu attached to @proxy is empty
 * 
 * Updates the visibility of @proxy from the visibility of @action
 * according to the following rules:

 * - if @action is invisible, @proxy is too
 *
 * - if @empty is %TRUE, hide @proxy unless the “hide-if-empty”
 *   property of @action indicates otherwise
 *
 * This function is used in the implementation of #GtkUIManager.
 *
 * Deprecated: 3.10
 **/
void
_gtk_action_sync_menu_visible (CafeAction *action,
			       GtkWidget *proxy,
			       gboolean   empty)
{
  gboolean visible = TRUE;
  gboolean hide_if_empty = TRUE;

  g_return_if_fail (GTK_IS_MENU_ITEM (proxy));
  g_return_if_fail (action == NULL || GTK_IS_ACTION (action));

  if (action == NULL)
    action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));

  if (action)
    {
      /* a GtkMenu for a <popup/> doesn't have to have an action */
      visible = gtk_action_is_visible (action);
      hide_if_empty = action->private_data->hide_if_empty;
    }

  if (visible && !(empty && hide_if_empty))
    gtk_widget_show (proxy);
  else
    gtk_widget_hide (proxy);
}

void
_gtk_action_emit_activate (CafeAction *action)
{
  CafeActionGroup *group = action->private_data->action_group;

  if (group != NULL)
    {
      g_object_ref (action);
      g_object_ref (group);
      _gtk_action_group_emit_pre_activate (group, action);
    }

  g_signal_emit (action, action_signals[ACTIVATE], 0);

  if (group != NULL)
    {
      _gtk_action_group_emit_post_activate (group, action);
      g_object_unref (group);
      g_object_unref (action);
    }
}

/**
 * gtk_action_activate:
 * @action: the action object
 *
 * Emits the “activate” signal on the specified action, if it isn't
 * insensitive. This gets called by the proxy widgets when they get 
 * activated.
 *
 * It can also be used to manually activate an action.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use g_action_group_activate_action() on a #GAction instead
 */
void
gtk_action_activate (CafeAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  
  if (action->private_data->activate_blocked)
    return;

  if (gtk_action_is_sensitive (action))
    _gtk_action_emit_activate (action);
}

/**
 * gtk_action_block_activate:
 * @action: a #CafeAction
 *
 * Disable activation signals from the action 
 *
 * This is needed when updating the state of your proxy
 * #GtkActivatable widget could result in calling gtk_action_activate(),
 * this is a convenience function to avoid recursing in those
 * cases (updating toggle state for instance).
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use g_simple_action_set_enabled() to disable the
 * #GSimpleAction instead
 */
void
gtk_action_block_activate (CafeAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  action->private_data->activate_blocked = TRUE;
}

/**
 * gtk_action_unblock_activate:
 * @action: a #CafeAction
 *
 * Reenable activation signals from the action 
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use g_simple_action_set_enabled() to enable the
 * #GSimpleAction instead
 */
void
gtk_action_unblock_activate (CafeAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  action->private_data->activate_blocked = FALSE;
}

/**
 * gtk_action_create_icon:
 * @action: the action object
 * @icon_size: (type int): the size of the icon (#GtkIconSize) that should
 *      be created.
 *
 * This function is intended for use by action implementations to
 * create icons displayed in the proxy widgets.
 *
 * Returns: (transfer none): a widget that displays the icon for this action.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use g_menu_item_set_icon() to set an icon on a #GMenuItem,
 * or gtk_container_add() to add a #GtkImage to a #GtkButton
 */
GtkWidget *
gtk_action_create_icon (CafeAction *action, GtkIconSize icon_size)
{
  GtkWidget *widget = NULL;

  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (action->private_data->stock_id &&
      gtk_icon_factory_lookup_default (action->private_data->stock_id))
    widget = gtk_image_new_from_stock (action->private_data->stock_id, icon_size);
  else if (action->private_data->gicon)
    widget = gtk_image_new_from_gicon (action->private_data->gicon, icon_size);
  else if (action->private_data->icon_name)
    widget = gtk_image_new_from_icon_name (action->private_data->icon_name, icon_size);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return widget;
}

/**
 * gtk_action_create_menu_item:
 * @action: the action object
 *
 * Creates a menu item widget that proxies for the given action.
 *
 * Returns: (transfer none): a menu item connected to the action.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use g_menu_item_new() and associate it with a #GAction
 * instead.
 */
GtkWidget *
gtk_action_create_menu_item (CafeAction *action)
{
  GtkWidget *menu_item;

  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  menu_item = CAFE_ACTION_GET_CLASS (action)->create_menu_item (action);

  gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (menu_item), TRUE);
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (menu_item), action);

  return menu_item;
}

/**
 * gtk_action_create_tool_item:
 * @action: the action object
 *
 * Creates a toolbar item widget that proxies for the given action.
 *
 * Returns: (transfer none): a toolbar item connected to the action.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use a #GtkToolItem and associate it with a #GAction using
 * gtk_actionable_set_action_name() instead
 */
GtkWidget *
gtk_action_create_tool_item (CafeAction *action)
{
  GtkWidget *button;

  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  button = CAFE_ACTION_GET_CLASS (action)->create_tool_item (action);

  gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (button), TRUE);
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);

  return button;
}

void
_gtk_action_add_to_proxy_list (CafeAction     *action,
			       GtkWidget     *proxy)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_WIDGET (proxy));
 
  CAFE_ACTION_GET_CLASS (action)->connect_proxy (action, proxy);
}

void
_gtk_action_remove_from_proxy_list (CafeAction     *action,
				    GtkWidget     *proxy)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_WIDGET (proxy));

  CAFE_ACTION_GET_CLASS (action)->disconnect_proxy (action, proxy);
}

/**
 * gtk_action_get_proxies:
 * @action: the action object
 * 
 * Returns the proxy widgets for an action.
 * See also gtk_activatable_get_related_action().
 *
 * Returns: (element-type GtkWidget) (transfer none): a #GSList of proxy widgets. The list is owned by GTK+
 * and must not be modified.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10
 **/
GSList*
gtk_action_get_proxies (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->proxies;
}

/**
 * gtk_action_get_name:
 * @action: the action object
 * 
 * Returns the name of the action.
 * 
 * Returns: the name of the action. The string belongs to GTK+ and should not
 *   be freed.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use g_action_get_name() on a #GAction instead
 **/
const gchar *
gtk_action_get_name (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->name;
}

/**
 * gtk_action_is_sensitive:
 * @action: the action object
 * 
 * Returns whether the action is effectively sensitive.
 *
 * Returns: %TRUE if the action and its associated action group 
 * are both sensitive.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use g_action_get_enabled() on a #GAction
 * instead
 **/
gboolean
gtk_action_is_sensitive (CafeAction *action)
{
  CafeActionPrivate *priv;
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  priv = action->private_data;
  return priv->sensitive &&
    (priv->action_group == NULL ||
     gtk_action_group_get_sensitive (priv->action_group));
}

/**
 * gtk_action_get_sensitive:
 * @action: the action object
 * 
 * Returns whether the action itself is sensitive. Note that this doesn’t 
 * necessarily mean effective sensitivity. See gtk_action_is_sensitive() 
 * for that.
 *
 * Returns: %TRUE if the action itself is sensitive.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use g_action_get_enabled() on a #GAction
 * instead
 **/
gboolean
gtk_action_get_sensitive (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->sensitive;
}

/**
 * gtk_action_set_sensitive:
 * @action: the action object
 * @sensitive: %TRUE to make the action sensitive
 * 
 * Sets the :sensitive property of the action to @sensitive. Note that 
 * this doesn’t necessarily mean effective sensitivity. See 
 * gtk_action_is_sensitive() 
 * for that.
 *
 * Since: 2.6
 *
 * Deprecated: 3.10: Use g_simple_action_set_enabled() on a #GSimpleAction
 * instead
 **/
void
gtk_action_set_sensitive (CafeAction *action,
			  gboolean   sensitive)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  sensitive = sensitive != FALSE;
  
  if (action->private_data->sensitive != sensitive)
    {
      action->private_data->sensitive = sensitive;

      g_object_notify (G_OBJECT (action), "sensitive");
    }
}

/**
 * gtk_action_is_visible:
 * @action: the action object
 * 
 * Returns whether the action is effectively visible.
 *
 * Returns: %TRUE if the action and its associated action group 
 * are both visible.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor the state of
 * #CafeActionable widgets directly
 **/
gboolean
gtk_action_is_visible (CafeAction *action)
{
  CafeActionPrivate *priv;
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  priv = action->private_data;
  return priv->visible &&
    (priv->action_group == NULL ||
     gtk_action_group_get_visible (priv->action_group));
}

/**
 * gtk_action_get_visible:
 * @action: the action object
 * 
 * Returns whether the action itself is visible. Note that this doesn’t 
 * necessarily mean effective visibility. See gtk_action_is_sensitive() 
 * for that.
 *
 * Returns: %TRUE if the action itself is visible.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor the state of
 * #CafeActionable widgets directly
 **/
gboolean
gtk_action_get_visible (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->visible;
}

/**
 * gtk_action_set_visible:
 * @action: the action object
 * @visible: %TRUE to make the action visible
 * 
 * Sets the :visible property of the action to @visible. Note that 
 * this doesn’t necessarily mean effective visibility. See 
 * gtk_action_is_visible() 
 * for that.
 *
 * Since: 2.6
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor the state of
 * #CafeActionable widgets directly
 **/
void
gtk_action_set_visible (CafeAction *action,
			gboolean   visible)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  visible = visible != FALSE;
  
  if (action->private_data->visible != visible)
    {
      action->private_data->visible = visible;

      g_object_notify (G_OBJECT (action), "visible");
    }
}
/**
 * gtk_action_set_is_important:
 * @action: the action object
 * @is_important: %TRUE to make the action important
 *
 * Sets whether the action is important, this attribute is used
 * primarily by toolbar items to decide whether to show a label
 * or not.
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor whether
 * labels are shown directly
 */
void 
gtk_action_set_is_important (CafeAction *action,
			     gboolean   is_important)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  is_important = is_important != FALSE;
  
  if (action->private_data->is_important != is_important)
    {
      action->private_data->is_important = is_important;
      
      g_object_notify (G_OBJECT (action), "is-important");
    }  
}

/**
 * gtk_action_get_is_important:
 * @action: a #CafeAction
 *
 * Checks whether @action is important or not
 * 
 * Returns: whether @action is important
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor whether
 * labels are shown directly
 */
gboolean 
gtk_action_get_is_important (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->is_important;
}

/**
 * gtk_action_set_always_show_image:
 * @action: a #CafeAction
 * @always_show: %TRUE if menuitem proxies should always show their image
 *
 * Sets whether @action's menu item proxies will ignore the
 * #GtkSettings:gtk-menu-images setting and always show their image, if available.
 *
 * Use this if the menu item would be useless or hard to use
 * without their image.
 *
 * Since: 2.20
 *
 * Deprecated: 3.10: Use g_menu_item_set_icon() on a #GMenuItem instead, if the
 * item should have an image
 */
void
gtk_action_set_always_show_image (CafeAction *action,
                                  gboolean   always_show)
{
  CafeActionPrivate *priv;

  g_return_if_fail (GTK_IS_ACTION (action));

  priv = action->private_data;

  always_show = always_show != FALSE;
  
  if (priv->always_show_image != always_show)
    {
      priv->always_show_image = always_show;

      g_object_notify (G_OBJECT (action), "always-show-image");
    }
}

/**
 * gtk_action_get_always_show_image:
 * @action: a #CafeAction
 *
 * Returns whether @action's menu item proxies will always
 * show their image, if available.
 *
 * Returns: %TRUE if the menu item proxies will always show their image
 *
 * Since: 2.20
 *
 * Deprecated: 3.10: Use g_menu_item_get_attribute_value() on a #GMenuItem
 * instead
 */
gboolean
gtk_action_get_always_show_image  (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->always_show_image;
}

/**
 * gtk_action_set_label:
 * @action: a #CafeAction
 * @label: the label text to set
 *
 * Sets the label of @action.
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and set a label on a menu item with
 * g_menu_item_set_label(). For #CafeActionable widgets, use the widget-specific
 * API to set a label
 */
void 
gtk_action_set_label (CafeAction	  *action,
		      const gchar *label)
{
  gchar *tmp;
  
  g_return_if_fail (GTK_IS_ACTION (action));
  
  tmp = action->private_data->label;
  action->private_data->label = g_strdup (label);
  g_free (tmp);
  action->private_data->label_set = (action->private_data->label != NULL);
  /* if label is unset, then use the label from the stock item */
  if (!action->private_data->label_set && action->private_data->stock_id)
    {
      GtkStockItem stock_item;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

      if (gtk_stock_lookup (action->private_data->stock_id, &stock_item))
	action->private_data->label = g_strdup (stock_item.label);

      G_GNUC_END_IGNORE_DEPRECATIONS;
    }

  g_object_notify (G_OBJECT (action), "label");
  
  /* if short_label is unset, set short_label=label */
  if (!action->private_data->short_label_set)
    {
      gtk_action_set_short_label (action, action->private_data->label);
      action->private_data->short_label_set = FALSE;
    }
}

/**
 * gtk_action_get_label:
 * @action: a #CafeAction
 *
 * Gets the label text of @action.
 *
 * Returns: the label text
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and get a label from a menu item
 * with g_menu_item_get_attribute_value(). For #CafeActionable widgets, use the
 * widget-specific API to get a label
 */
const gchar *
gtk_action_get_label (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->label;
}

/**
 * gtk_action_set_short_label:
 * @action: a #CafeAction
 * @short_label: the label text to set
 *
 * Sets a shorter label text on @action.
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, which has no equivalent of short
 * labels
 */
void 
gtk_action_set_short_label (CafeAction   *action,
			    const gchar *short_label)
{
  gchar *tmp;

  g_return_if_fail (GTK_IS_ACTION (action));

  tmp = action->private_data->short_label;
  action->private_data->short_label = g_strdup (short_label);
  g_free (tmp);
  action->private_data->short_label_set = (action->private_data->short_label != NULL);
  /* if short_label is unset, then use the value of label */
  if (!action->private_data->short_label_set)
    action->private_data->short_label = g_strdup (action->private_data->label);

  g_object_notify (G_OBJECT (action), "short-label");
}

/**
 * gtk_action_get_short_label:
 * @action: a #CafeAction
 *
 * Gets the short label text of @action.
 *
 * Returns: the short label text.
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, which has no equivalent of short
 * labels
 */
const gchar *
gtk_action_get_short_label (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->short_label;
}

/**
 * gtk_action_set_visible_horizontal:
 * @action: a #CafeAction
 * @visible_horizontal: whether the action is visible horizontally
 *
 * Sets whether @action is visible when horizontal
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor the
 * visibility of associated widgets and menu items directly
 */
void 
gtk_action_set_visible_horizontal (CafeAction *action,
				   gboolean   visible_horizontal)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  g_return_if_fail (GTK_IS_ACTION (action));

  visible_horizontal = visible_horizontal != FALSE;
  
  if (action->private_data->visible_horizontal != visible_horizontal)
    {
      action->private_data->visible_horizontal = visible_horizontal;
      
      g_object_notify (G_OBJECT (action), "visible-horizontal");
    }  
}

/**
 * gtk_action_get_visible_horizontal:
 * @action: a #CafeAction
 *
 * Checks whether @action is visible when horizontal
 * 
 * Returns: whether @action is visible when horizontal
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor the
 * visibility of associated widgets and menu items directly
 */
gboolean 
gtk_action_get_visible_horizontal (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->visible_horizontal;
}

/**
 * gtk_action_set_visible_vertical:
 * @action: a #CafeAction
 * @visible_vertical: whether the action is visible vertically
 *
 * Sets whether @action is visible when vertical 
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor the
 * visibility of associated widgets and menu items directly
 */
void 
gtk_action_set_visible_vertical (CafeAction *action,
				 gboolean   visible_vertical)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  g_return_if_fail (GTK_IS_ACTION (action));

  visible_vertical = visible_vertical != FALSE;
  
  if (action->private_data->visible_vertical != visible_vertical)
    {
      action->private_data->visible_vertical = visible_vertical;
      
      g_object_notify (G_OBJECT (action), "visible-vertical");
    }  
}

/**
 * gtk_action_get_visible_vertical:
 * @action: a #CafeAction
 *
 * Checks whether @action is visible when horizontal
 * 
 * Returns: whether @action is visible when horizontal
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and control and monitor the
 * visibility of associated widgets and menu items directly
 */
gboolean 
gtk_action_get_visible_vertical (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

  return action->private_data->visible_vertical;
}

/**
 * gtk_action_set_tooltip:
 * @action: a #CafeAction
 * @tooltip: the tooltip text
 *
 * Sets the tooltip text on @action
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and set tooltips on associated
 * #CafeActionable widgets with gtk_widget_set_tooltip_text()
 */
void 
gtk_action_set_tooltip (CafeAction   *action,
			const gchar *tooltip)
{
  gchar *tmp;

  g_return_if_fail (GTK_IS_ACTION (action));

  tmp = action->private_data->tooltip;
  action->private_data->tooltip = g_strdup (tooltip);
  g_free (tmp);

  g_object_notify (G_OBJECT (action), "tooltip");
}

/**
 * gtk_action_get_tooltip:
 * @action: a #CafeAction
 *
 * Gets the tooltip text of @action.
 *
 * Returns: the tooltip text
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and get tooltips from associated
 * #CafeActionable widgets with gtk_widget_get_tooltip_text()
 */
const gchar *
gtk_action_get_tooltip (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->tooltip;
}

/**
 * gtk_action_set_stock_id:
 * @action: a #CafeAction
 * @stock_id: the stock id
 *
 * Sets the stock id on @action
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, which has no equivalent of stock
 * items
 */
void 
gtk_action_set_stock_id (CafeAction   *action,
			 const gchar *stock_id)
{
  gchar *tmp;

  g_return_if_fail (GTK_IS_ACTION (action));

  g_return_if_fail (GTK_IS_ACTION (action));

  tmp = action->private_data->stock_id;
  action->private_data->stock_id = g_strdup (stock_id);
  g_free (tmp);

  g_object_notify (G_OBJECT (action), "stock-id");
  
  /* update label and short_label if appropriate */
  if (!action->private_data->label_set)
    {
      GtkStockItem stock_item;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

      if (action->private_data->stock_id &&
	  gtk_stock_lookup (action->private_data->stock_id, &stock_item))
	gtk_action_set_label (action, stock_item.label);
      else
	gtk_action_set_label (action, NULL);

      G_GNUC_END_IGNORE_DEPRECATIONS;

      action->private_data->label_set = FALSE;
    }
}

/**
 * gtk_action_get_stock_id:
 * @action: a #CafeAction
 *
 * Gets the stock id of @action.
 *
 * Returns: the stock id
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, which has no equivalent of stock
 * items
 */
const gchar *
gtk_action_get_stock_id (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->stock_id;
}

/**
 * gtk_action_set_icon_name:
 * @action: a #CafeAction
 * @icon_name: the icon name to set
 *
 * Sets the icon name on @action
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and g_menu_item_set_icon() to set an
 * icon on a #GMenuItem associated with a #GAction, or gtk_container_add() to
 * add a #GtkImage to a #GtkButton
 */
void 
gtk_action_set_icon_name (CafeAction   *action,
			  const gchar *icon_name)
{
  gchar *tmp;

  g_return_if_fail (GTK_IS_ACTION (action));

  tmp = action->private_data->icon_name;
  action->private_data->icon_name = g_strdup (icon_name);
  g_free (tmp);

  g_object_notify (G_OBJECT (action), "icon-name");
}

/**
 * gtk_action_get_icon_name:
 * @action: a #CafeAction
 *
 * Gets the icon name of @action.
 *
 * Returns: the icon name
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and
 * g_menu_item_get_attribute_value() to get an icon from a #GMenuItem
 * associated with a #GAction
 */
const gchar *
gtk_action_get_icon_name (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->icon_name;
}

/**
 * gtk_action_set_gicon:
 * @action: a #CafeAction
 * @icon: the #GIcon to set
 *
 * Sets the icon of @action.
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and g_menu_item_set_icon() to set an
 * icon on a #GMenuItem associated with a #GAction, or gtk_container_add() to
 * add a #GtkImage to a #GtkButton
 */
void
gtk_action_set_gicon (CafeAction *action,
                      GIcon     *icon)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  if (action->private_data->gicon)
    g_object_unref (action->private_data->gicon);

  action->private_data->gicon = icon;

  if (action->private_data->gicon)
    g_object_ref (action->private_data->gicon);

  g_object_notify (G_OBJECT (action), "gicon");
}

/**
 * gtk_action_get_gicon:
 * @action: a #CafeAction
 *
 * Gets the gicon of @action.
 *
 * Returns: (transfer none): The action’s #GIcon if one is set.
 *
 * Since: 2.16
 *
 * Deprecated: 3.10: Use #GAction instead, and
 * g_menu_item_get_attribute_value() to get an icon from a #GMenuItem
 * associated with a #GAction
 */
GIcon *
gtk_action_get_gicon (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->gicon;
}

static void
closure_accel_activate (GClosure     *closure,
                        GValue       *return_value,
                        guint         n_param_values,
                        const GValue *param_values,
                        gpointer      invocation_hint,
                        gpointer      marshal_data)
{
  if (gtk_action_is_sensitive (CAFE_ACTION (closure->data)))
    {
      _gtk_action_emit_activate (CAFE_ACTION (closure->data));
      
      /* we handled the accelerator */
      g_value_set_boolean (return_value, TRUE);
    }
}

static void
gtk_action_set_action_group (CafeAction	    *action,
			     CafeActionGroup *action_group)
{
  if (action->private_data->action_group == NULL)
    g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  else
    g_return_if_fail (action_group == NULL);

  action->private_data->action_group = action_group;
}

/**
 * gtk_action_set_accel_path:
 * @action: the action object
 * @accel_path: the accelerator path
 *
 * Sets the accel path for this action.  All proxy widgets associated
 * with the action will have this accel path, so that their
 * accelerators are consistent.
 *
 * Note that @accel_path string will be stored in a #GQuark. Therefore, if you
 * pass a static string, you can save some memory by interning it first with 
 * g_intern_static_string().
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use #GAction and the accelerator path on an associated
 * #GtkMenu instead
 */
void
gtk_action_set_accel_path (CafeAction   *action, 
			   const gchar *accel_path)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  action->private_data->accel_quark = g_quark_from_string (accel_path);
}

/**
 * gtk_action_get_accel_path:
 * @action: the action object
 *
 * Returns the accel path for this action.  
 *
 * Since: 2.6
 *
 * Returns: the accel path for this action, or %NULL
 *   if none is set. The returned string is owned by GTK+ 
 *   and must not be freed or modified.
 *
 * Deprecated: 3.10: Use #GAction and the accelerator path on an associated
 * #GtkMenu instead
 */
const gchar *
gtk_action_get_accel_path (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  if (action->private_data->accel_quark)
    return g_quark_to_string (action->private_data->accel_quark);
  else
    return NULL;
}

/**
 * gtk_action_get_accel_closure:
 * @action: the action object
 *
 * Returns the accel closure for this action.
 *
 * Since: 2.8
 *
 * Returns: (transfer none): the accel closure for this action. The
 *          returned closure is owned by GTK+ and must not be unreffed
 *          or modified.
 *
 * Deprecated: 3.10: Use #GAction and #GtkMenu instead, which have no
 * equivalent for getting the accel closure
 */
GClosure *
gtk_action_get_accel_closure (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  return action->private_data->accel_closure;
}


/**
 * gtk_action_set_accel_group:
 * @action: the action object
 * @accel_group: (allow-none): a #GtkAccelGroup or %NULL
 *
 * Sets the #GtkAccelGroup in which the accelerator for this action
 * will be installed.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use #GAction and the accelerator group on an associated
 * #GtkMenu instead
 **/
void
gtk_action_set_accel_group (CafeAction     *action,
			    GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (accel_group == NULL || GTK_IS_ACCEL_GROUP (accel_group));
  
  if (accel_group)
    g_object_ref (accel_group);
  if (action->private_data->accel_group)
    g_object_unref (action->private_data->accel_group);

  action->private_data->accel_group = accel_group;
}

/**
 * gtk_action_connect_accelerator:
 * @action: a #CafeAction
 * 
 * Installs the accelerator for @action if @action has an
 * accel path and group. See gtk_action_set_accel_path() and 
 * gtk_action_set_accel_group()
 *
 * Since multiple proxies may independently trigger the installation
 * of the accelerator, the @action counts the number of times this
 * function has been called and doesn’t remove the accelerator until
 * gtk_action_disconnect_accelerator() has been called as many times.
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use #GAction and the accelerator group on an associated
 * #GtkMenu instead
 **/
void 
gtk_action_connect_accelerator (CafeAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  if (!action->private_data->accel_quark ||
      !action->private_data->accel_group)
    return;

  if (action->private_data->accel_count == 0)
    {
      const gchar *accel_path = 
	g_quark_to_string (action->private_data->accel_quark);
      
      gtk_accel_group_connect_by_path (action->private_data->accel_group,
				       accel_path,
				       action->private_data->accel_closure);
    }

  action->private_data->accel_count++;
}

/**
 * gtk_action_disconnect_accelerator:
 * @action: a #CafeAction
 * 
 * Undoes the effect of one call to gtk_action_connect_accelerator().
 *
 * Since: 2.4
 *
 * Deprecated: 3.10: Use #GAction and the accelerator group on an associated
 * #GtkMenu instead
 **/
void 
gtk_action_disconnect_accelerator (CafeAction *action)
{
  g_return_if_fail (GTK_IS_ACTION (action));

  if (!action->private_data->accel_quark ||
      !action->private_data->accel_group)
    return;

  action->private_data->accel_count--;

  if (action->private_data->accel_count == 0)
    gtk_accel_group_disconnect (action->private_data->accel_group,
				action->private_data->accel_closure);
}

/**
 * gtk_action_create_menu:
 * @action: a #CafeAction
 *
 * If @action provides a #GtkMenu widget as a submenu for the menu
 * item or the toolbar item it creates, this function returns an
 * instance of that menu.
 *
 * Returns: (transfer none): the menu item provided by the
 *               action, or %NULL.
 *
 * Since: 2.12
 *
 * Deprecated: 3.10: Use #GAction and #GMenuModel instead, and create a
 * #GtkMenu with gtk_menu_new_from_model()
 */
GtkWidget *
gtk_action_create_menu (CafeAction *action)
{
  g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

  if (CAFE_ACTION_GET_CLASS (action)->create_menu)
    return CAFE_ACTION_GET_CLASS (action)->create_menu (action);

  return NULL;
}

