/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2016 Alexander Larsson <alexl@redhat.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <appstream-glib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>

G_GNUC_PRINTF (2, 3)
static void
as_compose_app_log (AsApp *app, const gchar *fmt, ...)
{
	const gchar *id;
	gsize i;
	va_list args;
	g_autofree gchar *tmp = NULL;

	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);

	/* print status */
	id = as_app_get_id (app);
	g_print ("%s: ", id);
	for (i = strlen (id) + 2; i < 35; i++)
		g_print (" ");
	g_print ("%s\n", tmp);
}

static gboolean
add_icons (AsApp *app,
	   const gchar *icons_dir,
	   guint min_icon_size,
	   const gchar *prefix,
	   const gchar *key,
	   GError **error)
{
	g_autofree gchar *fn_hidpi = NULL;
	g_autofree gchar *fn = NULL;
	g_autofree gchar *name_hidpi = NULL;
	g_autofree gchar *name = NULL;
	g_autofree gchar *icon_path = NULL;
	g_autofree gchar *icon_subdir = NULL;
	g_autofree gchar *icon_path_hidpi = NULL;
	g_autofree gchar *icon_subdir_hidpi = NULL;
	g_autoptr(AsIcon) icon_hidpi = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(AsImage) im = NULL;
	g_autoptr(GdkPixbuf) pixbuf_hidpi = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	g_autoptr(GError) error_local = NULL;

	/* find 64x64 icon */
	fn = as_utils_find_icon_filename_full (prefix, key,
					       AS_UTILS_FIND_ICON_NONE,
					       error);
	if (fn == NULL) {
		g_prefix_error (error, "Failed to find icon: ");
		return FALSE;
	}

	/* load the icon */
	im = as_image_new ();
	if (!as_image_load_filename_full (im, fn,
					  64, min_icon_size,
					  AS_IMAGE_LOAD_FLAG_ALWAYS_RESIZE |
					  AS_IMAGE_LOAD_FLAG_ONLY_SUPPORTED |
					  AS_IMAGE_LOAD_FLAG_SHARPEN,
					  error)) {
		g_prefix_error (error, "Failed to load icon: ");
		return FALSE;
	}
	pixbuf = g_object_ref (as_image_get_pixbuf (im));

	/* save in target directory */
	name = g_strdup_printf ("%ix%i/%s.png",
				64, 64,
				as_app_get_id_filename (AS_APP (app)));

	icon = as_icon_new ();
	as_icon_set_pixbuf (icon, pixbuf);
	as_icon_set_name (icon, name);
	as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
	as_icon_set_prefix (icon, as_app_get_icon_path (AS_APP (app)));
	as_app_add_icon (AS_APP (app), icon);

	icon_path = g_build_filename (icons_dir, name, NULL);

	icon_subdir = g_path_get_dirname (icon_path);
	if (g_mkdir_with_parents (icon_subdir, 0755)) {
		int errsv = errno;
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "failed to create %s: %s",
			     icon_subdir,
			     strerror (errsv));
		return FALSE;
	}

	/* TRANSLATORS: we've saving the icon file to disk */
	g_print ("%s %s\n", _("Saving icon"), icon_path);
	if (!gdk_pixbuf_save (pixbuf, icon_path, "png", error, NULL))
		return FALSE;

	/* try to get a HiDPI icon */
	fn_hidpi = as_utils_find_icon_filename_full (prefix, key,
						     AS_UTILS_FIND_ICON_HI_DPI,
						     NULL);
	if (fn_hidpi == NULL) {
		g_debug ("no HiDPI icon found with key %s in %s", key, prefix);
		return TRUE;
	}

	/* load the HiDPI icon */
	g_debug ("trying to load %s", fn_hidpi);
	if (!as_image_load_filename_full (im, fn_hidpi,
					  128, 128,
					  AS_IMAGE_LOAD_FLAG_ALWAYS_RESIZE |
					  AS_IMAGE_LOAD_FLAG_SHARPEN,
					  &error_local)) {
		g_debug ("failed to load HiDPI icon: %s", error_local->message);
		return TRUE;
	}
	pixbuf_hidpi = g_object_ref (as_image_get_pixbuf (im));
	if (gdk_pixbuf_get_width (pixbuf_hidpi) <= gdk_pixbuf_get_width (pixbuf) ||
	    gdk_pixbuf_get_height (pixbuf_hidpi) <= gdk_pixbuf_get_height (pixbuf)) {
		g_debug ("HiDPI icon no larger than normal icon");
		return TRUE;
	}
	as_app_add_kudo_kind (AS_APP (app), AS_KUDO_KIND_HI_DPI_ICON);

	/* save icon */
	name_hidpi = g_strdup_printf ("%ix%i/%s.png",
				      128, 128,
				      as_app_get_id_filename (AS_APP (app)));
	icon_hidpi = as_icon_new ();
	as_icon_set_pixbuf (icon_hidpi, pixbuf_hidpi);
	as_icon_set_name (icon_hidpi, name_hidpi);
	as_icon_set_kind (icon_hidpi, AS_ICON_KIND_CACHED);
	as_icon_set_prefix (icon_hidpi, as_app_get_icon_path (AS_APP (app)));
	as_app_add_icon (AS_APP (app), icon_hidpi);

	icon_path_hidpi = g_build_filename (icons_dir, name_hidpi, NULL);
	icon_subdir_hidpi = g_path_get_dirname (icon_path_hidpi);
	if (g_mkdir_with_parents (icon_subdir_hidpi, 0755)) {
		int errsv = errno;
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "failed to create %s: %s",
			     icon_subdir_hidpi,
			     strerror (errsv));
		return FALSE;
	}

	/* TRANSLATORS: we've saving the icon file to disk */
	g_print ("%s %s\n", _("Saving icon"), icon_path_hidpi);
	if (!gdk_pixbuf_save (pixbuf_hidpi, icon_path_hidpi, "png", error, NULL))
		return FALSE;
	return TRUE;
}

