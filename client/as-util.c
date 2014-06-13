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

#include "as-cleanup.h"

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
	AsUtilItem *item;
	guint i;
	_cleanup_strv_free_ gchar **names = NULL;

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
	AsUtilItem *item;
	guint i;
	_cleanup_string_free_ GString *string = NULL;

	/* for bash completion */
	if (g_strcmp0 (command, "list-commands") == 0) {
		string = g_string_new ("");
		for (i = 0; i < priv->cmd_array->len; i++) {
			item = g_ptr_array_index (priv->cmd_array, i);
			g_string_append_printf  (string, "%s ", item->name);
		}
		g_string_truncate (string, string->len - 1);
		g_print ("%s\n", string->str);
		return TRUE;
	}

	/* find command */
	for (i = 0; i < priv->cmd_array->len; i++) {
		item = g_ptr_array_index (priv->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0)
			return item->callback (priv, values, error);
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
	return FALSE;
}

/**
 * as_util_convert:
 **/
static gboolean
as_util_convert (AsUtilPrivate *priv, gchar **values, GError **error)
{
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file_input = NULL;
	_cleanup_object_unref_ GFile *file_output = NULL;

	/* check args */
	if (g_strv_length (values) != 3) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected old.xml new.xml version");
		return FALSE;
	}

	/* load file */
	store = as_store_new ();
	file_input = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file_input, NULL, NULL, error))
		return FALSE;
	g_print ("Old API version: %.2f\n", as_store_get_api_version (store));

	/* save file */
	as_store_set_api_version (store, g_ascii_strtod (values[2], NULL));
	file_output = g_file_new_for_path (values[1]);
	if (!as_store_to_file (store, file_output,
				AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
				AS_NODE_TO_XML_FLAG_ADD_HEADER,
				NULL, error))
		return FALSE;
	g_print ("New API version: %.2f\n", as_store_get_api_version (store));
	return TRUE;
}

/**
 * as_util_dump:
 **/
static gboolean
as_util_dump (AsUtilPrivate *priv, gchar **values, GError **error)
{
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file_input = NULL;
	_cleanup_string_free_ GString *xml = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected data.xml");
		return FALSE;
	}

	/* load file */
	store = as_store_new ();
	file_input = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file_input, NULL, NULL, error))
		return FALSE;

	/* dump to screen */
	as_store_set_api_version (store, 1.0);
	xml = as_store_to_xml (store,
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_ADD_HEADER);
	g_print ("%s\n", xml->str);
	return TRUE;
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
	gsize len;
	int r;
	struct archive *arch = NULL;
	struct archive_entry *entry;
	_cleanup_free_ gchar *data = NULL;
	_cleanup_free_ gchar *dir = NULL;

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
	const gchar *destdir;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *path_dest = NULL;
	_cleanup_free_ gchar *path_parent = NULL;
	_cleanup_object_unref_ GFile *file_dest = NULL;
	_cleanup_object_unref_ GFile *file_src = NULL;

	/* create directory structure */
	destdir = g_getenv ("DESTDIR");
	path_parent = g_strdup_printf ("%s/usr/share/app-info/xmls",
				       destdir != NULL ? destdir : "");
	if (g_mkdir_with_parents (path_parent, 0777) != 0) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Failed to create %s", path_parent);
		return FALSE;
	}

	/* copy XML file */
	file_src = g_file_new_for_path (filename);
	basename = g_path_get_basename (filename);
	path_dest = g_build_filename (path_parent, basename, NULL);
	file_dest = g_file_new_for_path (path_dest);
	return g_file_copy (file_src, file_dest, G_FILE_COPY_OVERWRITE,
			    NULL, NULL, NULL, error);
}

/**
 * as_util_install_filename:
 **/
static gboolean
as_util_install_filename (const gchar *filename, GError **error)
{
	gchar *tmp;
	_cleanup_free_ gchar *basename = NULL;

	/* xml */
	tmp = g_strstr_len (filename, -1, ".xml.gz");
	if (tmp != NULL)
		return as_util_install_xml (filename, error);

	/* icons */
	basename = g_path_get_basename (filename);
	tmp = g_strstr_len (basename, -1, "-icons.tar.gz");
	if (tmp != NULL) {
		*tmp = '\0';
		return as_util_install_icons (filename, basename, error);
	}

	/* unrecognised */
	g_set_error_literal (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "No idea how to process files of this type");
	return FALSE;
}

