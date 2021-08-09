/*
 * Copyright © 2010 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope info_bar it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "terminal-info-bar.h"

#include <ctk/ctk.h>

struct _TerminalInfoBarPrivate
{
	CtkWidget *content_box;
};

G_DEFINE_TYPE_WITH_PRIVATE (TerminalInfoBar, terminal_info_bar, CTK_TYPE_INFO_BAR)

/* helper functions */

static void
terminal_info_bar_init (TerminalInfoBar *bar)
{
	CtkInfoBar *info_bar = CTK_INFO_BAR (bar);
	TerminalInfoBarPrivate *priv;

	priv = bar->priv = terminal_info_bar_get_instance_private (bar);

	priv->content_box = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	ctk_box_pack_start (CTK_BOX (ctk_info_bar_get_content_area (info_bar)),
	                    priv->content_box, TRUE, TRUE, 0);
}

static void
terminal_info_bar_class_init (TerminalInfoBarClass *klass)
{
}

/* public API */

/**
 * terminal_info_bar_new:
 * @type: a #CtkMessageType
 *
 * Returns: a new #TerminalInfoBar for @screen
 */
CtkWidget *
terminal_info_bar_new (CtkMessageType type,
                       const char *first_button_text,
                       ...)
{
	CtkWidget *info_bar;
	va_list args;

	info_bar = g_object_new (TERMINAL_TYPE_INFO_BAR,
	                         "message-type", type,
	                         NULL);

	va_start (args, first_button_text);
	while (first_button_text != NULL)
	{
		int response_id;

		response_id = va_arg (args, int);
		ctk_info_bar_add_button (CTK_INFO_BAR (info_bar),
		                         first_button_text, response_id);

		first_button_text = va_arg (args, const char *);
	}
	va_end (args);

	return info_bar;
}

void
terminal_info_bar_format_text (TerminalInfoBar *bar,
                               const char *format,
                               ...)
{
	TerminalInfoBarPrivate *priv;
	char *text;
	CtkWidget *label;
	va_list args;

	g_return_if_fail (TERMINAL_IS_INFO_BAR (bar));

	priv = bar->priv;

	va_start (args, format);
	text = g_strdup_vprintf (format, args);
	va_end (args);

	label = ctk_label_new (text);
	g_free (text);

	ctk_label_set_line_wrap (CTK_LABEL (label), TRUE);
	ctk_label_set_selectable (CTK_LABEL (label), TRUE);
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.0);

	ctk_box_pack_start (CTK_BOX (priv->content_box), label, FALSE, FALSE, 0);
	ctk_widget_show_all (priv->content_box);
}