static AsApp *
load_desktop (const gchar *prefix,
	      const gchar *icons_dir,
	      guint        min_icon_size,
	      const gchar *app_name,
	      const gchar *desktop_path,
	      GError **error)
{
	AsIcon *icon;
	g_autoptr(AsApp) app = NULL;

	app = as_app_new ();
	if (!as_app_parse_file (app, desktop_path,
				AS_APP_PARSE_FLAG_USE_HEURISTICS |
				AS_APP_PARSE_FLAG_ALLOW_VETO,
				error))
		return NULL;
	if (as_app_get_kind (app) == AS_APP_KIND_UNKNOWN) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "%s has no recognised type",
			     as_app_get_id (AS_APP (app)));
		return NULL;
	}

	icon = as_app_get_icon_default (AS_APP (app));
	if (icon != NULL) {
		g_autofree gchar *key = NULL;
		key = g_strdup (as_icon_get_name (icon));
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
			as_compose_app_log (app,
					    "using stock icon %s", key);
		} else {
			g_autoptr(GError) error_local = NULL;
			gboolean ret;

			g_ptr_array_set_size (as_app_get_icons (AS_APP (app)), 0);
			ret = add_icons (app,
					 icons_dir,
					 min_icon_size,
					 prefix,
					 key,
					 error);
			if (!ret)
				return NULL;
		}
	}

	return g_steal_pointer (&app);
}

static gchar *
get_appdata_filename (const gchar *prefix, const gchar *app_name)
{
	const gchar *dirs[] = { "metainfo", "appdata", NULL };
	const gchar *exts[] = { ".metainfo.xml", ".appdata.xml", NULL };

	/* fall back to the legacy path and extensions */
	for (guint j = 0; dirs[j] != NULL; j++) {
		for (guint i = 0; exts[i] != NULL; i++) {
			g_autofree gchar *basename = NULL;
			g_autofree gchar *tmp = NULL;
			basename = g_strconcat (app_name, exts[i], NULL);
			tmp = g_build_filename (prefix,
						"share",
						dirs[j],
						basename,
						NULL);
			if (g_file_test (tmp, G_FILE_TEST_EXISTS))
				return g_steal_pointer (&tmp);
		}
	}
	return NULL;
}

