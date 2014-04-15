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

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <appstream-glib.h>
#include <archive_entry.h>
#include <archive.h>
#include <locale.h>

#define AS_ERROR			1
#define AS_ERROR_INVALID_ARGUMENTS	0
#define AS_ERROR_NO_SUCH_CMD		1
#define AS_ERROR_FAILED			2

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
} AsUtilPrivate;

typedef gboolean (*AsUtilPrivateCb)	(AsUtilPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
	gchar		*arguments;
	gchar		*description;
	AsUtilPrivateCb	 callback;
} AsUtilItem;

/**
 * as_util_item_free:
 **/
static void
as_util_item_free (AsUtilItem *item)
{
	g_free (item->name);
	g_free (item->arguments);
	g_free (item->description);
	g_free (item);
}

/**
 * as_sort_command_name_cb:
 **/
static gint
as_sort_command_name_cb (AsUtilItem **item1, AsUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

/**
 * as_util_add:
 **/
static void
as_util_add (GPtrArray *array,
	     const gchar *name,
	     const gchar *arguments,
	     const gchar *description,
	     AsUtilPrivateCb callback)
{
	gchar **names;
	guint i;
	AsUtilItem *item;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (callback != NULL);

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (i = 0; names[i] != NULL; i++) {
		item = g_new0 (AsUtilItem, 1);
		item->name = g_strdup (names[i]);
		if (i == 0) {
			item->description = g_strdup (description);
		} else {
			/* TRANSLATORS: this is a command alias */
			item->description = g_strdup_printf (_("Alias to %s"),
							     names[0]);
		}
		item->arguments = g_strdup (arguments);
		item->callback = callback;
		g_ptr_array_add (array, item);
	}
	g_strfreev (names);
}

/**
 * as_util_get_descriptions:
 **/
static gchar *
as_util_get_descriptions (GPtrArray *array)
{
	guint i;
	guint j;
	guint len;
	const guint max_len = 35;
	AsUtilItem *item;
	GString *string;

	/* print each command */
	string = g_string_new ("");
	for (i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name) + 2;
		if (item->arguments != NULL) {
			g_string_append (string, " ");
			g_string_append (string, item->arguments);
			len += strlen (item->arguments) + 1;
		}
		if (len < max_len) {
			for (j = len; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		} else {
			g_string_append_c (string, '\n');
			for (j = 0; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		}
	}

	/* remove trailing newline */
	if (string->len > 0)
		g_string_set_size (string, string->len - 1);

	return g_string_free (string, FALSE);
}

/**
 * as_util_run:
 **/
static gboolean
as_util_run (AsUtilPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	gboolean ret = FALSE;
	guint i;
	AsUtilItem *item;
	GString *string;

	/* find command */
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0) {
			ret = item->callback (priv, values, error);
			goto out;
		}
	}

	/* not found */
	string = g_string_new ("");
	/* TRANSLATORS: error message */
	g_string_append_printf (string, "%s\n",
				_("Command not found, valid commands are:"));
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		g_string_append_printf (string, " * %s %s\n",
					item->name,
					item->arguments ? item->arguments : "");
	}
	g_set_error_literal (error, AS_ERROR, AS_ERROR_NO_SUCH_CMD, string->str);
	g_string_free (string, TRUE);
out:
	return ret;
}

/**
 * as_util_convert:
 **/
static gboolean
as_util_convert (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsStore *store = NULL;
	GFile *file_input = NULL;
	GFile *file_output = NULL;
	gboolean ret = TRUE;

	/* check args */
	if (g_strv_length (values) != 3) {
		ret = FALSE;
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected old.xml new.xml version");
		goto out;
	}

	/* load file */
	store = as_store_new ();
	file_input = g_file_new_for_path (values[0]);
	ret = as_store_from_file (store, file_input, NULL, NULL, error);
	if (!ret)
		goto out;
	g_print ("Old API version: %.2f\n", as_store_get_api_version (store));

	/* save file */
	as_store_set_api_version (store, g_ascii_strtod (values[2], NULL));
	file_output = g_file_new_for_path (values[1]);
	ret = as_store_to_file (store, file_output,
				AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
				AS_NODE_TO_XML_FLAG_ADD_HEADER,
				NULL, error);
	if (!ret)
		goto out;
	g_print ("New API version: %.2f\n", as_store_get_api_version (store));
out:
	if (store != NULL)
		g_object_unref (store);
	if (file_input != NULL)
		g_object_unref (file_input);
	if (file_output != NULL)
		g_object_unref (file_output);
	return ret;
}

