/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2007, 2008, 2009 Christian Persch
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

#include <string.h>
#include <stdlib.h>
#include <ctk/ctk.h>
#include <cdk/cdk.h>
#ifdef CDK_WINDOWING_X11
#include <cdk/cdkx.h>
#endif
#include <cdk/cdkkeysyms.h>

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-encoding.h"
#include "terminal-intl.h"
#include "terminal-screen-container.h"
#include "terminal-search-dialog.h"
#include "terminal-tab-label.h"
#include "terminal-tabs-menu.h"
#include "terminal-util.h"
#include "terminal-window.h"

#ifdef ENABLE_SKEY
#include "skey-popup.h"
#endif

static gboolean detach_tab = FALSE;

struct _TerminalWindowPrivate
{
    CtkActionGroup *action_group;
    CtkUIManager *ui_manager;
    guint ui_id;

    CtkActionGroup *profiles_action_group;
    guint profiles_ui_id;

    CtkActionGroup *encodings_action_group;
    guint encodings_ui_id;

    TerminalTabsMenu *tabs_menu;

    TerminalScreenPopupInfo *popup_info;
    guint remove_popup_info_idle;

    CtkActionGroup *new_terminal_action_group;
    guint new_terminal_ui_id;

    CtkWidget *menubar;
    CtkWidget *notebook;
    CtkWidget *main_vbox;
    TerminalScreen *active_screen;

    /* Size of a character cell in pixels */
    int old_char_width;
    int old_char_height;

    /* Width and height added to the actual terminal grid by "chrome" inside
     * what was traditionally the X11 window: menu bar, title bar,
     * style-provided padding. This must be included when resizing the window
     * and also included in geometry hints. */
    int old_chrome_width;
    int old_chrome_height;

    /* Width and height of the padding around the geometry widget. */
    int old_padding_width;
    int old_padding_height;

    void *old_geometry_widget; /* only used for pointer value as it may be freed */

    CtkWidget *confirm_close_dialog;
    CtkWidget *search_find_dialog;

    guint menubar_visible : 1;
    guint use_default_menubar_visibility : 1;

    /* Compositing manager integration */
    guint have_argb_visual : 1;

    /* Used to clear stray "demands attention" flashing on our window when we
     * unmap and map it to switch to an ARGB visual.
     */
    guint clear_demands_attention : 1;

    guint disposed : 1;
    guint present_on_insert : 1;

    /* Workaround until ctk+ bug #535557 is fixed */
    guint icon_title_set : 1;
    time_t focus_time;

    /* should we copy selection to clibpoard */
    int copy_selection;
};

#define PROFILE_DATA_KEY "GT::Profile"

#define FILE_NEW_TERMINAL_TAB_UI_PATH     "/menubar/File/FileNewTabProfiles"
#define FILE_NEW_TERMINAL_WINDOW_UI_PATH  "/menubar/File/FileNewWindowProfiles"
#define SET_ENCODING_UI_PATH              "/menubar/Terminal/TerminalSetEncoding/EncodingsPH"
#define SET_ENCODING_ACTION_NAME_PREFIX   "TerminalSetEncoding"

#define PROFILES_UI_PATH        "/menubar/Terminal/TerminalProfiles/ProfilesPH"
#define PROFILES_POPUP_UI_PATH  "/Popup/PopupTerminalProfiles/ProfilesPH"

#define SIZE_TO_UI_PATH            "/menubar/Terminal/TerminalSizeToPH"
#define SIZE_TO_ACTION_NAME_PREFIX "TerminalSizeTo"

#define STOCK_NEW_TAB     "tab-new"

#define ENCODING_DATA_KEY "encoding"

#if 1
/*
 * We don't want to enable content saving until bte supports it async.
 * So we disable this code for stable versions.
 */
#include "terminal-version.h"

#if (TERMINAL_MINOR_VERSION & 1) != 0
#define ENABLE_SAVE
#else
#undef ENABLE_SAVE
#endif
#endif

static void terminal_window_dispose     (GObject             *object);
static void terminal_window_finalize    (GObject             *object);
static gboolean terminal_window_state_event (CtkWidget            *widget,
        CdkEventWindowState  *event);

static gboolean terminal_window_delete_event (CtkWidget *widget,
        CdkEvent *event,
        gpointer data);
static gboolean terminal_window_focus_in_event (CtkWidget *widget,
                                                CdkEventFocus *event,
                                                gpointer data);

static gboolean notebook_button_press_cb     (CtkWidget *notebook,
        CdkEventButton *event,
        GSettings *settings);
static gboolean window_key_press_cb     (CtkWidget *notebook,
        CdkEventKey *event,
        GSettings *settings);
static gboolean notebook_popup_menu_cb       (CtkWidget *notebook,
        TerminalWindow *window);
static void notebook_page_selected_callback  (CtkWidget       *notebook,
        CtkWidget       *page,
        guint            page_num,
        TerminalWindow  *window);
static void notebook_page_added_callback     (CtkWidget       *notebook,
        CtkWidget       *container,
        guint            page_num,
        TerminalWindow  *window);
static void notebook_page_removed_callback   (CtkWidget       *notebook,
        CtkWidget       *container,
        guint            page_num,
        TerminalWindow  *window);
static gboolean notebook_scroll_event_cb     (CtkWidget      *notebook,
                                              CdkEventScroll *event,
                                              TerminalWindow *window);

/* Menu action callbacks */
static void file_new_window_callback          (CtkAction *action,
        TerminalWindow *window);
static void file_new_tab_callback             (CtkAction *action,
        TerminalWindow *window);
static void file_new_profile_callback         (CtkAction *action,
        TerminalWindow *window);
static void file_close_window_callback        (CtkAction *action,
        TerminalWindow *window);
static void file_save_contents_callback       (CtkAction *action,
        TerminalWindow *window);
static void file_close_tab_callback           (CtkAction *action,
        TerminalWindow *window);
static void edit_copy_callback                (CtkAction *action,
        TerminalWindow *window);
static void edit_paste_callback               (CtkAction *action,
        TerminalWindow *window);
static void edit_select_all_callback          (CtkAction *action,
        TerminalWindow *window);
static void edit_keybindings_callback         (CtkAction *action,
        TerminalWindow *window);
static void edit_profiles_callback            (CtkAction *action,
        TerminalWindow *window);
static void edit_current_profile_callback     (CtkAction *action,
        TerminalWindow *window);
static void view_menubar_toggled_callback     (CtkToggleAction *action,
        TerminalWindow *window);
static void view_fullscreen_toggled_callback  (CtkToggleAction *action,
        TerminalWindow *window);
static void view_zoom_in_callback             (CtkAction *action,
        TerminalWindow *window);
static void view_zoom_out_callback            (CtkAction *action,
        TerminalWindow *window);
static void view_zoom_normal_callback         (CtkAction *action,
        TerminalWindow *window);
static void search_find_callback              (CtkAction *action,
        TerminalWindow *window);
static void search_find_next_callback         (CtkAction *action,
        TerminalWindow *window);
static void search_find_prev_callback         (CtkAction *action,
        TerminalWindow *window);
static void search_clear_highlight_callback   (CtkAction *action,
        TerminalWindow *window);
static void terminal_next_or_previous_profile_cb (CtkAction *action,
        TerminalWindow *window);
static void terminal_set_title_callback       (CtkAction *action,
        TerminalWindow *window);
static void terminal_add_encoding_callback    (CtkAction *action,
        TerminalWindow *window);
static void terminal_reset_callback           (CtkAction *action,
        TerminalWindow *window);
static void terminal_reset_clear_callback     (CtkAction *action,
        TerminalWindow *window);
static void tabs_next_or_previous_tab_cb      (CtkAction *action,
        TerminalWindow *window);
static void tabs_move_left_callback           (CtkAction *action,
        TerminalWindow *window);
static void tabs_move_right_callback          (CtkAction *action,
        TerminalWindow *window);
static void tabs_detach_tab_callback          (CtkAction *action,
        TerminalWindow *window);
static void help_contents_callback        (CtkAction *action,
        TerminalWindow *window);
static void help_about_callback           (CtkAction *action,
        TerminalWindow *window);

static gboolean find_larger_zoom_factor  (double  current,
        double *found);
static gboolean find_smaller_zoom_factor (double  current,
        double *found);

static void terminal_window_show (CtkWidget *widget);

static gboolean confirm_close_window_or_tab (TerminalWindow *window,
        TerminalScreen *screen);

static void
profile_set_callback (TerminalScreen *screen,
                      TerminalProfile *old_profile,
                      TerminalWindow *window);
static void
sync_screen_icon_title (TerminalScreen *screen,
                        GParamSpec *psepc,
                        TerminalWindow *window);

G_DEFINE_TYPE_WITH_PRIVATE (TerminalWindow, terminal_window, CTK_TYPE_WINDOW)

/* Menubar mnemonics & accel settings handling */

static void
app_setting_notify_cb (TerminalApp *app,
                       GParamSpec *pspec,
                       CdkScreen *screen)
{
    CtkSettings *settings;
    const char *prop_name;

    if (pspec)
        prop_name = pspec->name;
    else
        prop_name = NULL;

    settings = ctk_settings_get_for_screen (screen);

    if (!prop_name || prop_name == I_(TERMINAL_APP_ENABLE_MNEMONICS))
    {
        gboolean enable_mnemonics;

        g_object_get (app, TERMINAL_APP_ENABLE_MNEMONICS, &enable_mnemonics, NULL);
        g_object_set (settings, "ctk-enable-mnemonics", enable_mnemonics, NULL);
    }

    if (!prop_name || prop_name == I_(TERMINAL_APP_ENABLE_MENU_BAR_ACCEL))
    {
        /* const */ char *saved_menubar_accel;
        gboolean enable_menubar_accel;

        /* FIXME: Once ctk+ bug 507398 is fixed, use that to reset the property instead */
        /* Now this is a bad hack on so many levels. */
        saved_menubar_accel = g_object_get_data (G_OBJECT (settings), "GT::ctk-menu-bar-accel");
        if (!saved_menubar_accel)
        {
            g_object_get (settings, "ctk-menu-bar-accel", &saved_menubar_accel, NULL);
            g_object_set_data_full (G_OBJECT (settings), "GT::ctk-menu-bar-accel",
                                    saved_menubar_accel, (GDestroyNotify) g_free);
        }

        g_object_get (app, TERMINAL_APP_ENABLE_MENU_BAR_ACCEL, &enable_menubar_accel, NULL);
        if (enable_menubar_accel)
            g_object_set (settings, "ctk-menu-bar-accel", saved_menubar_accel, NULL);
        else
            g_object_set (settings, "ctk-menu-bar-accel", NULL, NULL);
    }
}

static void
app_setting_notify_destroy_cb (CdkScreen *screen)
{
    g_signal_handlers_disconnect_by_func (terminal_app_get (),
                                          G_CALLBACK (app_setting_notify_cb),
                                          screen);
}

/* utility functions */

/*
  Derived from XParseGeometry() in X.org

  Copyright 1985, 1986, 1987, 1998  The Open Group

  All Rights Reserved.

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  Except as contained in this notice, the name of The Open Group shall
  not be used in advertising or otherwise to promote the sale, use or
  other dealings in this Software without prior written authorization
  from The Open Group.
*/

/*
 *    XParseGeometry parses strings of the form
 *   "=<width>x<height>{+-}<xoffset>{+-}<yoffset>", where
 *   width, height, xoffset, and yoffset are unsigned integers.
 *   Example: "=80x24+300-49"
 *   The equal sign is optional.
 *   It returns a bitmask that indicates which of the four values
 *   were actually found in the string. For each value found,
 *   the corresponding argument is updated; for each value
 *   not found, the corresponding argument is left unchanged.
 */

/* The following code is from Xlib, and is minimally modified, so we
 * can track any upstream changes if required. Don’t change this
 * code. Or if you do, put in a huge comment marking which thing
 * changed.
 */

static int
terminal_window_ReadInteger (char  *string,
                             char **NextString)
{
    register int Result = 0;
    int Sign = 1;

    if (*string == '+')
	string++;
    else if (*string == '-')
    {
	string++;
	Sign = -1;
    }
    for (; (*string >= '0') && (*string <= '9'); string++)
    {
	Result = (Result * 10) + (*string - '0');
    }
    *NextString = string;
    if (Sign >= 0)
	return (Result);
    else
	return (-Result);
}

/*
 * Bitmask returned by XParseGeometry(). Each bit tells if the corresponding
 * value (x, y, width, height) was found in the parsed string.
 */
#define NoValue         0x0000
#define XValue          0x0001
#define YValue          0x0002
#define WidthValue      0x0004
#define HeightValue     0x0008
#define XNegative       0x0010
#define YNegative       0x0020

static int
terminal_window_XParseGeometry (const char *string,
                                int *x, int *y,
                                unsigned int *width,
                                unsigned int *height)
{
	int mask = NoValue;
	register char *strind;
	unsigned int tempWidth = 0, tempHeight = 0;
	int tempX = 0, tempY = 0;
	char *nextCharacter;

	if ( (string == NULL) || (*string == '\0')) return(mask);
	if (*string == '=')
		string++;  /* ignore possible '=' at beg of geometry spec */

	strind = (char *)string;
	if (*strind != '+' && *strind != '-' && *strind != 'x') {
		tempWidth = terminal_window_ReadInteger(strind, &nextCharacter);
		if (strind == nextCharacter)
		    return (0);
		strind = nextCharacter;
		mask |= WidthValue;
	}

	if (*strind == 'x' || *strind == 'X') {
		strind++;
		tempHeight = terminal_window_ReadInteger(strind, &nextCharacter);
		if (strind == nextCharacter)
		    return (0);
		strind = nextCharacter;
		mask |= HeightValue;
	}

	if ((*strind == '+') || (*strind == '-')) {
		if (*strind == '-') {
			strind++;
			tempX = -terminal_window_ReadInteger(strind, &nextCharacter);
			if (strind == nextCharacter)
			    return (0);
			strind = nextCharacter;
			mask |= XNegative;

		}
		else
		{	strind++;
			tempX = terminal_window_ReadInteger(strind, &nextCharacter);
			if (strind == nextCharacter)
			    return(0);
			strind = nextCharacter;
		}
		mask |= XValue;
		if ((*strind == '+') || (*strind == '-')) {
			if (*strind == '-') {
				strind++;
				tempY = -terminal_window_ReadInteger(strind, &nextCharacter);
				if (strind == nextCharacter)
				    return(0);
				strind = nextCharacter;
				mask |= YNegative;

			}
			else
			{
				strind++;
				tempY = terminal_window_ReadInteger(strind, &nextCharacter);
				if (strind == nextCharacter)
				    return(0);
				strind = nextCharacter;
			}
			mask |= YValue;
		}
	}

	/* If strind isn't at the end of the string the it's an invalid
		geometry specification. */

	if (*strind != '\0') return (0);

	if (mask & XValue)
	    *x = tempX;
	if (mask & YValue)
	    *y = tempY;
	if (mask & WidthValue)
            *width = tempWidth;
	if (mask & HeightValue)
            *height = tempHeight;
	return (mask);
}

static char *
escape_underscores (const char *name)
{
    GString *escaped_name;

    g_assert (name != NULL);

    /* Who'd use more that 4 underscores in a profile name... */
    escaped_name = g_string_sized_new (strlen (name) + 4 + 1);

    while (*name)
    {
        if (*name == '_')
            g_string_append (escaped_name, "__");
        else
            g_string_append_c (escaped_name, *name);
        name++;
    }

    return g_string_free (escaped_name, FALSE);
}

static int
find_tab_num_at_pos (CtkNotebook *notebook,
                     int screen_x,
                     int screen_y)
{
    CtkPositionType tab_pos;
    int page_num = 0;
    CtkNotebook *nb = CTK_NOTEBOOK (notebook);
    CtkWidget *page;
    CtkAllocation tab_allocation;

    tab_pos = ctk_notebook_get_tab_pos (CTK_NOTEBOOK (notebook));

    while ((page = ctk_notebook_get_nth_page (nb, page_num)))
    {
        CtkWidget *tab;
        int max_x, max_y, x_root, y_root;

        tab = ctk_notebook_get_tab_label (nb, page);
        g_return_val_if_fail (tab != NULL, -1);

        if (!ctk_widget_get_mapped (CTK_WIDGET (tab)))
        {
            page_num++;
            continue;
        }

        cdk_window_get_origin (ctk_widget_get_window (tab), &x_root, &y_root);

        ctk_widget_get_allocation (tab, &tab_allocation);
        max_x = x_root + tab_allocation.x + tab_allocation.width;
        max_y = y_root + tab_allocation.y + tab_allocation.height;

        if ((tab_pos == CTK_POS_TOP || tab_pos == CTK_POS_BOTTOM) && screen_x <= max_x)
            return page_num;

        if ((tab_pos == CTK_POS_LEFT || tab_pos == CTK_POS_RIGHT) && screen_y <= max_y)
            return page_num;

        page_num++;
    }

    return -1;
}

static void
terminal_set_profile_toggled_callback (CtkToggleAction *action,
                                       TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalProfile *profile;

    if (!ctk_toggle_action_get_active (action))
        return;

    if (priv->active_screen == NULL)
        return;

    profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
    g_assert (profile);

    if (_terminal_profile_get_forgotten (profile))
        return;

    g_signal_handlers_block_by_func (priv->active_screen, G_CALLBACK (profile_set_callback), window);
    terminal_screen_set_profile (priv->active_screen, profile);
    g_signal_handlers_unblock_by_func (priv->active_screen, G_CALLBACK (profile_set_callback), window);
}