/**
 * as_util_install:
 **/
static gboolean
as_util_install (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename(s)");
		return FALSE;
	}

	/* for each item on the command line, install the xml files and
	 * explode the icon files */
	for (i = 0; values[i] != NULL; i++) {
		if (!as_util_install_filename (values[i], error))
			return FALSE;
	}
	return TRUE;
}


/**
 * as_util_rmtree:
 **/
static gboolean
as_util_rmtree (const gchar *directory, GError **error)
{
	const gchar *filename;
	_cleanup_dir_close_ GDir *dir = NULL;

	/* try to open */
	dir = g_dir_open (directory, 0, error);
	if (dir == NULL)
		return FALSE;

	/* find each */
	while ((filename = g_dir_read_name (dir))) {
		_cleanup_free_ gchar *src = NULL;
		src = g_build_filename (directory, filename, NULL);
		if (g_file_test (src, G_FILE_TEST_IS_DIR)) {
			if (!as_util_rmtree (src, error))
				return FALSE;
		} else {
			if (g_unlink (src) != 0) {
				g_set_error (error,
					     G_IO_ERROR,
					     G_IO_ERROR_FAILED,
					     "Failed to delete %s", src);
				return FALSE;
			}
		}
	}

	/* remove directory */
	if (g_remove (directory) != 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Failed to delete %s", directory);
		return FALSE;
	}
	return TRUE;
}

/**
 * as_util_uninstall:
 **/