/**
 * as_util_dump:
 **/
static gboolean
as_util_dump (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsStore *store = NULL;
	GFile *file_input = NULL;
	GString *xml = NULL;
	gboolean ret = TRUE;

	/* check args */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected data.xml");
		goto out;
	}

	/* load file */
	store = as_store_new ();
	file_input = g_file_new_for_path (values[0]);
	ret = as_store_from_file (store, file_input, NULL, NULL, error);
	if (!ret)
		goto out;

	/* dump to screen */
	as_store_set_api_version (store, 1.0);
	xml = as_store_to_xml (store,
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_ADD_HEADER);
	g_print ("%s\n", xml->str);
out:
	if (store != NULL)
		g_object_unref (store);
	if (xml != NULL)
		g_string_free (xml, TRUE);
	if (file_input != NULL)
		g_object_unref (file_input);
	return ret;
}

/**
 * as_util_install_icons:
 **/
static gboolean
as_util_install_icons (const gchar *filename, const gchar *origin, GError **error)
{
	const gchar *destdir;
	const gchar *tmp;
	gboolean ret = TRUE;
	gchar buf[PATH_MAX];
	gchar *data = NULL;
	gchar *dir;
	gsize len;
	int r;
	struct archive *arch = NULL;
	struct archive_entry *entry;

	destdir = g_getenv ("DESTDIR");
	dir = g_strdup_printf ("%s/usr/share/app-info/icons/%s",
			       destdir != NULL ? destdir : "", origin);

	/* load file at once to avoid seeking */
	ret = g_file_get_contents (filename, &data, &len, error);
	if (!ret)
		goto out;

	/* read anything */
	arch = archive_read_new ();
	archive_read_support_format_all (arch);
	archive_read_support_filter_all (arch);
	r = archive_read_open_memory (arch, data, len);
	if (r) {
		ret = FALSE;
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Cannot open: %s",
			     archive_error_string (arch));
		goto out;
	}

	/* decompress each file */
	for (;;) {
		r = archive_read_next_header (arch, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			ret = FALSE;
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "Cannot read header: %s",
				     archive_error_string (arch));
			goto out;
		}

		/* no output file */
		if (archive_entry_pathname (entry) == NULL)
			continue;

		/* update output path */
		g_snprintf (buf, PATH_MAX, "%s/%s",
			    dir, archive_entry_pathname (entry));
		archive_entry_update_pathname_utf8 (entry, buf);

		/* update hardlinks */
		tmp = archive_entry_hardlink (entry);
		if (tmp != NULL) {
			g_snprintf (buf, PATH_MAX, "%s/%s", dir, tmp);
			archive_entry_update_hardlink_utf8 (entry, buf);
		}

		/* update symlinks */
		tmp = archive_entry_symlink (entry);
		if (tmp != NULL) {
			g_snprintf (buf, PATH_MAX, "%s/%s", dir, tmp);
			archive_entry_update_symlink_utf8 (entry, buf);
		}

		r = archive_read_extract (arch, entry, 0);
		if (r != ARCHIVE_OK) {
			ret = FALSE;
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "Cannot extract: %s",
				     archive_error_string (arch));
			goto out;
		}
	}
out:
	g_free (data);
	g_free (dir);
	if (arch != NULL) {
		archive_read_close (arch);
		archive_read_free (arch);
	}
	return ret;
}

/**
 * as_util_install_xml:
 **/
static gboolean
as_util_install_xml (const gchar *filename, GError **error)
{
	GFile *file_dest = NULL;
	GFile *file_src = NULL;
	const gchar *destdir;
	gboolean ret;
	gchar *basename = NULL;
	gchar *path_dest = NULL;
	gchar *path_parent = NULL;
	gint rc;

	/* create directory structure */
	destdir = g_getenv ("DESTDIR");
	path_parent = g_strdup_printf ("%s/usr/share/app-info/xmls",
				       destdir != NULL ? destdir : "");
	rc = g_mkdir_with_parents (path_parent, 0777);
	if (rc != 0) {
		ret = FALSE;
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Failed to create %s", path_parent);
		goto out;
	}

	/* copy XML file */
	file_src = g_file_new_for_path (filename);
	basename = g_path_get_basename (filename);
	path_dest = g_build_filename (path_parent, basename, NULL);
	file_dest = g_file_new_for_path (path_dest);
	ret = g_file_copy (file_src, file_dest, G_FILE_COPY_OVERWRITE,
			   NULL, NULL, NULL, error);
out:
	if (file_dest != NULL)
		g_object_unref (file_dest);
	if (file_src != NULL)
		g_object_unref (file_src);
	g_free (basename);
	g_free (path_parent);
	g_free (path_dest);
	return ret;
}

