/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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
#include <locale.h>

#include "as-cleanup.h"
#include "asb-context.h"
#include "asb-utils.h"

/**
 * as_builder_search_path:
 **/
static gboolean
as_builder_search_path (GPtrArray *array, const gchar *path, GError **error)
{
	const gchar *filename;
	_cleanup_dir_close_ GDir *dir = NULL;

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((filename = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *tmp = NULL;
		tmp = g_build_filename (path, filename, NULL);
		if (g_file_test (tmp, G_FILE_TEST_IS_DIR)) {
			if (!as_builder_search_path (array, tmp, error))
				return FALSE;
		} else {
			g_ptr_array_add (array, g_strdup (tmp));
		}
	}
	return TRUE;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	AsbContext *ctx = NULL;
	GOptionContext *option_context;
	const gchar *filename;
	gboolean add_cache_id = FALSE;
	gboolean no_net = FALSE;
	gboolean ret;
	gboolean verbose = FALSE;
	gdouble api_version = 0.0f;
	gint max_threads = 4;
	guint i;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *cache_dir = NULL;
	_cleanup_free_ gchar *extra_appdata = NULL;
	_cleanup_free_ gchar *extra_appstream = NULL;
	_cleanup_free_ gchar *extra_screenshots = NULL;
	_cleanup_free_ gchar *log_dir = NULL;
	_cleanup_free_ gchar *old_metadata = NULL;
	_cleanup_free_ gchar *output_dir = NULL;
	_cleanup_free_ gchar *packages_dir = NULL;
	_cleanup_free_ gchar *screenshot_dir = NULL;
	_cleanup_free_ gchar *screenshot_uri = NULL;
	_cleanup_free_ gchar *temp_dir = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *packages = NULL;
	_cleanup_timer_destroy_ GTimer *timer = NULL;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "no-net", '\0', 0, G_OPTION_ARG_NONE, &no_net,
			/* TRANSLATORS: command line option */
			_("Do not use the network to download screenshots"), NULL },
		{ "add-cache-id", '\0', 0, G_OPTION_ARG_NONE, &add_cache_id,
			/* TRANSLATORS: command line option */
			_("Add a cache ID to each component"), NULL },
		{ "log-dir", '\0', 0, G_OPTION_ARG_FILENAME, &log_dir,
			/* TRANSLATORS: command line option */
			_("Set the logging directory"), "DIR" },
		{ "screenshot-dir", '\0', 0, G_OPTION_ARG_FILENAME, &screenshot_dir,
			/* TRANSLATORS: command line option */
			_("Set the screenshots directory"), "DIR" },
		{ "packages-dir", '\0', 0, G_OPTION_ARG_FILENAME, &packages_dir,
			/* TRANSLATORS: command line option */
			_("Set the packages directory"), "DIR" },
		{ "temp-dir", '\0', 0, G_OPTION_ARG_FILENAME, &temp_dir,
			/* TRANSLATORS: command line option */
			_("Set the temporary directory"), "DIR" },
		{ "extra-appstream-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_appstream,
			/* TRANSLATORS: command line option */
			_("Use extra appstream data"), "DIR" },
		{ "extra-appdata-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_appdata,
			/* TRANSLATORS: command line option */
			_("Use extra appdata data"), "DIR" },
		{ "extra-screenshots-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_screenshots,
			/* TRANSLATORS: command line option */
			_("Use extra screenshots data"), "DIR" },
		{ "output-dir", '\0', 0, G_OPTION_ARG_FILENAME, &output_dir,
			/* TRANSLATORS: command line option */
			_("Set the output directory"), "DIR" },
		{ "cache-dir", '\0', 0, G_OPTION_ARG_FILENAME, &output_dir,
			/* TRANSLATORS: command line option */
			_("Set the cache directory"), "DIR" },
		{ "basename", '\0', 0, G_OPTION_ARG_STRING, &basename,
			/* TRANSLATORS: command line option */
			_("Set the origin name"), "NAME" },
		{ "max-threads", '\0', 0, G_OPTION_ARG_INT, &max_threads,
			/* TRANSLATORS: command line option */
			_("Set the number of threads"), "THREAD_COUNT" },
		{ "api-version", '\0', 0, G_OPTION_ARG_DOUBLE, &api_version,
			/* TRANSLATORS: command line option */
			_("Set the AppStream version"), "API_VERSION" },
		{ "screenshot-uri", '\0', 0, G_OPTION_ARG_STRING, &screenshot_uri,
			/* TRANSLATORS: command line option */
			_("Set the screenshot base URL"), "URI" },
		{ "old-metadata", '\0', 0, G_OPTION_ARG_FILENAME, &old_metadata,
			/* TRANSLATORS: command line option */
			_("Set the old metadata location"), "DIR" },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	option_context = g_option_context_new (NULL);

	g_option_context_add_main_entries (option_context, options, NULL);
	ret = g_option_context_parse (option_context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_print ("%s: %s\n", _("Failed to parse arguments"), error->message);
		goto out;
	}

	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

#if !GLIB_CHECK_VERSION(2,40,0)
	if (max_threads > 1) {
		/* TRANSLATORS: debug message */
		g_debug ("O_CLOEXEC not available, using 1 core");
		max_threads = 1;
	}
#endif
	/* set defaults */
	if (api_version < 0.01)
		api_version = 0.41;
	if (packages_dir == NULL)
		packages_dir = g_strdup ("./packages");
	if (temp_dir == NULL)
		temp_dir = g_strdup ("./tmp");
	if (log_dir == NULL)
		log_dir = g_strdup ("./logs");
	if (screenshot_dir == NULL)
		screenshot_dir = g_strdup ("./screenshots");
	if (output_dir == NULL)
		output_dir = g_strdup (".");
	if (cache_dir == NULL)
		cache_dir = g_strdup ("./cache");
	if (basename == NULL)
		basename = g_strdup ("fedora-21");
	if (screenshot_uri == NULL)
		screenshot_uri = g_strdup ("http://alt.fedoraproject.org/pub/alt/screenshots/f21/");
	if (extra_appstream == NULL)
		extra_appstream = g_strdup ("./appstream-extra");
	if (extra_appdata == NULL)
		extra_appdata = g_strdup ("./appdata-extra");
	if (extra_screenshots == NULL)
		extra_screenshots = g_strdup ("./screenshots-extra");
	setlocale (LC_ALL, "");

	ctx = asb_context_new ();
	asb_context_set_no_net (ctx, no_net);
	asb_context_set_api_version (ctx, api_version);
	asb_context_set_add_cache_id (ctx, add_cache_id);
	asb_context_set_old_metadata (ctx, old_metadata);
	asb_context_set_extra_appstream (ctx, extra_appstream);
	asb_context_set_extra_appdata (ctx, extra_appdata);
	asb_context_set_extra_screenshots (ctx, extra_screenshots);
	asb_context_set_screenshot_uri (ctx, screenshot_uri);
	asb_context_set_log_dir (ctx, log_dir);
	asb_context_set_screenshot_dir (ctx, screenshot_dir);
	asb_context_set_temp_dir (ctx, temp_dir);
	asb_context_set_output_dir (ctx, output_dir);
	asb_context_set_cache_dir (ctx, cache_dir);
	asb_context_set_basename (ctx, basename);
	asb_context_set_max_threads (ctx, max_threads);
	ret = asb_context_setup (ctx, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_warning ("%s: %s", _("Failed to set up builder"), error->message);
		goto out;
	}

	/* scan each package */
	packages = g_ptr_array_new_with_free_func (g_free);
	if (argc == 1) {
		if (!as_builder_search_path (packages, packages_dir, &error)) {
			/* TRANSLATORS: error message */
			g_warning ("%s: %s", _("Failed to open packages"), error->message);
			goto out;
		}
	} else {
		for (i = 1; i < (guint) argc; i++)
			g_ptr_array_add (packages, g_strdup (argv[i]));
	}
	/* TRANSLATORS: information message */
	g_print ("%s\n", _("Scanning packages..."));
	timer = g_timer_new ();
	for (i = 0; i < packages->len; i++) {
		_cleanup_error_free_ GError *error_local = NULL;

		filename = g_ptr_array_index (packages, i);

		/* add to list */
		if (!asb_context_add_filename (ctx, filename, &error_local)) {
			/* TRANSLATORS: error message */
			g_print ("%s %s: %s\n", _("Failed to add package"),
				 filename, error_local->message);
			continue;
		}
		if (g_timer_elapsed (timer, NULL) > 3.f) {
			/* TRANSLATORS: information message */
			g_print (_("Parsed %i/%i files..."), i, packages->len);
			g_print ("\n"),
			g_timer_reset (timer);
		}
	}

	/* process all packages in the pool */
	ret = asb_context_process (ctx, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_warning ("%s: %s", _("Failed to generate metadata"), error->message);
		goto out;
	}

	/* success */
	/* TRANSLATORS: information message */
	g_print ("%s\n", _("Done!"));
out:
	g_option_context_free (option_context);
	if (ctx != NULL)
		g_object_unref (ctx);
	return 0;
}
