/*
 * Copyright © 2008, 2010 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "terminal-screen-container.h"
#include "terminal-debug.h"
#include "terminal-intl.h"

#include <ctk/ctk.h>

struct _TerminalScreenContainerPrivate
{
	TerminalScreen *screen;
#ifdef USE_SCROLLED_WINDOW
	CtkWidget *scrolled_window;
#else
	CtkWidget *hbox;
	CtkWidget *vscrollbar;
#endif
	CtkPolicyType hscrollbar_policy;
	CtkPolicyType vscrollbar_policy;
	CtkCornerType window_placement;
	guint window_placement_set : 1;
};

enum
{
    PROP_0,
    PROP_SCREEN,
    PROP_HSCROLLBAR_POLICY,
    PROP_VSCROLLBAR_POLICY,
    PROP_WINDOW_PLACEMENT,
    PROP_WINDOW_PLACEMENT_SET
};

G_DEFINE_TYPE_WITH_PRIVATE (TerminalScreenContainer, terminal_screen_container, CTK_TYPE_BOX)

/* helper functions */

static void
terminal_screen_container_set_placement_internal (TerminalScreenContainer *container,
        CtkCornerType corner)
{
	TerminalScreenContainerPrivate *priv = container->priv;

#ifdef USE_SCROLLED_WINDOW
	ctk_scrolled_window_set_placement (CTK_SCROLLED_WINDOW (priv->scrolled_window), corner);
#else
	switch (corner)
	{
	case CTK_CORNER_TOP_LEFT:
	case CTK_CORNER_BOTTOM_LEFT:
		ctk_box_reorder_child (CTK_BOX (priv->hbox), priv->vscrollbar, 1);
		break;
	case CTK_CORNER_TOP_RIGHT:
	case CTK_CORNER_BOTTOM_RIGHT:
		ctk_box_reorder_child (CTK_BOX (priv->hbox), priv->vscrollbar, 0);
		break;
	default:
		g_assert_not_reached ();
	}
#endif

	priv->window_placement = corner;
	g_object_notify (G_OBJECT (container), "window-placement");
}

static void
terminal_screen_container_set_placement_set (TerminalScreenContainer *container,
        gboolean set)
{
	TerminalScreenContainerPrivate *priv = container->priv;

#ifdef USE_SCROLLED_WINDOW
	g_object_set (priv->scrolled_window, "window-placement-set", set, NULL);
#endif

	priv->window_placement_set = set != FALSE;
	g_object_notify (G_OBJECT (container), "window-placement-set");
}

#if defined(USE_SCROLLED_WINDOW) && defined(CAFE_ENABLE_DEBUG)
static void
size_allocate_cb (CtkWidget *widget,
                  CdkRectangle *rect,
                  TerminalScreenContainer *container)
{
	_terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
	                       "[screen %p] scrolled-window size alloc %d : %d\n",
	                       container->priv->screen, rect->width, rect->height);
}
#endif

/* Class implementation */

static void
terminal_screen_container_init (TerminalScreenContainer *container)
{
	TerminalScreenContainerPrivate *priv;

	priv = container->priv = terminal_screen_container_get_instance_private (container);

	priv->hscrollbar_policy = CTK_POLICY_AUTOMATIC;
	priv->vscrollbar_policy = CTK_POLICY_AUTOMATIC;
	priv->window_placement = CTK_CORNER_BOTTOM_RIGHT;
	priv->window_placement_set = FALSE;

	ctk_orientable_set_orientation (CTK_ORIENTABLE (container), CTK_ORIENTATION_VERTICAL);
}