static AsApp *
load_appdata (const gchar *prefix, const gchar *app_name, GError **error)
{
	g_autofree gchar *appdata_path = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GPtrArray) problems = NULL;
	AsProblemKind problem_kind;
	AsProblem *problem;
	guint i;

	appdata_path = get_appdata_filename (prefix, app_name);
	if (appdata_path == NULL) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "no file found for %s", app_name);
		return NULL;
	}
	g_debug ("looking for %s", appdata_path);

	app = as_app_new ();
	if (!as_app_parse_file (app, appdata_path,
				AS_APP_PARSE_FLAG_USE_HEURISTICS,
				error))
		return NULL;
	if (as_app_get_kind (app) == AS_APP_KIND_UNKNOWN) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "%s has no recognised type",
			     as_app_get_id (AS_APP (app)));
		return NULL;
	}

	problems = as_app_validate (app,
				    AS_APP_VALIDATE_FLAG_NO_NETWORK |
				    AS_APP_VALIDATE_FLAG_RELAX,
				    error);
	if (problems == NULL)
		return NULL;
	for (i = 0; i < problems->len; i++) {
		problem = g_ptr_array_index (problems, i);
		problem_kind = as_problem_get_kind (problem);
		as_compose_app_log (app,
				    "AppData problem: %s : %s",
				    as_problem_kind_to_string (problem_kind),
				    as_problem_get_message (problem));
	}
	if (problems->len > 0) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "AppData file %s was not valid",
			     appdata_path);
		return NULL;
	}

	return g_steal_pointer (&app);
}