static void
profile_visible_name_notify_cb (TerminalProfile *profile,
                                GParamSpec *pspec,
                                CtkAction *action)
{
    const char *visible_name;
    char *dot, *display_name;
    guint num;

    visible_name = terminal_profile_get_property_string (profile, TERMINAL_PROFILE_VISIBLE_NAME);
    display_name = escape_underscores (visible_name);

    dot = strchr (ctk_action_get_name (action), '.');
    if (dot != NULL)
    {
        char *free_me;

        num = g_ascii_strtoll (dot + 1, NULL, 10);

        free_me = display_name;
        if (num < 10)
            /* Translators: This is the label of a menu item to choose a profile.
             * _%d is used as the accelerator (with d between 1 and 9), and
             * the %s is the name of the terminal profile.
             */
            display_name = g_strdup_printf (_("_%d. %s"), num, display_name);
        else if (num < 36)
            /* Translators: This is the label of a menu item to choose a profile.
             * _%c is used as the accelerator (it will be a character between A and Z),
             * and the %s is the name of the terminal profile.
             */
            display_name = g_strdup_printf (_("_%c. %s"), ('A' + num - 10), display_name);
        else
            free_me = NULL;

        g_free (free_me);
    }

    g_object_set (action, "label", display_name, NULL);
    g_free (display_name);
}

static void
disconnect_profiles_from_actions_in_group (CtkActionGroup *action_group)
{
    GList *actions, *l;

    actions = ctk_action_group_list_actions (action_group);
    for (l = actions; l != NULL; l = l->next)
    {
        GObject *action = G_OBJECT (l->data);
        TerminalProfile *profile;

        profile = g_object_get_data (action, PROFILE_DATA_KEY);
        if (!profile)
            continue;

        g_signal_handlers_disconnect_by_func (profile, G_CALLBACK (profile_visible_name_notify_cb), action);
    }
    g_list_free (actions);
}

static void
terminal_window_update_set_profile_menu_active_profile (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalProfile *new_active_profile;
    GList *actions, *l;

    if (!priv->profiles_action_group)
        return;

    if (!priv->active_screen)
        return;

    new_active_profile = terminal_screen_get_profile (priv->active_screen);

    actions = ctk_action_group_list_actions (priv->profiles_action_group);
    for (l = actions; l != NULL; l = l->next)
    {
        GObject *action = G_OBJECT (l->data);
        TerminalProfile *profile;

        profile = g_object_get_data (action, PROFILE_DATA_KEY);
        if (profile != new_active_profile)
            continue;

        g_signal_handlers_block_by_func (action, G_CALLBACK (terminal_set_profile_toggled_callback), window);
        ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), TRUE);
        g_signal_handlers_unblock_by_func (action, G_CALLBACK (terminal_set_profile_toggled_callback), window);

        break;
    }
    g_list_free (actions);
}

static void
terminal_window_update_set_profile_menu (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalProfile *active_profile;
    CtkActionGroup *action_group;
    CtkAction *action;
    GList *profiles, *p;
    GSList *group;
    guint n;
    gboolean single_profile;

    /* Remove the old UI */
    if (priv->profiles_ui_id != 0)
    {
        ctk_ui_manager_remove_ui (priv->ui_manager, priv->profiles_ui_id);
        priv->profiles_ui_id = 0;
    }

    if (priv->profiles_action_group != NULL)
    {
        disconnect_profiles_from_actions_in_group (priv->profiles_action_group);
        ctk_ui_manager_remove_action_group (priv->ui_manager,
                                            priv->profiles_action_group);
        priv->profiles_action_group = NULL;
    }

    profiles = terminal_app_get_profile_list (terminal_app_get ());

    action = ctk_action_group_get_action (priv->action_group, "TerminalProfiles");
    single_profile = !profiles || profiles->next == NULL; /* list length <= 1 */
    ctk_action_set_sensitive (action, !single_profile);
    if (profiles == NULL)
        return;

    if (priv->active_screen)
        active_profile = terminal_screen_get_profile (priv->active_screen);
    else
        active_profile = NULL;

    action_group = priv->profiles_action_group = ctk_action_group_new ("Profiles");
    ctk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
    g_object_unref (action_group);

    priv->profiles_ui_id = ctk_ui_manager_new_merge_id (priv->ui_manager);

    group = NULL;
    n = 0;
    for (p = profiles; p != NULL; p = p->next)
    {
        TerminalProfile *profile = (TerminalProfile *) p->data;
        CtkRadioAction *profile_action;
        char name[32];

        g_snprintf (name, sizeof (name), "TerminalSetProfile%u", n++);

        profile_action = ctk_radio_action_new (name,
                                               NULL,
                                               NULL,
                                               NULL,
                                               n);

        ctk_radio_action_set_group (profile_action, group);
        group = ctk_radio_action_get_group (profile_action);

        if (profile == active_profile)
            ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (profile_action), TRUE);

        g_object_set_data_full (G_OBJECT (profile_action),
                                PROFILE_DATA_KEY,
                                g_object_ref (profile),
                                (GDestroyNotify) g_object_unref);
        profile_visible_name_notify_cb (profile, NULL, CTK_ACTION (profile_action));
        g_signal_connect (profile, "notify::" TERMINAL_PROFILE_VISIBLE_NAME,
                          G_CALLBACK (profile_visible_name_notify_cb), profile_action);
        g_signal_connect (profile_action, "toggled",
                          G_CALLBACK (terminal_set_profile_toggled_callback), window);

        ctk_action_group_add_action (action_group, CTK_ACTION (profile_action));
        g_object_unref (profile_action);

        ctk_ui_manager_add_ui (priv->ui_manager, priv->profiles_ui_id,
                               PROFILES_UI_PATH,
                               name, name,
                               CTK_UI_MANAGER_MENUITEM, FALSE);
        ctk_ui_manager_add_ui (priv->ui_manager, priv->profiles_ui_id,
                               PROFILES_POPUP_UI_PATH,
                               name, name,
                               CTK_UI_MANAGER_MENUITEM, FALSE);
    }

    g_list_free (profiles);
}

static void
terminal_window_create_new_terminal_action (TerminalWindow *window,
        TerminalProfile *profile,
        const char *name,
        guint num,
        GCallback callback)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkAction *action;

    action = ctk_action_new (name, NULL, NULL, NULL);

    g_object_set_data_full (G_OBJECT (action),
                            PROFILE_DATA_KEY,
                            g_object_ref (profile),
                            (GDestroyNotify) g_object_unref);
    profile_visible_name_notify_cb (profile, NULL, action);
    g_signal_connect (profile, "notify::" TERMINAL_PROFILE_VISIBLE_NAME,
                      G_CALLBACK (profile_visible_name_notify_cb), action);
    g_signal_connect (action, "activate", callback, window);

    ctk_action_group_add_action (priv->new_terminal_action_group, action);
    g_object_unref (action);
}

static void
terminal_window_update_new_terminal_menus (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkActionGroup *action_group;
    CtkAction *action;
    GList *profiles, *p;
    guint n;
    gboolean have_single_profile;

    /* Remove the old UI */
    if (priv->new_terminal_ui_id != 0)
    {
        ctk_ui_manager_remove_ui (priv->ui_manager, priv->new_terminal_ui_id);
        priv->new_terminal_ui_id = 0;
    }

    if (priv->new_terminal_action_group != NULL)
    {
        disconnect_profiles_from_actions_in_group (priv->new_terminal_action_group);
        ctk_ui_manager_remove_action_group (priv->ui_manager,
                                            priv->new_terminal_action_group);
        priv->new_terminal_action_group = NULL;
    }

    profiles = terminal_app_get_profile_list (terminal_app_get ());
    have_single_profile = !profiles || !profiles->next;

    action = ctk_action_group_get_action (priv->action_group, "FileNewTab");
    ctk_action_set_visible (action, have_single_profile);
    action = ctk_action_group_get_action (priv->action_group, "FileNewWindow");
    ctk_action_set_visible (action, have_single_profile);

    if (have_single_profile)
    {
        g_list_free (profiles);
        return;
    }

    /* Now build the submenus */

    action_group = priv->new_terminal_action_group = ctk_action_group_new ("NewTerminal");
    ctk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
    g_object_unref (action_group);

    priv->new_terminal_ui_id = ctk_ui_manager_new_merge_id (priv->ui_manager);

    n = 0;
    for (p = profiles; p != NULL; p = p->next)
    {
        TerminalProfile *profile = (TerminalProfile *) p->data;
        char name[32];

        g_snprintf (name, sizeof (name), "FileNewTab.%u", n);
        terminal_window_create_new_terminal_action (window,
                profile,
                name,
                n,
                G_CALLBACK (file_new_tab_callback));

        ctk_ui_manager_add_ui (priv->ui_manager, priv->new_terminal_ui_id,
                               FILE_NEW_TERMINAL_TAB_UI_PATH,
                               name, name,
                               CTK_UI_MANAGER_MENUITEM, FALSE);

        g_snprintf (name, sizeof (name), "FileNewWindow.%u", n);
        terminal_window_create_new_terminal_action (window,
                profile,
                name,
                n,
                G_CALLBACK (file_new_window_callback));

        ctk_ui_manager_add_ui (priv->ui_manager, priv->new_terminal_ui_id,
                               FILE_NEW_TERMINAL_WINDOW_UI_PATH,
                               name, name,
                               CTK_UI_MANAGER_MENUITEM, FALSE);

        ++n;
    }

    g_list_free (profiles);
}

static void
terminal_set_encoding_callback (CtkToggleAction *action,
                                TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalEncoding *encoding;

    if (!ctk_toggle_action_get_active (action))
        return;

    if (priv->active_screen == NULL)
        return;

    encoding = g_object_get_data (G_OBJECT (action), ENCODING_DATA_KEY);
    g_assert (encoding);

    bte_terminal_set_encoding (BTE_TERMINAL (priv->active_screen),
                               terminal_encoding_get_charset (encoding), NULL);
}

static void
terminal_window_update_encoding_menu (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalApp *app;
    CtkActionGroup *action_group;
    GSList *group;
    guint n;
    GSList *encodings, *l;
    const char *charset;
    TerminalEncoding *active_encoding;

    /* Remove the old UI */
    if (priv->encodings_ui_id != 0)
    {
        ctk_ui_manager_remove_ui (priv->ui_manager, priv->encodings_ui_id);
        priv->encodings_ui_id = 0;
    }

    if (priv->encodings_action_group != NULL)
    {
        ctk_ui_manager_remove_action_group (priv->ui_manager,
                                            priv->encodings_action_group);
        priv->encodings_action_group = NULL;
    }

    action_group = priv->encodings_action_group = ctk_action_group_new ("Encodings");
    ctk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
    g_object_unref (action_group);

    priv->encodings_ui_id = ctk_ui_manager_new_merge_id (priv->ui_manager);

    if (priv->active_screen)
        charset = bte_terminal_get_encoding (BTE_TERMINAL (priv->active_screen));
    else
        charset = "current";

    app = terminal_app_get ();
    active_encoding = terminal_app_ensure_encoding (app, charset);

    encodings = terminal_app_get_active_encodings (app);

    if (g_slist_find (encodings, active_encoding) == NULL)
        encodings = g_slist_append (encodings, terminal_encoding_ref (active_encoding));

    group = NULL;
    n = 0;
    for (l = encodings; l != NULL; l = l->next)
    {
        TerminalEncoding *e = (TerminalEncoding *) l->data;
        CtkRadioAction *encoding_action;
        char name[128];
        char *display_name;

        g_snprintf (name, sizeof (name), SET_ENCODING_ACTION_NAME_PREFIX "%s", terminal_encoding_get_id (e));
        display_name = g_strdup_printf ("%s (%s)", e->name, terminal_encoding_get_charset (e));

        encoding_action = ctk_radio_action_new (name,
                                                display_name,
                                                NULL,
                                                NULL,
                                                n);
        g_free (display_name);

        ctk_radio_action_set_group (encoding_action, group);
        group = ctk_radio_action_get_group (encoding_action);

        if (charset && strcmp (terminal_encoding_get_id (e), charset) == 0)
            ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (encoding_action), TRUE);

        g_signal_connect (encoding_action, "toggled",
                          G_CALLBACK (terminal_set_encoding_callback), window);

        g_object_set_data_full (G_OBJECT (encoding_action), ENCODING_DATA_KEY,
                                terminal_encoding_ref (e),
                                (GDestroyNotify) terminal_encoding_unref);

        ctk_action_group_add_action (action_group, CTK_ACTION (encoding_action));
        g_object_unref (encoding_action);

        ctk_ui_manager_add_ui (priv->ui_manager, priv->encodings_ui_id,
                               SET_ENCODING_UI_PATH,
                               name, name,
                               CTK_UI_MANAGER_MENUITEM, FALSE);
    }

    g_slist_foreach (encodings, (GFunc) terminal_encoding_unref, NULL);
    g_slist_free (encodings);
}

static void
terminal_window_update_encoding_menu_active_encoding (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkAction *action;
    char name[128];

    if (!priv->active_screen)
        return;
    if (!priv->encodings_action_group)
        return;

    g_snprintf (name, sizeof (name), SET_ENCODING_ACTION_NAME_PREFIX "%s",
                bte_terminal_get_encoding (BTE_TERMINAL (priv->active_screen)));
    action = ctk_action_group_get_action (priv->encodings_action_group, name);
    if (!action)
        return;

    g_signal_handlers_block_by_func (action, G_CALLBACK (terminal_set_encoding_callback), window);
    ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), TRUE);
    g_signal_handlers_unblock_by_func (action, G_CALLBACK (terminal_set_encoding_callback), window);
}

static void
terminal_size_to_cb (CtkAction *action,
                     TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    const char *name;
    char *end = NULL;
    guint width, height;

    if (priv->active_screen == NULL)
        return;

    name = ctk_action_get_name (action) + strlen (SIZE_TO_ACTION_NAME_PREFIX);
    width = g_ascii_strtoull (name, &end, 10);
    g_assert (end && *end == 'x');
    height = g_ascii_strtoull (end + 1, &end, 10);
    g_assert (end && *end == '\0');

    bte_terminal_set_size (BTE_TERMINAL (priv->active_screen), width, height);

    terminal_window_update_size (window, priv->active_screen, TRUE);
}

static void
terminal_window_update_size_to_menu (TerminalWindow *window)
{
    static const struct
    {
        guint grid_width;
        guint grid_height;
    } predefined_sizes[] =
    {
        { 80, 24 },
        { 80, 43 },
        { 132, 24 },
        { 132, 43 }
    };
    TerminalWindowPrivate *priv = window->priv;
    guint i;

    /* We only install this once, so there's no need for a separate action group
     * and any cleanup + build-new-one action here.
     */

    for (i = 0; i < G_N_ELEMENTS (predefined_sizes); ++i)
    {
        guint grid_width = predefined_sizes[i].grid_width;
        guint grid_height = predefined_sizes[i].grid_height;
        CtkAction *action;
        char name[40];
        char *display_name;

        g_snprintf (name, sizeof (name), SIZE_TO_ACTION_NAME_PREFIX "%ux%u",
                    grid_width, grid_height);

        /* If there are ever more than 9 of these, extend this to use A..Z as mnemonics,
         * like we do for the profiles menu.
         */
        display_name = g_strdup_printf ("_%u. %ux%u", i + 1, grid_width, grid_height);

        action = ctk_action_new (name, display_name, NULL, NULL);
        g_free (display_name);

        g_signal_connect (action, "activate",
                          G_CALLBACK (terminal_size_to_cb), window);

        ctk_action_group_add_action (priv->action_group, action);
        g_object_unref (action);

        ctk_ui_manager_add_ui (priv->ui_manager, priv->ui_id,
                               SIZE_TO_UI_PATH,
                               name, name,
                               CTK_UI_MANAGER_MENUITEM, FALSE);
    }
}

/* Actions stuff */

static void
terminal_window_update_copy_sensitivity (TerminalScreen *screen,
                                         TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkAction *action;
    gboolean can_copy;

    if (screen != priv->active_screen)
        return;

    can_copy = bte_terminal_get_has_selection (BTE_TERMINAL (screen));

    action = ctk_action_group_get_action (priv->action_group, "EditCopy");
    ctk_action_set_sensitive (action, can_copy);

    if (can_copy && priv->copy_selection)
#if BTE_CHECK_VERSION (0, 50, 0)
        bte_terminal_copy_clipboard_format (BTE_TERMINAL(screen), BTE_FORMAT_TEXT);
#else
        bte_terminal_copy_clipboard(BTE_TERMINAL(screen));
#endif
}

static void
terminal_window_update_zoom_sensitivity (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreen *screen;
    CtkAction *action;
    double current, zoom;

    screen = priv->active_screen;
    if (screen == NULL)
        return;

    current = terminal_screen_get_font_scale (screen);

    action = ctk_action_group_get_action (priv->action_group, "ViewZoomOut");
    ctk_action_set_sensitive (action, find_smaller_zoom_factor (current, &zoom));
    action = ctk_action_group_get_action (priv->action_group, "ViewZoomIn");
    ctk_action_set_sensitive (action, find_larger_zoom_factor (current, &zoom));
}

static void
terminal_window_update_search_sensitivity (TerminalScreen *screen,
        TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkAction *action;
    gboolean can_search;

    if (screen != priv->active_screen)
        return;

    can_search = bte_terminal_search_get_regex (BTE_TERMINAL (screen)) != NULL;

    action = ctk_action_group_get_action (priv->action_group, "SearchFindNext");
    ctk_action_set_sensitive (action, can_search);
    action = ctk_action_group_get_action (priv->action_group, "SearchFindPrevious");
    ctk_action_set_sensitive (action, can_search);
    action = ctk_action_group_get_action (priv->action_group, "SearchClearHighlight");
    ctk_action_set_sensitive (action, can_search);
}