static GObject *
terminal_screen_container_constructor (GType type,
                                       guint n_construct_properties,
                                       GObjectConstructParam *construct_params)
{
	GObject *object;
	TerminalScreenContainer *container;
	TerminalScreenContainerPrivate *priv;

	object = G_OBJECT_CLASS (terminal_screen_container_parent_class)->constructor
	         (type, n_construct_properties, construct_params);

	container = TERMINAL_SCREEN_CONTAINER (object);
	priv = container->priv;

	g_assert (priv->screen != NULL);

#ifdef USE_SCROLLED_WINDOW
	priv->scrolled_window = ctk_scrolled_window_new (NULL, ctk_scrollable_get_vadjustment (CTK_SCROLLABLE (priv->screen)));

	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (priv->scrolled_window),
	                                priv->hscrollbar_policy,
	                                priv->vscrollbar_policy);
	ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (priv->scrolled_window),
	                                     CTK_SHADOW_NONE);
	ctk_container_add (CTK_CONTAINER (priv->scrolled_window), CTK_WIDGET (priv->screen));
	ctk_widget_show (CTK_WIDGET (priv->screen));
	ctk_box_pack_end (CTK_BOX (container), priv->scrolled_window, TRUE, TRUE, 0);
	ctk_widget_show (priv->scrolled_window);

#ifdef CAFE_ENABLE_DEBUG
	g_signal_connect (priv->scrolled_window, "size-allocate", G_CALLBACK (size_allocate_cb), container);
#endif

#else

	priv->hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

	priv->vscrollbar = ctk_scrollbar_new (CTK_ORIENTATION_VERTICAL, ctk_scrollable_get_vadjustment (CTK_SCROLLABLE (priv->screen)));

	ctk_box_pack_start (CTK_BOX (priv->hbox), CTK_WIDGET (priv->screen), TRUE, TRUE, 0);
	ctk_box_pack_start (CTK_BOX (priv->hbox), priv->vscrollbar, FALSE, FALSE, 0);

	ctk_box_pack_end (CTK_BOX (container), priv->hbox, TRUE, TRUE, 0);
	ctk_widget_show_all (priv->hbox);
#endif /* USE_SCROLLED_WINDOW */

	_terminal_screen_update_scrollbar (priv->screen);

	return object;
}