/**
 * as_util_install_filename:
 **/
static gboolean
as_util_install_filename (const gchar *filename, GError **error)
{
	gchar *basename = NULL;
	gchar *tmp;
	gboolean ret = FALSE;

	/* xml */
	tmp = g_strstr_len (filename, -1, ".xml.gz");
	if (tmp != NULL) {
		ret = as_util_install_xml (filename, error);
		goto out;
	}

	/* icons */
	basename = g_path_get_basename (filename);
	tmp = g_strstr_len (basename, -1, "-icons.tar.gz");
	if (tmp != NULL) {
		*tmp = '\0';
		ret = as_util_install_icons (filename, basename, error);
		goto out;
	}

	/* unrecognised */
	g_set_error_literal (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "No idea how to process files of this type");
out:
	g_free (basename);
	return ret;
}

/**
 * as_util_install:
 **/
static gboolean
as_util_install (AsUtilPrivate *priv, gchar **values, GError **error)
{
	gboolean ret = TRUE;
	guint i;

	/* check args */
	if (g_strv_length (values) < 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename(s)");
		goto out;
	}

	/* for each item on the command line, install the xml files and
	 * explode the icon files */
	for (i = 0; values[i] != NULL; i++) {
		ret = as_util_install_filename (values[i], error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}


/**
 * as_util_rmtree:
 **/
static gboolean
as_util_rmtree (const gchar *directory, GError **error)
{
	const gchar *filename;
	gboolean ret = TRUE;
	gchar *src;
	GDir *dir = NULL;
	gint rc;

	/* try to open */
	dir = g_dir_open (directory, 0, error);
	if (dir == NULL) {
		ret = FALSE;
		goto out;
	}

	/* find each */
	while ((filename = g_dir_read_name (dir))) {
		src = g_build_filename (directory, filename, NULL);
		ret = g_file_test (src, G_FILE_TEST_IS_DIR);
		if (ret) {
			ret = as_util_rmtree (src, error);
			if (!ret)
				goto out;
		} else {
			rc = g_unlink (src);
			if (rc != 0) {
				ret = FALSE;
				g_set_error (error,
					     G_IO_ERROR,
					     G_IO_ERROR_FAILED,
					     "Failed to delete %s", src);
				goto out;
			}
		}
		g_free (src);
	}

	/* remove directory */
	rc = g_remove (directory);
	if (rc != 0) {
		ret = FALSE;
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Failed to delete %s", directory);
		goto out;
	}
out:
	if (dir != NULL)
		g_dir_close (dir);
	return ret;
}

/**
 * as_util_uninstall:
 **/
static gboolean
as_util_uninstall (AsUtilPrivate *priv, gchar **values, GError **error)
{
	GFile *file_xml = NULL;
	const gchar *destdir;
	gboolean ret = TRUE;
	gchar *path_icons = NULL;
	gchar *path_xml = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected appstream-id");
		goto out;
	}

	/* remove XML file */
	destdir = g_getenv ("DESTDIR");
	path_xml = g_strdup_printf ("%s/usr/share/app-info/xmls/%s.xml.gz",
				    destdir != NULL ? destdir : "", values[0]);
	if (!g_file_test (path_xml, G_FILE_TEST_EXISTS)) {
		ret = FALSE;
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_INVALID_ARGUMENTS,
			     "AppStream file with that ID not found: %s",
			     path_xml);
		goto out;
	}
	file_xml = g_file_new_for_path (path_xml);
	ret = g_file_delete (file_xml, NULL, error);
	if (!ret) {
		g_prefix_error (error, "Failed to remove %s: ", path_xml);
		goto out;
	}

	/* remove icons */
	path_icons = g_strdup_printf ("%s/usr/share/app-info/icons/%s",
				      destdir != NULL ? destdir : "", values[0]);
	if (g_file_test (path_icons, G_FILE_TEST_EXISTS)) {
		ret = as_util_rmtree (path_icons, error);
		if (!ret)
			goto out;
	}
out:
	g_free (path_icons);
	g_free (path_xml);
	if (file_xml != NULL)
		g_object_unref (file_xml);
	return ret;
}