static void
update_edit_menu_cb (CtkClipboard *clipboard,
                     CdkAtom *targets,
                     int n_targets,
                     TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkAction *action;
    gboolean can_paste, can_paste_uris;

    can_paste = targets != NULL && ctk_targets_include_text (targets, n_targets);
    can_paste_uris = targets != NULL && ctk_targets_include_uri (targets, n_targets);

    action = ctk_action_group_get_action (priv->action_group, "EditPaste");
    ctk_action_set_sensitive (action, can_paste);
    action = ctk_action_group_get_action (priv->action_group, "EditPasteURIPaths");
    ctk_action_set_visible (action, can_paste_uris);
    ctk_action_set_sensitive (action, can_paste_uris);

    /* Ref was added in ctk_clipboard_request_targets below */
    g_object_unref (window);
}

static void
update_edit_menu(TerminalWindow *window)
{
    CtkClipboard *clipboard;

    clipboard = ctk_widget_get_clipboard (CTK_WIDGET (window), CDK_SELECTION_CLIPBOARD);
    ctk_clipboard_request_targets (clipboard,
                                   (CtkClipboardTargetsReceivedFunc) update_edit_menu_cb,
                                   g_object_ref (window));
}

static void
screen_resize_window_cb (TerminalScreen *screen,
                         guint width,
                         guint height,
                         TerminalWindow* window)
{
    TerminalWindowPrivate *priv = window->priv;
    BteTerminal *terminal = BTE_TERMINAL (screen);
    CtkWidget *widget = CTK_WIDGET (screen);

    /* Don't do anything if we're maximised or fullscreened */
    // FIXME: realized && ... instead?
    if (!ctk_widget_get_realized (widget) ||
            (cdk_window_get_state (ctk_widget_get_window (widget)) & (CDK_WINDOW_STATE_MAXIMIZED | CDK_WINDOW_STATE_FULLSCREEN)) != 0)
        return;

    bte_terminal_set_size (terminal, width, height);

    if (screen != priv->active_screen)
        return;

    terminal_window_update_size (window, screen, TRUE);
}

static void
terminal_window_update_tabs_menu_sensitivity (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkNotebook *notebook = CTK_NOTEBOOK (priv->notebook);
    CtkActionGroup *action_group = priv->action_group;
    CtkAction *action;
    int num_pages, page_num;
    gboolean not_first, not_last;

    if (priv->disposed)
        return;

    num_pages = ctk_notebook_get_n_pages (notebook);
    page_num = ctk_notebook_get_current_page (notebook);
    not_first = page_num > 0;
    not_last = page_num + 1 < num_pages;

    /* Hide the tabs menu in single-tab windows */
    action = ctk_action_group_get_action (action_group, "Tabs");
    ctk_action_set_visible (action, num_pages > 1);

#if 1
    /* NOTE: We always make next/prev actions sensitive except in
     * single-tab windows, so the corresponding shortcut key escape code
     * isn't sent to the terminal. See bug #453193 and bug #138609.
     * This also makes tab cycling work, bug #92139.
     * FIXME: Find a better way to do this.
     */
    action = ctk_action_group_get_action (action_group, "TabsPrevious");
    ctk_action_set_sensitive (action, num_pages > 1);
    action = ctk_action_group_get_action (action_group, "TabsNext");
    ctk_action_set_sensitive (action, num_pages > 1);
#else
    /* This would be correct, but see the comment above. */
    action = ctk_action_group_get_action (action_group, "TabsPrevious");
    ctk_action_set_sensitive (action, not_first);
    action = ctk_action_group_get_action (action_group, "TabsNext");
    ctk_action_set_sensitive (action, not_last);
#endif

    action = ctk_action_group_get_action (action_group, "TabsMoveLeft");
    ctk_action_set_sensitive (action, not_first);
    action = ctk_action_group_get_action (action_group, "TabsMoveRight");
    ctk_action_set_sensitive (action, not_last);
    action = ctk_action_group_get_action (action_group, "TabsDetach");
    ctk_action_set_sensitive (action, num_pages > 1);
    action = ctk_action_group_get_action (action_group, "FileCloseTab");
    ctk_action_set_sensitive (action, num_pages > 1);
}

static void
update_tab_visibility (TerminalWindow *window,
                       int             change)
{
    TerminalWindowPrivate *priv = window->priv;
    gboolean show_tabs;
    guint num;

    num = ctk_notebook_get_n_pages (CTK_NOTEBOOK (priv->notebook));

    show_tabs = (num + change) > 1;
    ctk_notebook_set_show_tabs (CTK_NOTEBOOK (priv->notebook), show_tabs);
}

static CtkNotebook *
handle_tab_droped_on_desktop (CtkNotebook *source_notebook,
                              CtkWidget   *container,
                              gint         x,
                              gint         y,
                              gpointer     data)
{
    TerminalWindow *source_window;
    TerminalWindow *new_window;
    TerminalWindowPrivate *new_priv;

    source_window = TERMINAL_WINDOW (ctk_widget_get_toplevel (CTK_WIDGET (source_notebook)));
    g_return_val_if_fail (TERMINAL_IS_WINDOW (source_window), NULL);

    new_window = terminal_app_new_window (terminal_app_get (),
                                          ctk_widget_get_screen (CTK_WIDGET (source_window)));
    new_priv = new_window->priv;
    new_priv->present_on_insert = TRUE;

    update_tab_visibility (source_window, -1);
    update_tab_visibility (new_window, +1);

    return CTK_NOTEBOOK (new_priv->notebook);
}

/* Terminal screen popup menu handling */

static void
popup_open_url_callback (CtkAction *action,
                         TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreenPopupInfo *info = priv->popup_info;

    if (info == NULL)
        return;

    terminal_util_open_url (CTK_WIDGET (window), info->string, info->flavour,
                            ctk_get_current_event_time ());
}

static void
popup_copy_url_callback (CtkAction *action,
                         TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreenPopupInfo *info = priv->popup_info;
    CtkClipboard *clipboard;

    if (info == NULL)
        return;

    if (info->string == NULL)
        return;

    clipboard = ctk_widget_get_clipboard (CTK_WIDGET (window), CDK_SELECTION_CLIPBOARD);
    ctk_clipboard_set_text (clipboard, info->string, -1);
}

static void
popup_leave_fullscreen_callback (CtkAction *action,
                                 TerminalWindow *window)
{
    ctk_window_unfullscreen (CTK_WINDOW (window));
}

static void
remove_popup_info (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (priv->remove_popup_info_idle != 0)
    {
        g_source_remove (priv->remove_popup_info_idle);
        priv->remove_popup_info_idle = 0;
    }

    if (priv->popup_info != NULL)
    {
        terminal_screen_popup_info_unref (priv->popup_info);
        priv->popup_info = NULL;
    }
}

static gboolean
idle_remove_popup_info (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    priv->remove_popup_info_idle = 0;
    remove_popup_info (window);
    return FALSE;
}

static void
unset_popup_info (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    /* Unref the event from idle since we still need it
     * from the action callbacks which will run before idle.
     */
    if (priv->remove_popup_info_idle == 0 &&
            priv->popup_info != NULL)
    {
        priv->remove_popup_info_idle =
            g_idle_add ((GSourceFunc) idle_remove_popup_info, window);
    }
}

static void
popup_menu_deactivate_callback (CtkWidget *popup,
                                TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkWidget *im_menu_item;

    g_signal_handlers_disconnect_by_func
    (popup, G_CALLBACK (popup_menu_deactivate_callback), window);

    im_menu_item = ctk_ui_manager_get_widget (priv->ui_manager,
                   "/Popup/PopupInputMethods");
    ctk_menu_item_set_submenu (CTK_MENU_ITEM (im_menu_item), NULL);

    unset_popup_info (window);
}

static void
popup_clipboard_targets_received_cb (CtkClipboard *clipboard,
                                     CdkAtom *targets,
                                     int n_targets,
                                     TerminalScreenPopupInfo *info)
{
    TerminalWindow *window = info->window;
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreen *screen = info->screen;
    CtkWidget *popup_menu;
    CtkAction *action;
    gboolean can_paste, can_paste_uris, show_link, show_email_link, show_call_link, show_input_method_menu;
    int n_pages;
    CdkEvent *event;
    CdkSeat *seat;
    CdkDevice *device;

    if (!ctk_widget_get_realized (CTK_WIDGET (screen)))
    {
        terminal_screen_popup_info_unref (info);
        return;
    }

    /* Now we know that the screen is realized, we know that the window is still alive */
    remove_popup_info (window);

    priv->popup_info = info; /* adopt the ref added when requesting the clipboard */

    n_pages = ctk_notebook_get_n_pages (CTK_NOTEBOOK (priv->notebook));

    can_paste = targets != NULL && ctk_targets_include_text (targets, n_targets);
    can_paste_uris = targets != NULL && ctk_targets_include_uri (targets, n_targets);
    show_link = info->string != NULL && (info->flavour == FLAVOR_AS_IS || info->flavour == FLAVOR_DEFAULT_TO_HTTP);
    show_email_link = info->string != NULL && info->flavour == FLAVOR_EMAIL;
    show_call_link = info->string != NULL && info->flavour == FLAVOR_VOIP_CALL;

    action = ctk_action_group_get_action (priv->action_group, "PopupSendEmail");
    ctk_action_set_visible (action, show_email_link);
    action = ctk_action_group_get_action (priv->action_group, "PopupCopyEmailAddress");
    ctk_action_set_visible (action, show_email_link);
    action = ctk_action_group_get_action (priv->action_group, "PopupCall");
    ctk_action_set_visible (action, show_call_link);
    action = ctk_action_group_get_action (priv->action_group, "PopupCopyCallAddress");
    ctk_action_set_visible (action, show_call_link);
    action = ctk_action_group_get_action (priv->action_group, "PopupOpenLink");
    ctk_action_set_visible (action, show_link);
    action = ctk_action_group_get_action (priv->action_group, "PopupCopyLinkAddress");
    ctk_action_set_visible (action, show_link);

    action = ctk_action_group_get_action (priv->action_group, "PopupCloseWindow");
    ctk_action_set_visible (action, n_pages <= 1);
    action = ctk_action_group_get_action (priv->action_group, "PopupCloseTab");
    ctk_action_set_visible (action, n_pages > 1);

    action = ctk_action_group_get_action (priv->action_group, "PopupCopy");
    ctk_action_set_sensitive (action, bte_terminal_get_has_selection (BTE_TERMINAL (screen)));
    action = ctk_action_group_get_action (priv->action_group, "PopupPaste");
    ctk_action_set_sensitive (action, can_paste);
    action = ctk_action_group_get_action (priv->action_group, "PopupPasteURIPaths");
    ctk_action_set_visible (action, can_paste_uris);

    g_object_get (ctk_widget_get_settings (CTK_WIDGET (window)),
                  "ctk-show-input-method-menu", &show_input_method_menu,
                  NULL);

    action = ctk_action_group_get_action (priv->action_group, "PopupInputMethods");
    ctk_action_set_visible (action, show_input_method_menu);

    popup_menu = ctk_ui_manager_get_widget (priv->ui_manager, "/Popup");
    g_signal_connect (popup_menu, "deactivate",
                      G_CALLBACK (popup_menu_deactivate_callback), window);

    /* Pseudo activation of the popup menu's action */
    action = ctk_action_group_get_action (priv->action_group, "Popup");
    ctk_action_activate (action);

    if (info->button == 0)
        ctk_menu_shell_select_first (CTK_MENU_SHELL (popup_menu), FALSE);

    if (!ctk_menu_get_attach_widget (CTK_MENU (popup_menu)))
        ctk_menu_attach_to_widget (CTK_MENU (popup_menu),CTK_WIDGET (screen),NULL);

    event = ctk_get_current_event ();

    seat = cdk_display_get_default_seat (cdk_display_get_default());

    device = cdk_seat_get_pointer (seat);

    cdk_event_set_device (event, device);

    ctk_menu_popup_at_pointer (CTK_MENU (popup_menu), (const CdkEvent*) event);

    cdk_event_free (event);
}

static void
screen_show_popup_menu_callback (TerminalScreen *screen,
                                 TerminalScreenPopupInfo *info,
                                 TerminalWindow *window)
{
    CtkClipboard *clipboard;

    g_return_if_fail (info->window == window);

    clipboard = ctk_widget_get_clipboard (CTK_WIDGET (window), CDK_SELECTION_CLIPBOARD);
    ctk_clipboard_request_targets (clipboard,
                                   (CtkClipboardTargetsReceivedFunc) popup_clipboard_targets_received_cb,
                                   terminal_screen_popup_info_ref (info));
}

static gboolean
screen_match_clicked_cb (TerminalScreen *screen,
                         const char *match,
                         int flavour,
                         guint state,
                         TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (screen != priv->active_screen)
        return FALSE;

    switch (flavour)
    {
#ifdef ENABLE_SKEY
    case FLAVOR_SKEY:
        terminal_skey_do_popup (CTK_WINDOW (window), screen, match);
        break;
#endif
    default:
        ctk_widget_grab_focus (CTK_WIDGET (screen));
        terminal_util_open_url (CTK_WIDGET (window), match, flavour,
                                ctk_get_current_event_time ());
        break;
    }

    return TRUE;
}

static void
screen_close_cb (TerminalScreen *screen,
                 TerminalWindow *window)
{
    terminal_window_remove_screen (window, screen);
}

static gboolean
terminal_window_accel_activate_cb (CtkAccelGroup  *accel_group,
                                   GObject        *acceleratable,
                                   guint           keyval,
                                   CdkModifierType modifier,
                                   TerminalWindow *window)
{
    CtkAccelGroupEntry *entries;
    guint n_entries;
    gboolean retval = FALSE;

    entries = ctk_accel_group_query (accel_group, keyval, modifier, &n_entries);
    if (n_entries > 0)
    {
        const char *accel_path;

        accel_path = g_quark_to_string (entries[0].accel_path_quark);

        if (g_str_has_prefix (accel_path, "<Actions>/Main/"))
        {
            const char *action_name;

            /* We want to always consume these accelerators, even if the corresponding
             * action is insensitive, so the corresponding shortcut key escape code
             * isn't sent to the terminal. See bug #453193, bug #138609 and bug #559728.
             * This also makes tab cycling work, bug #92139. (NOT!)
             */

            action_name = I_(accel_path + strlen ("<Actions>/Main/"));

#if 0
            if (ctk_notebook_get_n_pages (CTK_NOTEBOOK (priv->notebook)) > 1 &&
                    (action_name == I_("TabsPrevious") || action_name == I_("TabsNext")))
                retval = TRUE;
            else
#endif
                if (action_name == I_("EditCopy") ||
                        action_name == I_("PopupCopy") ||
                        action_name == I_("EditPaste") ||
                        action_name == I_("PopupPaste"))
                    retval = TRUE;
        }
    }

    return retval;
}

/*****************************************/

#ifdef CAFE_ENABLE_DEBUG
static void
terminal_window_size_allocate_cb (CtkWidget *widget,
                                  CtkAllocation *allocation)
{
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                           "[window %p] size-alloc result %d : %d at (%d, %d)\n",
                           widget,
                           allocation->width, allocation->height,
                           allocation->x, allocation->y);
}
#endif /* CAFE_ENABLE_DEBUG */

static void
terminal_window_realize (CtkWidget *widget)
{
    TerminalWindow *window = TERMINAL_WINDOW (widget);
    TerminalWindowPrivate *priv = window->priv;
#if defined(CDK_WINDOWING_X11) || defined(CDK_WINDOWING_WAYLAND)
    CdkScreen *screen;
    CtkAllocation widget_allocation;
    CdkVisual *visual;

    ctk_widget_get_allocation (widget, &widget_allocation);
    screen = ctk_widget_get_screen (CTK_WIDGET (window));

    if (cdk_screen_is_composited (screen) &&
        (visual = cdk_screen_get_rgba_visual (screen)) != NULL)
    {
          /* Set RGBA visual if possible so BTE can use real transparency */
        ctk_widget_set_visual (CTK_WIDGET (widget), visual);
        priv->have_argb_visual = TRUE;
    }
    else
    {
        ctk_widget_set_visual (CTK_WIDGET (window), cdk_screen_get_system_visual (screen));
        priv->have_argb_visual = FALSE;
    }
#endif

    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                           "[window %p] realize, size %d : %d at (%d, %d)\n",
                           widget,
                           widget_allocation.width, widget_allocation.height,
                           widget_allocation.x, widget_allocation.y);

    CTK_WIDGET_CLASS (terminal_window_parent_class)->realize (widget);

    /* Need to do this now since this requires the window to be realized */
    if (priv->active_screen != NULL)
        sync_screen_icon_title (priv->active_screen, NULL, window);
}

static gboolean
terminal_window_map_event (CtkWidget    *widget,
                           CdkEventAny  *event)
{
    TerminalWindow *window = TERMINAL_WINDOW (widget);
    TerminalWindowPrivate *priv = window->priv;
    gboolean (* map_event) (CtkWidget *, CdkEventAny *) =
        CTK_WIDGET_CLASS (terminal_window_parent_class)->map_event;
    CtkAllocation widget_allocation;

    ctk_widget_get_allocation (widget, &widget_allocation);
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                           "[window %p] map-event, size %d : %d at (%d, %d)\n",
                           widget,
                           widget_allocation.width, widget_allocation.height,
                           widget_allocation.x, widget_allocation.y);

    if (priv->clear_demands_attention)
    {
#ifdef CDK_WINDOWING_X11
        terminal_util_x11_clear_demands_attention (ctk_widget_get_window (widget));
#endif

        priv->clear_demands_attention = FALSE;
    }

    if (map_event)
        return map_event (widget, event);

    return FALSE;
}