static gboolean
as_util_uninstall (AsUtilPrivate *priv, gchar **values, GError **error)
{
	const gchar *destdir;
	_cleanup_free_ gchar *path_icons = NULL;
	_cleanup_free_ gchar *path_xml = NULL;
	_cleanup_object_unref_ GFile *file_xml = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected appstream-id");
		return FALSE;
	}

	/* remove XML file */
	destdir = g_getenv ("DESTDIR");
	path_xml = g_strdup_printf ("%s/usr/share/app-info/xmls/%s.xml.gz",
				    destdir != NULL ? destdir : "", values[0]);
	if (!g_file_test (path_xml, G_FILE_TEST_EXISTS)) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_INVALID_ARGUMENTS,
			     "AppStream file with that ID not found: %s",
			     path_xml);
		return FALSE;
	}
	file_xml = g_file_new_for_path (path_xml);
	if (!g_file_delete (file_xml, NULL, error)) {
		g_prefix_error (error, "Failed to remove %s: ", path_xml);
		return FALSE;
	}

	/* remove icons */
	path_icons = g_strdup_printf ("%s/usr/share/app-info/icons/%s",
				      destdir != NULL ? destdir : "", values[0]);
	if (g_file_test (path_icons, G_FILE_TEST_EXISTS)) {
		if (!as_util_rmtree (path_icons, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_util_status_html_join:
 */
static gchar *
as_util_status_html_join (GPtrArray *array)
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
 * as_util_status_html_write_app:
 */
static void
as_util_status_html_write_app (AsApp *app, GString *html)
{
	GPtrArray *images;
	GPtrArray *screenshots;
	AsImage *im;
	AsImage *im_thumb;
	AsImage *im_scaled;
	AsScreenshot *ss;
	const gchar *pkgname;
	gchar *tmp;
	guint i;
	guint j;
	const gchar * const *kudos;

	g_string_append_printf (html, "<a name=\"%s\"/><h2>%s</h2>\n",
				as_app_get_id (app), as_app_get_id (app));

	/* print the screenshot thumbnails */
	screenshots = as_app_get_screenshots (app);
	for (i = 0; i < screenshots->len; i++) {
		ss  = g_ptr_array_index (screenshots, i);
		images = as_screenshot_get_images (ss);
		im_thumb = NULL;
		im_scaled = NULL;
		for (j = 0; j < images->len; j++) {
			im = g_ptr_array_index (images, j);
			if (as_image_get_width (im) == 112)
				im_thumb = im;
			else if (as_image_get_width (im) == 624)
				im_scaled = im;
		}
		if (im_thumb == NULL || im_scaled == NULL)
			continue;
		if (as_screenshot_get_caption (ss, "C") != NULL) {
			g_string_append_printf (html, "<a href=\"%s\">"
						"<img src=\"%s\" alt=\"%s\"/></a>\n",
						as_image_get_url (im_scaled),
						as_image_get_url (im_thumb),
						as_screenshot_get_caption (ss, "C"));
		} else {
			g_string_append_printf (html, "<a href=\"%s\">"
						"<img src=\"%s\"/></a>\n",
						as_image_get_url (im_scaled),
						as_image_get_url (im_thumb));
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
	tmp = as_util_status_html_join (as_app_get_pkgnames (app));
	if (tmp != NULL) {
		pkgname = g_ptr_array_index (as_app_get_pkgnames(app), 0);
		g_string_append_printf (html, "<tr><td>%s</td><td>"
					"<a href=\"https://apps.fedoraproject.org/packages/%s\">"
					"<code>%s</code></a></td></tr>\n",
					"Package", pkgname, tmp);
	}
	g_free (tmp);

	/* categories */
	tmp = as_util_status_html_join (as_app_get_categories (app));
	if (tmp != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
					"Categories", tmp);
	}
	g_free (tmp);

	/* keywords */
	tmp = as_util_status_html_join (as_app_get_keywords (app));
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
	tmp = as_util_status_html_join (as_app_get_compulsory_for_desktops (app));
	if (tmp != NULL) {
		g_string_append_printf (html, "<tr><td>%s</td><td>%s</td></tr>\n",
					"Compulsory for", tmp);
	}
	g_free (tmp);

	/* add all possible Kudo's for desktop files */
	if (as_app_get_id_kind (app) == AS_ID_KIND_DESKTOP) {
		kudos = as_util_get_possible_kudos ();
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
 * as_util_status_html_write_exec_summary:
 */
static gboolean
as_util_status_html_write_exec_summary (GPtrArray *apps,
					GString *html,
					GError **error)
{
	AsApp *app;
	const gchar *project_groups[] = { "GNOME", "KDE", "XFCE", NULL };
	gdouble perc;
	guint cnt;
	guint i;
	guint j;
	guint total = 0;

	g_string_append (html, "<h1>Executive summary</h1>\n");
	g_string_append (html, "<ul>\n");

	/* count number of desktop apps */
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) == AS_ID_KIND_DESKTOP)
			total++;
	}
	if (total == 0) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "No desktop applications found");
		return FALSE;
	}

	/* long descriptions */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) != AS_ID_KIND_DESKTOP)
			continue;
		if (as_app_get_description (app, "C") != NULL)
			cnt++;
	}
	perc = 100.f * (gdouble) cnt / (gdouble) total;
	g_string_append_printf (html, "<li>Applications in Fedora with "
				"long descriptions: %i/%i (%.1f%%)</li>\n",
				cnt, total, perc);

	/* keywords */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) != AS_ID_KIND_DESKTOP)
			continue;
		if (as_app_get_keywords(app)->len > 0)
			cnt++;
	}
	perc = 100.f * (gdouble) cnt / (gdouble) total;
	g_string_append_printf (html, "<li>Applications in Fedora with "
				"keywords: %i/%i (%.1f%%)</li>\n",
				cnt, total, perc);

	/* screenshots */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) != AS_ID_KIND_DESKTOP)
			continue;
		if (as_app_get_screenshots(app)->len > 0)
			cnt++;
	}
	perc = 100.f * (gdouble) cnt / (gdouble) total;
	g_string_append_printf (html, "<li>Applications in Fedora with "
				"screenshots: %i/%i (%.1f%%)</li>\n",
				cnt, total, perc);

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
			perc = 100.f * (gdouble) cnt / (gdouble) total;
		g_string_append_printf (html, "<li>Applications in %s "
					"with AppData: %i/%i (%.1f%%)</li>\n",
					project_groups[j], cnt,
					total, perc);
	}
	g_string_append (html, "</ul>\n");
	return TRUE;
}

/**
 * as_util_status_html:
 **/
