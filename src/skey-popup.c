/*
 * Copyright © 2002 Jonathan Blandford <jrb@gnome.org>
 * Copyright © 2008 Christian Persch
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

#include "terminal-intl.h"

#include "terminal-util.h"
#include "terminal-screen.h"
#include "skey-popup.h"
#include "skey/skey.h"
#include <stdlib.h>
#include <string.h>

#define SKEY_PREFIX "s/key "
#define OTP_PREFIX  "otp-"

typedef struct
{
	TerminalScreen *screen;
	char *seed;
	int seq;
	int hash;
} SkeyData;

static void
skey_data_free (SkeyData *data)
{
	g_free (data->seed);
	g_free (data);
}

static gboolean
extract_seq_and_seed (const gchar  *skey_match,
                      gint         *seq,
                      gchar       **seed)
{
	gchar *end_ptr = NULL;

	/* FIXME: use g_ascii_strtoll */
	*seq = strtol (skey_match + strlen (SKEY_PREFIX), &end_ptr, 0);

	if (end_ptr == NULL || *end_ptr == '\000')
		return FALSE;

	*seed = g_strdup (end_ptr + 1);

	return TRUE;
}

static gboolean
extract_hash_seq_and_seed (const gchar  *otp_match,
                           gint         *hash,
                           gint         *seq,
                           gchar       **seed)
{
	gchar *end_ptr = NULL;
	const gchar *p = otp_match + strlen (OTP_PREFIX);
	gint len = 3;

	if (strncmp (p, "md4", 3) == 0)
		*hash = MD4;
	else if (strncmp (p, "md5", 3) == 0)
		*hash = MD5;
	else if (strncmp (p, "sha1", 4) == 0)
	{
		*hash = SHA1;
		len++;
	}
	else
		return FALSE;

	p += len;

	/* RFC mandates the following skipping */
	while (*p == ' ' || *p == '\t')
	{
		if (*p == '\0')
			return FALSE;

		p++;
	}

	/* FIXME: use g_ascii_strtoll */
	*seq = strtol (p, &end_ptr, 0);

	if (end_ptr == NULL || *end_ptr == '\000')
		return FALSE;

	p = end_ptr;

	while (*p == ' ' || *p == '\t')
	{
		if (*p == '\0')
			return FALSE;
		p++;
	}

	*seed = g_strdup (p);
	return TRUE;
}

static void
skey_challenge_response_cb (CtkWidget *dialog,
                            int response_id,
                            SkeyData *data)
{
	if (response_id == CTK_RESPONSE_OK)
	{
		CtkWidget *entry;
		const char *password;
		char *response;

		entry = g_object_get_data (G_OBJECT (dialog), "skey-entry");
		password = ctk_entry_get_text (CTK_ENTRY (entry));

		/* FIXME: fix skey to use g_malloc */
		response = skey (data->hash, data->seq, data->seed, password);
		if (response)
		{
			BteTerminal *bte_terminal = BTE_TERMINAL (data->screen);
			static const char newline[2] = "\n";

			bte_terminal_feed_child (bte_terminal, response, strlen (response));
			bte_terminal_feed_child (bte_terminal, newline, strlen (newline));
			free (response);
		}
	}

	ctk_widget_destroy (dialog);
}

void
terminal_skey_do_popup (CtkWindow *window,
                        TerminalScreen *screen,
                        const gchar    *skey_match)
{
	CtkWidget *dialog, *label, *entry, *ok_button;
	char *title_text;
	char *seed;
	int seq;
	int hash = MD5;
	SkeyData *data;

	if (strncmp (SKEY_PREFIX, skey_match, strlen (SKEY_PREFIX)) == 0)
	{
		if (!extract_seq_and_seed (skey_match, &seq, &seed))
		{
			terminal_util_show_error_dialog (window, NULL, NULL,
			                                 _("The text you clicked on doesn't "
			                                   "seem to be a valid S/Key "
			                                   "challenge."));
			return;
		}
	}
	else
	{
		if (!extract_hash_seq_and_seed (skey_match, &hash, &seq, &seed))
		{
			terminal_util_show_error_dialog (window, NULL, NULL,
			                                 _("The text you clicked on doesn't "
			                                   "seem to be a valid OTP "
			                                   "challenge."));
			return;
		}
	}

	if (!terminal_util_load_builder_resource (TERMINAL_RESOURCES_PATH_PREFIX G_DIR_SEPARATOR_S "ui/skey-challenge.ui",
	                                      "skey-dialog", &dialog,
	                                      "skey-entry", &entry,
	                                      "text-label", &label,
	                                      "skey-ok-button", &ok_button,
	                                      NULL))
	{
		g_free (seed);
		return;
	}

	title_text = g_strdup_printf ("<big><b>%s</b></big>",
	                              ctk_label_get_text (CTK_LABEL (label)));
	ctk_label_set_label (CTK_LABEL (label), title_text);
	g_free (title_text);

	g_object_set_data (G_OBJECT (dialog), "skey-entry", entry);

	ctk_widget_grab_focus (entry);
	ctk_widget_grab_default (ok_button);
	ctk_entry_set_text (CTK_ENTRY (entry), "");

	ctk_window_set_transient_for (CTK_WINDOW (dialog), window);
	ctk_window_set_modal (CTK_WINDOW (dialog), TRUE);
	ctk_window_set_destroy_with_parent (CTK_WINDOW (dialog), TRUE);

	/* FIXME: make this dialogue close if the screen closes! */

	data = g_new (SkeyData, 1);
	data->hash = hash;
	data->seq = seq;
	data->seed = seed;
	data->screen = screen;

	g_signal_connect_data (dialog, "response",
	                       G_CALLBACK (skey_challenge_response_cb),
	                       data, (GClosureNotify) skey_data_free, 0);
	g_signal_connect (dialog, "delete-event",
	                  G_CALLBACK (terminal_util_dialog_response_on_delete), NULL);

	ctk_window_present (CTK_WINDOW (dialog));
}