static gboolean
terminal_window_state_event (CtkWidget            *widget,
                             CdkEventWindowState  *event)
{
    gboolean (* window_state_event) (CtkWidget *, CdkEventWindowState *event) =
        CTK_WIDGET_CLASS (terminal_window_parent_class)->window_state_event;

    if (event->changed_mask & CDK_WINDOW_STATE_FULLSCREEN)
    {
        TerminalWindow *window = TERMINAL_WINDOW (widget);
        TerminalWindowPrivate *priv = window->priv;
        CtkAction *action;
        gboolean is_fullscreen;

        is_fullscreen = (event->new_window_state & CDK_WINDOW_STATE_FULLSCREEN) != 0;

        action = ctk_action_group_get_action (priv->action_group, "ViewFullscreen");
        ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), is_fullscreen);

        action = ctk_action_group_get_action (priv->action_group, "PopupLeaveFullscreen");
        ctk_action_set_visible (action, is_fullscreen);
    }

    if (window_state_event)
        return window_state_event (widget, event);

    return FALSE;
}

#ifdef CDK_WINDOWING_X11
static void
terminal_window_window_manager_changed_cb (CdkScreen *screen,
        TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkAction *action;
    gboolean supports_fs;

    supports_fs = cdk_x11_screen_supports_net_wm_hint (screen, cdk_atom_intern ("_NET_WM_STATE_FULLSCREEN", FALSE));

    action = ctk_action_group_get_action (priv->action_group, "ViewFullscreen");
    ctk_action_set_sensitive (action, supports_fs);
}
#endif

static void
terminal_window_screen_update (TerminalWindow *window,
                               CdkScreen *screen)
{
    TerminalApp *app;

#ifdef CDK_WINDOWING_X11
    if (screen && CDK_IS_X11_SCREEN (screen))
    {
        terminal_window_window_manager_changed_cb (screen, window);
        g_signal_connect (screen, "window-manager-changed",
                          G_CALLBACK (terminal_window_window_manager_changed_cb), window);
    }
#endif

    if ((gint) (glong) (void *) (g_object_get_data (G_OBJECT (screen), "GT::HasSettingsConnection")))
        return;

    g_object_set_data_full (G_OBJECT (screen), "GT::HasSettingsConnection",
                            GINT_TO_POINTER (TRUE),
                            (GDestroyNotify) app_setting_notify_destroy_cb);

    app = terminal_app_get ();
    app_setting_notify_cb (app, NULL, screen);
    g_signal_connect (app, "notify::" TERMINAL_APP_ENABLE_MNEMONICS,
                      G_CALLBACK (app_setting_notify_cb), screen);
    g_signal_connect (app, "notify::" TERMINAL_APP_ENABLE_MENU_BAR_ACCEL,
                      G_CALLBACK (app_setting_notify_cb), screen);
}

static void
terminal_window_screen_changed (CtkWidget *widget,
                                CdkScreen *previous_screen)
{
    TerminalWindow *window = TERMINAL_WINDOW (widget);
    void (* screen_changed) (CtkWidget *, CdkScreen *) =
        CTK_WIDGET_CLASS (terminal_window_parent_class)->screen_changed;
    CdkScreen *screen;

    if (screen_changed)
        screen_changed (widget, previous_screen);

    screen = ctk_widget_get_screen (widget);
    if (previous_screen == screen)
        return;

#ifdef CDK_WINDOWING_X11
    if (previous_screen && CDK_IS_X11_SCREEN (previous_screen))
    {
        g_signal_handlers_disconnect_by_func (previous_screen,
                                              G_CALLBACK (terminal_window_window_manager_changed_cb),
                                              window);
    }
#endif

    if (!screen)
        return;

    terminal_window_screen_update (window, screen);
}

static void
terminal_window_profile_list_changed_cb (TerminalApp *app,
        TerminalWindow *window)
{
    terminal_window_update_set_profile_menu (window);
    terminal_window_update_new_terminal_menus (window);
}

static void
terminal_window_encoding_list_changed_cb (TerminalApp *app,
        TerminalWindow *window)
{
    terminal_window_update_encoding_menu (window);
}

static void
terminal_window_init (TerminalWindow *window)
{
    const CtkActionEntry menu_entries[] =
    {
        /* Toplevel */
        { "File", NULL, N_("_File"), NULL, NULL, NULL },
        { "FileNewWindowProfiles", "utilities-terminal", N_("Open _Terminal"), NULL, NULL, NULL},
        { "FileNewTabProfiles", STOCK_NEW_TAB, N_("Open Ta_b"), NULL, NULL, NULL },
        { "Edit", NULL, N_("_Edit"), NULL, NULL, NULL },
        { "View", NULL, N_("_View"), NULL, NULL, NULL },
        { "Search", NULL, N_("_Search"), NULL, NULL, NULL },
        { "Terminal", NULL, N_("_Terminal"), NULL, NULL, NULL },
        { "Tabs", NULL, N_("Ta_bs"), NULL, NULL, NULL },
        { "Help", NULL, N_("_Help"), NULL, NULL, NULL },
        { "Popup", NULL, NULL, NULL, NULL, NULL },
        { "NotebookPopup", NULL, "", NULL, NULL, NULL },

        /* File menu */
        {
            "FileNewWindow", "utilities-terminal", N_("Open _Terminal"), "<shift><control>N",
            NULL,
            G_CALLBACK (file_new_window_callback)
        },
        {
            "FileNewTab", STOCK_NEW_TAB, N_("Open Ta_b"), "<shift><control>T",
            NULL,
            G_CALLBACK (file_new_tab_callback)
        },
        {
            "FileNewProfile", "document-open", N_("New _Profile…"), "",
            NULL,
            G_CALLBACK (file_new_profile_callback)
        },
        {
            "FileSaveContents", "document-save", N_("_Save Contents"), "",
            NULL,
            G_CALLBACK (file_save_contents_callback)
        },
        {
            "FileCloseTab", "window-close", N_("C_lose Tab"), "<shift><control>W",
            NULL,
            G_CALLBACK (file_close_tab_callback)
        },
        {
            "FileCloseWindow", "window-close", N_("_Close Window"), "<shift><control>Q",
            NULL,
            G_CALLBACK (file_close_window_callback)
        },

        /* Edit menu */
        {
            "EditCopy", "edit-copy", N_("_Copy"), "<shift><control>C",
            NULL,
            G_CALLBACK (edit_copy_callback)
        },
        {
            "EditPaste", "edit-paste", N_("_Paste"), "<shift><control>V",
            NULL,
            G_CALLBACK (edit_paste_callback)
        },
        {
            "EditPasteURIPaths", "edit-paste", N_("Paste _Filenames"), "",
            NULL,
            G_CALLBACK (edit_paste_callback)
        },
        {
            "EditSelectAll", "edit-select-all", N_("Select _All"), "<shift><control>A",
            NULL,
            G_CALLBACK (edit_select_all_callback)
        },
        {
            "EditProfiles", NULL, N_("P_rofiles…"), NULL,
            NULL,
            G_CALLBACK (edit_profiles_callback)
        },
        {
            "EditKeybindings", NULL, N_("_Keyboard Shortcuts…"), NULL,
            NULL,
            G_CALLBACK (edit_keybindings_callback)
        },
        {
            "EditCurrentProfile", NULL, N_("Pr_ofile Preferences"), NULL,
            NULL,
            G_CALLBACK (edit_current_profile_callback)
        },

        /* View menu */
        {
            "ViewZoomIn", "zoom-in", N_("Zoom _In"), "<control>plus",
            NULL,
            G_CALLBACK (view_zoom_in_callback)
        },
        {
            "ViewZoomOut", "zoom-out", N_("Zoom _Out"), "<control>minus",
            NULL,
            G_CALLBACK (view_zoom_out_callback)
        },
        {
            "ViewZoom100", "zoom-original", N_("_Normal Size"), "<control>0",
            NULL,
            G_CALLBACK (view_zoom_normal_callback)
        },

        /* Search menu */
        {
            "SearchFind", "edit-find", N_("_Find..."), "<shift><control>F",
            NULL,
            G_CALLBACK (search_find_callback)
        },
        {
            "SearchFindNext", NULL, N_("Find Ne_xt"), "<shift><control>H",
            NULL,
            G_CALLBACK (search_find_next_callback)
        },
        {
            "SearchFindPrevious", NULL, N_("Find Pre_vious"), "<shift><control>G",
            NULL,
            G_CALLBACK (search_find_prev_callback)
        },
        {
            "SearchClearHighlight", NULL, N_("_Clear Highlight"), "<shift><control>J",
            NULL,
            G_CALLBACK (search_clear_highlight_callback)
        },
#if 0
        {
            "SearchGoToLine", "go-jump", N_("Go to _Line..."), "<shift><control>I",
            NULL,
            G_CALLBACK (search_goto_line_callback)
        },
        {
            "SearchIncrementalSearch", "edit-find", N_("_Incremental Search..."), "<shift><control>K",
            NULL,
            G_CALLBACK (search_incremental_search_callback)
        },
#endif

        /* Terminal menu */
        { "TerminalProfiles", NULL, N_("Change _Profile"), NULL, NULL, NULL },
        {
            "ProfilePrevious", NULL, N_("_Previous Profile"), "<alt>Page_Up",
            NULL,
            G_CALLBACK (terminal_next_or_previous_profile_cb)
        },
        {
            "ProfileNext", NULL, N_("_Next Profile"), "<alt>Page_Down",
            NULL,
            G_CALLBACK (terminal_next_or_previous_profile_cb)
        },
        {
            "TerminalSetTitle", NULL, N_("_Set Title…"), NULL,
            NULL,
            G_CALLBACK (terminal_set_title_callback)
        },
        { "TerminalSetEncoding", NULL, N_("Set _Character Encoding"), NULL, NULL, NULL },
        {
            "TerminalReset", NULL, N_("_Reset"), NULL,
            NULL,
            G_CALLBACK (terminal_reset_callback)
        },
        {
            "TerminalResetClear", NULL, N_("Reset and C_lear"), NULL,
            NULL,
            G_CALLBACK (terminal_reset_clear_callback)
        },

        /* Terminal/Encodings menu */
        {
            "TerminalAddEncoding", NULL, N_("_Add or Remove…"), NULL,
            NULL,
            G_CALLBACK (terminal_add_encoding_callback)
        },

        /* Tabs menu */
        {
            "TabsPrevious", NULL, N_("_Previous Tab"), "<control>Page_Up",
            NULL,
            G_CALLBACK (tabs_next_or_previous_tab_cb)
        },
        {
            "TabsNext", NULL, N_("_Next Tab"), "<control>Page_Down",
            NULL,
            G_CALLBACK (tabs_next_or_previous_tab_cb)
        },
        {
            "TabsMoveLeft", NULL, N_("Move Tab _Left"), "<shift><control>Page_Up",
            NULL,
            G_CALLBACK (tabs_move_left_callback)
        },
        {
            "TabsMoveRight", NULL, N_("Move Tab _Right"), "<shift><control>Page_Down",
            NULL,
            G_CALLBACK (tabs_move_right_callback)
        },
        {
            "TabsDetach", NULL, N_("_Detach tab"), NULL,
            NULL,
            G_CALLBACK (tabs_detach_tab_callback)
        },

        /* Help menu */
        {
            "HelpContents", "help-browser", N_("_Contents"), "F1",
            NULL,
            G_CALLBACK (help_contents_callback)
        },
        {
            "HelpAbout", "help-about", N_("_About"), NULL,
            NULL,
            G_CALLBACK (help_about_callback)
        },

        /* Popup menu */
        {
            "PopupSendEmail", NULL, N_("_Send Mail To…"), NULL,
            NULL,
            G_CALLBACK (popup_open_url_callback)
        },
        {
            "PopupCopyEmailAddress", NULL, N_("_Copy E-mail Address"), NULL,
            NULL,
            G_CALLBACK (popup_copy_url_callback)
        },
        {
            "PopupCall", NULL, N_("C_all To…"), NULL,
            NULL,
            G_CALLBACK (popup_open_url_callback)
        },
        {
            "PopupCopyCallAddress", NULL, N_("_Copy Call Address"), NULL,
            NULL,
            G_CALLBACK (popup_copy_url_callback)
        },
        {
            "PopupOpenLink", NULL, N_("_Open Link"), NULL,
            NULL,
            G_CALLBACK (popup_open_url_callback)
        },
        {
            "PopupCopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
            NULL,
            G_CALLBACK (popup_copy_url_callback)
        },
        { "PopupTerminalProfiles", NULL, N_("P_rofiles"), NULL, NULL, NULL },
        {
            "PopupCopy", "edit-copy", N_("_Copy"), "",
            NULL,
            G_CALLBACK (edit_copy_callback)
        },
        {
            "PopupPaste", "edit-paste", N_("_Paste"), "",
            NULL,
            G_CALLBACK (edit_paste_callback)
        },
        {
            "PopupPasteURIPaths", "edit-paste", N_("Paste _Filenames"), "",
            NULL,
            G_CALLBACK (edit_paste_callback)
        },
        {
            "PopupNewTerminal", "utilities-terminal", N_("Open _Terminal"), NULL,
            NULL,
            G_CALLBACK (file_new_window_callback)
        },
        {
            "PopupNewTab", NULL, N_("Open Ta_b"), NULL,
            NULL,
            G_CALLBACK (file_new_tab_callback)
        },
        {
            "PopupCloseWindow", NULL, N_("C_lose Window"), NULL,
            NULL,
            G_CALLBACK (file_close_window_callback)
        },
        {
            "PopupCloseTab", NULL, N_("C_lose Tab"), NULL,
            NULL,
            G_CALLBACK (file_close_tab_callback)
        },
        {
            "PopupLeaveFullscreen", NULL, N_("L_eave Full Screen"), NULL,
            NULL,
            G_CALLBACK (popup_leave_fullscreen_callback)
        },
        { "PopupInputMethods", NULL, N_("_Input Methods"), NULL, NULL, NULL }
    };

    const CtkToggleActionEntry toggle_menu_entries[] =
    {
        /* View Menu */
        {
            "ViewMenubar", NULL, N_("Show _Menubar"), NULL,
            NULL,
            G_CALLBACK (view_menubar_toggled_callback),
            FALSE
        },
        {
            "ViewFullscreen", NULL, N_("_Full Screen"), NULL,
            NULL,
            G_CALLBACK (view_fullscreen_toggled_callback),
            FALSE
        }
    };
    TerminalWindowPrivate *priv;
    TerminalApp *app;
    CtkActionGroup *action_group;
    CtkAction *action;
    CtkUIManager *manager;
    GError *error;
    CtkWindowGroup *window_group;
    CtkAccelGroup *accel_group;
    CtkClipboard *clipboard;

    priv = window->priv = terminal_window_get_instance_private (window);

    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK(terminal_window_delete_event),
                      NULL);
    g_signal_connect (G_OBJECT (window), "focus_in_event",
                      G_CALLBACK(terminal_window_focus_in_event),
                      NULL);

#ifdef CAFE_ENABLE_DEBUG
    _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_GEOMETRY)
    {
        g_signal_connect_after (window, "size-allocate", G_CALLBACK (terminal_window_size_allocate_cb), NULL);
    }
#endif

    CtkStyleContext *context;

    context = ctk_widget_get_style_context (CTK_WIDGET (window));
    ctk_style_context_add_class (context, "cafe-terminal");

    ctk_window_set_title (CTK_WINDOW (window), _("Terminal"));

    priv->active_screen = NULL;
    priv->menubar_visible = FALSE;

    priv->main_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
    ctk_container_add (CTK_CONTAINER (window), priv->main_vbox);
    ctk_widget_show (priv->main_vbox);

    priv->notebook = ctk_notebook_new ();
    ctk_notebook_set_scrollable (CTK_NOTEBOOK (priv->notebook), TRUE);
    ctk_notebook_set_show_border (CTK_NOTEBOOK (priv->notebook), FALSE);
    ctk_notebook_set_show_tabs (CTK_NOTEBOOK (priv->notebook), FALSE);
    ctk_notebook_set_group_name (CTK_NOTEBOOK (priv->notebook), I_("cafe-terminal-window"));
    g_signal_connect (priv->notebook, "button-press-event",
                      G_CALLBACK (notebook_button_press_cb), settings_global);
    g_signal_connect (window, "key-press-event",
                      G_CALLBACK (window_key_press_cb), settings_global);
    g_signal_connect (priv->notebook, "popup-menu",
                      G_CALLBACK (notebook_popup_menu_cb), window);
    g_signal_connect_after (priv->notebook, "switch-page",
                            G_CALLBACK (notebook_page_selected_callback), window);
    g_signal_connect_after (priv->notebook, "page-added",
                            G_CALLBACK (notebook_page_added_callback), window);
    g_signal_connect_after (priv->notebook, "page-removed",
                            G_CALLBACK (notebook_page_removed_callback), window);
    g_signal_connect_data (priv->notebook, "page-reordered",
                           G_CALLBACK (terminal_window_update_tabs_menu_sensitivity),
                           window, NULL, G_CONNECT_SWAPPED | G_CONNECT_AFTER);

    ctk_widget_add_events (priv->notebook, CDK_SCROLL_MASK);
    g_signal_connect (priv->notebook, "scroll-event",
                            G_CALLBACK (notebook_scroll_event_cb), window);

    g_signal_connect (priv->notebook, "create-window",
                    G_CALLBACK (handle_tab_droped_on_desktop), window);

    ctk_box_pack_end (CTK_BOX (priv->main_vbox), priv->notebook, TRUE, TRUE, 0);
    ctk_widget_show (priv->notebook);

    priv->old_char_width = -1;
    priv->old_char_height = -1;

    priv->old_chrome_width = -1;
    priv->old_chrome_height = -1;
    priv->old_padding_width = -1;
    priv->old_padding_height = -1;

    priv->old_geometry_widget = NULL;

    /* Create the UI manager */
    manager = priv->ui_manager = ctk_ui_manager_new ();

    accel_group = ctk_ui_manager_get_accel_group (manager);
    ctk_window_add_accel_group (CTK_WINDOW (window), accel_group);
    /* Workaround for bug #453193, bug #138609 and bug #559728 */
    g_signal_connect_after (accel_group, "accel-activate",
                            G_CALLBACK (terminal_window_accel_activate_cb), window);

    /* Create the actions */
    /* Note that this action group name is used in terminal-accels.c; do not change it */
    priv->action_group = action_group = ctk_action_group_new ("Main");
    ctk_action_group_set_translation_domain (action_group, NULL);
    ctk_action_group_add_actions (action_group, menu_entries,
                                  G_N_ELEMENTS (menu_entries), window);
    ctk_action_group_add_toggle_actions (action_group,
                                         toggle_menu_entries,
                                         G_N_ELEMENTS (toggle_menu_entries),
                                         window);
    ctk_ui_manager_insert_action_group (manager, action_group, 0);
    g_object_unref (action_group);

   clipboard = ctk_widget_get_clipboard (CTK_WIDGET (window), CDK_SELECTION_CLIPBOARD);
   g_signal_connect_swapped (clipboard, "owner-change",
                             G_CALLBACK (update_edit_menu), window);
   update_edit_menu (window);
    /* Idem for this action, since the window is not fullscreen. */
    action = ctk_action_group_get_action (priv->action_group, "PopupLeaveFullscreen");
    ctk_action_set_visible (action, FALSE);

