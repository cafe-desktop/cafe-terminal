/*
 * terminal-close-button.c
 *
 * Copyright © 2010 - Paolo Borelli
 * Copyright © 2011 - Ignacio Casal Quinteiro
 * Copyright © 2016 - Wolfgang Ulbrich
 *
 * Cafe-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cafe-terminal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "terminal-close-button.h"

struct _TerminalCloseButtonClassPrivate {
	CtkCssProvider *css;
};

G_DEFINE_TYPE_WITH_CODE (TerminalCloseButton, terminal_close_button, CTK_TYPE_BUTTON,
                         g_type_add_class_private (g_define_type_id, sizeof (TerminalCloseButtonClassPrivate)))

static void
terminal_close_button_class_init (TerminalCloseButtonClass *klass)
{
	static const gchar button_style[] =
		"* {\n"
		  "padding: 0;\n"
		"}";

	klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, TERMINAL_TYPE_CLOSE_BUTTON, TerminalCloseButtonClassPrivate);

	klass->priv->css = ctk_css_provider_new ();
	ctk_css_provider_load_from_data (klass->priv->css, button_style, -1, NULL);
}

static void
terminal_close_button_init (TerminalCloseButton *button)
{
	CtkWidget *image;
	CtkStyleContext *context;

	ctk_widget_set_name (CTK_WIDGET (button), "cafe-terminal-tab-close-button");

	image = ctk_image_new_from_icon_name ("window-close", CTK_ICON_SIZE_MENU);
	ctk_widget_show (image);

	ctk_container_add (CTK_CONTAINER (button), image);

	context = ctk_widget_get_style_context (CTK_WIDGET (button));
	ctk_style_context_add_provider (context,
	                                CTK_STYLE_PROVIDER (TERMINAL_CLOSE_BUTTON_GET_CLASS (button)->priv->css),
	                                CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

CtkWidget *
terminal_close_button_new ()
{
	return CTK_WIDGET (g_object_new (TERMINAL_TYPE_CLOSE_BUTTON,
	                                 "relief", CTK_RELIEF_NONE,
	                                 "focus-on-click", FALSE,
	                                 NULL));
}