/**
 * as_util_status_join:
 */
static gchar *
as_util_status_join (GPtrArray *array)
{
	const gchar *tmp;
	guint i;
	GString *txt;

	if (array == NULL)
		return NULL;
	if (array->len == 0)
		return NULL;

	txt = g_string_new ("");
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (txt->len > 0)
			g_string_append (txt, ", ");
		g_string_append (txt, tmp);
	}
	return g_string_free (txt, FALSE);
}

/**
 * as_util_status_write_app:
 */
static void
as_util_status_write_app (AsApp *app, GString *html)
{
	GPtrArray *images;
	GPtrArray *screenshots;
	AsImage *im;
	AsScreenshot *ss;
	const gchar *pkgname;
	gchar *tmp;
	guint i;
	guint j;
	const gchar *kudos[] = {
		"X-Kudo-SearchProvider",
		"X-Kudo-InstallsUserDocs",
		"X-Kudo-UsesAppMenu",
		"X-Kudo-GTK3",
		"X-Kudo-RecentRelease",
		"X-Kudo-UsesNotifications",
		NULL };

	g_string_append_printf (html, "<a name=\"%s\"/><h2>%s</h2>\n",
				as_app_get_id (app), as_app_get_id (app));

	/* print the screenshot thumbnails */
	screenshots = as_app_get_screenshots (app);
	for (i = 0; i < screenshots->len; i++) {
		ss  = g_ptr_array_index (screenshots, i);
		images = as_screenshot_get_images (ss);
		for (j = 0; j < images->len; j++) {
			im = g_ptr_array_index (images, j);
			if (as_image_get_width (im) != 624)
				continue;
			if (as_screenshot_get_caption (ss, "C") != NULL) {
				g_string_append_printf (html, "<a href=\"%s\">"
							"<img src=\"%s\" alt=\"%s\"/></a>\n",
							as_image_get_url (im),
							as_image_get_url (im),
							as_screenshot_get_caption (ss, "C"));
			} else {
				g_string_append_printf (html, "<a href=\"%s\">"
							"<img src=\"%s\"/></a>\n",
							as_image_get_url (im),
							as_image_get_url (im));
			}
		}
	}

	g_string_append (html, "<table>\n");

	/* summary */
	g_string_append_printf (html, "<tr><td>%s</td><td><code>%s</code></td></tr>\n",
				"Type", as_id_kind_to_string (as_app_get_id_kind (app)));
	g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
				"Name", as_app_get_name (app, "C"));
	g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
				"Comment", as_app_get_comment (app, "C"));
	if (as_app_get_description (app, "C") != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
				"Description", as_app_get_description (app, "C"));
	}

	/* packages */
	tmp = as_util_status_join (as_app_get_pkgnames (app));
	if (tmp != NULL) {
		pkgname = g_ptr_array_index (as_app_get_pkgnames(app), 0);
		g_string_append_printf (html, "<tr><td>%s</td><td>"
					"<a href=\"https://apps.fedoraproject.org/packages/%s\">"
					"<code>%s</code></a></td></tr>\n",
					"Package", pkgname, tmp);
	}
	g_free (tmp);

	/* categories */
	tmp = as_util_status_join (as_app_get_categories (app));
	if (tmp != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
					"Categories", tmp);
	}
	g_free (tmp);

	/* keywords */
	tmp = as_util_status_join (as_app_get_keywords (app));
	if (tmp != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
					"Keywords", tmp);
	}
	g_free (tmp);

	/* homepage */
	pkgname = as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE);
	if (pkgname != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td><a href=\"%s\">"
					"%s</a></td></tr>\n",
					"Homepage", pkgname, pkgname);
	}

	/* project */
	if (as_app_get_project_group (app) != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
					"Project", as_app_get_project_group (app));
	}

	/* desktops */
	tmp = as_util_status_join (as_app_get_compulsory_for_desktops (app));
	if (tmp != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
					"Compulsory for", tmp);
	}
	g_free (tmp);

	/* add all possible Kudo's for desktop files */
	if (as_app_get_id_kind (app) == AS_ID_KIND_DESKTOP) {
		for (i = 0; kudos[i] != NULL; i++) {
			pkgname = as_app_get_metadata_item (app, kudos[i]) ?
					"Yes" : "No";
			g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
						kudos[i], pkgname);
		}
	}

	g_string_append (html, "</table>\n");
	g_string_append (html, "<hr/>\n");
}

