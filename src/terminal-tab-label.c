/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2007, 2008 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope tab_label it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <ctk/ctk.h>

#include "terminal-intl.h"
#include "terminal-tab-label.h"
#include "terminal-close-button.h"

#define SPACING (4)

struct _TerminalTabLabelPrivate
{
	TerminalScreen *screen;
	CtkWidget *label;
	CtkWidget *close_button;
	gboolean bold;
};

enum
{
    PROP_0,
    PROP_SCREEN
};

enum
{
    CLOSE_BUTTON_CLICKED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (TerminalTabLabel, terminal_tab_label, CTK_TYPE_BOX);

/* helper functions */

static void
close_button_clicked_cb (CtkWidget        *widget G_GNUC_UNUSED,
			 TerminalTabLabel *tab_label)
{
	g_signal_emit (tab_label, signals[CLOSE_BUTTON_CLICKED], 0);
}

static void
sync_tab_label (TerminalScreen *screen,
		GParamSpec     *pspec G_GNUC_UNUSED,
		CtkWidget      *label)
{
	CtkWidget *hbox;
	const char *title;

	title = terminal_screen_get_title (screen);
	hbox = ctk_widget_get_parent (label);

	ctk_label_set_text (CTK_LABEL (label), title);

	ctk_widget_set_tooltip_text (hbox, title);
}

/* public functions */

/* Class implementation */

static void
terminal_tab_label_parent_set (CtkWidget *widget,
                               CtkWidget *old_parent)
{
	void (* parent_set) (CtkWidget *, CtkWidget *) = CTK_WIDGET_CLASS (terminal_tab_label_parent_class)->parent_set;

	if (parent_set)
		parent_set (widget, old_parent);
}

static void
terminal_tab_label_init (TerminalTabLabel *tab_label)
{
	tab_label->priv = terminal_tab_label_get_instance_private (tab_label);
}

static GObject *
terminal_tab_label_constructor (GType type,
                                guint n_construct_properties,
                                GObjectConstructParam *construct_params)
{
	GObject *object;
	TerminalTabLabel *tab_label;
	TerminalTabLabelPrivate *priv;
	CtkWidget *hbox, *label, *close_button;

	object = G_OBJECT_CLASS (terminal_tab_label_parent_class)->constructor
	         (type, n_construct_properties, construct_params);

	tab_label = TERMINAL_TAB_LABEL (object);
	hbox = CTK_WIDGET (tab_label);
	priv = tab_label->priv;

	g_assert (priv->screen != NULL);

	ctk_box_set_spacing (CTK_BOX (hbox), SPACING);

	priv->label = label = ctk_label_new (NULL);

	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.5);
	ctk_label_set_ellipsize (CTK_LABEL (label), PANGO_ELLIPSIZE_END);
	ctk_label_set_single_line_mode (CTK_LABEL (label), TRUE);

	ctk_box_pack_start (CTK_BOX (hbox), label, TRUE, TRUE, 0);

	priv->close_button = close_button = terminal_close_button_new ();
	ctk_widget_set_tooltip_text (close_button, _("Close tab"));

	ctk_box_pack_end (CTK_BOX (hbox), close_button, FALSE, FALSE, 0);

	sync_tab_label (priv->screen, NULL, label);
	g_signal_connect (priv->screen, "notify::title",
	                  G_CALLBACK (sync_tab_label), label);

	g_signal_connect (close_button, "clicked",
	                  G_CALLBACK (close_button_clicked_cb), tab_label);

	ctk_widget_show_all (hbox);

	return object;
}

static void
terminal_tab_label_dispose (GObject *object)
{
	TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);
	TerminalTabLabelPrivate *priv = tab_label->priv;

	if (priv->screen != NULL) {
		g_signal_handlers_disconnect_by_func (priv->screen,
		                                      G_CALLBACK (sync_tab_label),
		                                      priv->label);
		g_object_unref (priv->screen);
		priv->screen = NULL;
	}

	G_OBJECT_CLASS (terminal_tab_label_parent_class)->dispose (object);
}

static void
terminal_tab_label_finalize (GObject *object)
{
//   TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);

	G_OBJECT_CLASS (terminal_tab_label_parent_class)->finalize (object);
}

static void
terminal_tab_label_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);
	TerminalTabLabelPrivate *priv = tab_label->priv;

	switch (prop_id)
	{
	case PROP_SCREEN:
		priv->screen = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
terminal_tab_label_class_init (TerminalTabLabelClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

	gobject_class->constructor = terminal_tab_label_constructor;
	gobject_class->dispose = terminal_tab_label_dispose;
	gobject_class->finalize = terminal_tab_label_finalize;
	gobject_class->set_property = terminal_tab_label_set_property;

	widget_class->parent_set = terminal_tab_label_parent_set;

	signals[CLOSE_BUTTON_CLICKED] =
	    g_signal_new (I_("close-button-clicked"),
	                  G_OBJECT_CLASS_TYPE (gobject_class),
	                  G_SIGNAL_RUN_LAST,
	                  G_STRUCT_OFFSET (TerminalTabLabelClass, close_button_clicked),
	                  NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID,
	                  G_TYPE_NONE,
	                  0);

	g_object_class_install_property
	(gobject_class,
	 PROP_SCREEN,
	 g_param_spec_object ("screen", NULL, NULL,
	                      TERMINAL_TYPE_SCREEN,
	                      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
	                      G_PARAM_CONSTRUCT_ONLY));
}

/* public API */

/**
 * terminal_tab_label_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalTabLabel for @screen
 */
CtkWidget *
terminal_tab_label_new (TerminalScreen *screen)
{
	return g_object_new (TERMINAL_TYPE_TAB_LABEL,
	                     "screen", screen,
	                     NULL);
}

/**
 * terminal_tab_label_set_bold:
 * @tab_label: a #TerminalTabLabel
 * @bold: whether to enable label bolding
 *
 * Sets the tab label text bold, or unbolds it.
 */
void
terminal_tab_label_set_bold (TerminalTabLabel *tab_label,
                             gboolean bold)
{
	TerminalTabLabelPrivate *priv = tab_label->priv;
	PangoAttrList *attr_list;
	PangoAttribute *weight_attr;
	gboolean free_list = FALSE;

	bold = bold != FALSE;
	if (priv->bold == bold)
		return;

	priv->bold = bold;

	attr_list = ctk_label_get_attributes (CTK_LABEL (priv->label));
	if (!attr_list)
	{
		attr_list = pango_attr_list_new ();
		free_list = TRUE;
	}

	if (bold)
		weight_attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	else
		weight_attr = pango_attr_weight_new (PANGO_WEIGHT_NORMAL);

	/* ctk_label_get_attributes() returns the label's internal list,
	 * which we're probably not supposed to modify directly.
	 * It seems to work ok however.
	 */
	pango_attr_list_change (attr_list, weight_attr);

	ctk_label_set_attributes (CTK_LABEL (priv->label), attr_list);

	if (free_list)
		pango_attr_list_unref (attr_list);
}
