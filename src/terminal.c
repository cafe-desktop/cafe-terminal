/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010 Christian Persch
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

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <cdk/cdk.h>
#include <cdk/cdkx.h>

#include "eggsmclient.h"

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-options.h"
#include "terminal-util.h"

#define TERMINAL_FACTORY_SERVICE_NAME_PREFIX  "org.cafe.Terminal.Display"
#define TERMINAL_FACTORY_SERVICE_PATH         "/org/cafe/Terminal/Factory"
#define TERMINAL_FACTORY_INTERFACE_NAME       "org.cafe.Terminal.Factory"

static char *
ay_to_string (GVariant *variant,
              GError **error)
{
	gsize len;
	const char *data;

	data = g_variant_get_fixed_array (variant, &len, sizeof (char));
	if (len == 0)
		return NULL;

	/* Make sure there are no embedded NULs */
	if (memchr (data, '\0', len) != NULL)
	{
		g_set_error_literal (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
		                     "String is shorter than claimed");
		return NULL;
	}

	return g_strndup (data, len);
}

static char **
ay_to_strv (GVariant *variant,
            int *argc)
{
	GPtrArray *argv;
	const char *data, *nullbyte;
	gsize data_len;
	gssize len;

	data = g_variant_get_fixed_array (variant, &data_len, sizeof (char));
	if (data_len == 0 || data_len > G_MAXSSIZE)
	{
		if (argc != NULL)
			*argc = 0;
		return NULL;
	}

	argv = g_ptr_array_new ();

	len = data_len;
	do
	{
		gssize string_len;

		nullbyte = memchr (data, '\0', len);

		string_len = nullbyte ? (gssize) (nullbyte - data) : len;
		g_ptr_array_add (argv, g_strndup (data, string_len));

		len -= string_len + 1;
		data += string_len + 1;
	}
	while (len > 0);

	if (argc)
		*argc = argv->len;

	/* NULL terminate */
	g_ptr_array_add (argv, NULL);
	return (char **) g_ptr_array_free (argv, FALSE);
}

static GVariant *
string_to_ay (const char *string)
{
	gsize len;
	char *data;

	len = strlen (string);
	data = g_strndup (string, len);

	return g_variant_new_from_data (G_VARIANT_TYPE ("ay"), data, len, TRUE, g_free, data);
}

typedef struct
{
	char *factory_name;
	TerminalOptions *options;
	int exit_code;
	char **argv;
	int argc;
} OwnData;