#ifndef ENABLE_SAVE
    action = ctk_action_group_get_action (priv->action_group, "FileSaveContents");
    ctk_action_set_visible (action, FALSE);
#endif

    /* Load the UI */
    error = NULL;
    priv->ui_id = ctk_ui_manager_add_ui_from_resource (manager,
                  TERMINAL_RESOURCES_PATH_PREFIX G_DIR_SEPARATOR_S "ui/terminal.xml",
                  &error);
    g_assert_no_error (error);

    priv->menubar = ctk_ui_manager_get_widget (manager, "/menubar");
    ctk_box_pack_start (CTK_BOX (priv->main_vbox),
                        priv->menubar,
                        FALSE, FALSE, 0);

    /* Add tabs menu */
    priv->tabs_menu = terminal_tabs_menu_new (window);

    app = terminal_app_get ();
    terminal_window_profile_list_changed_cb (app, window);
    g_signal_connect (app, "profile-list-changed",
                      G_CALLBACK (terminal_window_profile_list_changed_cb), window);

    terminal_window_encoding_list_changed_cb (app, window);
    g_signal_connect (app, "encoding-list-changed",
                      G_CALLBACK (terminal_window_encoding_list_changed_cb), window);

    terminal_window_set_menubar_visible (window, TRUE);
    priv->use_default_menubar_visibility = TRUE;

    terminal_window_update_size_to_menu (window);

    /* We have to explicitly call this, since screen-changed is NOT
     * emitted for the toplevel the first time!
     */
    terminal_window_screen_update (window, ctk_widget_get_screen (CTK_WIDGET (window)));

    window_group = ctk_window_group_new ();
    ctk_window_group_add_window (window_group, CTK_WINDOW (window));
    g_object_unref (window_group);

    terminal_util_set_unique_role (CTK_WINDOW (window), "cafe-terminal-window");
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

    object_class->dispose = terminal_window_dispose;
    object_class->finalize = terminal_window_finalize;

    widget_class->show = terminal_window_show;
    widget_class->realize = terminal_window_realize;
    widget_class->map_event = terminal_window_map_event;
    widget_class->window_state_event = terminal_window_state_event;
    widget_class->screen_changed = terminal_window_screen_changed;
}

static void
terminal_window_dispose (GObject *object)
{
    TerminalWindow *window = TERMINAL_WINDOW (object);
    TerminalWindowPrivate *priv = window->priv;
    TerminalApp *app;
    CtkClipboard *clipboard;
#ifdef CDK_WINDOWING_X11
    CdkScreen *screen;
#endif

    remove_popup_info (window);

    priv->disposed = TRUE;

    if (priv->tabs_menu)
    {
        g_object_unref (priv->tabs_menu);
        priv->tabs_menu = NULL;
    }

    if (priv->profiles_action_group != NULL)
        disconnect_profiles_from_actions_in_group (priv->profiles_action_group);
    if (priv->new_terminal_action_group != NULL)
        disconnect_profiles_from_actions_in_group (priv->new_terminal_action_group);

    app = terminal_app_get ();
    g_signal_handlers_disconnect_by_func (app,
                                          G_CALLBACK (terminal_window_profile_list_changed_cb),
                                          window);
    g_signal_handlers_disconnect_by_func (app,
                                          G_CALLBACK (terminal_window_encoding_list_changed_cb),
                                          window);
    clipboard = ctk_widget_get_clipboard (CTK_WIDGET (window), CDK_SELECTION_CLIPBOARD);
    g_signal_handlers_disconnect_by_func (clipboard,
                                          G_CALLBACK (update_edit_menu),
                                          window);

#ifdef CDK_WINDOWING_X11
    screen = ctk_widget_get_screen (CTK_WIDGET (object));
    if (screen && CDK_IS_X11_SCREEN (screen))
    {
        g_signal_handlers_disconnect_by_func (screen,
                                              G_CALLBACK (terminal_window_window_manager_changed_cb),
                                              window);
    }
#endif

    G_OBJECT_CLASS (terminal_window_parent_class)->dispose (object);
}

static void
terminal_window_finalize (GObject *object)
{
    TerminalWindow *window = TERMINAL_WINDOW (object);
    TerminalWindowPrivate *priv = window->priv;

    g_object_unref (priv->ui_manager);

    if (priv->confirm_close_dialog)
        ctk_dialog_response (CTK_DIALOG (priv->confirm_close_dialog),
                             CTK_RESPONSE_DELETE_EVENT);

    if (priv->search_find_dialog)
        ctk_dialog_response (CTK_DIALOG (priv->search_find_dialog),
                             CTK_RESPONSE_DELETE_EVENT);

    G_OBJECT_CLASS (terminal_window_parent_class)->finalize (object);
}

static gboolean
terminal_window_delete_event (CtkWidget *widget,
                              CdkEvent *event,
                              gpointer data)
{
    return confirm_close_window_or_tab (TERMINAL_WINDOW (widget), NULL);
}

static gboolean
terminal_window_focus_in_event (CtkWidget *widget,
                                CdkEventFocus *event,
                                gpointer data)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;

  if (event->in)
    priv->focus_time = time(NULL);

  return FALSE;
}

static void
terminal_window_show (CtkWidget *widget)
{
    TerminalWindow *window = TERMINAL_WINDOW (widget);
    CtkAllocation widget_allocation;

    ctk_widget_get_allocation (widget, &widget_allocation);

    TerminalWindowPrivate *priv = window->priv;

    if (priv->active_screen != NULL)
    {
        terminal_window_update_copy_selection (priv->active_screen, window);
#if 0
        /* At this point, we have our CdkScreen, and hence the right
         * font size, so we can go ahead and size the window. */
        terminal_window_update_size (window, priv->active_screen, FALSE);
#endif
    }

    terminal_window_update_geometry (window);

    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                           "[window %p] show, size %d : %d at (%d, %d)\n",
                           widget,
                           widget_allocation.width, widget_allocation.height,
                           widget_allocation.x, widget_allocation.y);

    CTK_WIDGET_CLASS (terminal_window_parent_class)->show (widget);
}

TerminalWindow*
terminal_window_new (void)
{
    return g_object_new (TERMINAL_TYPE_WINDOW, NULL);
}

/**
 * terminal_window_set_is_restored:
 * @window:
 *
 * Marks the window as restored from session.
 */
void
terminal_window_set_is_restored (TerminalWindow *window)
{
    g_return_if_fail (TERMINAL_IS_WINDOW (window));
    g_return_if_fail (!ctk_widget_get_mapped (CTK_WIDGET (window)));

    window->priv->clear_demands_attention = TRUE;
}

static void
profile_set_callback (TerminalScreen *screen,
                      TerminalProfile *old_profile,
                      TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (!ctk_widget_get_realized (CTK_WIDGET (window)))
        return;

    if (screen != priv->active_screen)
        return;

    terminal_window_update_set_profile_menu_active_profile (window);
}

static void
sync_screen_title (TerminalScreen *screen,
                   GParamSpec *psepc,
                   TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (screen != priv->active_screen)
        return;

    ctk_window_set_title (CTK_WINDOW (window), terminal_screen_get_title (screen));
}

static void
sync_screen_icon_title (TerminalScreen *screen,
                        GParamSpec *psepc,
                        TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (!ctk_widget_get_realized (CTK_WIDGET (window)))
        return;

    if (screen != priv->active_screen)
        return;

    if (!terminal_screen_get_icon_title_set (screen))
        return;

    cdk_window_set_icon_name (ctk_widget_get_window (CTK_WIDGET (window)), terminal_screen_get_icon_title (screen));

    priv->icon_title_set = TRUE;
}

static void
sync_screen_icon_title_set (TerminalScreen *screen,
                            GParamSpec *psepc,
                            TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (!ctk_widget_get_realized (CTK_WIDGET (window)))
        return;

    /* No need to restore the title if we never set an icon title */
    if (!priv->icon_title_set)
        return;

    if (screen != priv->active_screen)
        return;

    if (terminal_screen_get_icon_title_set (screen))
        return;

    /* Need to reset the icon name */
    /* FIXME: Once ctk+ bug 535557 is fixed, use that to unset the icon title. */

    g_object_set_qdata (G_OBJECT (ctk_widget_get_window (CTK_WIDGET (window))),
                        g_quark_from_static_string ("cdk-icon-name-set"),
                        GUINT_TO_POINTER (FALSE));
    priv->icon_title_set = FALSE;

    /* Re-setting the right title will be done by the notify::title handler which comes after this one */
}

/* Notebook callbacks */

static void
close_button_clicked_cb (CtkWidget *tab_label,
                         CtkWidget *screen_container)
{
    CtkWidget *toplevel;
    TerminalWindow *window;
    TerminalScreen *screen;

    toplevel = ctk_widget_get_toplevel (screen_container);
    if (!ctk_widget_is_toplevel (toplevel))
        return;

    if (!TERMINAL_IS_WINDOW (toplevel))
        return;

    window = TERMINAL_WINDOW (toplevel);

    screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (screen_container));
    if (confirm_close_window_or_tab (window, screen))
        return;

    terminal_window_remove_screen (window, screen);
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen,
                            int            position)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkWidget *old_window;
    CtkWidget *screen_container, *tab_label;

    old_window = ctk_widget_get_toplevel (CTK_WIDGET (screen));
    if (ctk_widget_is_toplevel (old_window) &&
            TERMINAL_IS_WINDOW (old_window) &&
            TERMINAL_WINDOW (old_window)== window)
        return;

    if (TERMINAL_IS_WINDOW (old_window))
        terminal_window_remove_screen (TERMINAL_WINDOW (old_window), screen);

    screen_container = terminal_screen_container_new (screen);
    ctk_widget_show (screen_container);

    update_tab_visibility (window, +1);

    tab_label = terminal_tab_label_new (screen);
    g_signal_connect (tab_label, "close-button-clicked",
                      G_CALLBACK (close_button_clicked_cb), screen_container);

    ctk_notebook_insert_page (CTK_NOTEBOOK (priv->notebook),
                              screen_container,
                              tab_label,
                              position);
    ctk_container_child_set (CTK_CONTAINER (priv->notebook),
                             screen_container,
                             "tab-expand", TRUE,
                             "tab-fill", TRUE,
                             NULL);
    ctk_notebook_set_tab_reorderable (CTK_NOTEBOOK (priv->notebook),
                                      screen_container,
                                      TRUE);
    ctk_notebook_set_tab_detachable (CTK_NOTEBOOK (priv->notebook),
                                     screen_container,
                                     TRUE);
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreenContainer *screen_container;

    g_return_if_fail (ctk_widget_get_toplevel (CTK_WIDGET (screen)) == CTK_WIDGET (window));

    update_tab_visibility (window, -1);

    screen_container = terminal_screen_container_get_from_screen (screen);
    if (detach_tab)
    {
        ctk_notebook_detach_tab (CTK_NOTEBOOK (priv->notebook),
                                 CTK_WIDGET (screen_container));
        detach_tab = FALSE;
    }
    else
        ctk_container_remove (CTK_CONTAINER (priv->notebook),
                              CTK_WIDGET (screen_container));
}

void
terminal_window_move_screen (TerminalWindow *source_window,
                             TerminalWindow *dest_window,
                             TerminalScreen *screen,
                             int dest_position)
{
    TerminalScreenContainer *screen_container;

    g_return_if_fail (TERMINAL_IS_WINDOW (source_window));
    g_return_if_fail (TERMINAL_IS_WINDOW (dest_window));
    g_return_if_fail (TERMINAL_IS_SCREEN (screen));
    g_return_if_fail (ctk_widget_get_toplevel (CTK_WIDGET (screen)) == CTK_WIDGET (source_window));
    g_return_if_fail (dest_position >= -1);

    screen_container = terminal_screen_container_get_from_screen (screen);
    g_assert (TERMINAL_IS_SCREEN_CONTAINER (screen_container));

    /* We have to ref the screen container as well as the screen,
     * because otherwise removing the screen container from the source
     * window's notebook will cause the container and its containing
     * screen to be ctk_widget_destroy()ed!
     */
    g_object_ref_sink (screen_container);
    g_object_ref_sink (screen);

    detach_tab = TRUE;

    terminal_window_remove_screen (source_window, screen);

    /* Now we can safely remove the screen from the container and let the container die */
    ctk_container_remove (CTK_CONTAINER (ctk_widget_get_parent (CTK_WIDGET (screen))), CTK_WIDGET (screen));
    g_object_unref (screen_container);

    terminal_window_add_screen (dest_window, screen, dest_position);
    ctk_notebook_set_current_page (CTK_NOTEBOOK (dest_window->priv->notebook), dest_position);
    g_object_unref (screen);
}

GList*
terminal_window_list_screen_containers (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    /* We are trusting that CtkNotebook will return pages in order */
    return ctk_container_get_children (CTK_CONTAINER (priv->notebook));
}

void
terminal_window_set_menubar_visible (TerminalWindow *window,
                                     gboolean        setting)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkAction *action;

    /* it's been set now, so don't override when adding a screen.
     * this side effect must happen before we short-circuit below.
     */
    priv->use_default_menubar_visibility = FALSE;

    if (setting == priv->menubar_visible)
        return;

    priv->menubar_visible = setting;

    action = ctk_action_group_get_action (priv->action_group, "ViewMenubar");
    ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action), setting);

    g_object_set (priv->menubar, "visible", setting, NULL);

    /* FIXMEchpe: use ctk_widget_get_realized instead? */
    if (priv->active_screen)
    {
        _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                               "[window %p] setting size after toggling menubar visibility\n",
                               window);

        terminal_window_update_size (window, priv->active_screen, TRUE);
    }
}

gboolean
terminal_window_get_menubar_visible (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    return priv->menubar_visible;
}

CtkWidget *
terminal_window_get_notebook (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

    return CTK_WIDGET (priv->notebook);
}

void
terminal_window_update_size (TerminalWindow *window,
                          TerminalScreen *screen,
                          gboolean        even_if_mapped)
{
    terminal_window_update_size_set_geometry (window, screen,
                                              even_if_mapped, NULL);
}

gboolean
terminal_window_update_size_set_geometry (TerminalWindow *window,
                                          TerminalScreen *screen,
                                          gboolean        even_if_mapped,
                                          gchar          *geometry_string)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkWidget *widget;
    CtkWidget *app;
    gboolean result;
    int geom_result;
    gint force_pos_x = 0, force_pos_y = 0;
    unsigned int force_grid_width = 0, force_grid_height = 0;
    int grid_width, grid_height;
    gint pixel_width, pixel_height;
    CdkWindow *cdk_window;
    CdkGravity pos_gravity;

    cdk_window = ctk_widget_get_window (CTK_WIDGET (window));
    result = TRUE;

    if (cdk_window != NULL &&
        (cdk_window_get_state (cdk_window) &
         (CDK_WINDOW_STATE_MAXIMIZED | CDK_WINDOW_STATE_TILED)))
    {
        /* Don't adjust the size of maximized or tiled (snapped, half-maximized)
         * windows: if we do, there will be ugly gaps of up to 1 character cell
         * around otherwise tiled windows. */
        return result;
    }

    /* be sure our geometry is up-to-date */
    terminal_window_update_geometry (window);

    if (CTK_IS_WIDGET (screen))
        widget = CTK_WIDGET (screen);
    else
        widget = CTK_WIDGET (window);

    app = ctk_widget_get_toplevel (widget);
    g_assert (app != NULL);

    terminal_screen_get_size (screen, &grid_width, &grid_height);
    if (geometry_string != NULL)
    {
        geom_result = terminal_window_XParseGeometry (geometry_string,
                                                      &force_pos_x,
                                                      &force_pos_y,
                                                      &force_grid_width,
                                                      &force_grid_height);
        if (geom_result == NoValue)
            result = FALSE;
    }
    else
        geom_result = NoValue;

    if ((geom_result & WidthValue) != 0)
        grid_width = force_grid_width;
    if ((geom_result & HeightValue) != 0)
        grid_height = force_grid_height;

    /* the "old" struct members were updated by update_geometry */
    pixel_width = priv->old_chrome_width + grid_width * priv->old_char_width;
    pixel_height = priv->old_chrome_height + grid_height * priv->old_char_height;

    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                           "[window %p] size is %dx%d cells of %dx%d px\n",
                           window, grid_width, grid_height,
                           priv->old_char_width, priv->old_char_height);

    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                           "[window %p] %dx%d + %dx%d = %dx%d\n",
                           window, grid_width * priv->old_char_width,
                           grid_height * priv->old_char_height,
                           priv->old_chrome_width, priv->old_chrome_height,
                           pixel_width, pixel_height);

    pos_gravity = CDK_GRAVITY_NORTH_WEST;
    if ((geom_result & XNegative) != 0 && (geom_result & YNegative) != 0)
        pos_gravity = CDK_GRAVITY_SOUTH_EAST;
    else if ((geom_result & XNegative) != 0)
        pos_gravity = CDK_GRAVITY_NORTH_EAST;
    else if ((geom_result & YNegative) != 0)
        pos_gravity = CDK_GRAVITY_SOUTH_WEST;

    if ((geom_result & XValue) == 0)
        force_pos_x = 0;
    if ((geom_result & YValue) == 0)
        force_pos_y = 0;

    if (pos_gravity == CDK_GRAVITY_SOUTH_EAST ||
        pos_gravity == CDK_GRAVITY_NORTH_EAST)
        force_pos_x = WidthOfScreen (cdk_x11_screen_get_xscreen (ctk_widget_get_screen (app))) -
                      pixel_width + force_pos_x;
    if (pos_gravity == CDK_GRAVITY_SOUTH_WEST ||
        pos_gravity == CDK_GRAVITY_SOUTH_EAST)
        force_pos_y = HeightOfScreen (cdk_x11_screen_get_xscreen (ctk_widget_get_screen (app))) -
                      pixel_height + force_pos_y;

    /* we don't let you put a window offscreen; maybe some people would
     * prefer to be able to, but it's kind of a bogus thing to do.
     */
    if (force_pos_x < 0)
        force_pos_x = 0;
    if (force_pos_y < 0)
        force_pos_y = 0;

    if (even_if_mapped && ctk_widget_get_mapped (app))
        ctk_window_resize (CTK_WINDOW (app), pixel_width, pixel_height);
    else
        ctk_window_set_default_size (CTK_WINDOW (app), pixel_width, pixel_height);

    if ((geom_result & XValue) != 0 || (geom_result & YValue) != 0)
    {
        ctk_window_set_gravity (CTK_WINDOW (app), pos_gravity);
        ctk_window_move (CTK_WINDOW (app), force_pos_x, force_pos_y);
    }

    return result;
}