static gboolean
as_util_status_html (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps = NULL;
	guint i;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_string_free_ GString *html = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename.xml.gz");
		return FALSE;
	}

	/* load file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file, NULL, NULL, error))
		return FALSE;
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
	if (apps->len > 0) {
		if (!as_util_status_html_write_exec_summary (apps, html, error))
			return FALSE;
	}

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
		as_util_status_html_write_app (app, html);
	}

	g_string_append (html, "</body>\n");
	g_string_append (html, "</html>\n");

	/* save file */
	if (!g_file_set_contents ("./status.html", html->str, -1, error))
		return FALSE;
	return TRUE;
}

/**
 * as_util_non_package_yaml:
 **/
static gboolean
as_util_non_package_yaml (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps = NULL;
	guint i;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_string_free_ GString *yaml = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename.xml.gz");
		return FALSE;
	}

	/* load file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file, NULL, NULL, error))
		return FALSE;
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
	if (!g_file_set_contents ("./applications-to-import.yaml", yaml->str, -1, error))
		return FALSE;
	return TRUE;
}

/**
 * as_util_validate_file:
 **/
static gboolean
as_util_validate_file (const gchar *filename,
		       AsAppValidateFlags flags,
		       GError **error)
{
	AsProblemKind kind;
	AsProblem *problem;
	guint i;
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *probs = NULL;

	/* load file */
	app = as_app_new ();
	g_print ("%s: ", filename);
	if (!as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, error))
		return FALSE;
	probs = as_app_validate (app, flags, error);
	if (probs == NULL)
		return FALSE;
	if (probs->len > 0) {
		g_print ("%s:\n", _("FAILED"));
		for (i = 0; i < probs->len; i++) {
			problem = g_ptr_array_index (probs, i);
			kind = as_problem_get_kind (problem);
			g_print ("%s\t%s\n",
				 as_problem_kind_to_string (kind),
				 as_problem_get_message (problem));
		}
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     _("Validation failed"));
		return FALSE;
	}
	g_print ("%s\n", _("OK"));
	return TRUE;
}

/**
 * as_util_validate_files:
 **/
static gboolean
as_util_validate_files (gchar **filenames,
		        AsAppValidateFlags flags,
		        GError **error)
{
	GError *error_local = NULL;
	guint i;
	guint n_failed = 0;

	/* check args */
	if (g_strv_length (filenames) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected example.appdata.xml");
		return FALSE;
	}

	/* check each file */
	for (i = 0; filenames[i] != NULL; i++) {
		if (as_util_validate_file (filenames[i], flags, &error_local))
			continue;

		/* anything other than AsProblems bail */
		n_failed++;
		if (!g_error_matches (error_local, AS_ERROR,
				      AS_ERROR_INVALID_ARGUMENTS)) {
			g_propagate_error (error, error_local);
			return FALSE;
		}
		g_clear_error (&error_local);
	}
	if (n_failed > 0) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     _("Validation of files failed"));
		return FALSE;
	}
	return n_failed == 0;
}

/**
 * as_util_validate:
 **/
static gboolean
as_util_validate (AsUtilPrivate *priv, gchar **values, GError **error)
{
	return as_util_validate_files (values, AS_APP_VALIDATE_FLAG_NONE, error);
}

/**
 * as_util_validate_relax:
 **/
static gboolean
as_util_validate_relax (AsUtilPrivate *priv, gchar **values, GError **error)
{
	return as_util_validate_files (values, AS_APP_VALIDATE_FLAG_RELAX, error);
}

/**
 * as_util_validate_strict:
 **/
static gboolean
as_util_validate_strict (AsUtilPrivate *priv, gchar **values, GError **error)
{
	return as_util_validate_files (values, AS_APP_VALIDATE_FLAG_STRICT, error);
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
	GError *error = NULL;
	guint retval = 1;
	_cleanup_free_ gchar *cmd_descriptions = NULL;
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
		     "status-html",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Create an HTML status page"),
		     as_util_status_html);
	as_util_add (priv->cmd_array,
		     "non-package-yaml",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("List applications not backed by packages"),
		     as_util_non_package_yaml);
	as_util_add (priv->cmd_array,
		     "validate",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Validate an AppData or AppStream file"),
		     as_util_validate);
	as_util_add (priv->cmd_array,
		     "validate-relax",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Validate an AppData or AppStream file (relaxed)"),
		     as_util_validate_relax);
	as_util_add (priv->cmd_array,
		     "validate-strict",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Validate an AppData or AppStream file (strict)"),
		     as_util_validate_strict);

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
	return retval;
}