static void
method_call_cb (GDBusConnection *connection,
                const char *sender,
                const char *object_path,
                const char *interface_name,
                const char *method_name,
                GVariant *parameters,
                GDBusMethodInvocation *invocation,
                gpointer user_data)
{
	if (g_strcmp0 (method_name, "HandleArguments") == 0)
	{
		TerminalOptions *options = NULL;
		GVariant *v_wd, *v_display, *v_sid, *v_envv, *v_argv;
		char *working_directory = NULL, *display_name = NULL, *startup_id = NULL;
		int initial_workspace = -1;
		char **envv = NULL, **argv = NULL;
		int argc;
		GError *error = NULL;

		g_variant_get (parameters, "(@ay@ay@ay@ayi@ay)",
		               &v_wd, &v_display, &v_sid, &v_envv, &initial_workspace, &v_argv);

		working_directory = ay_to_string (v_wd, &error);
		if (error)
			goto out;
		display_name = ay_to_string (v_display, &error);
		if (error)
			goto out;
		startup_id = ay_to_string (v_sid, &error);
		if (error)
			goto out;
		envv = ay_to_strv (v_envv, NULL);
		argv = ay_to_strv (v_argv, &argc);

		_terminal_debug_print (TERMINAL_DEBUG_FACTORY,
		                       "Factory invoked with working-dir='%s' display='%s' startup-id='%s'"
		                       "workspace='%d'\n",
		                       working_directory ? working_directory : "(null)",
		                       display_name ? display_name : "(null)",
		                       startup_id ? startup_id : "(null)",
		                       initial_workspace);

		options = terminal_options_parse (working_directory,
		                                  display_name,
		                                  startup_id,
		                                  envv,
		                                  TRUE,
		                                  TRUE,
		                                  &argc, &argv,
		                                  &error,
		                                  NULL);

		if (options != NULL)
		{
			options->initial_workspace = initial_workspace;

			terminal_app_handle_options (terminal_app_get (), options, FALSE /* no resume */, &error);
			terminal_options_free (options);
		}

out:
		g_variant_unref (v_wd);
		g_free (working_directory);
		g_variant_unref (v_display);
		g_free (display_name);
		g_variant_unref (v_sid);
		g_free (startup_id);
		g_variant_unref (v_envv);
		g_strfreev (envv);
		g_variant_unref (v_argv);
		g_strfreev (argv);

		if (error == NULL)
		{
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
		}
		else
		{
			g_dbus_method_invocation_return_gerror (invocation, error);
			g_error_free (error);
		}
	}
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char *name,
                 gpointer user_data)
{
	static const char dbus_introspection_xml[] =
	    "<node name='/org/cafe/Terminal'>"
	    "<interface name='org.cafe.Terminal.Factory'>"
	    "<method name='HandleArguments'>"
	    "<arg type='ay' name='working_directory' direction='in' />"
	    "<arg type='ay' name='display_name' direction='in' />"
	    "<arg type='ay' name='startup_id' direction='in' />"
	    "<arg type='ay' name='environment' direction='in' />"
	    "<arg type='i' name='workspace' direction='in' />"
	    "<arg type='ay' name='arguments' direction='in' />"
	    "</method>"
	    "</interface>"
	    "</node>";

	static const GDBusInterfaceVTable interface_vtable =
	{
		method_call_cb,
		NULL,
		NULL,
		{ NULL }
	};

	OwnData *data = (OwnData *) user_data;
	GDBusNodeInfo *introspection_data;
	guint registration_id;
	GError *error = NULL;

	_terminal_debug_print (TERMINAL_DEBUG_FACTORY,
	                       "Bus %s acquired\n", name);

	introspection_data = g_dbus_node_info_new_for_xml (dbus_introspection_xml, NULL);
	g_assert (introspection_data != NULL);

	registration_id = g_dbus_connection_register_object (connection,
	                  TERMINAL_FACTORY_SERVICE_PATH,
	                  introspection_data->interfaces[0],
	                  &interface_vtable,
	                  NULL, NULL,
	                  &error);
	g_dbus_node_info_unref (introspection_data);

	if (registration_id == 0)
	{
		g_printerr ("Failed to register object: %s\n", error->message);
		g_error_free (error);
		data->exit_code = EXIT_FAILURE;
		ctk_main_quit ();
	}
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const char *name,
                  gpointer user_data)
{
	OwnData *data = (OwnData *) user_data;
	GError *error = NULL;

	_terminal_debug_print (TERMINAL_DEBUG_FACTORY,
	                       "Acquired the name %s on the session bus\n", name);

	if (data->options == NULL)
	{
		/* Name re-acquired!? */
		g_assert_not_reached ();
	}


	if (!terminal_app_handle_options (terminal_app_get (), data->options, TRUE /* do resume */, &error))
	{
		g_printerr ("Failed to handle options: %s\n", error->message);
		g_error_free (error);
		data->exit_code = EXIT_FAILURE;
		ctk_main_quit ();
	}

	terminal_options_free (data->options);
	data->options = NULL;
}

static void
name_lost_cb (GDBusConnection *connection,
              const char *name,
              gpointer user_data)
{
	OwnData *data = (OwnData *) user_data;
	GError *error = NULL;
	char **envv;
	int i;
	GVariantBuilder builder;
	GVariant *value;
	GString *string;
	char *s;
	gsize len;

	_terminal_debug_print (TERMINAL_DEBUG_FACTORY,
	                       "Lost the name %s on the session bus\n", name);

	/* Couldn't get the connection? No way to continue! */
	if (connection == NULL)
	{
		data->exit_code = EXIT_FAILURE;
		ctk_main_quit ();
		return;
	}

	if (data->options == NULL)
	{
		/* Already handled */
		data->exit_code = EXIT_SUCCESS;
		ctk_main_quit ();
		return;
	}

	_terminal_debug_print (TERMINAL_DEBUG_FACTORY,
	                       "Forwarding arguments to existing instance\n");

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ayayayayiay)"));

	g_variant_builder_add (&builder, "@ay", string_to_ay (data->options->default_working_dir));
	g_variant_builder_add (&builder, "@ay", string_to_ay (data->options->display_name));
	g_variant_builder_add (&builder, "@ay", string_to_ay (data->options->startup_id));

	string = g_string_new (NULL);
	envv = g_get_environ ();
	for (i = 0; envv[i]; ++i)
	{
		if (i > 0)
			g_string_append_c (string, '\0');

		g_string_append (string, envv[i]);
	}
	g_strfreev (envv);

	len = string->len;
	s = g_string_free (string, FALSE);
	g_variant_builder_add (&builder, "@ay",
	                       g_variant_new_from_data (G_VARIANT_TYPE ("ay"), s, len, TRUE, g_free, s));

	g_variant_builder_add (&builder, "@i", g_variant_new_int32 (data->options->initial_workspace));

	string = g_string_new (NULL);

	for (i = 0; i < data->argc; ++i)
	{
		if (i > 0)
			g_string_append_c (string, '\0');
		g_string_append (string, data->argv[i]);
	}

	len = string->len;
	s = g_string_free (string, FALSE);
	g_variant_builder_add (&builder, "@ay",
	                       g_variant_new_from_data (G_VARIANT_TYPE ("ay"), s, len, TRUE, g_free, s));

	value = g_dbus_connection_call_sync (connection,
	                                     data->factory_name,
	                                     TERMINAL_FACTORY_SERVICE_PATH,
	                                     TERMINAL_FACTORY_INTERFACE_NAME,
	                                     "HandleArguments",
	                                     g_variant_builder_end (&builder),
	                                     G_VARIANT_TYPE ("()"),
	                                     G_DBUS_CALL_FLAGS_NONE,
	                                     -1,
	                                     NULL,
	                                     &error);
	if (value == NULL)
	{
		g_printerr ("Failed to forward arguments: %s\n", error->message);
		g_error_free (error);
		data->exit_code = EXIT_FAILURE;
		ctk_main_quit ();
	}
	else
	{
		g_variant_unref (value);
		data->exit_code = EXIT_SUCCESS;
	}

	terminal_options_free (data->options);
	data->options = NULL;

	ctk_main_quit ();
}