void
terminal_window_switch_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreenContainer *screen_container;
    int page_num;

    screen_container = terminal_screen_container_get_from_screen (screen);
    g_assert (TERMINAL_IS_SCREEN_CONTAINER (screen_container));
    page_num = ctk_notebook_page_num (CTK_NOTEBOOK (priv->notebook),
                                      CTK_WIDGET (screen_container));
    ctk_notebook_set_current_page (CTK_NOTEBOOK (priv->notebook), page_num);
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    return priv->active_screen;
}

static gboolean
notebook_button_press_cb (CtkWidget *widget,
                          CdkEventButton *event,
                          GSettings *settings)
{
    TerminalWindow *window = TERMINAL_WINDOW (ctk_widget_get_toplevel (widget));
    TerminalWindowPrivate *priv = window->priv;
    CtkNotebook *notebook = CTK_NOTEBOOK (widget);
    CtkWidget *tab;
    CtkWidget *menu;
    CtkAction *action;
    int tab_clicked;

    if ((event->type == CDK_BUTTON_PRESS && event->button == 2) &&
            (g_settings_get_boolean (settings, "middle-click-closes-tabs")))
    {
        tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);
        if (tab_clicked >= 0)
        {
            int page_num;
            int before_pages;
            int later_pages;

            before_pages = ctk_notebook_get_n_pages (CTK_NOTEBOOK (notebook));
            page_num = ctk_notebook_get_current_page (notebook);
            ctk_notebook_set_current_page (notebook, tab_clicked);
            TerminalScreen *active_screen = priv->active_screen;

            if (!(confirm_close_window_or_tab (window, active_screen)))
            {
                update_tab_visibility (window, -1);
                ctk_notebook_remove_page(notebook, tab_clicked);
            }

            later_pages = ctk_notebook_get_n_pages (CTK_NOTEBOOK (notebook));

            if (before_pages > later_pages) {
                if (tab_clicked > page_num)
                    ctk_notebook_set_current_page (notebook, page_num);
                else if (tab_clicked < page_num)
                    ctk_notebook_set_current_page (notebook, page_num - 1);
            }
            else
                ctk_notebook_set_current_page (notebook, page_num);

        }
    }

    if (event->type != CDK_BUTTON_PRESS ||
            event->button != 3 ||
            (event->state & ctk_accelerator_get_default_mod_mask ()) != 0)
        return FALSE;

    tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);
    if (tab_clicked < 0)
        return FALSE;

    /* switch to the page the mouse is over */
    ctk_notebook_set_current_page (notebook, tab_clicked);

    action = ctk_action_group_get_action (priv->action_group, "NotebookPopup");
    ctk_action_activate (action);

    menu = ctk_ui_manager_get_widget (priv->ui_manager, "/NotebookPopup");
    if (ctk_menu_get_attach_widget (CTK_MENU (menu)))
      ctk_menu_detach (CTK_MENU (menu));
    tab = ctk_notebook_get_nth_page (notebook, tab_clicked);
    ctk_menu_attach_to_widget (CTK_MENU (menu), tab, NULL);
    ctk_menu_popup_at_pointer (CTK_MENU (menu), NULL);

    return TRUE;
}

static gboolean
window_key_press_cb (CtkWidget *widget,
                     CdkEventKey *event,
                     GSettings *settings)
{
    if ((g_settings_get_boolean (settings, "exit-ctrl-d") == FALSE) &&
        (event->state & CDK_CONTROL_MASK) && (event->keyval == CDK_KEY_d))
        return TRUE;

    if (g_settings_get_boolean (settings, "ctrl-tab-switch-tabs") &&
        event->state & CDK_CONTROL_MASK)
    {
        TerminalWindow *window = TERMINAL_WINDOW (widget);
        TerminalWindowPrivate *priv = window->priv;
        CtkNotebook *notebook = CTK_NOTEBOOK (priv->notebook);

        int pages = ctk_notebook_get_n_pages (notebook);
        int page_num = ctk_notebook_get_current_page (notebook);

        if (event->keyval == CDK_KEY_ISO_Left_Tab)
        {
            if (page_num != 0)
                ctk_notebook_prev_page (notebook);
            else
                ctk_notebook_set_current_page (notebook, (pages - 1));
            return TRUE;
        }

        if (event->keyval == CDK_KEY_Tab)
        {
            if (page_num != (pages -1))
                ctk_notebook_next_page (notebook);
            else
                ctk_notebook_set_current_page (notebook, 0);
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
notebook_popup_menu_cb (CtkWidget *widget,
                        TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkNotebook *notebook = CTK_NOTEBOOK (priv->notebook);
    CtkWidget *focus_widget, *tab, *tab_label, *menu;
    CtkAction *action;
    int page_num;

    focus_widget = ctk_window_get_focus (CTK_WINDOW (window));
    /* Only respond if the notebook is the actual focus */
    if (focus_widget != priv->notebook)
        return FALSE;

    page_num = ctk_notebook_get_current_page (notebook);
    tab = ctk_notebook_get_nth_page (notebook, page_num);
    tab_label = ctk_notebook_get_tab_label (notebook, tab);

    action = ctk_action_group_get_action (priv->action_group, "NotebookPopup");
    ctk_action_activate (action);

    menu = ctk_ui_manager_get_widget (priv->ui_manager, "/NotebookPopup");
    if (ctk_menu_get_attach_widget (CTK_MENU (menu)))
      ctk_menu_detach (CTK_MENU (menu));
    ctk_menu_attach_to_widget (CTK_MENU (menu), tab_label, NULL);
    ctk_menu_popup_at_widget (CTK_MENU (menu),
                              tab_label,
                              CDK_GRAVITY_SOUTH_WEST,
                              CDK_GRAVITY_NORTH_WEST,
                              NULL);
    ctk_menu_shell_select_first (CTK_MENU_SHELL (menu), FALSE);

    return TRUE;
}

static void
notebook_page_selected_callback (CtkWidget       *notebook,
                                 CtkWidget       *page_widget,
                                 guint            page_num,
                                 TerminalWindow  *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkWidget *widget;
    TerminalScreen *screen;
    int old_grid_width, old_grid_height;

    _terminal_debug_print (TERMINAL_DEBUG_MDI,
                           "[window %p] MDI: page-selected %d\n",
                           window, page_num);

    if (priv->disposed)
        return;

    screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (page_widget));
    widget = CTK_WIDGET (screen);
    g_assert (screen != NULL);

    _terminal_debug_print (TERMINAL_DEBUG_MDI,
                           "[window %p] MDI: setting active tab to screen %p (old active screen %p)\n",
                           window, screen, priv->active_screen);

    if (priv->active_screen == screen)
        return;

    if (priv->active_screen != NULL)
    {
        terminal_screen_get_size (priv->active_screen, &old_grid_width, &old_grid_height);

        /* This is so that we maintain the same grid */
        bte_terminal_set_size (BTE_TERMINAL (screen), old_grid_width, old_grid_height);
    }

    /* Workaround to remove ctknotebook's feature of computing its size based on
     * all pages. When the widget is hidden, its size will not be taken into
     * account.
     */
    if (priv->active_screen)
        ctk_widget_hide (CTK_WIDGET (priv->active_screen)); /* FIXME */

    /* Make sure that the widget is no longer hidden due to the workaround */
    ctk_widget_show (widget);

    priv->active_screen = screen;

    /* Override menubar setting if it wasn't restored from session */
    if (priv->use_default_menubar_visibility)
    {
        gboolean setting =
            terminal_profile_get_property_boolean (terminal_screen_get_profile (screen), TERMINAL_PROFILE_DEFAULT_SHOW_MENUBAR);

        terminal_window_set_menubar_visible (window, setting);
    }

    sync_screen_icon_title_set (screen, NULL, window);
    sync_screen_icon_title (screen, NULL, window);
    sync_screen_title (screen, NULL, window);

    /* set size of window to current grid size */
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                           "[window %p] setting size after flipping notebook pages\n",
                           window);
    terminal_window_update_size (window, screen, TRUE);

    terminal_window_update_tabs_menu_sensitivity (window);
    terminal_window_update_encoding_menu_active_encoding (window);
    terminal_window_update_set_profile_menu_active_profile (window);
    terminal_window_update_copy_sensitivity (screen, window);
    terminal_window_update_zoom_sensitivity (window);
    terminal_window_update_search_sensitivity (screen, window);
}

static void
notebook_page_added_callback (CtkWidget       *notebook,
                              CtkWidget       *container,
                              guint            page_num,
                              TerminalWindow  *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreen *screen;
    int pages;

    screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (container));

    _terminal_debug_print (TERMINAL_DEBUG_MDI,
                           "[window %p] MDI: screen %p inserted\n",
                           window, screen);

    g_signal_connect (G_OBJECT (screen),
                      "profile-set",
                      G_CALLBACK (profile_set_callback),
                      window);

    /* FIXME: only connect on the active screen, not all screens! */
    g_signal_connect (screen, "notify::title",
                      G_CALLBACK (sync_screen_title), window);
    g_signal_connect (screen, "notify::icon-title",
                      G_CALLBACK (sync_screen_icon_title), window);
    g_signal_connect (screen, "notify::icon-title-set",
                      G_CALLBACK (sync_screen_icon_title_set), window);
    g_signal_connect (screen, "selection-changed",
                      G_CALLBACK (terminal_window_update_copy_sensitivity), window);

    g_signal_connect (screen, "show-popup-menu",
                      G_CALLBACK (screen_show_popup_menu_callback), window);
    g_signal_connect (screen, "match-clicked",
                      G_CALLBACK (screen_match_clicked_cb), window);
    g_signal_connect (screen, "resize-window",
                      G_CALLBACK (screen_resize_window_cb), window);

    g_signal_connect (screen, "close-screen",
                      G_CALLBACK (screen_close_cb), window);

    update_tab_visibility (window, 0);
    terminal_window_update_tabs_menu_sensitivity (window);
    terminal_window_update_search_sensitivity (screen, window);

#if 0
    /* FIXMEchpe: wtf is this doing? */

    /* If we have an active screen, match its size and zoom */
    if (priv->active_screen)
    {
        int current_width, current_height;
        double scale;

        terminal_screen_get_size (priv->active_screen, &current_width, &current_height);
        bte_terminal_set_size (BTE_TERMINAL (screen), current_width, current_height);

        scale = terminal_screen_get_font_scale (priv->active_screen);
        terminal_screen_set_font_scale (screen, scale);
    }
#endif

    if (priv->present_on_insert)
    {
        ctk_window_present_with_time (CTK_WINDOW (window), ctk_get_current_event_time ());
        priv->present_on_insert = FALSE;
    }
    pages = ctk_notebook_get_n_pages (CTK_NOTEBOOK (notebook));
    if (pages == 2) terminal_window_update_size (window, priv->active_screen, TRUE);
}

static void
notebook_page_removed_callback (CtkWidget       *notebook,
                                CtkWidget       *container,
                                guint            page_num,
                                TerminalWindow  *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreen *screen;
    int pages;

    if (priv->disposed)
        return;

    screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (container));

    _terminal_debug_print (TERMINAL_DEBUG_MDI,
                           "[window %p] MDI: screen %p removed\n",
                           window, screen);

    g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                          G_CALLBACK (profile_set_callback),
                                          window);

    g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                          G_CALLBACK (sync_screen_title),
                                          window);

    g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                          G_CALLBACK (sync_screen_icon_title),
                                          window);

    g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                          G_CALLBACK (sync_screen_icon_title_set),
                                          window);

    g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                          G_CALLBACK (terminal_window_update_copy_sensitivity),
                                          window);

    g_signal_handlers_disconnect_by_func (screen,
                                          G_CALLBACK (screen_show_popup_menu_callback),
                                          window);

    g_signal_handlers_disconnect_by_func (screen,
                                          G_CALLBACK (screen_match_clicked_cb),
                                          window);
    g_signal_handlers_disconnect_by_func (screen,
                                          G_CALLBACK (screen_resize_window_cb),
                                          window);

    g_signal_handlers_disconnect_by_func (screen,
                                          G_CALLBACK (screen_close_cb),
                                          window);

    terminal_window_update_tabs_menu_sensitivity (window);
    update_tab_visibility (window, 0);
    terminal_window_update_search_sensitivity (screen, window);

    pages = ctk_notebook_get_n_pages (CTK_NOTEBOOK (notebook));
    if (pages == 1)
    {
        terminal_window_update_size (window, priv->active_screen, TRUE);
    }
    else if (pages == 0)
    {
        ctk_widget_destroy (CTK_WIDGET (window));
    }
}

void
terminal_window_update_copy_selection (TerminalScreen *screen,
        TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    priv->copy_selection =
        terminal_profile_get_property_boolean (terminal_screen_get_profile (screen),
            TERMINAL_PROFILE_COPY_SELECTION);
}

static gboolean
notebook_scroll_event_cb (CtkWidget      *widget,
                          CdkEventScroll *event,
                          TerminalWindow *window)
{
  CtkNotebook *notebook = CTK_NOTEBOOK (widget);
  CtkWidget *child, *event_widget, *action_widget;

  child = ctk_notebook_get_nth_page (notebook, ctk_notebook_get_current_page (notebook));
  if (child == NULL)
    return FALSE;

  event_widget = ctk_get_event_widget ((CdkEvent *) event);

  /* Ignore scroll events from the content of the page */
  if (event_widget == NULL ||
      event_widget == child ||
      ctk_widget_is_ancestor (event_widget, child))
    return FALSE;

  /* And also from the action widgets */
  action_widget = ctk_notebook_get_action_widget (notebook, CTK_PACK_START);
  if (event_widget == action_widget ||
      (action_widget != NULL && ctk_widget_is_ancestor (event_widget, action_widget)))
    return FALSE;
  action_widget = ctk_notebook_get_action_widget (notebook, CTK_PACK_END);
  if (event_widget == action_widget ||
      (action_widget != NULL && ctk_widget_is_ancestor (event_widget, action_widget)))
    return FALSE;

  switch (event->direction) {
    case CDK_SCROLL_RIGHT:
    case CDK_SCROLL_DOWN:
      ctk_notebook_next_page (notebook);
      break;
    case CDK_SCROLL_LEFT:
    case CDK_SCROLL_UP:
      ctk_notebook_prev_page (notebook);
      break;
    case CDK_SCROLL_SMOOTH:
      switch (ctk_notebook_get_tab_pos (notebook)) {
        case CTK_POS_LEFT:
        case CTK_POS_RIGHT:
          if (event->delta_y > 0)
            ctk_notebook_next_page (notebook);
          else if (event->delta_y < 0)
            ctk_notebook_prev_page (notebook);
          break;
        case CTK_POS_TOP:
        case CTK_POS_BOTTOM:
          if (event->delta_x > 0)
            ctk_notebook_next_page (notebook);
          else if (event->delta_x < 0)
            ctk_notebook_prev_page (notebook);
          break;
      }
      break;
    }

  return TRUE;
}

