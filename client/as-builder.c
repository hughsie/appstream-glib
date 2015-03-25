/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_OSTREE
#include <ostree.h>
#include "asb-package-ostree.h"
#endif

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
 * as_builder_setup_ostree:
 **/
static gboolean
as_builder_setup_ostree (AsbContext *ctx, const gchar *ostree_repo, GError **error)
{
#ifdef HAVE_OSTREE
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	_cleanup_hashtable_unref_ GHashTable *refs = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_object_unref_ OstreeRepo *repo = NULL;

	/* load repo */
	file = g_file_new_for_path (ostree_repo);
	repo = ostree_repo_new (file);
	if (!ostree_repo_list_refs (repo, NULL, &refs, NULL, error))
		return FALSE;

	/* create a fake package for each of the refs */
	g_hash_table_iter_init (&iter, refs);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *refspec = key;
		_cleanup_free_ gchar *remote = NULL;
		_cleanup_free_ gchar *name = NULL;
		_cleanup_free_ gchar *filename = NULL;
		_cleanup_free_ gchar *ref = NULL;
		_cleanup_object_unref_ AsbPackage *pkg = NULL;

		if (!ostree_parse_refspec (refspec, &remote, &ref, error))
			return FALSE;

                if (!g_str_has_prefix (ref, "app/") &&
                    !g_str_has_prefix (ref, "runtime/"))
                  continue;

		pkg = asb_package_ostree_new ();
		asb_package_set_source (pkg, ref);
		asb_package_ostree_set_repodir (ASB_PACKAGE_OSTREE (pkg), ostree_repo);

		/* open the package */
		filename = g_build_filename (ostree_repo, name, NULL);
		if (!asb_package_open (pkg, filename, error))
			return FALSE;
		asb_context_add_package (ctx, pkg);
	}
	return TRUE;
#else
	g_set_error_literal (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "No ostree support enabled");
	return FALSE;