/* Settings storage works as follows:
 *   /apps/cafe-terminal/global/
 *   /apps/cafe-terminal/profiles/Foo/
 *
 * It's somewhat tricky to manage the profiles/ dir since we need to track
 * the list of profiles, but GSettings doesn't have a concept of notifying that
 * a directory has appeared or disappeared.
 *
 * Session state is stored entirely in the RestartCommand command line.
 *
 * The number one rule: all stored information is EITHER per-session,
 * per-profile, or set from a command line option. THERE CAN BE NO
 * OVERLAP. The UI and implementation totally break if you overlap
 * these categories. See cafe-terminal 1.x for why.
 *
 * Don't use this code as an example of how to use GSettings - it's hugely
 * overcomplicated due to the profiles stuff. Most apps should not
 * have to do scary things of this nature, and should not have
 * a profiles feature.
 *
 */

/* Copied from libbaul/baul-program-choosing.c; Needed in case
 * we have no DESKTOP_STARTUP_ID (with its accompanying timestamp).
 */
static Time
slowly_and_stupidly_obtain_timestamp (Display *xdisplay)
{
	Window xwindow;
	XEvent event;

	{
		XSetWindowAttributes attrs;
		Atom atom_name;
		Atom atom_type;
		const char *name;

		attrs.override_redirect = True;
		attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

		xwindow =
		    XCreateWindow (xdisplay,
		                   RootWindow (xdisplay, 0),
		                   -100, -100, 1, 1,
		                   0,
		                   CopyFromParent,
		                   CopyFromParent,
		                   (Visual *)CopyFromParent,
		                   CWOverrideRedirect | CWEventMask,
		                   &attrs);

		atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
		g_assert (atom_name != None);
		atom_type = XInternAtom (xdisplay, "STRING", TRUE);
		g_assert (atom_type != None);

		name = "Fake Window";
		XChangeProperty (xdisplay,
		                 xwindow, atom_name,
		                 atom_type,
		                 8, PropModeReplace, (unsigned char *)name, strlen (name));
	}

	XWindowEvent (xdisplay,
	              xwindow,
	              PropertyChangeMask,
	              &event);

	XDestroyWindow(xdisplay, xwindow);

	return event.xproperty.time;
}

static char *
get_factory_name_for_display (const char *display_name)
{
	GString *name;
	const char *p;

	name = g_string_sized_new (strlen (TERMINAL_FACTORY_SERVICE_NAME_PREFIX) + strlen (display_name) + 1 /* NUL */);
	g_string_append (name, TERMINAL_FACTORY_SERVICE_NAME_PREFIX);

	for (p = display_name; *p; ++p)
	{
		if (g_ascii_isalnum (*p))
			g_string_append_c (name, *p);
		else
			g_string_append_c (name, '_');
	}

	_terminal_debug_print (TERMINAL_DEBUG_FACTORY,
	                       "Factory name is \"%s\"\n", name->str);

	return g_string_free (name, FALSE);
}