static void
terminal_screen_container_get_property (GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);
	TerminalScreenContainerPrivate *priv = container->priv;

	switch (prop_id)
	{
	case PROP_SCREEN:
		break;
	case PROP_HSCROLLBAR_POLICY:
		g_value_set_enum (value, priv->hscrollbar_policy);
		break;
	case PROP_VSCROLLBAR_POLICY:
		g_value_set_enum (value, priv->vscrollbar_policy);
		break;
	case PROP_WINDOW_PLACEMENT:
		g_value_set_enum (value, priv->window_placement);
		break;
	case PROP_WINDOW_PLACEMENT_SET:
		g_value_set_boolean (value, priv->window_placement_set);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
terminal_screen_container_set_property (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);
	TerminalScreenContainerPrivate *priv = container->priv;

	switch (prop_id)
	{
	case PROP_SCREEN:
		priv->screen = g_value_get_object (value);
		break;
	case PROP_HSCROLLBAR_POLICY:
		terminal_screen_container_set_policy (container,
		                                      g_value_get_enum (value),
		                                      priv->vscrollbar_policy);
		break;
	case PROP_VSCROLLBAR_POLICY:
		terminal_screen_container_set_policy (container,
		                                      priv->hscrollbar_policy,
		                                      g_value_get_enum (value));
		break;
	case PROP_WINDOW_PLACEMENT:
		terminal_screen_container_set_placement_internal (container, g_value_get_enum (value));
		break;
	case PROP_WINDOW_PLACEMENT_SET:
		terminal_screen_container_set_placement_set (container, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
terminal_screen_container_class_init (TerminalScreenContainerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructor = terminal_screen_container_constructor;
	gobject_class->get_property = terminal_screen_container_get_property;
	gobject_class->set_property = terminal_screen_container_set_property;

	g_object_class_install_property
	(gobject_class,
	 PROP_SCREEN,
	 g_param_spec_object ("screen", NULL, NULL,
	                      TERMINAL_TYPE_SCREEN,
	                      G_PARAM_READWRITE |
	                      G_PARAM_CONSTRUCT_ONLY |
	                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
	(gobject_class,
	 PROP_HSCROLLBAR_POLICY,
	 g_param_spec_enum ("hscrollbar-policy", NULL, NULL,
	                    CTK_TYPE_POLICY_TYPE,
	                    CTK_POLICY_AUTOMATIC,
	                    G_PARAM_READWRITE |
	                    G_PARAM_STATIC_STRINGS));
	g_object_class_install_property
	(gobject_class,
	 PROP_VSCROLLBAR_POLICY,
	 g_param_spec_enum ("vscrollbar-policy", NULL, NULL,
	                    CTK_TYPE_POLICY_TYPE,
	                    CTK_POLICY_AUTOMATIC,
	                    G_PARAM_READWRITE |
	                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
	(gobject_class,
	 PROP_WINDOW_PLACEMENT,
	 g_param_spec_enum ("window-placement", NULL, NULL,
	                    CTK_TYPE_CORNER_TYPE,
	                    CTK_CORNER_TOP_LEFT,
	                    G_PARAM_READWRITE |
	                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
	(gobject_class,
	 PROP_WINDOW_PLACEMENT_SET,
	 g_param_spec_boolean ("window-placement-set", NULL, NULL,
	                       FALSE,
	                       G_PARAM_READWRITE |
	                       G_PARAM_STATIC_STRINGS));
}

/* public API */

/**
 * terminal_screen_container_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalScreenContainer for @screen
 */
CtkWidget *
terminal_screen_container_new (TerminalScreen *screen)
{
	return g_object_new (TERMINAL_TYPE_SCREEN_CONTAINER,
	                     "screen", screen,
	                     NULL);
}

/**
 * terminal_screen_container_get_screen:
 * @container: a #TerminalScreenContainer
 *
 * Returns: @container's #TerminalScreen
 */
TerminalScreen *
terminal_screen_container_get_screen (TerminalScreenContainer *container)
{
	g_return_val_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container), NULL);

	return container->priv->screen;
}

/**
 * terminal_screen_container_get_from_screen:
 * @screen: a #TerminalScreenContainerPrivate
 *
 * Returns the #TerminalScreenContainer containing @screen.
 */
TerminalScreenContainer *
terminal_screen_container_get_from_screen (TerminalScreen *screen)
{
	g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

	return TERMINAL_SCREEN_CONTAINER (ctk_widget_get_ancestor (CTK_WIDGET (screen), TERMINAL_TYPE_SCREEN_CONTAINER));
}

/**
 * terminal_screen_container_set_policy:
 * @container: a #TerminalScreenContainer
 * @hpolicy: a #CtkPolicyType
 * @vpolicy: a #CtkPolicyType
 *
 * Sets @container's scrollbar policy.
 */
void
terminal_screen_container_set_policy (TerminalScreenContainer *container,
                                      CtkPolicyType hpolicy G_GNUC_UNUSED,
                                      CtkPolicyType vpolicy)
{
	TerminalScreenContainerPrivate *priv;
	GObject *object;

	g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

	object = G_OBJECT (container);
	priv = container->priv;

	g_object_freeze_notify (object);

	if (priv->hscrollbar_policy != hpolicy)
	{
		priv->hscrollbar_policy = hpolicy;
		g_object_notify (object, "hscrollbar-policy");
	}
	if (priv->vscrollbar_policy != vpolicy)
	{
		priv->vscrollbar_policy = vpolicy;
		g_object_notify (object, "vscrollbar-policy");
	}

#ifdef USE_SCROLLED_WINDOW
	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (priv->scrolled_window), hpolicy, vpolicy);
#else
	switch (vpolicy)
	{
	case CTK_POLICY_NEVER:
		ctk_widget_hide (priv->vscrollbar);
		break;
	case CTK_POLICY_AUTOMATIC:
	case CTK_POLICY_ALWAYS:
		ctk_widget_show (priv->vscrollbar);
		break;
	default:
		g_assert_not_reached ();
	}
#endif

	g_object_thaw_notify (object);
}

/**
 * terminal_screen_container_set_placement:
 * @container: a #TerminalScreenContainer
 * @corner: a #CtkCornerType
 *
 * Sets @container's window placement.
 */
void
terminal_screen_container_set_placement (TerminalScreenContainer *container,
        CtkCornerType corner)
{
	g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

	terminal_screen_container_set_placement_internal (container, corner);
	terminal_screen_container_set_placement_set (container, TRUE);
}