#endif
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	AsbContext *ctx = NULL;
	AsbContextFlags flags = ASB_CONTEXT_FLAG_NONE;
	GOptionContext *option_context;
	const gchar *filename;
	gboolean add_cache_id = FALSE;
	gboolean batch_output = FALSE;
	gboolean embedded_icons = FALSE;
	gboolean hidpi_enabled = FALSE;
	gboolean include_failed = FALSE;
	gboolean ret;
	gboolean uncompressed_icons = FALSE;
	gboolean verbose = FALSE;
	gdouble api_version = 0.0f;
	gint max_threads = 4;
	gint min_icon_size = 32;
	guint i;
	int retval = EXIT_SUCCESS;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *cache_dir = NULL;
	_cleanup_free_ gchar *extra_appdata = NULL;
	_cleanup_free_ gchar *extra_screenshots = NULL;
	_cleanup_free_ gchar *log_dir = NULL;
	_cleanup_free_ gchar *icons_dir = NULL;
	_cleanup_free_ gchar *old_metadata = NULL;
	_cleanup_free_ gchar *origin = NULL;
	_cleanup_free_ gchar *ostree_repo = NULL;
	_cleanup_free_ gchar *output_dir = NULL;
	_cleanup_free_ gchar *temp_dir = NULL;
	_cleanup_free_ gchar **veto_ignore = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *packages = NULL;
	_cleanup_strv_free_ gchar **packages_dirs = NULL;
	_cleanup_timer_destroy_ GTimer *timer = NULL;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "add-cache-id", '\0', 0, G_OPTION_ARG_NONE, &add_cache_id,
			/* TRANSLATORS: command line option */
			_("Add a cache ID to each component"), NULL },
		{ "include-failed", '\0', 0, G_OPTION_ARG_NONE, &include_failed,
			/* TRANSLATORS: command line option */
			_("Include failed results in the output"), NULL },
		{ "enable-hidpi", '\0', 0, G_OPTION_ARG_NONE, &hidpi_enabled,
			/* TRANSLATORS: command line option */
			_("Add HiDPI icons to the tarball"), NULL },
		{ "enable-embed", '\0', 0, G_OPTION_ARG_NONE, &embedded_icons,
			/* TRANSLATORS: command line option */
			_("Add encoded icons to the XML"), NULL },
		{ "uncompressed-icons", '\0', 0, G_OPTION_ARG_NONE, &uncompressed_icons,
			/* TRANSLATORS: command line option */
			_("Do not compress the icons into a tarball"), NULL },
		{ "batch-output", '\0', 0, G_OPTION_ARG_NONE, &batch_output,
			/* TRANSLATORS: command line option */
			_("Do not use escape sequences in terminal output"), NULL },
		{ "log-dir", '\0', 0, G_OPTION_ARG_FILENAME, &log_dir,
			/* TRANSLATORS: command line option */
			_("Set the logging directory"), "DIR" },
		{ "packages-dir", '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &packages_dirs,
			/* TRANSLATORS: command line option */
			_("Set the packages directory"), "DIR" },
		{ "temp-dir", '\0', 0, G_OPTION_ARG_FILENAME, &temp_dir,
			/* TRANSLATORS: command line option */
			_("Set the temporary directory"), "DIR" },
		{ "extra-appdata-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_appdata,
			/* TRANSLATORS: command line option */
			_("Use extra appdata data"), "DIR" },
		{ "extra-screenshots-dir", '\0', 0, G_OPTION_ARG_FILENAME, &extra_screenshots,
			/* TRANSLATORS: command line option */
			_("Use extra screenshots data"), "DIR" },
		{ "output-dir", '\0', 0, G_OPTION_ARG_FILENAME, &output_dir,
			/* TRANSLATORS: command line option */
			_("Set the output directory"), "DIR" },
		{ "icons-dir", '\0', 0, G_OPTION_ARG_FILENAME, &icons_dir,
			/* TRANSLATORS: command line option */
			_("Set the icons directory"), "DIR" },
		{ "cache-dir", '\0', 0, G_OPTION_ARG_FILENAME, &cache_dir,
			/* TRANSLATORS: command line option */
			_("Set the cache directory"), "DIR" },
		{ "basename", '\0', 0, G_OPTION_ARG_STRING, &basename,
			/* TRANSLATORS: command line option */
			_("Set the basenames of the output files"), "NAME" },
		{ "origin", '\0', 0, G_OPTION_ARG_STRING, &origin,
			/* TRANSLATORS: command line option */
			_("Set the origin name"), "NAME" },
		{ "max-threads", '\0', 0, G_OPTION_ARG_INT, &max_threads,
			/* TRANSLATORS: command line option */
			_("Set the number of threads"), "THREAD_COUNT" },
		{ "min-icon-size", '\0', 0, G_OPTION_ARG_INT, &min_icon_size,
			/* TRANSLATORS: command line option */
			_("Set the minimum icon size in pixels"), "ICON_SIZE" },
		{ "api-version", '\0', 0, G_OPTION_ARG_DOUBLE, &api_version,
			/* TRANSLATORS: command line option */
			_("Set the AppStream version"), "API_VERSION" },
		{ "ostree-repo", '\0', 0, G_OPTION_ARG_STRING, &ostree_repo,
			/* TRANSLATORS: command line option */
			_("Set the ostree repo name"), "REPO" },
		{ "old-metadata", '\0', 0, G_OPTION_ARG_FILENAME, &old_metadata,
			/* TRANSLATORS: command line option */
			_("Set the old metadata location"), "DIR" },
		{ "veto-ignore", '\0', 0, G_OPTION_ARG_STRING_ARRAY, &veto_ignore,
			/* TRANSLATORS: command line option */
			_("Ignore certain types of veto"), "NAME" },
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
		retval = EXIT_FAILURE;
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
		api_version = 0.8;
	if (temp_dir == NULL)
		temp_dir = g_strdup ("./tmp");
	if (log_dir == NULL)
		log_dir = g_strdup ("./logs");
	if (output_dir == NULL)
		output_dir = g_strdup (".");
	if (icons_dir == NULL)
		icons_dir = g_build_filename (temp_dir, "icons", NULL);
	if (cache_dir == NULL)
		cache_dir = g_strdup ("./cache");
	if (basename == NULL)
		basename = g_strdup ("appstream");
	if (origin == NULL)
		origin = g_strdup ("example");
	if (extra_appdata == NULL)
		extra_appdata = g_strdup ("./appdata-extra");
	if (extra_screenshots == NULL)
		extra_screenshots = g_strdup ("./screenshots-extra");
	setlocale (LC_ALL, "");

	ctx = asb_context_new ();
	asb_context_set_api_version (ctx, api_version);
	asb_context_set_old_metadata (ctx, old_metadata);
	asb_context_set_extra_appdata (ctx, extra_appdata);
	asb_context_set_extra_screenshots (ctx, extra_screenshots);
	asb_context_set_log_dir (ctx, log_dir);
	asb_context_set_temp_dir (ctx, temp_dir);
	asb_context_set_output_dir (ctx, output_dir);
	asb_context_set_icons_dir (ctx, icons_dir);
	asb_context_set_cache_dir (ctx, cache_dir);
	asb_context_set_basename (ctx, basename);
	asb_context_set_origin (ctx, origin);
	asb_context_set_max_threads (ctx, max_threads);
	asb_context_set_min_icon_size (ctx, min_icon_size);

	/* parse the veto ignore flags */
	if (veto_ignore != NULL) {
		for (i = 0; veto_ignore[i] != NULL; i++) {
			if (g_strcmp0 (veto_ignore[i], "missing-info") == 0) {
				flags |= ASB_CONTEXT_FLAG_IGNORE_MISSING_INFO;
				continue;
			}
			if (g_strcmp0 (veto_ignore[i], "missing-parents") == 0) {
				flags |= ASB_CONTEXT_FLAG_IGNORE_MISSING_PARENTS;
				continue;
			}
			g_warning ("Unknown flag name: %s, "
				   "expected 'missing-info' or 'missing-parents'",
				   veto_ignore[i]);
		}
	}

	/* set build flags */
	if (hidpi_enabled)
		flags |= ASB_CONTEXT_FLAG_HIDPI_ICONS;
	if (add_cache_id)
		flags |= ASB_CONTEXT_FLAG_ADD_CACHE_ID;
	if (embedded_icons)
		flags |= ASB_CONTEXT_FLAG_EMBEDDED_ICONS;
	if (include_failed)
		flags |= ASB_CONTEXT_FLAG_INCLUDE_FAILED;
	if (uncompressed_icons)
		flags |= ASB_CONTEXT_FLAG_UNCOMPRESSED_ICONS;
	if (batch_output)
		flags |= ASB_CONTEXT_FLAG_BATCH_OUTPUT;
	asb_context_set_flags (ctx, flags);

	ret = asb_context_setup (ctx, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_warning ("%s: %s", _("Failed to set up builder"), error->message);
		retval = EXIT_FAILURE;
		goto out;
	}

	/* scan each package */
	packages = g_ptr_array_new_with_free_func (g_free);
	if (argc == 1 && ostree_repo == NULL) {
		/* if the user launches the tool with no arguments */
		if (packages_dirs == NULL) {
			_cleanup_free_ gchar *tmp = NULL;
			tmp = g_option_context_get_help (option_context, TRUE, NULL);
			g_print ("%s", tmp);
			retval = EXIT_FAILURE;
			goto out;
		}
		for (i = 0; packages_dirs[i] != NULL; i++) {
			if (!as_builder_search_path (packages, packages_dirs[i], &error)) {
				/* TRANSLATORS: error message */
				g_warning ("%s: %s", _("Failed to open packages"), error->message);
				retval = EXIT_FAILURE;
				goto out;
			}
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
			g_debug ("%s %s: %s", _("Failed to add package"),
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

	/* load and data from an ostree repo */
	if (ostree_repo != NULL) {
		if (!as_builder_setup_ostree (ctx, ostree_repo, &error)) {
			g_print ("%s\n", error->message);
			retval = EXIT_FAILURE;
			goto out;
		}
	}

	/* process all packages in the pool */
	ret = asb_context_process (ctx, &error);
	if (!ret) {
		/* TRANSLATORS: error message */
		g_warning ("%s: %s", _("Failed to generate metadata"), error->message);
		retval = EXIT_FAILURE;
		goto out;
	}

	/* success */
	/* TRANSLATORS: information message */
	g_print ("%s\n", _("Done!"));
out:
	g_option_context_free (option_context);
	if (ctx != NULL)
		g_object_unref (ctx);
	return retval;
}