int
main (int argc, char **argv)
{
	g_autoptr(GOptionContext) option_context = NULL;
	gboolean ret;
	gboolean verbose = FALSE;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *icons_dir = NULL;
	g_autofree gchar *origin = NULL;
	g_autofree gchar *xml_basename = NULL;
	g_autofree gchar *output_dir = NULL;
	g_autofree gchar *prefix = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) xml_dir = NULL;
	g_autoptr(GFile) xml_file = NULL;
	guint min_icon_size = 32;
	guint i;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "prefix", '\0', 0, G_OPTION_ARG_FILENAME, &prefix,
			/* TRANSLATORS: command line option */
			_("Set the prefix"), "DIR" },
		{ "output-dir", '\0', 0, G_OPTION_ARG_FILENAME, &output_dir,
			/* TRANSLATORS: command line option */
			_("Set the output directory"), "DIR" },
		{ "icons-dir", '\0', 0, G_OPTION_ARG_FILENAME, &icons_dir,
			/* TRANSLATORS: command line option */
			_("Set the icons directory"), "DIR" },
		{ "origin", '\0', 0, G_OPTION_ARG_STRING, &origin,
			/* TRANSLATORS: command line option */
			_("Set the origin name"), "NAME" },
		{ "min-icon-size", '\0', 0, G_OPTION_ARG_INT, &min_icon_size,
			/* TRANSLATORS: command line option */
			_("Set the minimum icon size in pixels"), "ICON_SIZE" },
		{ "basename", '\0', 0, G_OPTION_ARG_STRING, &basename,
			/* TRANSLATORS: command line option */
			_("Set the basenames of the output files"), "NAME" },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	option_context = g_option_context_new (" - APP-IDS");

	g_option_context_add_main_entries (option_context, options, NULL);
	ret = g_option_context_parse (option_context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_print ("%s: %s\n", _("Failed to parse arguments"), error->message);
		return EXIT_FAILURE;
	}

	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* set defaults */
	if (prefix == NULL)
		prefix = g_strdup ("/usr");
	if (output_dir == NULL)
		output_dir = g_build_filename (prefix, "share/app-info/xmls", NULL);
	if (icons_dir == NULL)
		icons_dir = g_build_filename (prefix, "share/app-info/icons", origin, NULL);
	if (origin == NULL) {
		g_print ("WARNING: Metadata origin not set, using 'example'\n");
		origin = g_strdup ("example");
	}
	if (basename == NULL)
		basename = g_strdup (origin);

	if (argc == 1) {
		g_autofree gchar *tmp = NULL;
		tmp = g_option_context_get_help (option_context, TRUE, NULL);
		g_print ("%s", tmp);
		return  EXIT_FAILURE;
	}

	store = as_store_new ();
	as_store_set_api_version (store, 0.8);
	as_store_set_origin (store, origin);

	/* load each application specified */
	for (i = 1; i < (guint) argc; i++) {
		AsLaunchable *launchable;
		const gchar *app_name = argv[i];
		g_auto(GStrv) intl_domains = NULL;
		g_autoptr(AsApp) app_appdata = NULL;
		g_autoptr(AsApp) app_desktop = NULL;
		g_autoptr(GString) desktop_basename = NULL;
		g_autofree gchar *desktop_path = NULL;

		/* TRANSLATORS: we're generating the AppStream data */
		g_print ("%s %s\n", _("Processing application"), app_name);

		app_appdata = load_appdata (prefix, app_name, &error);
		if (app_appdata == NULL) {
			/* TRANSLATORS: the .appdata.xml file could not
			 * be loaded */
			g_print ("%s: %s\n", _("Error loading AppData file"),
				 error->message);
			return EXIT_FAILURE;
		}

		/* set translations */
		if (!as_app_builder_search_translations (app_appdata,
							 prefix,
							 25,
							 AS_APP_BUILDER_FLAG_NONE,
							 NULL,
							 &error)) {
			/* TRANSLATORS: the .mo files could not be parsed */
			g_print ("%s: %s\n", _("Error parsing translations"),
				 error->message);
			return EXIT_FAILURE;
		}

		/* auto-add kudos */
		if (!as_app_builder_search_kudos (app_appdata,
						  prefix,
						  AS_APP_BUILDER_FLAG_NONE,
						  &error)) {
			/* TRANSLATORS: we could not auto-add the kudo */
			g_print ("%s: %s\n", _("Error parsing kudos"),
				 error->message);
			return EXIT_FAILURE;
		}

		/* auto-add provides */
		if (!as_app_builder_search_provides (app_appdata,
						     prefix,
						     AS_APP_BUILDER_FLAG_NONE,
						     &error)) {
			/* TRANSLATORS: we could not auto-add the provides */
			g_print ("%s: %s\n", _("Error parsing provides"),
				 error->message);
			return EXIT_FAILURE;
		}

		as_store_add_app (store, app_appdata);

		/* use the ID from the AppData file if it was found */
		launchable = as_app_get_launchable_by_kind (app_appdata,
							    AS_LAUNCHABLE_KIND_DESKTOP_ID);
		if (launchable != NULL) {
			desktop_basename = g_string_new (as_launchable_get_value (launchable));
		} else {
			const gchar *appdata_id = as_app_get_id (app_appdata);

			/* append the .desktop suffix if using a new-style name */
			desktop_basename = g_string_new (appdata_id != NULL ? appdata_id : app_name);
			if (!g_str_has_suffix (desktop_basename->str, ".desktop"))
				g_string_append (desktop_basename, ".desktop");
		}

		desktop_path = g_build_filename (prefix, "share", "applications",
						 desktop_basename->str, NULL);
		g_debug ("looking for %s", desktop_path);

		if (g_file_test (desktop_path, G_FILE_TEST_EXISTS)) {
			app_desktop = load_desktop (prefix,
						    icons_dir,
						    min_icon_size,
						    app_name,
						    desktop_path,
						    &error);
			if (app_desktop == NULL) {
				/* TRANSLATORS: the .desktop file could not
				 * be loaded */
				g_print ("%s: %s\n", _("Error loading desktop file"),
					 error->message);
				return EXIT_FAILURE;
			}

			/* if the appdata <name> exists, do not inherit from
			 * the desktop file as it may be prefixed */
			if (g_hash_table_size (as_app_get_names (app_appdata)) > 0)
				g_hash_table_remove_all (as_app_get_names (app_desktop));
			if (g_hash_table_size (as_app_get_comments (app_appdata)) > 0)
				g_hash_table_remove_all (as_app_get_comments (app_desktop));

			/* does the app already exist with a launchable that matches this ID */
			if (g_strcmp0 (as_app_get_id (app_appdata), as_app_get_id (app_desktop)) != 0) {
				g_debug ("fixing up ID for desktop merge");
				as_app_set_id (app_desktop, as_app_get_id (app_appdata));
			}

			as_store_add_app (store, app_desktop);
		}
	}

	/* create output directory */
	if (g_mkdir_with_parents (output_dir, 0755)) {
		int errsv = errno;
		g_print ("%s: %s\n",
			 /* TRANSLATORS: this is when the folder could
			  * not be created */
			 _("Error creating output directory"),
			 strerror (errsv));
		return EXIT_FAILURE;
	}

	xml_dir = g_file_new_for_path (output_dir);
	xml_basename = g_strconcat (basename, ".xml.gz", NULL);
	xml_file = g_file_get_child (xml_dir, xml_basename);
	/* TRANSLATORS: we've saving the XML file to disk */
	g_print ("%s %s\n", _("Saving AppStream"),
		 g_file_get_path (xml_file));
	if (!as_store_to_file (store,
			       xml_file,
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_ADD_HEADER,
			       NULL, &error)) {
		/* TRANSLATORS: this is when the destination file
		 * cannot be saved for some reason */
		g_print ("%s: %s\n", _("Error saving AppStream file"),
			 error->message);
		return EXIT_FAILURE;
	}

	/* TRANSLATORS: information message */
	g_print ("%s\n", _("Done!"));

	return EXIT_SUCCESS;
}