/**
 * as_util_status_write_exec_summary:
 */
static void
as_util_status_write_exec_summary (GPtrArray *apps, GString *html)
{
	AsApp *app;
	const gchar *project_groups[] = { "GNOME", "KDE", "XFCE", NULL };
	guint cnt;
	guint i;
	guint j;
	guint perc;
	guint total;

	g_string_append (html, "<h1>Executive summary</h1>\n");
	g_string_append (html, "<ul>\n");

	/* long descriptions */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_description (app, "C") != NULL)
			cnt++;
	}
	perc = 100 * cnt / apps->len;
	g_string_append_printf (html, "<li>Applications in Fedora with "
				"long descriptions: %i (%i%%)</li>\n", cnt, perc);

	/* keywords */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_keywords(app)->len > 0)
			cnt++;
	}
	perc = 100 * cnt / apps->len;
	g_string_append_printf (html, "<li>Applications in Fedora with "
				"keywords: %i (%i%%)</li>\n", cnt, perc);

	/* categories */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_categories(app)->len > 0)
			cnt++;
	}
	perc = 100 * cnt / apps->len;
	g_string_append_printf (html, "<li>Applications in Fedora with "
				"categories: %i (%i%%)</li>\n", cnt, perc);

	/* screenshots */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_screenshots(app)->len > 0)
			cnt++;
	}
	perc = 100 * cnt / apps->len;
	g_string_append_printf (html, "<li>Applications in Fedora with "
				"screenshots: %i (%i%%)</li>\n", cnt, perc);

	/* project apps with appdata */
	for (j = 0; project_groups[j] != NULL; j++) {
		cnt = 0;
		total = 0;
		for (i = 0; i < apps->len; i++) {
			app = g_ptr_array_index (apps, i);
			if (g_strcmp0 (as_app_get_project_group (app),
				       project_groups[j]) != 0)
				continue;
			total += 1;
			if (as_app_get_screenshots(app)->len > 0 ||
			    as_app_get_description (app, "C") != NULL)
				cnt++;
		}
		perc = 0;
		if (total > 0)
			perc = 100 * cnt / total;
		g_string_append_printf (html, "<li>Applications in %s "
					"with AppData: %i (%i%%)</li>\n",
					project_groups[j], cnt, perc);
	}
	g_string_append (html, "</ul>\n");
}

/**
 * as_util_status:
 **/
static gboolean
as_util_status (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	AsStore *store = NULL;
	GFile *file = NULL;
	GPtrArray *apps = NULL;
	GString *html = NULL;
	gboolean ret = TRUE;
	guint i;

	/* check args */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename.xml.gz");
		goto out;
	}

	/* load file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	ret = as_store_from_file (store, file, NULL, NULL, error);
	if (!ret)
		goto out;
	apps = as_store_get_apps (store);

	/* create header */
	html = g_string_new ("");
	g_string_append (html, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 "
			       "Transitional//EN\" "
			       "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	g_string_append (html, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	g_string_append (html, "<head>\n");
	g_string_append (html, "<meta http-equiv=\"Content-Type\" content=\"text/html; "
			       "charset=UTF-8\" />\n");
	g_string_append (html, "<title>Application Data Review</title>\n");
	g_string_append (html, "</head>\n");
	g_string_append (html, "<body>\n");

	/* summary section */
	if (apps->len > 0)
		as_util_status_write_exec_summary (apps, html);

	/* write applications */
	g_string_append (html, "<h1>Applications</h1>\n");
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) == AS_ID_KIND_FONT)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_INPUT_METHOD)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_CODEC)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_SOURCE)
			continue;
		as_util_status_write_app (app, html);
	}

	g_string_append (html, "</body>\n");
	g_string_append (html, "</html>\n");

	/* save file */
	ret = g_file_set_contents ("./status.html", html->str, -1, error);
	if (!ret)
		goto out;