void
terminal_window_update_geometry (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkWidget *widget;
    CdkGeometry hints;
    CtkBorder padding;
    CtkRequisition toplevel_request, vbox_request, widget_request;
    int grid_width, grid_height;
    int char_width, char_height;
    int chrome_width, chrome_height;

    if (priv->active_screen == NULL)
        return;

    widget = CTK_WIDGET (priv->active_screen);

    /* We set geometry hints from the active term; best thing
     * I can think of to do. Other option would be to try to
     * get some kind of union of all hints from all terms in the
     * window, but that doesn't make too much sense.
     */
    terminal_screen_get_cell_size (priv->active_screen, &char_width, &char_height);

    terminal_screen_get_size (priv->active_screen, &grid_width, &grid_height);
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "%dx%d cells of %dx%d px = %dx%d px\n",
                           grid_width, grid_height, char_width, char_height,
                           char_width * grid_width, char_height * grid_height);

    ctk_style_context_get_padding(ctk_widget_get_style_context (widget),
                                  ctk_widget_get_state_flags (widget),
                                  &padding);

    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "padding = %dx%d px\n",
                           padding.left + padding.right,
                           padding.top + padding.bottom);

    ctk_widget_get_preferred_size (priv->main_vbox, NULL, &vbox_request);
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "content area requests %dx%d px\n",
                           vbox_request.width, vbox_request.height);

    ctk_widget_get_preferred_size (CTK_WIDGET (window), NULL, &toplevel_request);
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "window requests %dx%d px\n",
                           toplevel_request.width, toplevel_request.height);

    chrome_width = vbox_request.width - (char_width * grid_width);
    chrome_height = vbox_request.height - (char_height * grid_height);
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "chrome: %dx%d px\n",
                           chrome_width, chrome_height);

    ctk_widget_get_preferred_size (widget, NULL, &widget_request);
    _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "terminal widget requests %dx%d px\n",
                           widget_request.width, widget_request.height);

    if (char_width != priv->old_char_width ||
             char_height != priv->old_char_height ||
             padding.left + padding.right != priv->old_padding_width ||
             padding.top + padding.bottom != priv->old_padding_height ||
             chrome_width != priv->old_chrome_width ||
             chrome_height != priv->old_chrome_height ||
             widget != CTK_WIDGET (priv->old_geometry_widget))
    {
        hints.base_width = chrome_width;
        hints.base_height = chrome_height;

#define MIN_WIDTH_CHARS 4
#define MIN_HEIGHT_CHARS 1

        hints.width_inc = char_width;
        hints.height_inc = char_height;

        /* min size is min size of the whole window, remember. */
        hints.min_width = hints.base_width + hints.width_inc * MIN_WIDTH_CHARS;
        hints.min_height = hints.base_height + hints.height_inc * MIN_HEIGHT_CHARS;

        ctk_window_set_geometry_hints (CTK_WINDOW (window),
                                       NULL,
                                       &hints,
                                       CDK_HINT_RESIZE_INC |
                                       CDK_HINT_MIN_SIZE |
                                       CDK_HINT_BASE_SIZE);

        _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                               "[window %p] hints: base %dx%d min %dx%d inc %d %d\n",
                               window,
                               hints.base_width,
                               hints.base_height,
                               hints.min_width,
                               hints.min_height,
                               hints.width_inc,
                               hints.height_inc);

        priv->old_geometry_widget = widget;
    }
    else
    {
        _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                               "[window %p] hints: increment unchanged, not setting\n",
                               window);
    }

    /* We need these for the size calculation in terminal_window_update_size(),
     * so we set them unconditionally. */
    priv->old_char_width = char_width;
    priv->old_char_height = char_height;
    priv->old_chrome_width = chrome_width;
    priv->old_chrome_height = chrome_height;
    priv->old_padding_width = padding.left + padding.right;
    priv->old_padding_height = padding.top + padding.bottom;
}

static void
file_new_window_callback (CtkAction *action,
                          TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalApp *app;
    TerminalWindow *new_window;
    TerminalProfile *profile;
    char *new_working_directory;

    app = terminal_app_get ();

    profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
    if (!profile)
        profile = terminal_screen_get_profile (priv->active_screen);
    if (!profile)
        profile = terminal_app_get_profile_for_new_term (app);
    if (!profile)
        return;

    if (_terminal_profile_get_forgotten (profile))
        return;

    new_window = terminal_app_new_window (app, ctk_widget_get_screen (CTK_WIDGET (window)));

    new_working_directory = terminal_screen_get_current_dir_with_fallback (priv->active_screen);
    terminal_app_new_terminal (app, new_window, profile,
                               NULL, NULL,
                               new_working_directory,
                               terminal_screen_get_initial_environment (priv->active_screen),
                               1.0);
    g_free (new_working_directory);

    ctk_window_present (CTK_WINDOW (new_window));
}

static void
file_new_tab_callback (CtkAction *action,
                       TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalApp *app;
    TerminalProfile *profile;
    char *new_working_directory;

    app = terminal_app_get ();
    profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
    if (!profile)
        profile = terminal_screen_get_profile (priv->active_screen);
    if (!profile)
        profile = terminal_app_get_profile_for_new_term (app);
    if (!profile)
        return;

    if (_terminal_profile_get_forgotten (profile))
        return;

    new_working_directory = terminal_screen_get_current_dir_with_fallback (priv->active_screen);
    terminal_app_new_terminal (app, window, profile,
                               NULL, NULL,
                               new_working_directory,
                               terminal_screen_get_initial_environment (priv->active_screen),
                               1.0);
    g_free (new_working_directory);
}

static void
confirm_close_response_cb (CtkWidget *dialog,
                           int response,
                           TerminalWindow *window)
{
    TerminalScreen *screen;

    screen = g_object_get_data (G_OBJECT (dialog), "close-screen");

    ctk_widget_destroy (dialog);

    if (response != CTK_RESPONSE_ACCEPT)
        return;

    if (screen)
        terminal_window_remove_screen (window, screen);
    else
        ctk_widget_destroy (CTK_WIDGET (window));
}

/* Returns: TRUE if closing needs to wait until user confirmation;
 * FALSE if the terminal or window can close immediately.
 */
static gboolean
confirm_close_window_or_tab (TerminalWindow *window,
                             TerminalScreen *screen)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkWidget *dialog;
    gboolean do_confirm;
    gboolean has_processes;
    int n_tabs;
    char *confirm_msg;

    if (priv->confirm_close_dialog)
    {
        /* WTF, already have one? It's modal, so how did that happen? */
        ctk_dialog_response (CTK_DIALOG (priv->confirm_close_dialog),
                             CTK_RESPONSE_DELETE_EVENT);
    }

    do_confirm = g_settings_get_boolean (settings_global, "confirm-window-close");

    if (!do_confirm)
        return FALSE;

    if (screen)
    {
        has_processes = terminal_screen_has_foreground_process (screen);
        n_tabs = 1;
    }
    else
    {
        GList *tabs, *t;

        tabs = terminal_window_list_screen_containers (window);
        n_tabs = g_list_length (tabs);

        for (t = tabs; t != NULL; t = t->next)
        {
            TerminalScreen *terminal_screen;

            terminal_screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (t->data));
            has_processes = terminal_screen_has_foreground_process (terminal_screen);
            if (has_processes)
                break;
        }
        g_list_free (tabs);
    }


    if (has_processes)
    {
        if (n_tabs > 1)
            confirm_msg = _("There are still processes running in some terminals in this window. "
                            "Closing the window will kill all of them.");
        else
            confirm_msg = _("There is still a process running in this terminal. "
                            "Closing the terminal will kill it.");
    } else if (n_tabs > 1)
            confirm_msg = _("There are multiple tabs open in this window.");
    else
        return FALSE;

    dialog = priv->confirm_close_dialog =
                 ctk_message_dialog_new (CTK_WINDOW (window),
                                         CTK_DIALOG_MODAL | CTK_DIALOG_DESTROY_WITH_PARENT,
                                         CTK_MESSAGE_WARNING,
                                         CTK_BUTTONS_CANCEL,
                                         "%s", n_tabs > 1 ? _("Close this window?") : _("Close this terminal?"));

    ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
            "%s", confirm_msg);

    ctk_window_set_title (CTK_WINDOW (dialog), "");

    ctk_dialog_add_button (CTK_DIALOG (dialog), n_tabs > 1 ? _("C_lose Window") : _("C_lose Terminal"), CTK_RESPONSE_ACCEPT);
    ctk_dialog_set_default_response (CTK_DIALOG (dialog), CTK_RESPONSE_ACCEPT);

    g_object_set_data (G_OBJECT (dialog), "close-screen", screen);

    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (ctk_widget_destroyed), &priv->confirm_close_dialog);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (confirm_close_response_cb), window);

    ctk_window_present (CTK_WINDOW (dialog));

    return TRUE;
}

static void
file_close_window_callback (CtkAction *action,
                            TerminalWindow *window)
{
    if (confirm_close_window_or_tab (window, NULL))
        return;

    ctk_widget_destroy (CTK_WIDGET (window));
}

#ifdef ENABLE_SAVE
static void
save_contents_dialog_on_response (CtkDialog *dialog, gint response_id, gpointer terminal)
{
    CtkWindow *parent;
    gchar *filename_uri = NULL;
    GFile *file;
    GOutputStream *stream;
    GError *error = NULL;

    if (response_id != CTK_RESPONSE_ACCEPT)
    {
        ctk_widget_destroy (CTK_WIDGET (dialog));
        return;
    }

    parent = (CtkWindow*) ctk_widget_get_ancestor (CTK_WIDGET (terminal), CTK_TYPE_WINDOW);
    filename_uri = ctk_file_chooser_get_uri (CTK_FILE_CHOOSER (dialog));

    ctk_widget_destroy (CTK_WIDGET (dialog));

    if (filename_uri == NULL)
        return;

    file = g_file_new_for_uri (filename_uri);
    stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

    if (stream)
    {
        /* FIXME
         * Should be replaced with the async version when bte implements that.
         */
        bte_terminal_write_contents_sync (terminal, stream,
                                          BTE_WRITE_DEFAULT,
                                          NULL, &error);
        g_object_unref (stream);
    }

    if (error)
    {
        terminal_util_show_error_dialog (parent, NULL, error,
                                         "%s", _("Could not save contents"));
        g_error_free (error);
    }

    g_object_unref(file);
    g_free(filename_uri);
}
#endif /* ENABLE_SAVE */

static void
file_save_contents_callback (CtkAction *action,
                             TerminalWindow *window)
{
#ifdef ENABLE_SAVE
    CtkWidget *dialog = NULL;
    TerminalWindowPrivate *priv = window->priv;
    BteTerminal *terminal;

    if (!priv->active_screen)
        return;

    terminal = BTE_TERMINAL (priv->active_screen);
    g_return_if_fail (BTE_IS_TERMINAL (terminal));

    dialog = ctk_file_chooser_dialog_new (_("Save as..."),
                                          CTK_WINDOW(window),
                                          CTK_FILE_CHOOSER_ACTION_SAVE,
                                          "ctk-cancel", CTK_RESPONSE_CANCEL,
                                          "ctk-save", CTK_RESPONSE_ACCEPT,
                                          NULL);

    ctk_file_chooser_set_do_overwrite_confirmation (CTK_FILE_CHOOSER (dialog), TRUE);
    /* XXX where should we save to? */
    ctk_file_chooser_set_current_folder (CTK_FILE_CHOOSER (dialog), g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));

    ctk_window_set_transient_for (CTK_WINDOW (dialog), CTK_WINDOW(window));
    ctk_window_set_modal (CTK_WINDOW (dialog), TRUE);
    ctk_window_set_destroy_with_parent (CTK_WINDOW (dialog), TRUE);

    g_signal_connect (dialog, "response", G_CALLBACK (save_contents_dialog_on_response), terminal);
    g_signal_connect (dialog, "delete_event", G_CALLBACK (terminal_util_dialog_response_on_delete), NULL);

    ctk_window_present (CTK_WINDOW (dialog));
#endif /* ENABLE_SAVE */
}

static void
file_close_tab_callback (CtkAction *action,
                         TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalScreen *active_screen = priv->active_screen;

    if (!active_screen)
        return;

    if (confirm_close_window_or_tab (window, active_screen))
        return;

    terminal_window_remove_screen (window, active_screen);
}

static void
edit_copy_callback (CtkAction *action,
                    TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (!priv->active_screen)
        return;

#if BTE_CHECK_VERSION (0, 50, 0)
    bte_terminal_copy_clipboard_format (BTE_TERMINAL (priv->active_screen), BTE_FORMAT_TEXT);
#else
    bte_terminal_copy_clipboard (BTE_TERMINAL (priv->active_screen));
#endif
}

typedef struct
{
    TerminalScreen *screen;
    gboolean uris_as_paths;
} PasteData;

static void
clipboard_uris_received_cb (CtkClipboard *clipboard,
                            /* const */ char **uris,
                            PasteData *data)
{
    char *text;
    gsize len;

    if (!uris)
    {
        g_object_unref (data->screen);
        g_slice_free (PasteData, data);
        return;
    }

    /* This potentially modifies the strings in |uris| but that's ok */
    if (data->uris_as_paths)
        terminal_util_transform_uris_to_quoted_fuse_paths (uris);

    text = terminal_util_concat_uris (uris, &len);
    bte_terminal_feed_child (BTE_TERMINAL (data->screen), text, len);
    g_free (text);

    g_object_unref (data->screen);
    g_slice_free (PasteData, data);
}

static void
clipboard_targets_received_cb (CtkClipboard *clipboard,
                               CdkAtom *targets,
                               int n_targets,
                               PasteData *data)
{
    if (!targets)
    {
        g_object_unref (data->screen);
        g_slice_free (PasteData, data);
        return;
    }

    if (ctk_targets_include_uri (targets, n_targets))
    {
        ctk_clipboard_request_uris (clipboard,
                                    (CtkClipboardURIReceivedFunc) clipboard_uris_received_cb,
                                    data);
        return;
    }
    else /* if (ctk_targets_include_text (targets, n_targets)) */
    {
        bte_terminal_paste_clipboard (BTE_TERMINAL (data->screen));
    }

    g_object_unref (data->screen);
    g_slice_free (PasteData, data);
}

static void
edit_paste_callback (CtkAction *action,
                     TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkClipboard *clipboard;
    PasteData *data;
    const char *name;

    if (!priv->active_screen)
        return;

    clipboard = ctk_widget_get_clipboard (CTK_WIDGET (window), CDK_SELECTION_CLIPBOARD);
    name = ctk_action_get_name (action);

    data = g_slice_new (PasteData);
    data->screen = g_object_ref (priv->active_screen);
    data->uris_as_paths = (name == I_("EditPasteURIPaths") || name == I_("PopupPasteURIPaths"));

    ctk_clipboard_request_targets (clipboard,
                                   (CtkClipboardTargetsReceivedFunc) clipboard_targets_received_cb,
                                   data);
}

static void
edit_select_all_callback (CtkAction *action,
                          TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (!priv->active_screen)
        return;

    bte_terminal_select_all (BTE_TERMINAL (priv->active_screen));
}

static void
edit_keybindings_callback (CtkAction *action,
                           TerminalWindow *window)
{
    terminal_app_edit_keybindings (terminal_app_get (),
                                   CTK_WINDOW (window));
}

static void
edit_current_profile_callback (CtkAction *action,
                               TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    terminal_app_edit_profile (terminal_app_get (),
                               terminal_screen_get_profile (priv->active_screen),
                               CTK_WINDOW (window),
                               NULL);
}

static void
file_new_profile_callback (CtkAction *action,
                           TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    terminal_app_new_profile (terminal_app_get (),
                              terminal_screen_get_profile (priv->active_screen),
                              CTK_WINDOW (window));
}

static void
edit_profiles_callback (CtkAction *action,
                        TerminalWindow *window)
{
    terminal_app_manage_profiles (terminal_app_get (),
                                  CTK_WINDOW (window));
}

static void
view_menubar_toggled_callback (CtkToggleAction *action,
                               TerminalWindow *window)
{
    terminal_window_set_menubar_visible (window, ctk_toggle_action_get_active (action));
}

static void
view_fullscreen_toggled_callback (CtkToggleAction *action,
                                  TerminalWindow *window)
{
    gboolean toggle_action_check;

    g_return_if_fail (ctk_widget_get_realized (CTK_WIDGET (window)));

    toggle_action_check = ctk_toggle_action_get_active (action);
    if (toggle_action_check)
        ctk_window_fullscreen (CTK_WINDOW (window));
    else
        ctk_window_unfullscreen (CTK_WINDOW (window));
}

static const double zoom_factors[] =
{
    TERMINAL_SCALE_MINIMUM,
    TERMINAL_SCALE_XXXXX_SMALL,
    TERMINAL_SCALE_XXXX_SMALL,
    TERMINAL_SCALE_XXX_SMALL,
    PANGO_SCALE_XX_SMALL,
    PANGO_SCALE_X_SMALL,
    PANGO_SCALE_SMALL,
    PANGO_SCALE_MEDIUM,
    PANGO_SCALE_LARGE,
    PANGO_SCALE_X_LARGE,
    PANGO_SCALE_XX_LARGE,
    TERMINAL_SCALE_XXX_LARGE,
    TERMINAL_SCALE_XXXX_LARGE,
    TERMINAL_SCALE_XXXXX_LARGE,
    TERMINAL_SCALE_MAXIMUM
};