static int
get_initial_workspace (void)
{
  int ret = -1;
  CdkWindow *window;
  guchar *data = NULL;
  CdkAtom atom;
  CdkAtom cardinal_atom;

  window = cdk_get_default_root_window();

  atom = cdk_atom_intern_static_string ("_NET_CURRENT_DESKTOP");
  cardinal_atom = cdk_atom_intern_static_string ("CARDINAL");

  if (cdk_property_get (window, atom, cardinal_atom, 0, 8, FALSE, NULL, NULL, NULL, &data)) {
	  ret = *(int *)data;
	  g_free (data);
  }
  return ret;
}

int
main (int argc, char **argv)
{
	int i;
	char **argv_copy;
	int argc_copy;
	const char *startup_id, *display_name, *home_dir;
	CdkDisplay *display;
	TerminalOptions *options;
	GError *error = NULL;
	char *working_directory;
	int ret = EXIT_SUCCESS;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	_terminal_debug_init ();

	/* Make a NULL-terminated copy since we may need it later */
	argv_copy = g_new (char *, argc + 1);
	for (i = 0; i < argc; ++i)
		argv_copy [i] = argv [i];
	argv_copy [i] = NULL;
	argc_copy = argc;

	startup_id = g_getenv ("DESKTOP_STARTUP_ID");

	working_directory = g_get_current_dir ();

	cdk_set_allowed_backends ("x11");

	/* Now change directory to $HOME so we don't prevent unmounting, e.g. if the
	 * factory is started by baul-open-terminal. See bug #565328.
	 * On failure back to /.
	 */
	home_dir = g_get_home_dir ();
	if (home_dir == NULL || chdir (home_dir) < 0)
		if (chdir ("/") < 0)
			g_warning ("Could not change working directory.");

	options = terminal_options_parse (working_directory,
	                                  NULL,
	                                  startup_id,
	                                  NULL,
	                                  FALSE,
	                                  FALSE,
	                                  &argc, &argv,
	                                  &error,
	                                  ctk_get_option_group (TRUE),
	                                  egg_sm_client_get_option_group (),
	                                  NULL);

	g_free (working_directory);

	if (options == NULL)
	{
		g_printerr (_("Failed to parse arguments: %s\n"), error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	g_set_application_name (_("Terminal"));

	/* Unset the these env variables, so they doesn't end up
	 * in the factory's env and thus in the terminals' envs.
	 */
	g_unsetenv ("DESKTOP_STARTUP_ID");
	g_unsetenv ("GIO_LAUNCHED_DESKTOP_FILE_PID");
	g_unsetenv ("GIO_LAUNCHED_DESKTOP_FILE");

	display = cdk_display_get_default ();
	display_name = cdk_display_get_name (display);
	options->display_name = g_strdup (display_name);

	if (options->startup_id == NULL)
	{
		/* Create a fake one containing a timestamp that we can use */
		Time timestamp;

		timestamp = slowly_and_stupidly_obtain_timestamp (CDK_DISPLAY_XDISPLAY (cdk_display_get_default ()));

		options->startup_id = g_strdup_printf ("_TIME%lu", timestamp);
	}

	g_object_set (ctk_settings_get_default (), "ctk-button-images", TRUE, NULL);
	g_object_set (ctk_settings_get_default (), "ctk-menu-images", TRUE, NULL);

	if (options->use_factory)
	{
		OwnData *data;
		guint owner_id;

		data = g_new (OwnData, 1);
		data->factory_name = get_factory_name_for_display (display_name);
		data->options = options;
		data->exit_code = -1;
		data->argv = argv_copy;
		data->argc = argc_copy;

		ctk_init(&argc, &argv);
		options->initial_workspace = get_initial_workspace ();

		owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
		                           data->factory_name,
		                           G_BUS_NAME_OWNER_FLAGS_NONE,
		                           bus_acquired_cb,
		                           name_acquired_cb,
		                           name_lost_cb,
		                           data, NULL);

		ctk_main ();

		ret = data->exit_code;
		g_bus_unown_name (owner_id);

		g_free (data->factory_name);
		g_free (data);

	}
	else
	{

		terminal_app_handle_options (terminal_app_get (), options, TRUE /* allow resume */, &error);
		terminal_options_free (options);

		if (error == NULL)
		{
			ctk_main ();
		}
		else
		{
			g_printerr ("Error handling options: %s\n", error->message);
			g_error_free (error);
			ret = EXIT_FAILURE;
		}
	}

	terminal_app_shutdown ();

	g_free (argv_copy);

	return ret;
}