out:
	if (html != NULL)
		g_string_free (html, TRUE);
	if (store != NULL)
		g_object_unref (store);
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * as_util_non_package_yaml:
 **/
static gboolean
as_util_non_package_yaml (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	AsStore *store = NULL;
	GFile *file = NULL;
	GPtrArray *apps = NULL;
	GString *yaml = NULL;
	gboolean ret = TRUE;
	guint i;

	/* check args */
	if (g_strv_length (values) != 1) {
		ret = FALSE;
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename.xml.gz");
		goto out;
	}

	/* load file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	ret = as_store_from_file (store, file, NULL, NULL, error);
	if (!ret)
		goto out;
	apps = as_store_get_apps (store);

	/* write applications */
	yaml = g_string_new ("# automatically generated, do not edit\n");
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_pkgnames(app)->len > 0)
			continue;
		g_string_append_printf (yaml, "- id: %s\n",
					as_app_get_id (app));
		g_string_append_printf (yaml, "  name: %s\n",
					as_app_get_name (app, "C"));
		g_string_append_printf (yaml, "  summary: %s\n",
					as_app_get_comment (app, "C"));
	}

	/* save file */
	ret = g_file_set_contents ("./applications-to-import.yaml", yaml->str, -1, error);
	if (!ret)
		goto out;
out:
	if (yaml != NULL)
		g_string_free (yaml, TRUE);
	if (store != NULL)
		g_object_unref (store);
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * as_util_ignore_cb:
 **/
static void
as_util_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		   const gchar *message, gpointer user_data)
{
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	AsUtilPrivate *priv;
	gboolean ret;
	gboolean verbose = FALSE;
	gboolean version = FALSE;
	gchar *cmd_descriptions = NULL;
	GError *error = NULL;
	guint retval = 1;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
			/* TRANSLATORS: command line option */
			_("Show client and daemon versions"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	/* create helper object */
	priv = g_new0 (AsUtilPrivate, 1);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) as_util_item_free);
	as_util_add (priv->cmd_array,
		     "convert",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Converts AppStream metadata from one version to another"),
		     as_util_convert);
	as_util_add (priv->cmd_array,
		     "dump",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Dumps the applications in the AppStream metadata"),
		     as_util_dump);
	as_util_add (priv->cmd_array,
		     "install",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Installs AppStream metadata"),
		     as_util_install);
	as_util_add (priv->cmd_array,
		     "uninstall",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Uninstalls AppStream metadata"),
		     as_util_uninstall);
	as_util_add (priv->cmd_array,
		     "status",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Create an HTML status page"),
		     as_util_status);
	as_util_add (priv->cmd_array,
		     "non-package-yaml",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("List applications not backed by packages"),
		     as_util_non_package_yaml);

	/* sort by command name */
	g_ptr_array_sort (priv->cmd_array,
			  (GCompareFunc) as_sort_command_name_cb);

	/* get a list of the commands */
	priv->context = g_option_context_new (NULL);
	cmd_descriptions = as_util_get_descriptions (priv->cmd_array);
	g_option_context_set_summary (priv->context, cmd_descriptions);

	/* TRANSLATORS: program name */
	g_set_application_name (_("AppStream Utility"));
	g_option_context_add_main_entries (priv->context, options, NULL);
	ret = g_option_context_parse (priv->context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n",
			 _("Failed to parse arguments"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* set verbose? */
	if (verbose) {
		g_setenv ("AS_VERBOSE", "1", FALSE);
	} else {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   as_util_ignore_cb, NULL);
	}

	/* get version */
	if (version) {
		g_print ("%s\t%s\n", _("Version:"), PACKAGE_VERSION);
		goto out;
	}

	/* run the specified command */
	ret = as_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		if (g_error_matches (error, AS_ERROR, AS_ERROR_NO_SUCH_CMD)) {
			gchar *tmp;
			tmp = g_option_context_get_help (priv->context, TRUE, NULL);
			g_print ("%s", tmp);
			g_free (tmp);
		} else {
			g_print ("%s\n", error->message);
		}
		g_error_free (error);
		goto out;
	}

	/* success */
	retval = 0;
out:
	if (priv != NULL) {
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	g_free (cmd_descriptions);
	return retval;
}