static gboolean
find_larger_zoom_factor (double  current,
                         double *found)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (zoom_factors); ++i)
    {
        /* Find a font that's larger than this one */
        if ((zoom_factors[i] - current) > 1e-6)
        {
            *found = zoom_factors[i];
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
find_smaller_zoom_factor (double  current,
                          double *found)
{
    int i;

    i = (int) G_N_ELEMENTS (zoom_factors) - 1;
    while (i >= 0)
    {
        /* Find a font that's smaller than this one */
        if ((current - zoom_factors[i]) > 1e-6)
        {
            *found = zoom_factors[i];
            return TRUE;
        }

        --i;
    }

    return FALSE;
}

static void
view_zoom_in_callback (CtkAction *action,
                       TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    double current;

    if (priv->active_screen == NULL)
        return;

    current = terminal_screen_get_font_scale (priv->active_screen);
    if (!find_larger_zoom_factor (current, &current))
        return;

    terminal_screen_set_font_scale (priv->active_screen, current);
    terminal_window_update_zoom_sensitivity (window);
}

static void
view_zoom_out_callback (CtkAction *action,
                        TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    double current;

    if (priv->active_screen == NULL)
        return;

    current = terminal_screen_get_font_scale (priv->active_screen);
    if (!find_smaller_zoom_factor (current, &current))
        return;

    terminal_screen_set_font_scale (priv->active_screen, current);
    terminal_window_update_zoom_sensitivity (window);
}

static void
view_zoom_normal_callback (CtkAction *action,
                           TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (priv->active_screen == NULL)
        return;

    terminal_screen_set_font_scale (priv->active_screen, PANGO_SCALE_MEDIUM);
    terminal_window_update_zoom_sensitivity (window);
}


static void
search_find_response_callback (CtkWidget *dialog,
                               int        response,
                               gpointer   user_data)
{
    TerminalWindow *window = TERMINAL_WINDOW (user_data);
    TerminalWindowPrivate *priv = window->priv;
    TerminalSearchFlags flags;
    BteRegex *regex;

    if (response != CTK_RESPONSE_ACCEPT)
        return;

    if (G_UNLIKELY (!priv->active_screen))
        return;

    regex = terminal_search_dialog_get_regex (dialog);
    g_return_if_fail (regex != NULL);

    flags = terminal_search_dialog_get_search_flags (dialog);

    bte_terminal_search_set_regex (BTE_TERMINAL (priv->active_screen), regex, 0);
    bte_terminal_search_set_wrap_around (BTE_TERMINAL (priv->active_screen),
                                         (flags & TERMINAL_SEARCH_FLAG_WRAP_AROUND));

    if (flags & TERMINAL_SEARCH_FLAG_BACKWARDS)
        bte_terminal_search_find_previous (BTE_TERMINAL (priv->active_screen));
    else
        bte_terminal_search_find_next (BTE_TERMINAL (priv->active_screen));

    terminal_window_update_search_sensitivity (priv->active_screen, window);
}

static gboolean
search_dialog_delete_event_cb (CtkWidget   *widget,
                               CdkEventAny *event,
                               gpointer     user_data)
{
    /* prevent destruction */
    return TRUE;
}

static void
search_find_callback (CtkAction *action,
                      TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (!priv->search_find_dialog)
    {
        CtkWidget *dialog;

        dialog = priv->search_find_dialog = terminal_search_dialog_new (CTK_WINDOW (window));

        g_signal_connect (dialog, "destroy",
                          G_CALLBACK (ctk_widget_destroyed), &priv->search_find_dialog);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (search_find_response_callback), window);
        g_signal_connect (dialog, "delete-event",
                          G_CALLBACK (search_dialog_delete_event_cb), NULL);
    }

    terminal_search_dialog_present (priv->search_find_dialog);
}

static void
search_find_next_callback (CtkAction *action,
                           TerminalWindow *window)
{
    if (G_UNLIKELY (!window->priv->active_screen))
        return;

    bte_terminal_search_find_next (BTE_TERMINAL (window->priv->active_screen));
}

static void
search_find_prev_callback (CtkAction *action,
                           TerminalWindow *window)
{
    if (G_UNLIKELY (!window->priv->active_screen))
        return;

    bte_terminal_search_find_previous (BTE_TERMINAL (window->priv->active_screen));
}

static void
search_clear_highlight_callback (CtkAction *action,
                                 TerminalWindow *window)
{
    if (G_UNLIKELY (!window->priv->active_screen))
        return;

    bte_terminal_search_set_regex (BTE_TERMINAL (window->priv->active_screen), NULL, 0);
}

static void
terminal_next_or_previous_profile_cb (CtkAction *action,
                              TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalProfile *active_profile, *new_profile = NULL;
    GList *profiles, *p;

    const char *name;
    guint backwards = 0;

    name = ctk_action_get_name (action);
    if (strcmp (name, "ProfilePrevious") == 0)
    {
        backwards = 1;
    }

    profiles = terminal_app_get_profile_list (terminal_app_get ());
    if (profiles == NULL)
        return;

    if (priv->active_screen)
        active_profile = terminal_screen_get_profile (priv->active_screen);
    else
        return;

    for (p = profiles; p != NULL; p = p->next)
    {
        TerminalProfile *profile = (TerminalProfile *) p->data;
        if (profile == active_profile)
        {
            if (backwards) {
                p = p->prev;
                if (p == NULL)
                    p = g_list_last (profiles);
                new_profile = p->data;
                break;
            }
            else
            {
                p = p->next;
                if (p == NULL)
                    p = g_list_first (profiles);
                new_profile = p->data;
                break;
            }
        }
    }

    if (new_profile)
        terminal_screen_set_profile (priv->active_screen, new_profile);

    g_list_free (profiles);
}

static void
terminal_set_title_dialog_response_cb (CtkWidget *dialog,
                                       int response,
                                       TerminalScreen *screen)
{
    if (response == CTK_RESPONSE_OK)
    {
        CtkEntry *entry;
        const char *text;

        entry = CTK_ENTRY (g_object_get_data (G_OBJECT (dialog), "title-entry"));
        text = ctk_entry_get_text (entry);
        terminal_screen_set_user_title (screen, text);
    }

    ctk_widget_destroy (dialog);
}

static void
terminal_set_title_callback (CtkAction *action,
                             TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkWidget *dialog, *message_area, *hbox, *label, *entry;

    if (priv->active_screen == NULL)
        return;

    /* FIXME: hook the screen up so this dialogue closes if the terminal screen closes */

    dialog = ctk_message_dialog_new (CTK_WINDOW (window),
                                     CTK_DIALOG_MODAL | CTK_DIALOG_DESTROY_WITH_PARENT,
                                     CTK_MESSAGE_OTHER,
                                     CTK_BUTTONS_OK_CANCEL,
                                     "%s", "");

    ctk_window_set_title (CTK_WINDOW (dialog), _("Set Title"));
    ctk_window_set_resizable (CTK_WINDOW (dialog), FALSE);
    ctk_window_set_role (CTK_WINDOW (dialog), "cafe-terminal-change-title");
    ctk_dialog_set_default_response (CTK_DIALOG (dialog), CTK_RESPONSE_OK);
    /* Alternative button order was set automatically by CtkMessageDialog */

    g_signal_connect (dialog, "response",
                      G_CALLBACK (terminal_set_title_dialog_response_cb), priv->active_screen);
    g_signal_connect (dialog, "delete-event",
                      G_CALLBACK (terminal_util_dialog_response_on_delete), NULL);

    message_area = ctk_message_dialog_get_message_area (CTK_MESSAGE_DIALOG (dialog));
    ctk_container_foreach (CTK_CONTAINER (message_area), (CtkCallback) ctk_widget_hide, NULL);

    hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
    ctk_box_pack_start (CTK_BOX (message_area), hbox, FALSE, FALSE, 0);

    label = ctk_label_new_with_mnemonic (_("_Title:"));
    ctk_label_set_xalign (CTK_LABEL (label), 0.0);
    ctk_label_set_yalign (CTK_LABEL (label), 0.5);
    ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);

    entry = ctk_entry_new ();
    ctk_entry_set_width_chars (CTK_ENTRY (entry), 32);
    ctk_entry_set_activates_default (CTK_ENTRY (entry), TRUE);
    ctk_label_set_mnemonic_widget (CTK_LABEL (label), entry);
    ctk_box_pack_start (CTK_BOX (hbox), entry, TRUE, TRUE, 0);
    ctk_widget_show_all (hbox);

    ctk_widget_grab_focus (entry);
    ctk_entry_set_text (CTK_ENTRY (entry), terminal_screen_get_raw_title (priv->active_screen));
    ctk_editable_select_region (CTK_EDITABLE (entry), 0, -1);
    g_object_set_data (G_OBJECT (dialog), "title-entry", entry);

    ctk_window_present (CTK_WINDOW (dialog));
}

static void
terminal_add_encoding_callback (CtkAction *action,
                                TerminalWindow *window)
{
    terminal_app_edit_encodings (terminal_app_get (),
                                 CTK_WINDOW (window));
}

static void
terminal_reset_callback (CtkAction *action,
                         TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (priv->active_screen == NULL)
        return;

    bte_terminal_reset (BTE_TERMINAL (priv->active_screen), TRUE, FALSE);
}

static void
terminal_reset_clear_callback (CtkAction *action,
                               TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    if (priv->active_screen == NULL)
        return;

    bte_terminal_reset (BTE_TERMINAL (priv->active_screen), TRUE, TRUE);
}

static void
tabs_next_or_previous_tab_cb (CtkAction *action,
                              TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkNotebookClass *klass;
    const char *name;
    guint keyval = 0;

    name = ctk_action_get_name (action);
    if (strcmp (name, "TabsNext") == 0)
    {
        keyval = CDK_KEY_Page_Down;
    }
    else if (strcmp (name, "TabsPrevious") == 0)
    {
        keyval = CDK_KEY_Page_Up;
    }

    klass = CTK_NOTEBOOK_GET_CLASS (CTK_NOTEBOOK (priv->notebook));
    ctk_binding_set_activate (ctk_binding_set_by_class (klass),
                              keyval,
                              CDK_CONTROL_MASK,
                              G_OBJECT (priv->notebook));
}

static void
tabs_move_left_callback (CtkAction *action,
                         TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkNotebook *notebook = CTK_NOTEBOOK (priv->notebook);
    gint page_num,last_page;
    CtkWidget *page;

    page_num = ctk_notebook_get_current_page (notebook);
    last_page = ctk_notebook_get_n_pages (notebook) - 1;
    page = ctk_notebook_get_nth_page (notebook, page_num);

    ctk_notebook_reorder_child (notebook, page, page_num == 0 ? last_page : page_num - 1);
}

static void
tabs_move_right_callback (CtkAction *action,
                          TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    CtkNotebook *notebook = CTK_NOTEBOOK (priv->notebook);
    gint page_num,last_page;
    CtkWidget *page;

    page_num = ctk_notebook_get_current_page (notebook);
    last_page = ctk_notebook_get_n_pages (notebook) - 1;
    page = ctk_notebook_get_nth_page (notebook, page_num);

    ctk_notebook_reorder_child (notebook, page, page_num == last_page ? 0 : page_num + 1);
}

static void
tabs_detach_tab_callback (CtkAction *action,
                          TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;
    TerminalApp *app;
    TerminalWindow *new_window;
    TerminalScreen *screen;

    app = terminal_app_get ();

    screen = priv->active_screen;

    new_window = terminal_app_new_window (app, ctk_widget_get_screen (CTK_WIDGET (window)));

    terminal_window_move_screen (window, new_window, screen, -1);

    /* FIXME: this seems wrong if tabs are shown in the window */
    terminal_window_update_size (new_window, screen, FALSE);

    ctk_window_present_with_time (CTK_WINDOW (new_window), ctk_get_current_event_time ());
}

static void
help_contents_callback (CtkAction *action,
                        TerminalWindow *window)
{
    terminal_util_show_help (NULL, CTK_WINDOW (window));
}

#define ABOUT_GROUP "About"
#define EMAILIFY(string) (g_strdelimit ((string), "%", '@'))

static void
help_about_callback (CtkAction *action,
                     TerminalWindow *window)
{
    char *licence_text;
    GBytes *bytes;
    const guint8 *data;
    GKeyFile *key_file;
    GError *error = NULL;
    char **authors, **contributors, **artists, **documenters, **array_strv;
    gchar *comments = NULL;
    gsize data_len, n_authors = 0, n_contributors = 0, n_artists = 0, n_documenters = 0 , i;
    GPtrArray *array;


    bytes = g_resources_lookup_data (TERMINAL_RESOURCES_PATH_PREFIX G_DIR_SEPARATOR_S "ui/terminal.about",
                                     G_RESOURCE_LOOKUP_FLAGS_NONE,
                                     &error);
    g_assert_no_error (error);

    data = g_bytes_get_data (bytes, &data_len);
    key_file = g_key_file_new ();
    g_key_file_load_from_data (key_file, (const char *) data, data_len, 0, &error);
    g_assert_no_error (error);

    authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Authors", &n_authors, NULL);
    contributors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Contributors", &n_contributors, NULL);
    artists = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Artists", &n_artists, NULL);
    documenters = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", &n_documenters, NULL);
    g_key_file_free (key_file);
    g_bytes_unref (bytes);

    array = g_ptr_array_new ();

    for (i = 0; i < n_authors; ++i)
        g_ptr_array_add (array, EMAILIFY (authors[i]));
    g_free (authors); /* strings are now owned by the array */

    if (n_contributors > 0)
    {
        g_ptr_array_add (array, g_strdup (""));
        g_ptr_array_add (array, g_strdup (_("Contributors:")));
        for (i = 0; i < n_contributors; ++i)
            g_ptr_array_add (array, EMAILIFY (contributors[i]));
    }
    g_free (contributors); /* strings are now owned by the array */

    g_ptr_array_add (array, NULL);
    array_strv = (char **) g_ptr_array_free (array, FALSE);

    for (i = 0; i < n_artists; ++i)
        artists[i] = EMAILIFY (artists[i]);
    for (i = 0; i < n_documenters; ++i)
        documenters[i] = EMAILIFY (documenters[i]);

    licence_text = terminal_util_get_licence_text ();

    comments = g_strdup_printf (_("A terminal emulator for the CAFE desktop\nUsing CTK %d.%d.%d\nand BTE %d.%d.%d"),
                                ctk_get_major_version (), ctk_get_minor_version (), ctk_get_micro_version (),
                                bte_get_major_version (), bte_get_minor_version (), bte_get_micro_version ());

    ctk_show_about_dialog (CTK_WINDOW (window),
                           "program-name", _("CAFE Terminal"),
                           "version", VERSION,
                           "title", _("About CAFE Terminal"),
                           "copyright", _("Copyright \xc2\xa9 2002–2004 Havoc Pennington\n"
                                          "Copyright \xc2\xa9 2003–2004, 2007 Mariano Suárez-Alvarez\n"
                                          "Copyright \xc2\xa9 2006 Guilherme de S. Pastore\n"
                                          "Copyright \xc2\xa9 2007–2010 Christian Persch\n"
                                          "Copyright \xc2\xa9 2011 Perberos\n"
                                          "Copyright \xc2\xa9 2012-2020 MATE developers\n"
                                          "Copyright \xc2\xa9 2020-2024 Pablo Barciela"),
                           "comments", comments,
                           "authors", array_strv,
                           "artists", artists,
                           "documenters", documenters,
                           "license", licence_text,
                           "wrap-license", TRUE,
                           "translator-credits", _("translator-credits"),
                           "logo-icon-name", CAFE_TERMINAL_ICON_NAME,
                           "website", "https://cafe-desktop.org",
                           NULL);

    g_free (comments);
    g_strfreev (array_strv);
    g_strfreev (artists);
    g_strfreev (documenters);
    g_free (licence_text);
}

CtkUIManager *
terminal_window_get_ui_manager (TerminalWindow *window)
{
    TerminalWindowPrivate *priv = window->priv;

    return priv->ui_manager;
}

void
terminal_window_save_state (TerminalWindow *window,
                            GKeyFile *key_file,
                            const char *group)
{
    TerminalWindowPrivate *priv = window->priv;
    GList *tabs, *lt;
    TerminalScreen *active_screen;
    CdkWindowState state;
    GPtrArray *tab_names_array;
    char **tab_names;
    gsize len;

    //XXXif (priv->menub)//XXX
    g_key_file_set_boolean (key_file, group, TERMINAL_CONFIG_WINDOW_PROP_MENUBAR_VISIBLE,
                            priv->menubar_visible);

    g_key_file_set_string (key_file, group, TERMINAL_CONFIG_WINDOW_PROP_ROLE,
                           ctk_window_get_role (CTK_WINDOW (window)));

    state = cdk_window_get_state (ctk_widget_get_window (CTK_WIDGET (window)));
    if (state & CDK_WINDOW_STATE_MAXIMIZED)
        g_key_file_set_boolean (key_file, group, TERMINAL_CONFIG_WINDOW_PROP_MAXIMIZED, TRUE);
    if (state & CDK_WINDOW_STATE_FULLSCREEN)
        g_key_file_set_boolean (key_file, group, TERMINAL_CONFIG_WINDOW_PROP_FULLSCREEN, TRUE);

    active_screen = terminal_window_get_active (window);
    tabs = terminal_window_list_screen_containers (window);

    tab_names_array = g_ptr_array_sized_new (g_list_length (tabs) + 1);

    for (lt = tabs; lt != NULL; lt = lt->next)
    {
        TerminalScreen *screen;
        char *tab_group;

        screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (lt->data));

        tab_group = g_strdup_printf ("Terminal%p", screen);
        g_ptr_array_add (tab_names_array, tab_group);

        terminal_screen_save_config (screen, key_file, tab_group);

        if (screen == active_screen)
        {
            int w, h, x, y;
            char *geometry;

            g_key_file_set_string (key_file, group, TERMINAL_CONFIG_WINDOW_PROP_ACTIVE_TAB, tab_group);

            /* FIXME saving the geometry is not great :-/ */
            terminal_screen_get_size (screen, &w, &h);
            ctk_window_get_position (CTK_WINDOW (window), &x, &y);
            geometry = g_strdup_printf ("%dx%d+%d+%d", w, h, x, y);
            g_key_file_set_string (key_file, group, TERMINAL_CONFIG_WINDOW_PROP_GEOMETRY, geometry);
            g_free (geometry);
        }
    }

    g_list_free (tabs);

    len = tab_names_array->len;
    g_ptr_array_add (tab_names_array, NULL);
    tab_names = (char **) g_ptr_array_free (tab_names_array, FALSE);
    g_key_file_set_string_list (key_file, group, TERMINAL_CONFIG_WINDOW_PROP_TABS, (const char * const *) tab_names, len);
    g_strfreev (tab_names);
}


TerminalWindow *
terminal_window_get_latest_focused (TerminalWindow *window1,
                                    TerminalWindow *window2)
{
  TerminalWindowPrivate *priv1 = NULL;
  TerminalWindowPrivate *priv2 = NULL;

  if (!window1)
    return window2;

  if (!window2)
    return window1;

  priv1 = window1->priv;
  priv2 = window2->priv;

  if (priv2->focus_time > priv1->focus_time)
    return window2;

  return window1;
}
