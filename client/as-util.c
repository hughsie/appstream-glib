/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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
#include <glib-unix.h>
#include <gio/gio.h>

#include <appstream-glib.h>
#include <archive_entry.h>
#include <archive.h>
#include <libsoup/soup.h>
#include <locale.h>
#include <stdlib.h>

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-app-private.h>

#define AS_ERROR			1
#define AS_ERROR_INVALID_ARGUMENTS	0
#define AS_ERROR_NO_SUCH_CMD		1
#define AS_ERROR_FAILED			2

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	gboolean		 nonet;
	gboolean		 verbose;
	GMainLoop		*loop;
	GCancellable		*cancellable;
	AsProfile		*profile;
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

static void
as_util_item_free (AsUtilItem *item)
{
	g_free (item->name);
	g_free (item->arguments);
	g_free (item->description);
	g_free (item);
}

static gint
as_sort_command_name_cb (AsUtilItem **item1, AsUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

static void
as_util_add (GPtrArray *array,
	     const gchar *name,
	     const gchar *arguments,
	     const gchar *description,
	     AsUtilPrivateCb callback)
{
	AsUtilItem *item;
	guint i;
	g_auto(GStrv) names = NULL;

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

static gchar *
as_util_get_descriptions (GPtrArray *array)
{
	guint i;
	gsize j;
	gsize len;
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

static gboolean
as_util_run (AsUtilPrivate *priv, const gchar *command, gchar **values, GError **error)
{
	AsUtilItem *item;
	guint i;
	g_autoptr(GString) string = NULL;

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

static gboolean
as_util_convert_appdata (GFile *file_input,
			 GFile *file_output,
			 gdouble new_version,
			 GError **error)
{
	AsNodeInsertFlags flags_translate = AS_NODE_INSERT_FLAG_NONE;
	AsNode *n;
	AsNode *n2;
	AsNode *n3;
	const gchar *tmp;
	const gchar *project_group = NULL;
	gboolean action_required = FALSE;
	g_autoptr(AsNode) root = NULL;

	/* load to AsNode */
	root = as_node_from_file (file_input,
				  AS_NODE_FROM_XML_FLAG_LITERAL_TEXT |
				  AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS,
				  NULL,
				  error);
	if (root == NULL)
		return FALSE;

	/* convert from <application> to <component> */
	n = as_node_find (root, "application");
	if (n != NULL) {
		as_node_set_name (n, "component");
		n2 = as_node_find (n, "id");
		if (n2 != NULL) {
			tmp = as_node_get_attribute (n2, "type");
			if (tmp != NULL)
				as_node_add_attribute (n, "type", tmp);
			as_node_remove_attribute (n2, "type");
		}
	}

	/* anything translatable */
	n = as_node_find (root, "component");
	if (as_node_find (n, "_name") != NULL ||
	    as_node_find (n, "_summary") != NULL ||
	    as_node_find (n, "screenshots/_caption") != NULL ||
	    as_node_find (n, "description/_p") != NULL) {
		flags_translate = AS_NODE_INSERT_FLAG_MARK_TRANSLATABLE;
	}

	/* convert from <licence> to <metadata_license> */
	n2 = as_node_find (n, "licence");
	if (n2 != NULL)
		as_node_set_name (n2, "metadata_license");

	/* ensure this exists */
	n2 = as_node_find (n, "metadata_license");
	if (n2 == NULL) {
		action_required = TRUE;
		as_node_insert (n, "metadata_license", "<!-- Insert SPDX ID Here -->",
				AS_NODE_INSERT_FLAG_PRE_ESCAPED, NULL);
	} else {
		tmp = as_node_get_data (n2);

		/* convert the old license defines */
		if (g_strcmp0 (tmp, "CC0") == 0)
			as_node_set_data (n2, "CC0-1.0",
					  AS_NODE_INSERT_FLAG_NONE);
		else if (g_strcmp0 (tmp, "CC-BY") == 0)
			as_node_set_data (n2, "CC-BY-3.0",
					  AS_NODE_INSERT_FLAG_NONE);
		else if (g_strcmp0 (tmp, "CC-BY-SA") == 0)
			as_node_set_data (n2, "CC-BY-SA-3.0",
					  AS_NODE_INSERT_FLAG_NONE);
		else if (g_strcmp0 (tmp, "GFDL") == 0)
			as_node_set_data (n2, "GFDL-1.3",
					  AS_NODE_INSERT_FLAG_NONE);

		/* ensure in SPDX format */
		if (!as_utils_is_spdx_license_id (as_node_get_data (n2))) {
			action_required = TRUE;
			as_node_set_comment (n2, "FIXME: convert to an SPDX ID");
		}
	}

	/* ensure this exists */
	n2 = as_node_find (n, "project_license");
	if (n2 == NULL) {
		action_required = TRUE;
		n2 = as_node_insert (n, "project_license", "<!-- Insert SPDX ID Here -->",
				     AS_NODE_INSERT_FLAG_PRE_ESCAPED, NULL);
		as_node_set_comment (n2, "FIXME: Use https://spdx.org/licenses/");
	} else {
		/* ensure in SPDX format */
		if (!as_utils_is_spdx_license (as_node_get_data (n2))) {
			action_required = TRUE;
			as_node_set_comment (n2, "FIXME: convert to an SPDX license string");
		}
	}

	/* ensure <project_group> exists for <compulsory_for_desktop> */
	n2 = as_node_find (n, "compulsory_for_desktop");
	if (n2 != NULL && as_node_find (n, "project_group") == NULL) {
		as_node_insert (n, "project_group", as_node_get_data (n2),
				AS_NODE_INSERT_FLAG_NONE, NULL);
	}

	/* add <developer_name> */
	n2 = as_node_find (n, "_developer_name");
	if (n2 == NULL)
		n2 = as_node_find (n, "developer_name");
	if (n2 == NULL) {
		n3 = as_node_find (n, "project_group");
		if (n3 != NULL) {
			if (g_strcmp0 (as_node_get_data (n3), "GNOME") == 0) {
				n3 = as_node_insert (n, "developer_name", "The GNOME Project",
						     flags_translate, NULL);
				as_node_set_comment (n3, "FIXME: this is a translatable version of project_group");
			} else if (g_strcmp0 (as_node_get_data (n3), "KDE") == 0) {
				n3 = as_node_insert (n, "developer_name", "The KDE Community",
						     AS_NODE_INSERT_FLAG_NONE, NULL);
				as_node_set_comment (n3, "FIXME: this is a translatable version of project_group");
			} else if (g_strcmp0 (as_node_get_data (n3), "XFCE") == 0) {
				n3 = as_node_insert (n, "developer_name", "Xfce Development Team",
						     flags_translate, NULL);
				as_node_set_comment (n3, "FIXME: this is a translatable version of project_group");
			} else if (g_strcmp0 (as_node_get_data (n3), "MATE") == 0) {
				n3 = as_node_insert (n, "developer_name", "The MATE Project",
						     flags_translate, NULL);
				as_node_set_comment (n3, "FIXME: this is a translatable version of project_group");
			} else {
				action_required = TRUE;
				n3 = as_node_insert (n, "developer_name", "<!-- Company Name -->",
						     AS_NODE_INSERT_FLAG_PRE_ESCAPED, NULL);
				as_node_set_comment (n3, "FIXME: compulsory_for_desktop was not recognised");
			}
		} else {
			action_required = TRUE;
			n3 = as_node_insert (n, "developer_name", "<!-- Company Name -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED, NULL);
			as_node_set_comment (n3, "FIXME: You can use a project or "
					     "developer name if there's no company");
		}
	}

	n3 = as_node_find (n, "project_group");
	if (n3 != NULL)
		project_group = as_node_get_data (n3);

	/* ensure each URL type exists */
	if (as_node_find_with_attribute (n, "url", "type", "homepage") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "<!-- https://wiki.gnome.org/Apps/FIXME -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "homepage", NULL);
		} else {
			n3 = as_node_insert (n, "url", "<!-- http://www.homepage.com/ -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "homepage", NULL);
		}
		as_node_set_comment (n3, "FIXME: homepage for the application");
	}
	if (as_node_find_with_attribute (n, "url", "type", "bugtracker") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "<!-- https://bugzilla.gnome.org/enter_bug.cgi?product=FIXME -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "bugtracker", NULL);
		} else if (g_strcmp0 (project_group, "KDE") == 0) {
			n3 = as_node_insert (n, "url", "<!-- https://bugs.kde.org/enter_bug.cgi?format=guided -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "bugtracker", NULL);
		} else if (g_strcmp0 (project_group, "XFCE") == 0) {
			n3 = as_node_insert (n, "url", "<!-- https://bugzilla.xfce.org/enter_bug.cgi -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "bugtracker", NULL);
		} else {
			n3 = as_node_insert (n, "url", "<!-- http://www.homepage.com/where-to-report_bug.html -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "bugtracker", NULL);
		}
		as_node_set_comment (n3, "FIXME: where to report bugs for "
				     "the application");
	}
	if (as_node_find_with_attribute (n, "url", "type", "donation") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "http://www.gnome.org/friends/",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "donation", NULL);
			as_node_set_comment (n3, "GNOME Projects usually have no per-app donation page");
		} else {
			n3 = as_node_insert (n, "url", "<!-- http://www.homepage.com/donation.html -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "donation", NULL);
			as_node_set_comment (n3, "FIXME: where to donate to the application");
		}
	}
	if (as_node_find_with_attribute (n, "url", "type", "help") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "<!-- https://help.gnome.org/users/FIXME/stable/ -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "help", NULL);
			as_node_set_comment (n3, "FIXME: where on the internet users "
					     "can find help");
		} else {
			n3 = as_node_insert (n, "url", "<!-- http://www.homepage.com/docs/ -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "help", NULL);
			as_node_set_comment (n3, "FIXME: where on the internet users "
					     "can find help");
		}
	}
	if (as_node_find_with_attribute (n, "url", "type", "translate") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "https://wiki.gnome.org/TranslationProject",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "translate", NULL);
		} else if (g_strcmp0 (project_group, "KDE") == 0) {
			n3 = as_node_insert (n, "url", "http://i18n.kde.org/",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "translate", NULL);
		} else if (g_strcmp0 (project_group, "XFCE") == 0) {
			n3 = as_node_insert (n, "url", "https://wiki.xfce.org/translations",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "translate", NULL);
		} else {
			n3 = as_node_insert (n, "url", "<!-- http://www.homepage.com/how-to-submit-translations.html -->",
					     AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					     "type", "translate", NULL);
		}
		as_node_set_comment (n3, "FIXME: where to submit translations");
	}

	/* fix old <update_contact> name */
	n2 = as_node_find (n, "updatecontact");
	if (n2 != NULL)
		as_node_set_name (n2, "update_contact");

	/* add <update_contact> */
	n2 = as_node_find (n, "updatecontact");
	if (n2 != NULL)
		as_node_set_name (n2, "update_contact");
	n2 = as_node_find (n, "update_contact");
	if (n2 == NULL) {
		action_required = TRUE;
		n3 = as_node_insert (n, "update_contact", "<!-- upstream-contact_at_email.com -->",
				     AS_NODE_INSERT_FLAG_PRE_ESCAPED, NULL);
		as_node_set_comment (n3, "FIXME: this is optional, but recommended");
	}

	/* convert from <screenshot>url</screenshot> to:
	 *
	 * <screenshot>
	 * <caption>XXX: Describe this screenshot</caption>
	 * <image>url</image>
	 * </screenshot>
	 */
	n = as_node_find (n, "screenshots");
	if (n != NULL) {
		for (n2 = n->children; n2 != NULL; n2 = n2->next) {
			tmp = as_node_get_data (n2);
			if (tmp == NULL)
				continue;
			n3 = as_node_insert (n2, "image", tmp,
					     AS_NODE_INSERT_FLAG_NONE, NULL);
			as_node_set_data (n3, tmp, AS_NODE_INSERT_FLAG_NONE);
			as_node_set_data (n2, NULL, AS_NODE_INSERT_FLAG_NONE);
			action_required = TRUE;
			as_node_insert (n2, "caption", "<!-- Describe this "
					"screenshot in less than ~10 words -->",
					flags_translate | AS_NODE_INSERT_FLAG_PRE_ESCAPED,
					NULL);
		}
	}

	/* save to file */
	if (!as_node_to_file (root, file_output,
			      AS_NODE_TO_XML_FLAG_ADD_HEADER |
			      AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			      AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
			      NULL, error))
		return FALSE;

	if (action_required) {
		/* TRANSLATORS: any manual changes required?
		 * also note: FIXME is a hardcoded string */
		g_print ("%s\n", _("Please review the file and fix any 'FIXME' items"));
	}

	return TRUE;
}

static gboolean
as_util_convert_appstream (GFile *file_input,
			   GFile *file_output,
			   gdouble new_version,
			   GError **error)
{
	g_autoptr(AsStore) store = NULL;

	store = as_store_new ();
	if (!as_store_from_file (store, file_input, NULL, NULL, error))
		return FALSE;
	/* TRANSLATORS: information message */
	g_print ("%s: %.2f\n", _("Old API version"),
		 as_store_get_api_version (store));

	/* save file */
	as_store_set_api_version (store, new_version);
	if (!as_store_to_file (store, file_output,
				AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
				AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				AS_NODE_TO_XML_FLAG_ADD_HEADER,
				NULL, error))
		return FALSE;
	/* TRANSLATORS: information message */
	g_print ("%s: %.2f\n", _("New API version"), as_store_get_api_version (store));
	return TRUE;
}

static gboolean
as_util_convert (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsFormatKind input_kind;
	AsFormatKind output_kind;
	gdouble new_version;
	g_autoptr(GFile) file_input = NULL;
	g_autoptr(GFile) file_output = NULL;

	/* check args */
	if (g_strv_length (values) != 3) {
		/* TRANSLATORS: error message */
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     _("Not enough arguments, "
				     "expected old.xml new.xml version"));
		return FALSE;
	}

	/* work out what to do */
	input_kind = as_format_guess_kind (values[0]);
	output_kind = as_format_guess_kind (values[1]);
	file_input = g_file_new_for_path (values[0]);
	file_output = g_file_new_for_path (values[1]);
	new_version = g_ascii_strtod (values[2], NULL);

	/* AppData -> AppData */
	if (input_kind == AS_FORMAT_KIND_APPDATA &&
	    output_kind == AS_FORMAT_KIND_APPDATA) {
		return as_util_convert_appdata (file_input,
						file_output,
						new_version,
						error);
	}

	/* AppStream -> AppStream */
	if (input_kind == AS_FORMAT_KIND_APPSTREAM &&
	    output_kind == AS_FORMAT_KIND_APPSTREAM) {
		return as_util_convert_appstream (file_input,
						  file_output,
						  new_version,
						  error);
	}

	/* don't know what to do */
	g_set_error (error,
		     AS_ERROR,
		     AS_ERROR_INVALID_ARGUMENTS,
		     /* TRANSLATORS: the %s and %s are file types,
		      * e.g. "appdata" to "appstream" */
		     _("Conversion %s to %s is not implemented"),
		     as_format_kind_to_string (input_kind),
		     as_format_kind_to_string (output_kind));
	return FALSE;
}

static gboolean
as_util_upgrade (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;

	/* check args */
	if (g_strv_length (values) < 1) {
		/* TRANSLATORS: error message */
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     _("Not enough arguments, "
				     "expected file.xml"));
		return FALSE;
	}

	/* process each file */
	for (i = 0; values[i] != NULL; i++) {
		g_autoptr(GFile) file = NULL;
		AsFormatKind format_kind = as_format_guess_kind (values[i]);
		switch (format_kind) {
		case AS_FORMAT_KIND_APPDATA:
			file = g_file_new_for_path (values[i]);
			if (!as_util_convert_appdata (file, file, 0.8, error))
				return FALSE;
			break;
		case AS_FORMAT_KIND_APPSTREAM:
			file = g_file_new_for_path (values[i]);
			if (!as_util_convert_appstream (file, file, 0.8, error))
				return FALSE;
			break;
		default:
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     /* TRANSLATORS: %s is a file type,
				      * e.g. 'appdata' */
				     _("File format '%s' cannot be upgraded"),
				     as_format_kind_to_string (format_kind));
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
as_util_appdata_to_news (AsUtilPrivate *priv, gchar **values, GError **error)
{
	GPtrArray *releases;
	guint i;
	guint j;
	guint f;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected .appdata.xml");
		return FALSE;
	}

	/* convert all the AppData files */
	for (f = 0; values[f] != NULL; f++) {

		g_autoptr(AsApp) app = NULL;
		g_autoptr(GString) str = NULL;

		/* add separator */
		if (f > 0)
			g_print ("\n\n");

		/* check types */
		if (as_format_guess_kind (values[f]) != AS_FORMAT_KIND_APPDATA) {
			g_set_error_literal (error,
					     AS_ERROR,
					     AS_ERROR_INVALID_ARGUMENTS,
					     /* TRANSLATORS: not a recognised file type */
					     _("Format not recognised"));
			return FALSE;
		}

		app = as_app_new ();
		if (!as_app_parse_file (app, values[f], 0, error))
			return FALSE;

		/* print all the releases that match */
		str = g_string_new ("");
		releases = as_app_get_releases (app);
		for (i = 0; i < releases->len; i++) {
			AsRelease *rel;
			const gchar *tmp;
			g_autofree gchar *version = NULL;
			g_autofree gchar *date = NULL;
			g_autoptr(GDateTime) dt = NULL;

			rel = g_ptr_array_index (releases, i);

			/* print version with underline */
			version = g_strdup_printf ("Version %s", as_release_get_version (rel));
			g_string_append_printf (str, "%s\n", version);
			for (j = 0; version[j] != '\0'; j++)
				g_string_append (str, "~");
			g_string_append (str, "\n");

			/* print release */
			if (as_release_get_timestamp (rel) > 0) {
				dt = g_date_time_new_from_unix_utc ((gint64) as_release_get_timestamp (rel));
				date = g_date_time_format (dt, "%F");
				g_string_append_printf (str, "Released: %s\n\n", date);
			}

			/* print description */
			tmp = as_release_get_description (rel, NULL);
			if (tmp != NULL) {
				g_autofree gchar *md = NULL;
				md = as_markup_convert (tmp,
							AS_MARKUP_CONVERT_FORMAT_MARKDOWN,
							error);
				if (md == NULL)
					return FALSE;
				g_string_append_printf (str, "%s\n", md);
			}
			g_string_append (str, "\n");
		}
		if (str->len > 1)
			g_string_truncate (str, str->len - 1);
		g_print ("%s", str->str);
	}
	return TRUE;
}

typedef enum {
	AS_UTIL_SECTION_KIND_UNKNOWN,
	AS_UTIL_SECTION_KIND_HEADER,
	AS_UTIL_SECTION_KIND_BUGFIX,
	AS_UTIL_SECTION_KIND_FEATURES,
	AS_UTIL_SECTION_KIND_NOTES,
	AS_UTIL_SECTION_KIND_TRANSLATION,
	AS_UTIL_SECTION_KIND_LAST
} AsUtilSectionKind;

static AsUtilSectionKind
as_util_news_to_appdata_guess_section (const gchar *lines)
{
	if (g_strstr_len (lines, -1, "~~~") != NULL)
		return AS_UTIL_SECTION_KIND_HEADER;
	if (g_strstr_len (lines, -1, "Bugfix:\n") != NULL)
		return AS_UTIL_SECTION_KIND_BUGFIX;
	if (g_strstr_len (lines, -1, "Bugfixes:\n") != NULL)
		return AS_UTIL_SECTION_KIND_BUGFIX;
	if (g_strstr_len (lines, -1, "Bug fixes:\n") != NULL)
		return AS_UTIL_SECTION_KIND_BUGFIX;
	if (g_strstr_len (lines, -1, "Features:\n") != NULL)
		return AS_UTIL_SECTION_KIND_FEATURES;
	if (g_strstr_len (lines, -1, "Removed features:\n") != NULL)
		return AS_UTIL_SECTION_KIND_FEATURES;
	if (g_strstr_len (lines, -1, "Notes:\n") != NULL)
		return AS_UTIL_SECTION_KIND_NOTES;
	if (g_strstr_len (lines, -1, "Note:\n") != NULL)
		return AS_UTIL_SECTION_KIND_NOTES;
	if (g_strstr_len (lines, -1, "Translations:\n") != NULL)
		return AS_UTIL_SECTION_KIND_TRANSLATION;
	if (g_strstr_len (lines, -1, "Translations\n") != NULL)
		return AS_UTIL_SECTION_KIND_TRANSLATION;
	return AS_UTIL_SECTION_KIND_UNKNOWN;
}

static void
as_util_news_add_indent (GString *str, guint indent_level)
{
	guint i;
	for (i = 0; i < indent_level; i++)
		g_string_append (str, "  ");
}

static void
as_util_news_add_markup (GString *desc, const gchar *tag, const gchar *line)
{
	guint i;
	guint indent = 0;
	g_autofree gchar *escaped = NULL;

	/* empty line means do nothing */
	if (line != NULL && line[0] == '\0')
		return;

	/* work out indent */
	if (g_strcmp0 (tag, "p") == 0)
		indent = 4;
	else if (g_strcmp0 (tag, "ul") == 0)
		indent = 4;
	else if (g_strcmp0 (tag, "/ul") == 0)
		indent = 4;
	else if (g_strcmp0 (tag, "release") == 0)
		indent = 2;
	else if (g_strcmp0 (tag, "/release") == 0)
		indent = 2;
	else if (g_strcmp0 (tag, "description") == 0)
		indent = 3;
	else if (g_strcmp0 (tag, "/description") == 0)
		indent = 3;
	else if (g_strcmp0 (tag, "li") == 0)
		indent = 5;

	/* write XML with the correct maximum width */
	if (line == NULL) {
		as_util_news_add_indent (desc, indent);
		g_string_append_printf (desc, "<%s>\n", tag);
	} else {
		gchar *tmp;
		g_auto(GStrv) lines = NULL;
		escaped = g_markup_escape_text (line, -1);
		tmp = g_strrstr (escaped, " (");
		if (tmp != NULL)
			*tmp = '\0';
		lines = as_markup_strsplit_words (escaped, 80 - indent * 2);
		if (lines == NULL) {
			g_warning ("line cannot be split `%s`", escaped);
			return;
		}
		/* all fits on one line? */
		if (g_strv_length (lines) == 1) {
			as_util_news_add_indent (desc, indent);
			g_string_append_printf (desc, "<%s>%s</%s>\n",
						tag, escaped, tag);
		} else {
			as_util_news_add_indent (desc, indent);
			g_string_append_printf (desc, "<%s>\n", tag);
			for (i = 0; lines[i] != NULL; i++) {
				as_util_news_add_indent (desc, indent + 1);
				g_string_append (desc, lines[i]);
			}
			as_util_news_add_indent (desc, indent);
			g_string_append_printf (desc, "</%s>\n", tag);
		}
	}
}

static gboolean
as_util_news_to_appdata_hdr (GString *desc, const gchar *txt, GError **error)
{
	guint i;
	const gchar *version = NULL;
	const gchar *release = NULL;
	g_auto(GStrv) release_split = NULL;
	g_autoptr(GDateTime) dt = NULL;
	g_auto(GStrv) lines = NULL;

	/* get info */
	lines = g_strsplit (txt, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix (lines[i], "Version ")) {
			version = lines[i] + 8;
			continue;
		}
		if (g_str_has_prefix (lines[i], "Released: ")) {
			release = lines[i] + 10;
			continue;
		}
	}

	/* check these exist */
	if (version == NULL) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Unable to find version in: %s", txt);
		return FALSE;
	}
	if (release == NULL) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Unable to find release in: %s", txt);
		return FALSE;
	}

	/* parse date */
	release_split = g_strsplit (release, "-", -1);
	if (g_strv_length (release_split) != 3) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Unable to parse release: %s", release);
		return FALSE;
	}
	dt = g_date_time_new_local (atoi (release_split[0]),
				    atoi (release_split[1]),
				    atoi (release_split[2]),
				    0, 0, 0);
	if (dt == NULL) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Unable to create release: %s", release);
		return FALSE;
	}

	/* add markup */
	as_util_news_add_indent (desc, 2);
	g_string_append_printf (desc, "<release version=\"%s\" "
				"date=\"%s-%s-%s\">\n",
				version,
				release_split[0],
				release_split[1],
				release_split[2]);
	as_util_news_add_indent (desc, 3);
	g_string_append (desc, "<description>\n");

	return TRUE;
}

static gboolean
as_util_news_to_appdata_list (GString *desc, gchar **lines, GError **error)
{
	guint i;

	as_util_news_add_markup (desc, "ul", NULL);
	for (i = 0; lines[i] != NULL; i++) {
		guint prefix = 0;
		if (g_str_has_prefix (lines[i], " - "))
			prefix = 3;
		as_util_news_add_markup (desc, "li", lines[i] + prefix);
	}
	as_util_news_add_markup (desc, "/ul", NULL);
	return TRUE;
}

static gboolean
as_util_news_to_appdata_para (GString *desc, const gchar *txt, GError **error)
{
	guint i;
	g_auto(GStrv) lines = NULL;

	lines = g_strsplit (txt, "\n", -1);
	for (i = 1; lines[i] != NULL; i++) {
		guint prefix = 0;
		if (g_str_has_prefix (lines[i], " - "))
			prefix = 3;
		as_util_news_add_markup (desc, "p", lines[i] + prefix);
	}
	return TRUE;
}

static gboolean
as_util_news_to_appdata (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;
	g_autofree gchar *data = NULL;
	g_autoptr(GString) data_str = NULL;
	g_autoptr(GString) desc = NULL;
	g_auto(GStrv) split = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected NEWS");
		return FALSE;
	}

	/* get data */
	if (!g_file_get_contents (values[0], &data, NULL, error))
		return FALSE;

	/* try to unsplit lines */
	data_str = g_string_new (data);
	as_utils_string_replace (data_str, "\n   ", " ");

	/* break up into sections */
	desc = g_string_new ("");
	split = g_strsplit (data_str->str, "\n\n", -1);
	for (i = 0; split[i] != NULL; i++) {
		g_auto(GStrv) lines = NULL;

		/* ignore empty sections */
		if (split[i][0] == '\0')
			continue;

		switch (as_util_news_to_appdata_guess_section (split[i])) {
		case AS_UTIL_SECTION_KIND_HEADER:
		{
			/* flush old release content */
			if (desc->len > 0) {
				as_util_news_add_markup (desc, "/description", NULL);
				as_util_news_add_markup (desc, "/release", NULL);
				g_print ("%s", desc->str);
				g_string_truncate (desc, 0);
			}

			/* parse header */
			if (!as_util_news_to_appdata_hdr (desc, split[i], error))
				return FALSE;
			break;
		}
		case AS_UTIL_SECTION_KIND_BUGFIX:
			lines = g_strsplit (split[i], "\n", -1);
			if (g_strv_length (lines) == 2) {
				as_util_news_add_markup (desc, "p",
							 "This release fixes the following bug:");
			} else {
				as_util_news_add_markup (desc, "p",
							 "This release fixes the following bugs:");
			}
			if (!as_util_news_to_appdata_list (desc, lines + 1, error))
				return FALSE;
			break;
		case AS_UTIL_SECTION_KIND_NOTES:
			if (!as_util_news_to_appdata_para (desc, split[i], error))
				return FALSE;
			break;
		case AS_UTIL_SECTION_KIND_FEATURES:
			lines = g_strsplit (split[i], "\n", -1);
			if (g_strv_length (lines) == 2) {
				as_util_news_add_markup (desc, "p",
							 "This release adds the following feature:");
			} else {
				as_util_news_add_markup (desc, "p",
							 "This release adds the following features:");
			}
			if (!as_util_news_to_appdata_list (desc, lines + 1, error))
				return FALSE;
			break;
		case AS_UTIL_SECTION_KIND_TRANSLATION:
			as_util_news_add_markup (desc, "p",
						 "This release updates translations.");
			break;
		default:
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "failed to detect section '%s'", split[i]);
			return FALSE;
		}
	}

	/* flush old release content */
	if (desc->len > 0) {
		as_util_news_add_markup (desc, "/description", NULL);
		as_util_news_add_markup (desc, "/release", NULL);
		g_print ("%s", desc->str);
	}

	return TRUE;
}

static gboolean
as_util_appdata_from_desktop (AsUtilPrivate *priv, gchar **values, GError **error)
{
	gchar *instr = NULL;
	g_autofree gchar *id_new = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(AsImage) im1 = NULL;
	g_autoptr(AsImage) im2 = NULL;
	g_autoptr(AsScreenshot) ss1 = NULL;
	g_autoptr(AsScreenshot) ss2 = NULL;
	g_autoptr(GFile) file = NULL;

	/* check args */
	if (g_strv_length (values) != 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected app.desktop new.appdata.xml");
		return FALSE;
	}

	/* check types */
	if (as_format_guess_kind (values[0]) != AS_FORMAT_KIND_DESKTOP ||
	    as_format_guess_kind (values[1]) != AS_FORMAT_KIND_APPDATA) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     /* TRANSLATORS: not a recognised file type */
				     _("Format not recognised"));
		return FALSE;
	}

	/* import the .desktop file */
	app = as_app_new ();
	if (!as_app_parse_file (app, values[0],
				 AS_APP_PARSE_FLAG_USE_HEURISTICS,
				error))
		return FALSE;

	/* set some initial values */
	as_app_set_description (app, NULL,
				"\n  <p>\n   This should be long prose.\n  </p>\n"
				"  <p>\n   This should a second paragraph.\n  </p>\n");
	as_app_set_developer_name (app, NULL, "XXX: Insert Company or Developer Name");
	as_app_set_project_group (app, "XXX: Values valid are none, GNOME, KDE or XFCE");

	/* fix the ID */
	id_new = g_strdup (as_app_get_id (app));
	instr = g_strstr_len (id_new, -1, ".desktop.in");
	if (instr != NULL) {
		instr[8] = '\0';
		as_app_set_id (app, id_new);
	}

	/* set things that don't belong in the AppData file */
	g_ptr_array_set_size (as_app_get_keywords (app, NULL), 0);
	g_ptr_array_set_size (as_app_get_categories (app), 0);
	g_ptr_array_set_size (as_app_get_mimetypes (app), 0);
	g_ptr_array_set_size (as_app_get_icons (app), 0);

	/* add urls */
	as_app_add_url (app, AS_URL_KIND_HOMEPAGE,
			"XXX: http://www.homepage.com/");
	as_app_add_url (app, AS_URL_KIND_BUGTRACKER,
			"XXX: http://www.homepage.com/where-to-report_bug.html");
	as_app_add_url (app, AS_URL_KIND_FAQ,
			"XXX: http://www.homepage.com/faq.html");
	as_app_add_url (app, AS_URL_KIND_DONATION,
			"XXX: http://www.homepage.com/donation.html");
	as_app_add_url (app, AS_URL_KIND_HELP,
			"XXX: http://www.homepage.com/docs/");
	as_app_set_project_license (app, "XXX: Insert SPDX value here");
	as_app_set_metadata_license (app, "XXX: Insert SPDX value here");
	as_app_set_update_contact (app, "XXX: upstream-contact_at_email.com");

	/* add first screenshot */
	ss1 = as_screenshot_new ();
	as_screenshot_set_kind (ss1, AS_SCREENSHOT_KIND_DEFAULT);
	as_screenshot_set_caption (ss1, NULL, "XXX: Describe the default screenshot");
	im1 = as_image_new ();
	as_image_set_url (im1, "XXX: http://www.my-screenshot-default.png");
	as_image_set_width (im1, 1120);
	as_image_set_height (im1, 630);
	as_screenshot_add_image (ss1, im1);
	as_app_add_screenshot (app, ss1);

	/* add second screenshot */
	ss2 = as_screenshot_new ();
	as_screenshot_set_kind (ss2, AS_SCREENSHOT_KIND_NORMAL);
	as_screenshot_set_caption (ss2, NULL, "XXX: Describe another screenshot");
	im2 = as_image_new ();
	as_image_set_url (im2, "XXX: http://www.my-screenshot.png");
	as_image_set_width (im2, 1120);
	as_image_set_height (im2, 630);
	as_screenshot_add_image (ss2, im2);
	as_app_add_screenshot (app, ss2);

	/* save to disk */
	file = g_file_new_for_path (values[1]);
	return as_app_to_file (app, file, NULL, error);
}

static gboolean
as_util_add_file_to_store (AsStore *store, const gchar *filename, GError **error)
{
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GFile) file_input = NULL;

	switch (as_format_guess_kind (filename)) {
	case AS_FORMAT_KIND_APPDATA:
	case AS_FORMAT_KIND_METAINFO:
	case AS_FORMAT_KIND_DESKTOP:
		app = as_app_new ();
		if (!as_app_parse_file (app, filename,
					AS_APP_PARSE_FLAG_USE_HEURISTICS, error))
			return FALSE;
		as_store_add_app (store, app);
		break;
	case AS_FORMAT_KIND_APPSTREAM:
		/* load file */
		file_input = g_file_new_for_path (filename);
		if (!as_store_from_file (store, file_input, NULL, NULL, error))
			return FALSE;
		break;
	default:
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     /* TRANSLATORS: not a recognised file type */
				     _("Format not recognised"));
		return FALSE;
	}
	return TRUE;
}

static gboolean
as_util_dump (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;
	g_autoptr(GString) xml = NULL;
	g_autoptr(AsStore) store = NULL;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected data.xml");
		return FALSE;
	}

	/* magic value */
	store = as_store_new ();
	if (g_strcmp0 (values[0], "installed") == 0) {
		if (!as_store_load (store,
				    AS_STORE_LOAD_FLAG_IGNORE_INVALID |
				    AS_STORE_LOAD_FLAG_APPDATA |
				    AS_STORE_LOAD_FLAG_DESKTOP,
				    NULL, error)) {
			return FALSE;
		}
	} else {
		for (i = 0; values[i] != NULL; i++) {
			if (!as_util_add_file_to_store (store, values[i], error))
				return FALSE;
		}
	}

	/* dump to screen */
	as_store_set_api_version (store, 1.0);
	xml = as_store_to_xml (store,
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_ADD_HEADER);
	g_print ("%s", xml->str);
	return TRUE;
}

static void
as_util_watch_store_changed_cb (AsStore *store, AsUtilPrivate *priv)
{
	g_print ("Store changed, now have %u components\n",
		 as_store_get_size (store));
}

static void
as_util_watch_store_app_added_cb (AsStore *store, AsApp *app, AsUtilPrivate *priv)
{
	g_print ("Component added to store: %s\n",
		 as_app_get_unique_id (app));
}

static void
as_util_watch_store_app_removed_cb (AsStore *store, AsApp *app, AsUtilPrivate *priv)
{
	g_print ("Component removed from store: %s\n",
		 as_app_get_unique_id (app));
}

static void
as_util_watch_store_app_changed_cb (AsStore *store, AsApp *app, AsUtilPrivate *priv)
{
	g_print ("Component changed in store: %s\n",
		 as_app_get_unique_id (app));
}

static gboolean
as_util_watch (AsUtilPrivate *priv, gchar **values, GError **error)
{
	g_autoptr(GString) xml = NULL;
	g_autoptr(AsStore) store = NULL;

	/* magic value */
	store = as_store_new ();
	as_store_set_watch_flags (store,
				  AS_STORE_WATCH_FLAG_ADDED |
				  AS_STORE_WATCH_FLAG_REMOVED);
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_IGNORE_INVALID |
			    AS_STORE_LOAD_FLAG_APPDATA |
			    AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM |
			    AS_STORE_LOAD_FLAG_APP_INFO_USER |
			    AS_STORE_LOAD_FLAG_DESKTOP,
			    priv->cancellable, error)) {
		return FALSE;
	}
	g_signal_connect (store, "changed",
			  G_CALLBACK (as_util_watch_store_changed_cb), priv);
	g_signal_connect (store, "app-added",
			  G_CALLBACK (as_util_watch_store_app_added_cb), priv);
	g_signal_connect (store, "app-removed",
			  G_CALLBACK (as_util_watch_store_app_removed_cb), priv);
	g_signal_connect (store, "app-changed",
			  G_CALLBACK (as_util_watch_store_app_changed_cb), priv);

	/* wait */
	g_main_loop_run (priv->loop);
	return TRUE;
}

static gint
as_util_sort_apps_by_sort_key_cb (gconstpointer a, gconstpointer b)
{
	AsApp *app1 = *((AsApp **) a);
	AsApp *app2 = *((AsApp **) b);
	return g_strcmp0 (as_app_get_metadata_item (app2, "SortKey"),
			  as_app_get_metadata_item (app1, "SortKey"));
}

static gboolean
as_util_search (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps;
	guint i;
	guint score;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GPtrArray) array = NULL;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected search terms");
		return FALSE;
	}

	/* load system database */
	store = as_store_new ();
	as_store_set_add_flags (store,
				AS_STORE_ADD_FLAG_USE_UNIQUE_ID |
				AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS |
				AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC);
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_IGNORE_INVALID |
			    AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM |
			    AS_STORE_LOAD_FLAG_APP_INFO_USER |
			    AS_STORE_LOAD_FLAG_APPDATA |
			    AS_STORE_LOAD_FLAG_DESKTOP,
			    NULL, error))
		return FALSE;

	/* prime the search cache */
	as_store_set_search_match (store,
				   AS_APP_SEARCH_MATCH_MIMETYPE |
				   AS_APP_SEARCH_MATCH_PKGNAME |
				   AS_APP_SEARCH_MATCH_COMMENT |
				   AS_APP_SEARCH_MATCH_NAME |
				   AS_APP_SEARCH_MATCH_KEYWORD |
				   AS_APP_SEARCH_MATCH_ORIGIN |
				   AS_APP_SEARCH_MATCH_ID);
	as_store_load_search_cache (store);

	/* add matches to an array */
	apps = as_store_get_apps (store);
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		score = as_app_search_matches_all (app, values);
		if (score > 0) {
			g_autofree gchar *sort_key = NULL;
			sort_key = g_strdup_printf ("%05x", score);
			as_app_add_metadata (app, "SortKey", sort_key);
			g_ptr_array_add (array, g_object_ref (app));
		}
	}

	/* print sorted results */
	g_ptr_array_sort (array, as_util_sort_apps_by_sort_key_cb);
	for (i = 0; i < array->len; i++) {
		app = g_ptr_array_index (array, i);
		switch (as_app_get_state (app)) {
		case AS_APP_STATE_INSTALLED:
			g_print ("[%s] %s (installed)\n",
				 as_app_get_metadata_item (app, "SortKey"),
				 as_app_get_unique_id (app));
			break;
		default:
			g_print ("[%s] %s\n",
				 as_app_get_metadata_item (app, "SortKey"),
				 as_app_get_unique_id (app));
			break;
		}
	}

	/* dump XML */
	if (priv->verbose) {
		g_autoptr(GString) xml = NULL;
		g_autoptr(AsStore) store_results = as_store_new ();
		as_store_set_add_flags (store_results,
					AS_STORE_ADD_FLAG_USE_UNIQUE_ID);
		for (i = 0; i < array->len; i++) {
			app = g_ptr_array_index (array, i);
			as_app_remove_metadata (app, "SortKey");
			as_store_add_app (store_results, app);
		}
		xml = as_store_to_xml (store_results,
				       AS_NODE_TO_XML_FLAG_ADD_HEADER |
				       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
		g_print ("%s\n", xml->str);
	}

	/* dump refcounted string debug data */
	if (g_getenv ("AS_REF_STR_DEBUG") != NULL) {
		g_autofree gchar *tmp = as_ref_string_debug (AS_REF_STRING_DEBUG_DEDUPED |
							     AS_REF_STRING_DEBUG_DUPES);
		g_print ("%s", tmp);
	}

	return TRUE;
}

static gboolean
as_util_search_pkgname (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	guint i;
	g_autoptr(AsStore) store = NULL;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected search terms");
		return FALSE;
	}

	/* load system database */
	store = as_store_new ();
	as_store_set_add_flags (store,
				AS_STORE_ADD_FLAG_USE_UNIQUE_ID |
				AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC);
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_IGNORE_INVALID |
			    AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM |
			    AS_STORE_LOAD_FLAG_APP_INFO_USER |
			    AS_STORE_LOAD_FLAG_APPDATA |
			    AS_STORE_LOAD_FLAG_DESKTOP,
			    NULL, error))
		return FALSE;

	/* find by source */
	for (i = 0; values[i] != NULL; i++) {
		app = as_store_get_app_by_pkgname (store, values[i]);
		if (app == NULL)
			continue;
		g_print ("%s\n", as_app_get_id (app));
	}
	return TRUE;
}

static gboolean
as_util_search_category (AsUtilPrivate *priv, gchar **values, GError **error)
{
	GPtrArray *apps;
	guint i, j;
	g_autoptr(AsStore) store = NULL;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected search terms");
		return FALSE;
	}

	/* load system database */
	store = as_store_new ();
	as_store_set_add_flags (store,
				AS_STORE_ADD_FLAG_USE_UNIQUE_ID |
				AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC);
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_IGNORE_INVALID |
			    AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM |
			    AS_STORE_LOAD_FLAG_APP_INFO_USER |
			    AS_STORE_LOAD_FLAG_APPDATA |
			    AS_STORE_LOAD_FLAG_DESKTOP,
			    NULL, error))
		return FALSE;

	/* find by source */
	apps = as_store_get_apps (store);
	for (j = 0; j < apps->len; j++) {
		gboolean found = FALSE;
		AsApp *app = g_ptr_array_index (apps, j);
		for (i = 0; values[i] != NULL; i++) {
			if (as_app_has_category (app, values[i])) {
				found = TRUE;
				break;
			}
		}
		if (!found)
			continue;
		g_print ("%s\n", as_app_get_unique_id (app));
	}
	return TRUE;
}

static gboolean
as_util_query_installed (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *array;
	guint i;
	g_autoptr(AsStore) store = NULL;

	/* load system database */
	store = as_store_new ();
	as_store_set_add_flags (store,
				AS_STORE_ADD_FLAG_USE_UNIQUE_ID |
				AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC);
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_IGNORE_INVALID |
			    AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM |
			    AS_STORE_LOAD_FLAG_APP_INFO_USER |
			    AS_STORE_LOAD_FLAG_APPDATA |
			    AS_STORE_LOAD_FLAG_DESKTOP,
			    NULL, error))
		return FALSE;

	/* add matches to an array */
	array = as_store_get_apps (store);
	for (i = 0; i < array->len; i++) {
		app = g_ptr_array_index (array, i);
		if (as_app_get_state (app) != AS_APP_STATE_INSTALLED)
			continue;
		g_print ("%s\n", as_app_get_unique_id (app));
	}

	/* dump XML */
	if (priv->verbose) {
		g_autoptr(GString) xml = NULL;
		g_autoptr(AsStore) store_results = as_store_new ();
		as_store_set_add_flags (store_results,
					AS_STORE_ADD_FLAG_USE_UNIQUE_ID);
		for (i = 0; i < array->len; i++) {
			app = g_ptr_array_index (array, i);
			if (as_app_get_state (app) != AS_APP_STATE_INSTALLED)
				continue;
			as_store_add_app (store_results, app);
		}
		xml = as_store_to_xml (store_results,
				       AS_NODE_TO_XML_FLAG_ADD_HEADER |
				       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
		g_print ("%s\n", xml->str);
	}

	return TRUE;
}

static gint
as_util_search_token_sort_cb (gconstpointer a, gconstpointer b, gpointer user_data)
{
	guint *cnt_a;
	guint *cnt_b;
	GHashTable *dict = (GHashTable *) user_data;
	cnt_a = g_hash_table_lookup (dict, (const gchar *) a);
	cnt_b = g_hash_table_lookup (dict, (const gchar *) b);
	if (*cnt_a < *cnt_b)
		return 1;
	if (*cnt_a > *cnt_b)
		return -1;
	return 0;
}

static gboolean
as_util_show_search_tokens (AsUtilPrivate *priv, gchar **values, GError **error)
{
	GPtrArray *apps;
	GList *l;
	guint i;
	guint j;
	const gchar *tmp;
	guint *cnt;
	g_autoptr(GHashTable) dict = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GList) keys = NULL;

	/* load system database */
	store = as_store_new ();
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM |
			    AS_STORE_LOAD_FLAG_APP_INFO_USER,
			    NULL, error))
		return FALSE;
	dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		AsApp *app;
		g_autoptr(GPtrArray) tokens = NULL;
		app = g_ptr_array_index (apps, i);
		tokens = as_app_get_search_tokens (app);
		for (j = 0; j < tokens->len; j++) {
			tmp = g_ptr_array_index (tokens, j);
			cnt = g_hash_table_lookup (dict, tmp);
			if (cnt == NULL) {
				cnt = g_new0 (guint, 1);
				g_hash_table_insert (dict,
						     g_strdup (tmp),
						     cnt);
			}
			(*cnt)++;
		}
	}

	/* display the keywords sorted */
	keys = g_hash_table_get_keys (dict);
	keys = g_list_sort_with_data (keys, as_util_search_token_sort_cb, dict);
	for (l = keys; l != NULL; l = l->next) {
		tmp = l->data;
		cnt = g_hash_table_lookup (dict, tmp);
		g_print ("%s [%u]\n", tmp, *cnt);
	}

	return TRUE;
}

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
		if (!as_utils_install_filename (AS_UTILS_LOCATION_SHARED,
						values[i], NULL,
						g_getenv ("DESTDIR"),
						error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
as_util_install_origin (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;
	const gchar *destdir;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected origin filename(s)");
		return FALSE;
	}

	/* for each item on the command line, install the xml files and
	 * explode the icon files */
	destdir = g_getenv ("DESTDIR");
	for (i = 1; values[i] != NULL; i++) {
		if (!as_utils_install_filename (destdir != NULL ? AS_UTILS_LOCATION_SHARED :
								  AS_UTILS_LOCATION_CACHE,
						values[i], values[0],
						destdir,
						error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
as_util_rmtree (const gchar *directory, GError **error)
{
	const gchar *filename;
	g_autoptr(GDir) dir = NULL;

	/* try to open */
	dir = g_dir_open (directory, 0, error);
	if (dir == NULL)
		return FALSE;

	/* find each */
	while ((filename = g_dir_read_name (dir))) {
		g_autofree gchar *src = NULL;
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

static gboolean
as_util_uninstall (AsUtilPrivate *priv, gchar **values, GError **error)
{
	const gchar *destdir;
	guint i;
	const gchar *locations[] = { "/usr/share", "/var/cache", NULL };

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
	for (i = 0; locations[i] != NULL; i++) {
		g_autofree gchar *path_xml = NULL;
		path_xml = g_strdup_printf ("%s%s/app-info/xmls/%s.xml.gz",
					    destdir != NULL ? destdir : "",
					    locations[i], values[0]);
		if (g_file_test (path_xml, G_FILE_TEST_EXISTS)) {
			g_autoptr(GFile) file = NULL;
			file = g_file_new_for_path (path_xml);
			if (!g_file_delete (file, NULL, error))
				return FALSE;
		}
	}

	/* remove icons */
	for (i = 0; locations[i] != NULL; i++) {
		g_autofree gchar *path_icons = NULL;
		path_icons = g_strdup_printf ("%s%s/app-info/icons/%s",
					      destdir != NULL ? destdir : "",
					      locations[i], values[0]);
		if (g_file_test (path_icons, G_FILE_TEST_EXISTS)) {
			if (!as_util_rmtree (path_icons, error))
				return FALSE;
		}
	}
	return TRUE;
}

typedef enum {
	AS_UTIL_DISTRO_UNKNOWN,
	AS_UTIL_DISTRO_FEDORA,
	AS_UTIL_DISTRO_LAST
} AsUtilDistro;

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
		g_autofree gchar *escaped = NULL;
		tmp = g_ptr_array_index (array, i);
		if (txt->len > 0)
			g_string_append (txt, ", ");
		escaped = g_markup_escape_text (tmp, -1);
		g_string_append (txt, escaped);
	}
	return g_string_free (txt, FALSE);
}

static void
as_util_status_html_write_app (AsApp *app, GString *html, AsUtilDistro distro)
{
	GPtrArray *images;
	GPtrArray *screenshots;
	AsImage *im;
	AsImage *im_thumb;
	AsImage *im_scaled;
	AsScreenshot *ss;
	const gchar *pkgname;
	gchar *tmp;
	gchar *tmp2;
	guint i;
	guint j;
	const gchar *important_md[] = { "DistroMetadata",
					"DistroScreenshots",
					NULL };
	g_autoptr(GString) classes = NULL;

	/* generate class list */
	classes = g_string_new ("");
	if (as_app_get_screenshots(app)->len > 0)
		g_string_append (classes, "screenshots ");
	if (as_app_get_description(app, NULL) != NULL)
		g_string_append (classes, "description ");
	if (as_app_get_keywords(app, NULL) != NULL)
		g_string_append (classes, "keywords ");
	if (g_strcmp0 (as_app_get_project_group (app), "GNOME") == 0)
		g_string_append (classes, "gnome ");
	if (g_strcmp0 (as_app_get_project_group (app), "KDE") == 0)
		g_string_append (classes, "kde ");
	if (g_strcmp0 (as_app_get_project_group (app), "XFCE") == 0)
		g_string_append (classes, "xfce ");
	if (as_app_get_kind (app) == AS_APP_KIND_ADDON)
		g_string_append (classes, "addon ");
	if (classes->len > 0)
		g_string_truncate (classes, classes->len - 1);

	g_string_append_printf (html, "<div id=\"app-%s\" class=\"%s\">\n",
				as_app_get_id (app), classes->str);
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

	g_string_append (html, "<table class=\"app\">\n");

	/* type */
	g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td><code>%s</code></td></tr>\n",
				"Type", as_app_kind_to_string (as_app_get_kind (app)));

	/* extends */
	tmp = as_util_status_html_join (as_app_get_extends (app));
	if (tmp != NULL) {
		tmp2 = g_strrstr (tmp, ".desktop");
		if (tmp2 != NULL)
			*tmp2 = '\0';
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td>"
					"<td><a href=\"#%s.desktop\">%s</a></td></tr>\n",
					"Extends", tmp, tmp);
	}
	g_free (tmp);

	/* summary */
	g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
				"Name", as_app_get_name (app, "C"));
	if (as_app_get_comment (app, "C") != NULL) {
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td>"
					"<td>%s</td></tr>\n",
					"Comment", as_app_get_comment (app, "C"));
	}
	if (as_app_get_description (app, "C") != NULL) {
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td>"
					"<td>%s</td></tr>\n",
					"Description", as_app_get_description (app, "C"));
	}

	/* packages */
	pkgname = as_app_get_pkgname_default (app);
	if (pkgname != NULL) {
		tmp = as_util_status_html_join (as_app_get_pkgnames (app));
		switch (distro) {
		case AS_UTIL_DISTRO_FEDORA:
			g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>"
						"<a href=\"https://apps.fedoraproject.org/packages/%s\">"
						"<code>%s</code></a></td></tr>\n",
						"Package", pkgname, tmp);
			break;
		default:
			g_string_append_printf (html, "<tr><td class=\"alt\">%s</td>"
						"<td><code>%s</code></td></tr>\n",
						"Package", tmp);
			break;
		}
		g_free (tmp);
	}

	/* categories */
	tmp = as_util_status_html_join (as_app_get_categories (app));
	if (tmp != NULL) {
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
					"Categories", tmp);
	}
	g_free (tmp);

	/* keywords */
	if (as_app_get_keywords (app, NULL) != NULL) {
		tmp = as_util_status_html_join (as_app_get_keywords (app, NULL));
		if (tmp != NULL) {
			g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
						"Keywords", tmp);
		}
		g_free (tmp);
	}

	/* homepage */
	pkgname = as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE);
	if (pkgname != NULL) {
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td><a href=\"%s\">"
					"%s</a></td></tr>\n",
					"Homepage", pkgname, pkgname);
	}

	/* project */
	if (as_app_get_project_group (app) != NULL) {
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
					"Project", as_app_get_project_group (app));
	}

	/* desktops */
	tmp = as_util_status_html_join (as_app_get_compulsory_for_desktops (app));
	if (tmp != NULL) {
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
					"Compulsory for", tmp);
	}
	g_free (tmp);

	/* certain metadata keys */
	for (i = 0; important_md[i] != NULL; i++) {
		if (as_app_get_metadata_item (app, important_md[i]) == NULL)
			continue;
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
					important_md[i], "Yes");
	}

	/* kudos */
	if (as_app_get_kind (app) == AS_APP_KIND_DESKTOP) {
		tmp = as_util_status_html_join (as_app_get_kudos (app));
		if (tmp != NULL) {
			g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
						"Kudos", tmp);
		}
		g_free (tmp);
	}

	/* vetos */
	if (as_app_get_kind (app) == AS_APP_KIND_DESKTOP) {
		tmp = as_util_status_html_join (as_app_get_vetos (app));
		if (tmp != NULL) {
			g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
						"Vetos", tmp);
		}
		g_free (tmp);
	}

	g_string_append (html, "</table>\n");
	g_string_append (html, "<br/>\n");
	g_string_append (html, "</div>\n");
}

static gboolean
as_util_status_html_write_exec_summary (GPtrArray *apps,
					GString *html,
					GError **error)
{
	AsApp *app;
	gdouble perc;
	guint cnt;
	guint i;
	guint j;
	guint total = 0;

	g_string_append (html, "<h1>Executive Summary</h1>\n");

	/* count number of desktop apps */
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) == AS_APP_KIND_DESKTOP)
			total++;
	}
	if (total == 0) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     /* TRANSLATORS: probably wrong XML */
				     _("No desktop applications found"));
		return FALSE;
	}
	g_string_append (html, "<table class=\"summary\">\n");

	/* keywords */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) != AS_APP_KIND_DESKTOP)
			continue;
		if (as_app_get_keywords(app, NULL) != NULL)
			cnt++;
	}
	perc = 100.f * (gdouble) cnt / (gdouble) total;
	g_string_append_printf (html, "<tr><td class=\"alt\">Keywords</td>"
				"<td>%u/%u</td><td class=\"thin\">%.1f%%</td></tr>\n",
				cnt, total, perc);

	/* screenshots */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) != AS_APP_KIND_DESKTOP)
			continue;
		if (as_app_get_screenshots(app)->len > 0)
			cnt++;
	}
	perc = 100.f * (gdouble) cnt / (gdouble) total;
	g_string_append_printf (html, "<tr><td class=\"alt\">Screenshots</td>"
				"<td>%u/%u</td><td class=\"thin\">%.1f%%</td></tr>\n",
				cnt, total, perc);

	/* specific kudos */
	total = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) != AS_APP_KIND_DESKTOP)
			continue;
		total++;
	}
	for (j = 1; j < AS_KUDO_KIND_LAST; j++) {
		cnt = 0;
		for (i = 0; i < apps->len; i++) {
			app = g_ptr_array_index (apps, i);
			if (!as_app_has_kudo_kind (app, j))
				continue;
			cnt += 1;
		}
		perc = 0;
		if (total > 0)
			perc = 100.f * (gdouble) cnt / (gdouble) total;
		g_string_append_printf (html, "<tr><td class=\"alt\">"
					"<i>%s</i></td><td>%u</td>"
					"<td class=\"thin\">%.1f%%</td></tr>\n",
					as_kudo_kind_to_string (j),
					cnt, perc);
	}

	/* addons with MetaInfo */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) == AS_APP_KIND_ADDON)
			cnt++;
	}
	g_string_append_printf (html, "<tr><td class=\"alt\">MetaInfo</td>"
				"<td>%u</td><td class=\"thin\"></td></tr>\n", cnt);


	g_string_append (html, "</table>\n");
	g_string_append (html, "<br/>\n");
	return TRUE;
}

static void
as_util_status_html_write_javascript (GString *html)
{
	g_string_append (html,
	"<script type=\"text/javascript\"><!--\n"
	"function\n"
	"setVisibility(what, val)\n"
	"{\n"
	"	var obj = typeof what == 'object' ? what : document.getElementById(what);\n"
	"	obj.style.display = val ? 'block' : 'none';\n"
	"}\n"
	"function\n"
	"getVisibility(what)\n"
	"{\n"
	"	var obj = typeof what == 'object' ? what : document.getElementById(what);\n"
	"	return obj.style.display != 'none';\n"
	"}\n"
	"function\n"
	"statusReset(class_name, val)\n"
	"{\n"
	"	var class_names = [ 'description', 'screenshots', 'keywords', 'gnome', 'kde', 'xfce', 'addon' ];\n"
	"	for (var i = 0; i < class_names.length; i++) {\n"
	"		document.getElementById('button-' + class_names[i] + '-with').disabled = false;\n"
	"		document.getElementById('button-' + class_names[i] + '-without').disabled = false;\n"
	"	}\n"
	"	var apps = document.getElementById('apps');\n"
	"	for (var i = 0; i < apps.children.length; i++) {\n"
	"		var child_id = apps.children[i].id;\n"
	"		if (!child_id)\n"
	"			continue;\n"
	"		setVisibility(child_id, true);\n"
	"	}\n"
	"}\n"
	"function\n"
	"statusHideFilter(class_name, val)\n"
	"{\n"
	"	document.getElementById('button-' + class_name + '-with').disabled = true;\n"
	"	document.getElementById('button-' + class_name + '-without').disabled = true;\n"
	"	var apps = document.getElementById('apps');\n"
	"	for (var i = 0; i < apps.children.length; i++) {\n"
	"		var child_id = apps.children[i].id\n"
	"		if (!child_id)\n"
	"			continue;\n"
	"		if (!getVisibility(apps.children[i]))\n"
	"			continue;\n"
	"		var classes = apps.children[i].className.split(' ');\n"
	"		var has_classname = false;\n"
	"		for (var j = 0; j < classes.length; j++) {\n"
	"			if (classes[j] == class_name) {\n"
	"				has_classname = true;\n"
	"				break;\n"
	"			}\n"
	"		}\n"
	"		if ((val && !has_classname) || (!val && has_classname))\n"
	"			setVisibility(child_id, false);\n"
	"	}\n"
	"	return;\n"
	"}\n"
	"//-->\n"
	"</script>\n");
}

static void
as_util_status_html_write_css (GString *html)
{
	g_string_append (html,
	"<style type=\"text/css\">\n"
	"body {\n"
	"	margin-top: 2em;\n"
	"	margin-left: 5%;\n"
	"	margin-right: 5%;\n"
	"	font-family: 'Lucida Grande', Verdana, Arial, Sans-Serif;\n"
	"}\n"
	"table.app {\n"
	"	border: 1px solid #dddddd;\n"
	"	border-collapse: collapse;\n"
	"	width: 80%;\n"
	"}\n"
	"img {\n"
	"	border: 1px solid #dddddd;\n"
	"}\n"
	"table.summary {\n"
	"	border: 1px solid #dddddd;\n"
	"	border-collapse: collapse;\n"
	"	width: 20%;\n"
	"}\n"
	"td {\n"
	"	padding: 7px;\n"
	"}\n"
	"td.alt {\n"
	"	background-color: #eeeeee;\n"
	"	width: 150px;\n"
	"}\n"
	"td.thin {\n"
	"	width: 100px;\n"
	"	text-align: right;\n"
	"}\n"
	"a:link {\n"
	"	color: #2b5e82;\n"
	"	text-decoration: none;\n"
	"}\n"
	"a:visited {\n"
	"	color: #52188b;\n"
	"}\n"
	"</style>\n");
}

static void
as_util_status_html_write_filter_section (GString *html)
{
	g_string_append (html,
	"<h2>Filtering</h2>\n"
	"<table class=\"summary\">\n"
	"<tr>\n"
	"<td class=\"alt\">Hide Descriptions</td>\n"
	"<td><button id=\"button-description-with\" onclick=\"statusHideFilter('description', false);\">&#10003;</button></td>\n"
	"<td><button id=\"button-description-without\" onclick=\"statusHideFilter('description', true);\">&#x2715;</button></td>\n"
	"</tr>\n"
	"<tr>\n"
	"<td class=\"alt\">Hide Screenshots</td>\n"
	"<td><button id=\"button-screenshots-with\" onclick=\"statusHideFilter('screenshots', false);\">&#10003;</button></td>\n"
	"<td><button id=\"button-screenshots-without\" onclick=\"statusHideFilter('screenshots', true);\">&#x2715;</button></td>\n"
	"</tr>\n"
	"<tr>\n"
	"<td class=\"alt\">Hide Keywords</td>\n"
	"<td><button id=\"button-keywords-with\" onclick=\"statusHideFilter('keywords', false);\">&#10003;</button></td>\n"
	"<td><button id=\"button-keywords-without\" onclick=\"statusHideFilter('keywords', true);\">&#x2715;</button></td>\n"
	"</tr>\n"
	"<tr>\n"
	"<td class=\"alt\">Hide GNOME</td>\n"
	"<td><button id=\"button-gnome-with\" onclick=\"statusHideFilter('gnome', false);\">&#10003;</button></td>\n"
	"<td><button id=\"button-gnome-without\" onclick=\"statusHideFilter('gnome', true);\">&#x2715;</button></td>\n"
	"</tr>\n"
	"<tr>\n"
	"<td class=\"alt\">Hide KDE</td>\n"
	"<td><button id=\"button-kde-with\" onclick=\"statusHideFilter('kde', false);\">&#10003;</button></td>\n"
	"<td><button id=\"button-kde-without\" onclick=\"statusHideFilter('kde', true);\">&#x2715;</button></td>\n"
	"</tr>\n"
	"<tr>\n"
	"<td class=\"alt\">Hide XFCE</td>\n"
	"<td><button id=\"button-xfce-with\" onclick=\"statusHideFilter('xfce', false);\">&#10003;</button></td>\n"
	"<td><button id=\"button-xfce-without\" onclick=\"statusHideFilter('xfce', true);\">&#x2715;</button></td>\n"
	"</tr>\n"
	"<tr>\n"
	"<tr>\n"
	"<td class=\"alt\">Hide Addons</td>\n"
	"<td><button id=\"button-addon-with\" onclick=\"statusHideFilter('addon', false);\">&#10003;</button></td>\n"
	"<td><button id=\"button-addon-without\" onclick=\"statusHideFilter('addon', true);\">&#x2715;</button></td>\n"
	"</tr>\n"
	"<tr>\n"
	"<td colspan=\"3\"><button id=\"button-reset\" onclick=\"statusReset('description');\">Reset Filters</button></td>\n"
	"</tr>\n"
	"</table>\n");
}

static gboolean
as_util_status_html (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	AsUtilDistro distro = AS_UTIL_DISTRO_UNKNOWN;
	GPtrArray *apps = NULL;
	guint i;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GString) html = NULL;

	/* check args */
	if (g_strv_length (values) != 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename.xml.gz status.html");
		return FALSE;
	}

	/* load file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file, NULL, NULL, error))
		return FALSE;
	apps = as_store_get_apps (store);

	/* detect distro */
	if (g_strstr_len (values[0], -1, "fedora") != NULL)
		distro = AS_UTIL_DISTRO_FEDORA;

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
	as_util_status_html_write_css (html);
	as_util_status_html_write_javascript (html);
	g_string_append (html, "</head>\n");
	g_string_append (html, "<body>\n");

	/* summary section */
	if (!g_str_has_suffix (as_store_get_origin (store), "failed")) {
		if (!as_util_status_html_write_exec_summary (apps, html, error))
			return FALSE;
	}

	/* write */
	as_util_status_html_write_filter_section (html);

	/* write applications */
	g_string_append (html, "<h1>Applications</h1>\n");
	g_string_append (html, "<div id=\"apps\">\n");
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) == AS_APP_KIND_FONT)
			continue;
		if (as_app_get_kind (app) == AS_APP_KIND_INPUT_METHOD)
			continue;
		if (as_app_get_kind (app) == AS_APP_KIND_CODEC)
			continue;
		if (as_app_get_kind (app) == AS_APP_KIND_SOURCE)
			continue;
		as_util_status_html_write_app (app, html, distro);
	}
	g_string_append (html, "</div>\n");

	g_string_append (html, "</body>\n");
	g_string_append (html, "</html>\n");

	/* save file */
	if (!g_file_set_contents (values[1], html->str, -1, error))
		return FALSE;
	return TRUE;
}

typedef enum {
	AS_UTIL_PKG_STATE_OK, /* in order of badness */
	AS_UTIL_PKG_STATE_INFO,
	AS_UTIL_PKG_STATE_WARN,
	AS_UTIL_PKG_STATE_FAIL,
	AS_UTIL_PKG_STATE_DEAD
} AsUtilPkgState;

static void
as_util_matrix_html_write_item (AsUtilPkgState *state_app,
				    AsUtilPkgState state,
				    GString *html,
				    const gchar *comment)
{
	g_string_append (html, "<td>");

	/* ab-use acronym for the mouse-over explaination */
	if (comment != NULL)
		g_string_append_printf (html, "<acronym title=\"%s\">", comment);

	switch (state) {
	case AS_UTIL_PKG_STATE_OK:
		g_string_append (html, "OK");
		break;
	case AS_UTIL_PKG_STATE_INFO:
		g_string_append (html, "Info");
		break;
	case AS_UTIL_PKG_STATE_WARN:
		g_string_append (html, "Warning");
		break;
	case AS_UTIL_PKG_STATE_FAIL:
		g_string_append (html, "Failed");
		break;
	case AS_UTIL_PKG_STATE_DEAD:
		g_string_append (html, "Dead");
		break;
	default:
		break;
	}
	if (comment != NULL)
		g_string_append (html, "</acronym>");

	g_string_append (html, "</td>\n");

	/* update the global state */
	if (state_app != NULL) {
		if (state > *state_app)
			*state_app = state;
	}
}

static void
as_util_matrix_html_write_app (AsApp *app, GString *html, AsUtilDistro distro)
{
	AsIcon *ic;
	AsUtilPkgState state_app = AS_UTIL_PKG_STATE_OK;
	GPtrArray *arr;
	g_autoptr(GString) str = NULL;

	str = g_string_new ("");
	g_string_append_printf (str, "<td>%s</td>\n", as_app_get_id_filename (app));

	/* pkgname */
	switch (distro) {
	case AS_UTIL_DISTRO_FEDORA:
		g_string_append_printf (str, "<td><a href=\"https://apps.fedoraproject.org/packages/%s\">%s</a></td>\n",
					as_app_get_pkgname_default (app),
					as_app_get_pkgname_default (app));
		break;
	default:
		g_string_append_printf (str, "<td>%s</td>\n", as_app_get_pkgname_default (app));
		break;
	}

	/* summary */
	if (as_app_get_comment (app, NULL) == NULL) {
		as_util_matrix_html_write_item (&state_app,
						AS_UTIL_PKG_STATE_FAIL,
						str,
						"No summary in AppData file");
	} else {
		as_util_matrix_html_write_item (NULL, AS_UTIL_PKG_STATE_OK, str, NULL);
	}

	/* description */
	if (as_app_get_description (app, NULL) == NULL) {
		as_util_matrix_html_write_item (&state_app,
						AS_UTIL_PKG_STATE_WARN,
						str,
						"No long description in AppData file");
	} else {
		as_util_matrix_html_write_item (NULL, AS_UTIL_PKG_STATE_OK, str, NULL);
	}

	/* screenshots */
	arr = as_app_get_screenshots (app);
	if (arr ==  NULL || arr->len == 0) {
		as_util_matrix_html_write_item (&state_app,
						AS_UTIL_PKG_STATE_WARN,
						str,
						"No screenshots in AppData file");
	} else {
		as_util_matrix_html_write_item (NULL, AS_UTIL_PKG_STATE_OK, str, NULL);
	}

	/* icons */
	ic = as_app_get_icon_default (app);
	if (ic == NULL) {
		as_util_matrix_html_write_item (&state_app,
						AS_UTIL_PKG_STATE_FAIL,
						str,
						"No icon");
	} else {
		if (!as_app_has_kudo_kind (app, AS_KUDO_KIND_HI_DPI_ICON))
			as_util_matrix_html_write_item (&state_app,
							AS_UTIL_PKG_STATE_INFO,
							str,
							"No HiDPI icon");
		else
			as_util_matrix_html_write_item (NULL, AS_UTIL_PKG_STATE_OK, str, NULL);
	}

	/* keywords */
	arr = as_app_get_keywords (app, NULL);
	if (arr == NULL || arr->len == 0) {
		as_util_matrix_html_write_item (&state_app,
						AS_UTIL_PKG_STATE_WARN,
						str,
						"No keywords in .desktop file");
	} else {
		as_util_matrix_html_write_item (NULL, AS_UTIL_PKG_STATE_OK, str, NULL);
	}

	/* veto */
	arr = as_app_get_vetos (app);
	if (arr == NULL || arr->len == 0) {
		as_util_matrix_html_write_item (NULL, AS_UTIL_PKG_STATE_OK, str, NULL);
	} else {
		g_autofree gchar *tmp = NULL;
		tmp = as_util_status_html_join (arr);
		if (g_strstr_len (tmp, -1, "Dead upstream") != NULL) {
			as_util_matrix_html_write_item (&state_app,
							AS_UTIL_PKG_STATE_DEAD,
							str, tmp);
		} else {
			as_util_matrix_html_write_item (&state_app,
							AS_UTIL_PKG_STATE_FAIL,
							str, tmp);
		}
	}

	/* global state */
	switch (state_app) {
	case AS_UTIL_PKG_STATE_OK:
		g_string_prepend (str, "<tr bgcolor=\"#a7f6a7\">\n");
		break;
	case AS_UTIL_PKG_STATE_INFO:
		g_string_prepend (str, "<tr bgcolor=\"#e0edfb\">\n");
		break;
	case AS_UTIL_PKG_STATE_WARN:
		g_string_prepend (str, "<tr bgcolor=\"#fbfbe0\">\n");
		break;
	case AS_UTIL_PKG_STATE_FAIL:
		g_string_prepend (str, "<tr bgcolor=\"#ffa1a5\">\n");
		break;
	case AS_UTIL_PKG_STATE_DEAD:
		g_string_prepend (str, "<tr bgcolor=\"#888888\">\n");
		break;
	default:
		g_string_prepend (str, "<tr>\n");
		break;
	}

	g_string_append (str, "</tr>\n");
	g_string_append (html, str->str);
}

static gint
as_util_array_sort_by_pkgname_cb (gconstpointer a, gconstpointer b)
{
	AsApp *app1 = *((AsApp **) a);
	AsApp *app2 = *((AsApp **) b);
	return g_strcmp0 (as_app_get_pkgname_default (app1),
			  as_app_get_pkgname_default (app2));
}

static gboolean
as_util_matrix_html (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	AsUtilDistro distro = AS_UTIL_DISTRO_UNKNOWN;
	GPtrArray *apps = NULL;
	guint i;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GString) html = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected matrix.html filename.xml.gz");
		return FALSE;
	}

	/* load file */
	store = as_store_new ();
	for (i = 1; values[i] != NULL; i++) {
		g_autoptr(GFile) file = NULL;
		file = g_file_new_for_path (values[i]);
		if (!as_store_from_file (store, file, NULL, NULL, error))
			return FALSE;
	}
	apps = as_store_get_apps (store);

	/* order by package name */
	g_ptr_array_sort (apps, as_util_array_sort_by_pkgname_cb);

	/* detect distro */
	if (g_strstr_len (values[1], -1, "fedora") != NULL)
		distro = AS_UTIL_DISTRO_FEDORA;

	/* create header */
	html = g_string_new ("");
	g_string_append (html, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 "
			       "Transitional//EN\" "
			       "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	g_string_append (html, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	g_string_append (html, "<head>\n");
	g_string_append (html, "<meta http-equiv=\"Content-Type\" content=\"text/html; "
			       "charset=UTF-8\" />\n");
	g_string_append (html, "<title>Application Data Matrix</title>\n");
	as_util_status_html_write_css (html);
	g_string_append (html, "</head>\n");
	g_string_append (html, "<body>\n");

	/* write applications */
	g_string_append (html, "<h1>Packages</h1>\n");
	g_string_append (html, "<div id=\"apps\">\n");
	g_string_append (html, "<table>\n");

	/* table header */
	g_string_append (html, "<tr>\n");
	g_string_append (html, "<th>Application ID</th>\n");
	g_string_append (html, "<th>Package Name</th>\n");
	g_string_append (html, "<th>Summary</th>\n");
	g_string_append (html, "<th>Description</th>\n");
	g_string_append (html, "<th>Screenshots</th>\n");
	g_string_append (html, "<th>Icon</th>\n");
	g_string_append (html, "<th>Keywords</th>\n");
	g_string_append (html, "<th>Veto</th>\n");
	g_string_append (html, "</tr>\n");

	/* apps */
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) != AS_APP_KIND_DESKTOP)
			continue;
		as_util_matrix_html_write_app (app, html, distro);
	}

	/* footer */
	g_string_append (html, "</table>\n");
	g_string_append (html, "</div>\n");
	g_string_append (html, "</body>\n");
	g_string_append (html, "</html>\n");

	/* save file */
	return g_file_set_contents (values[0], html->str, -1, error);
}

static gboolean
as_util_status_csv_filter_func (AsApp *app, gchar **filters)
{
	const gchar *tmp;
	guint i;
	AsAppKind id_kind = AS_APP_KIND_DESKTOP;

	for (i = 0; filters[i] != NULL; i++) {
		g_auto(GStrv) split = NULL;
		split = g_strsplit (filters[i], "=", 2);
		if (g_strv_length (split) != 2)
			continue;
		if (g_strcmp0 (split[0], "id-kind") == 0) {
			id_kind = as_app_kind_from_string (split[1]);
			if (as_app_get_kind (app) != id_kind)
				return FALSE;
			continue;
		}
		if (g_strcmp0 (split[0], "metadata") == 0) {
			tmp = as_app_get_metadata_item (app, split[1]);
			if (tmp == NULL)
				return FALSE;
			continue;
		}
		g_warning ("Unknown filter option %s:%s", split[0], split[1]);
	}
	return TRUE;
}

static gboolean
as_util_status_csv (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps = NULL;
	const gchar *tmp;
	guint i;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GString) data = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename.xml.gz status.csv");
		return FALSE;
	}

	/* load file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file, NULL, NULL, error))
		return FALSE;
	apps = as_store_get_apps (store);

	/* write applications */
	data = g_string_new ("id,pkgname,name,comment,description,url\n");
	for (i = 0; i < apps->len; i++) {
		g_autofree gchar *description = NULL;
		app = g_ptr_array_index (apps, i);

		/* process filters */
		if (!as_util_status_csv_filter_func (app, values + 2))
			continue;
		g_string_append_printf (data, "%s,", as_app_get_id (app));
		g_string_append_printf (data, "%s,", as_app_get_pkgname_default (app));
		g_string_append_printf (data, "\"%s\",", as_app_get_name (app, "C"));
		g_string_append_printf (data, "\"%s\",", as_app_get_comment (app, "C"));
		description = g_strdup (as_app_get_description (app, "C"));
		if (description != NULL) {
			g_strdelimit (description, "\n", '|');
			g_strdelimit (description, "\"", '\'');
		}
		g_string_append_printf (data, "\"%s\",", description ? description : "");
		tmp = as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE);
		g_string_append_printf (data, "\"%s\",", tmp ? tmp : "");
		g_string_truncate (data, data->len - 1);
		g_string_append (data, "\n");
	}

	/* save file */
	if (!g_file_set_contents (values[1], data->str, -1, error))
		return FALSE;
	return TRUE;
}

static gboolean
as_util_non_package_yaml (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps = NULL;
	guint i;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GString) yaml = NULL;

	/* check args */
	if (g_strv_length (values) != 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected filename.xml.gz "
				     "applications-to-import.yaml");
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
	if (!g_file_set_contents (values[1], yaml->str, -1, error))
		return FALSE;
	return TRUE;
}

static void
as_util_validate_output_text (const gchar *filename, GPtrArray *probs)
{
	AsProblem *problem;
	const gchar *tmp;
	guint i;
	gsize j;

	/* success */
	if (probs->len == 0) {
		/* TRANSLATORS: the file is valid */
		g_print ("%s\n", _("OK"));
		return;
	}

	/* list failures */
	g_print ("%s:\n", _("FAILED"));
	for (i = 0; i < probs->len; i++) {
		problem = g_ptr_array_index (probs, i);
		tmp = as_problem_kind_to_string (as_problem_get_kind (problem));
		g_print ("• %s ", tmp);
		for (j = strlen (tmp); j < 20; j++)
			g_print (" ");
		if (as_problem_get_line_number (problem) > 0) {
			g_print (" : %s [ln:%u]\n",
				 as_problem_get_message (problem),
				 as_problem_get_line_number (problem));
		} else {
			g_print (" : %s\n", as_problem_get_message (problem));
		}
	}
}

static void
as_util_validate_output_html (const gchar *filename, GPtrArray *probs)
{
	g_print ("<html>\n");
	g_print ("<head>\n");
	g_print ("<style type=\"text/css\">\n");
	g_print ("body {width: 70%%; font: 12px/20px Arial, Helvetica;}\n");
	g_print ("p {color: #333;}\n");
	g_print ("</style>\n");
	g_print ("<title>AppData Validation Results for %s</title>\n", filename);
	g_print ("</head>\n");
	g_print ("<body>\n");
	if (probs->len == 0) {
		g_print ("<h1>Success!</h1>\n");
		g_print ("<p>%s validated successfully.</p>\n", filename);
	} else {
		guint i;
		g_print ("<h1>Validation failed!</h1>\n");
		g_print ("<p>%s did not validate:</p>\n", filename);
		g_print ("<ul>\n");
		for (i = 0; i < probs->len; i++) {
			AsProblem *problem;
			g_autofree gchar *tmp = NULL;
			problem = g_ptr_array_index (probs, i);
			tmp = g_markup_escape_text (as_problem_get_message (problem), -1);
			g_print ("<li>");
			g_print ("%s\n", tmp);
			if (as_problem_get_line_number (problem) > 0) {
				g_print (" (line %u)",
					 as_problem_get_line_number (problem));
			}
			g_print ("</li>\n");
		}
		g_print ("</ul>\n");
	}
	g_print ("</body>\n");
	g_print ("</html>\n");
}

static gboolean
as_util_validate_file (const gchar *filename,
		       AsAppValidateFlags flags,
		       GError **error)
{
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GPtrArray) probs = NULL;

	/* is AppStream */
	g_print ("%s: ", filename);
	if (as_format_guess_kind (filename) == AS_FORMAT_KIND_APPSTREAM) {
		gboolean ret;
		g_autoptr(AsStore) store = NULL;
		g_autoptr(GFile) file = NULL;
		file = g_file_new_for_path (filename);
		store = as_store_new ();
		ret = as_store_from_file (store, file, NULL, NULL, error);
		if (!ret)
			return FALSE;
		flags |= AS_APP_VALIDATE_FLAG_ALL_APPS;
		probs = as_store_validate (store, flags, error);
		if (probs == NULL)
			return FALSE;
	} else {
		/* load file */
		app = as_app_new ();
		if (!as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, error))
			return FALSE;
		probs = as_app_validate (app, flags, error);
		if (probs == NULL)
			return FALSE;
	}
	if (g_strcmp0 (g_getenv ("OUTPUT_FORMAT"), "html") == 0)
		as_util_validate_output_html (filename, probs);
	else
		as_util_validate_output_text (filename, probs);
	if (probs->len > 0) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     _("Validation failed"));
		return FALSE;
	}
	return TRUE;
}

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
		/* TRANSLATORS: error message */
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     _("Validation of files failed"));
		return FALSE;
	}
	return n_failed == 0;
}

static gboolean
as_util_validate (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsAppValidateFlags flags = AS_APP_VALIDATE_FLAG_NONE;
	if (priv->nonet)
		flags |= AS_APP_VALIDATE_FLAG_NO_NETWORK;
	return as_util_validate_files (values, flags, error);
}

static gboolean
as_util_validate_relax (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsAppValidateFlags flags = AS_APP_VALIDATE_FLAG_RELAX;
	if (priv->nonet)
		flags |= AS_APP_VALIDATE_FLAG_NO_NETWORK;
	return as_util_validate_files (values, flags, error);
}

static gboolean
as_util_validate_strict (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsAppValidateFlags flags = AS_APP_VALIDATE_FLAG_STRICT;
	if (priv->nonet)
		flags |= AS_APP_VALIDATE_FLAG_NO_NETWORK;
	return as_util_validate_files (values, flags, error);
}

static gboolean
as_util_check_root_app_icon (AsApp *app, GError **error)
{
	AsIcon *icon_default;
	const gchar *name;
	g_autofree gchar *icon = NULL;
	g_autoptr(GdkPixbuf) pb = NULL;

	/* these are set by the software center */
	switch (as_app_get_kind (app)) {
	case AS_APP_KIND_INPUT_METHOD:
	case AS_APP_KIND_CODEC:
	case AS_APP_KIND_RUNTIME:
	case AS_APP_KIND_GENERIC:
	case AS_APP_KIND_OS_UPDATE:
	case AS_APP_KIND_OS_UPGRADE:
		return TRUE;
	default:
		break;
	}

	/* nothing found */
	icon_default = as_app_get_icon_default (app);
	if (icon_default == NULL) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "%s has no Icon",
			     as_app_get_id (app));
		return FALSE;
	}

	/* is stock icon */
	if (as_utils_is_stock_icon_name (as_icon_get_name (icon_default)))
		return TRUE;

	/* can we find it */
	name = as_icon_get_name (icon_default);
	if (name == NULL)
		name = as_icon_get_filename (icon_default);
	if (name == NULL) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "%s has no icon set",
			     as_app_get_id (app));
		return FALSE;
	}
	icon = as_utils_find_icon_filename (g_getenv ("DESTDIR"), name, error);
	if (icon == NULL) {
		g_prefix_error (error,
				"%s missing icon %s: ",
				as_app_get_id (app),
				as_icon_get_name (icon_default));
		return FALSE;
	}

	/* can we can load it */
	pb = gdk_pixbuf_new_from_file (icon, error);
	if (pb == NULL) {
		g_prefix_error (error,
				"%s invalid icon %s: ",
				as_app_get_id (app),
				as_icon_get_name (icon_default));
		return FALSE;
	}

	/* check size */
	if (gdk_pixbuf_get_width (pb) < AS_APP_ICON_MIN_WIDTH ||
	    gdk_pixbuf_get_height (pb) < AS_APP_ICON_MIN_HEIGHT) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "%s has undersized icon (%ix%i)",
			     as_app_get_id (app),
			     gdk_pixbuf_get_width (pb),
			     gdk_pixbuf_get_height (pb));
		return FALSE;
	}
	return TRUE;
}

static void
as_util_check_root_app (AsApp *app, GPtrArray *problems)
{
	AsFormat *format;
	g_autoptr(GError) error_local = NULL;

	/* skip */
	format = as_app_get_format_default (app);
	if (as_format_get_kind (format) == AS_FORMAT_KIND_METAINFO)
		return;

	/* relax this for now */
	if (as_format_get_kind (format) == AS_FORMAT_KIND_DESKTOP)
		return;

	/* check one line summary */
	if (as_app_get_comment (app, NULL) == NULL) {
		g_ptr_array_add (problems,
				 g_strdup_printf ("%s has no Comment\n - Source: %s",
						  as_app_get_id (app),
						  as_format_get_filename (format)));
	}

	/* check icon exists and is large enough */
	if (!as_util_check_root_app_icon (app, &error_local)) {
		g_ptr_array_add (problems,
				 g_strdup_printf ("%s\n - Source: %s",
						  error_local->message,
						  as_format_get_filename (format)));
	}
}

static void
as_util_check_component_app (AsApp *app, GPtrArray *problems)
{
	AsFormat *format;

	g_print ("\nUsing %s for %s\n",
		 as_app_get_unique_id (app),
		 as_app_get_id (app));

	/* has desktop file */
	if (as_app_get_kind (app) == AS_APP_KIND_DESKTOP) {
		format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_DESKTOP);
		if (format == NULL) {
			g_ptr_array_add (problems, g_strdup ("No desktop file"));
		} else {
			g_print ("Checking source: %s\n", as_format_get_filename (format));
		}
	}

	/* has AppData file */
	format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPDATA);
	if (format == NULL)
		format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_METAINFO);
	if (format == NULL) {
		g_ptr_array_add (problems,
				 g_strdup_printf ("%s has no AppData file",
						  as_app_get_id (app)));
	} else {
		g_print ("Checking source: %s\n", as_format_get_filename (format));
		if (as_app_get_description (app, NULL) == NULL) {
			g_ptr_array_add (problems,
					 g_strdup_printf ("%s has no <description>",
							  as_app_get_id (app)));
		}
		if (as_app_get_comment (app, NULL) == NULL) {
			g_ptr_array_add (problems,
					 g_strdup_printf ("%s has no <summary>",
							  as_app_get_id (app)));
		}
	}

	/* check icon exists and is large enough */
	if (as_app_get_kind (app) == AS_APP_KIND_DESKTOP) {
		g_autoptr(GError) error_local = NULL;
		if (!as_util_check_root_app_icon (app, &error_local))
			g_ptr_array_add (problems, g_strdup (error_local->message));
	}
}

static gboolean
as_util_check_component (AsUtilPrivate *priv, gchar **values, GError **error)
{
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GPtrArray) problems = NULL;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, "
				     "expected example.desktop");
		return FALSE;
	}

	/* load root */
	store = as_store_new ();
	as_store_set_add_flags (store, AS_STORE_ADD_FLAG_PREFER_LOCAL);
	as_store_set_destdir (store, g_getenv ("DESTDIR"));
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_DESKTOP |
			    AS_STORE_LOAD_FLAG_APPDATA |
			    AS_STORE_LOAD_FLAG_ALLOW_VETO,
			    NULL,
			    error))
		return FALSE;

	/* sanity check each */
	problems = g_ptr_array_new_with_free_func (g_free);
	for (guint j = 0; values[j] != NULL; j++) {
		g_autoptr(GPtrArray) apps = as_store_get_apps_by_id (store, values[j]);
		if (apps->len == 0) {
			g_printerr ("Failed to find %s\n", values[j]);
			continue;
		}
		for (guint i = 0; i < apps->len; i++) {
			AsApp *app = g_ptr_array_index (apps, i);
			as_util_check_component_app (app, problems);
		}
	}

	/* show problems */
	if (problems->len) {
		g_printerr ("\n");
		for (guint i = 0; i < problems->len; i++) {
			const gchar *tmp = g_ptr_array_index (problems, i);
			g_printerr ("• %s\n", tmp);
		}
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Failed to check component, %u problems detected",
			     problems->len);
		return FALSE;
	}

	/* success */
	g_print ("\nNo problems found\n");
	return TRUE;
}

G_GNUC_PRINTF (2, 3)
static void
as_util_app_log (AsApp *app, const gchar *fmt, ...)
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
as_util_mirror_screenshots_thumb (AsScreenshot *ss, AsImage *im_src,
				  guint width, guint height, guint scale,
				  const gchar *mirror_uri,
				  const gchar *output_dir,
				  GError **error)
{
	g_autofree gchar *fn = NULL;
	g_autofree gchar *size_str = NULL;
	g_autofree gchar *url_tmp = NULL;
	g_autoptr(AsImage) im_tmp = NULL;

	/* only save the HiDPI screenshot if it's not padded */
	if (scale > 1) {
		if (width * scale > as_image_get_width (im_src) ||
		    height * scale > as_image_get_height (im_src))
			return TRUE;
	}

	/* save to disk */
	size_str = g_strdup_printf ("%ux%u", width * scale, height * scale);
	fn = g_build_filename (output_dir, size_str,
			       as_image_get_basename (im_src),
			       NULL);
	if (!g_file_test (fn, G_FILE_TEST_EXISTS)) {
		if (!as_image_save_filename (im_src, fn,
					     width * scale,
					     height * scale,
					     AS_IMAGE_SAVE_FLAG_PAD_16_9 |
					     AS_IMAGE_SAVE_FLAG_SHARPEN,
					     error))
			return FALSE;
	}

	/* add resized image to the screenshot */
	im_tmp = as_image_new ();
	as_image_set_width (im_tmp, width * scale);
	as_image_set_height (im_tmp, height * scale);
	url_tmp = g_build_filename (mirror_uri,
				    size_str,
				    as_image_get_basename (im_src),
				    NULL);
	as_image_set_url (im_tmp, url_tmp);
	as_image_set_kind (im_tmp, AS_IMAGE_KIND_THUMBNAIL);
	as_image_set_basename (im_tmp, as_image_get_basename (im_src));
	as_screenshot_add_image (ss, im_tmp);
	return TRUE;
}

static gboolean
as_util_mirror_screenshots_app_file (AsApp *app,
				     AsScreenshot *ss,
				     const gchar *filename,
				     const gchar *mirror_uri,
				     const gchar *output_dir,
				     GError **error)
{
	AsImageAlphaFlags alpha_flags;
	guint i;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *filename_no_path = NULL;
	g_autofree gchar *url_src = NULL;
	g_autoptr(AsImage) im_src = NULL;
	guint sizes[] = { AS_IMAGE_NORMAL_WIDTH,    AS_IMAGE_NORMAL_HEIGHT,
			  AS_IMAGE_THUMBNAIL_WIDTH, AS_IMAGE_THUMBNAIL_HEIGHT,
			  AS_IMAGE_LARGE_WIDTH,     AS_IMAGE_LARGE_HEIGHT,
			  0 };

	im_src = as_image_new ();
	if (!as_image_load_filename (im_src, filename, error))
		return FALSE;

	/* is the aspect ratio of the source perfectly 16:9 */
	if ((as_image_get_width (im_src) / 16) * 9 !=
	     as_image_get_height (im_src)) {
		filename_no_path = g_path_get_basename (filename);
		g_debug ("%s is not in 16:9 aspect ratio",
			 filename_no_path);
	}

	/* check screenshot is reasonable in size */
	if (as_image_get_width (im_src) * 2 < AS_IMAGE_NORMAL_WIDTH ||
	    as_image_get_height (im_src) * 2 < AS_IMAGE_NORMAL_HEIGHT) {
		filename_no_path = g_path_get_basename (filename);
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "%s is too small to be used: %ux%u",
			     filename_no_path,
			     as_image_get_width (im_src),
			     as_image_get_height (im_src));
		return FALSE;
	}

	/* check the image is not padded */
	alpha_flags = as_image_get_alpha_flags (im_src);
	if ((alpha_flags & AS_IMAGE_ALPHA_FLAG_TOP) > 0||
	    (alpha_flags & AS_IMAGE_ALPHA_FLAG_BOTTOM) > 0) {
		filename_no_path = g_path_get_basename (filename);
		g_debug ("%s has vertical alpha padding",
			 filename_no_path);
	}
	if ((alpha_flags & AS_IMAGE_ALPHA_FLAG_LEFT) > 0||
	    (alpha_flags & AS_IMAGE_ALPHA_FLAG_RIGHT) > 0) {
		filename_no_path = g_path_get_basename (filename);
		g_debug ("%s has horizontal alpha padding",
			 filename_no_path);
	}

	/* include the app-id in the basename */
	basename = g_strdup_printf ("%s-%s.png",
				    as_app_get_id_filename (AS_APP (app)),
				    as_image_get_md5 (im_src));
	as_image_set_basename (im_src, basename);

	/* fonts only have full sized screenshots */
	if (as_app_get_kind (AS_APP (app)) != AS_APP_KIND_FONT) {
		for (i = 0; sizes[i] != 0; i += 2) {

			/* save LoDPI */
			if (!as_util_mirror_screenshots_thumb (ss,
							       im_src,
							       sizes[i],
							       sizes[i+1],
							       1, /* scale */
							       mirror_uri,
							       output_dir,
							       error))
				return FALSE;

			/* save HiDPI version */
			if (!as_util_mirror_screenshots_thumb (ss, im_src,
							       sizes[i],
							       sizes[i+1],
							       2, /* scale */
							       mirror_uri,
							       output_dir,
							       error))
				return FALSE;
		}
	}
	return TRUE;
}

static gboolean
as_util_mirror_screenshots_app_url (AsUtilPrivate *priv,
				    AsApp *app,
				    const gchar *url,
				    const gchar *mirror_uri,
				    const gchar *cache_dir,
				    const gchar *output_dir,
				    GError **error)
{
	gboolean is_default;
	gboolean ret = TRUE;
	SoupStatus status;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *cache_filename = NULL;
	g_autoptr(AsImage) im = NULL;
	g_autoptr(AsScreenshot) ss = NULL;
	g_autoptr(SoupMessage) msg = NULL;
	g_autoptr(SoupSession) session = NULL;
	g_autoptr(SoupURI) uri = NULL;

	/* fonts screenshots are auto-generated */
	if (as_app_get_kind (app) == AS_APP_KIND_FONT) {
		g_autofree gchar *url_new = NULL;
		basename = g_path_get_basename (url);
		url_new = g_build_filename (mirror_uri, "source", basename, NULL);
		im = as_image_new ();
		as_image_set_url (im, url_new);
		as_image_set_kind (im, AS_IMAGE_KIND_SOURCE);
		ss = as_screenshot_new ();
		as_screenshot_set_kind (ss, AS_SCREENSHOT_KIND_DEFAULT);
		as_screenshot_add_image (ss, im);
		as_app_add_screenshot (app, ss);
		return TRUE;
	}

	/* set up networking */
	session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT, "appstream-util",
						 SOUP_SESSION_TIMEOUT, 10,
						 NULL);
	soup_session_add_feature_by_type (session,
					  SOUP_TYPE_PROXY_RESOLVER_DEFAULT);

	/* download to cache if not already added */
	basename = g_path_get_basename (url);
	cache_filename = g_strdup_printf ("%s/%s-%s",
					  cache_dir,
					  as_app_get_id_filename (AS_APP (app)),
					  basename);
	if (g_file_test (cache_filename, G_FILE_TEST_EXISTS)) {
		as_util_app_log (app, "In cache %s", cache_filename);
	} else if (priv->nonet) {
		as_util_app_log (app, "Missing %s:%s", url, cache_filename);
	} else {
		uri = soup_uri_new (url);
		if (uri == NULL) {
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "Could not parse '%s' as a URL", url);
			return FALSE;
		}
		msg = soup_message_new_from_uri (SOUP_METHOD_GET, uri);
		as_util_app_log (app, "Downloading %s", url);
		status = soup_session_send_message (session, msg);
		if (status != SOUP_STATUS_OK) {
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "Downloading failed: %s",
				     soup_status_get_phrase (status));
			return FALSE;
		}

		/* save new file */
		ret = g_file_set_contents (cache_filename,
					   msg->response_body->data,
					   (gssize) msg->response_body->length,
					   error);
		if (!ret)
			return FALSE;
		as_util_app_log (app, "Saved to cache %s", cache_filename);
	}

	/* add back the source image */
	ss = as_screenshot_new ();
	is_default = as_app_get_screenshots(AS_APP(app))->len == 0;
	as_screenshot_set_kind (ss, is_default ? AS_SCREENSHOT_KIND_DEFAULT :
						 AS_SCREENSHOT_KIND_NORMAL);
	im = as_image_new ();
	as_image_set_url (im, url);
	as_image_set_kind (im, AS_IMAGE_KIND_SOURCE);
	as_screenshot_add_image (ss, im);
	as_app_add_screenshot (app, ss);

	/* mirror the filename */
	return as_util_mirror_screenshots_app_file (app,
						    ss,
						    cache_filename,
						    mirror_uri,
						    output_dir,
						    error);
}

static gboolean
as_util_mirror_screenshots_app (AsUtilPrivate *priv,
				AsApp *app,
				GPtrArray *urls,
				const gchar *mirror_uri,
				const gchar *cache_dir,
				const gchar *output_dir,
				GError **error)
{
	guint i;
	const gchar *url;

	for (i = 0; i < urls->len; i++) {
		g_autoptr(GError) error_local = NULL;

		/* download URL or get from cache */
		url = g_ptr_array_index (urls, i);
		if (!as_util_mirror_screenshots_app_url (priv,
							 app,
							 url,
							 mirror_uri,
							 cache_dir,
							 output_dir,
							 &error_local)) {
			as_util_app_log (app, "Failed to download %s: %s",
					 url, error_local->message);
			continue;
		}
	}
	return TRUE;
}

static gboolean
as_util_mirror_screenshots (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	AsImage *im;
	AsScreenshot *ss;
	GPtrArray *apps;
	GPtrArray *images;
	GPtrArray *screenshots;
	guint i;
	guint j;
	guint k;
	const gchar *cache_dir = "./cache/";
	const gchar *output_dir = "./screenshots/";
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;
	guint sizes[] = { AS_IMAGE_NORMAL_WIDTH,    AS_IMAGE_NORMAL_HEIGHT,
			  AS_IMAGE_THUMBNAIL_WIDTH, AS_IMAGE_THUMBNAIL_HEIGHT,
			  AS_IMAGE_LARGE_WIDTH,     AS_IMAGE_LARGE_HEIGHT,
			  0 };

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: "
				     "file url [cachedir] [outputdir]");
		return FALSE;
	}

	/* overrides */
	if (g_strv_length (values) >= 3)
		cache_dir = values[2];
	if (g_strv_length (values) >= 4)
		output_dir = values[3];

	/* create dirs */
	if (g_mkdir_with_parents (cache_dir, 0700) != 0) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Failed to create: %s", cache_dir);
		return FALSE;
	}

	/* create the tree of screenshot directories */
	for (j = 1; j <= 2; j++) {
		for (i = 0; sizes[i] != 0; i += 2) {
			g_autofree gchar *size_str = NULL;
			g_autofree gchar *fn = NULL;
			size_str = g_strdup_printf ("%ux%u",
						    sizes[i+0] * j,
						    sizes[i+1] * j);
			fn = g_build_filename (output_dir, size_str, NULL);
			if (g_mkdir_with_parents (fn, 0700) != 0) {
				g_set_error (error,
					     AS_ERROR,
					     AS_ERROR_FAILED,
					     "Failed to create: %s", fn);
				return FALSE;
			}
		}
	}

	/* open file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file, NULL, NULL, error))
		return FALSE;

	/* convert all the screenshots */
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		g_autoptr(GPtrArray) urls = NULL;

		/* get app */
		app = g_ptr_array_index (apps, i);
		screenshots = as_app_get_screenshots (app);
		if (screenshots->len == 0)
			continue;

		/* get source screenshots */
		urls = g_ptr_array_new_with_free_func (g_free);
		for (j = 0; j < screenshots->len; j++) {
			ss = g_ptr_array_index (screenshots, j);
			images = as_screenshot_get_images (ss);
			for (k = 0; k < images->len; k++) {
				im = g_ptr_array_index (images, k);
				if (as_image_get_kind (im) != AS_IMAGE_KIND_SOURCE)
					continue;
				g_ptr_array_add (urls, g_strdup (as_image_get_url (im)));
			}
		}

		/* invalidate */
		g_ptr_array_set_size (screenshots, 0);
		if (urls->len == 0)
			continue;

		/* download and save new versions */
		if (!as_util_mirror_screenshots_app (priv, app, urls,
						     values[1],
						     cache_dir,
						     output_dir,
						     error))
			return FALSE;
	}

	/* save file */
	if (!as_store_to_file (store, file,
			       AS_NODE_TO_XML_FLAG_ADD_HEADER |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
			       NULL, error))
		return FALSE;

	return TRUE;
}

static gboolean
as_util_mirror_local_firmware (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps;
	guint i;
	guint j;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;

	/* check args */
	if (g_strv_length (values) != 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: "
				     "file url");
		return FALSE;
	}

	/* open file */
	store = as_store_new ();
	file = g_file_new_for_path (values[0]);
	if (!as_store_from_file (store, file, NULL, NULL, error))
		return FALSE;

	/* convert all the screenshots */
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		GPtrArray *releases;
		AsRelease *rel;

		/* get app */
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) != AS_APP_KIND_FIRMWARE)
			continue;
		releases = as_app_get_releases (app);
		if (releases->len == 0)
			continue;
		for (j = 0; j < releases->len; j++) {
			AsChecksum *csum;
			const gchar *tmp;
			g_autofree gchar *loc = NULL;
			g_autofree gchar *fn = NULL;
			rel = g_ptr_array_index (releases, j);

			/* get the release filename, but fall back to
			 * the default location basename if unset */
			csum = as_release_get_checksum_by_target (rel, AS_CHECKSUM_TARGET_CONTAINER);
			if (csum != NULL) {
				tmp = as_checksum_get_filename (csum);
				if (tmp == NULL)
					continue;
				fn = g_strdup (tmp);
			} else {
				tmp = as_release_get_location_default (rel);
				if (tmp == NULL)
					continue;
				fn = g_path_get_basename (tmp);
			}
			loc = g_build_filename (values[1], fn, NULL);
			g_ptr_array_set_size (as_release_get_locations (rel), 0);
			as_release_add_location (rel, loc);
		}
	}

	/* save file */
	if (!as_store_to_file (store, file,
			       AS_NODE_TO_XML_FLAG_ADD_HEADER |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
			       NULL, error))
		return FALSE;

	return TRUE;
}

static gboolean
as_util_agreement_export (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsAgreement *pp;
	GPtrArray *sections;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GFile) file = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: file type, "
				     "e.g. foo.metainfo.xml eula");
		return FALSE;
	}

	/* parse file */
	app = as_app_new ();
	if (!as_app_parse_file (app, values[0], AS_APP_PARSE_FLAG_NONE, error))
		return FALSE;

	/* get all policy sections */
	pp = as_app_get_agreement_by_kind (app, as_agreement_kind_from_string (values[1]));
	if (pp == NULL) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_INVALID_ARGUMENTS,
			     "no privacy policy with type %s",
			     values[1]);
		return FALSE;
	}
	sections = as_agreement_get_sections (pp);
	for (guint i = 0; i < sections->len; i++) {
		AsAgreementSection *ps = g_ptr_array_index (sections, i);
		const gchar *tmp;
		g_autofree gchar *plain = NULL;

		g_print ("%s\n^^^\n", as_agreement_section_get_name (ps, NULL));
		tmp = as_agreement_section_get_description (ps, NULL);
		if (tmp == NULL)
			continue;
		plain = as_markup_convert_simple (tmp, error);
		if (plain == NULL)
			return FALSE;
		g_print ("%s\n\n", plain);
	}

	return TRUE;
}

static gboolean
as_util_replace_screenshots (AsUtilPrivate *priv, gchar **values, GError **error)
{
	GPtrArray *screenshots;
	guint i;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GFile) file = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: file screenshot");
		return FALSE;
	}

	/* parse file */
	app = as_app_new ();
	if (!as_app_parse_file (app, values[0],
				AS_APP_PARSE_FLAG_KEEP_COMMENTS, error))
		return FALSE;

	/* replace screenshots */
	screenshots = as_app_get_screenshots (app);
	g_ptr_array_set_size (screenshots, 0);
	for (i = 1; values[i] != NULL; i++) {
		g_autoptr(AsImage) im = NULL;
		g_autoptr(AsScreenshot) ss = NULL;
		im = as_image_new ();
		as_image_set_url (im, values[i]);
		as_image_set_kind (im, AS_IMAGE_KIND_SOURCE);
		ss = as_screenshot_new ();
		as_screenshot_add_image (ss, im);
		as_screenshot_set_kind (ss, i == 1 ? AS_SCREENSHOT_KIND_DEFAULT :
						     AS_SCREENSHOT_KIND_NORMAL);
		as_app_add_screenshot (app, ss);
	}

	/* save file */
	file = g_file_new_for_path (values[0]);
	if (!as_app_to_file (app, file, NULL, error))
		return FALSE;

	return TRUE;
}

static gboolean
as_util_add_provide (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;
	AsProvideKind provide_kind;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GFile) file = NULL;

	/* check args */
	if (g_strv_length (values) < 3) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: file provide-type provide-value");
		return FALSE;
	}

	/* get provide type */
	provide_kind = as_provide_kind_from_string (values[1]);
	if (provide_kind == AS_PROVIDE_KIND_UNKNOWN) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Provide type not supported");
		return FALSE;
	}

	/* parse file */
	app = as_app_new ();
	if (!as_app_parse_file (app, values[0],
				AS_APP_PARSE_FLAG_KEEP_COMMENTS, error))
		return FALSE;

	/* create and add provides */
	for (i = 2; values[i] != NULL; i++) {
		g_autoptr(AsProvide) provide = NULL;
		provide = as_provide_new ();
		as_provide_set_kind (provide, provide_kind);
		as_provide_set_value (provide, values[i]);
		as_app_add_provide (app, provide);
	}

	/* save */
	file = g_file_new_for_path (values[0]);
	if (!as_app_to_file (app, file, NULL, error))
		return FALSE;

	return TRUE;
}

static gboolean
as_util_add_language (AsUtilPrivate *priv, gchar **values, GError **error)
{
	gint percentage = 0;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GFile) file = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: file locale [value]");
		return FALSE;
	}

	/* parse file */
	app = as_app_new ();
	if (!as_app_parse_file (app, values[0],
				AS_APP_PARSE_FLAG_KEEP_COMMENTS, error))
		return FALSE;

	/* parse optional percentage and add locale */
	if (g_strv_length (values) > 2) {
		guint64 tmp = g_ascii_strtoull (values[2], NULL, 10);
		if (tmp == 0 || tmp > 100) {
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "failed to parse percentage: %s",
				     values[2]);
			return FALSE;
		}
		percentage = (gint) tmp;
	}
	as_app_add_language (app, percentage, values[1]);
	file = g_file_new_for_path (values[0]);
	return as_app_to_file (app, file, NULL, error);
}

static void
as_util_pad_strings (const gchar *id, const gchar *msg, guint align)
{
	gsize i;
	g_print ("%s", id);
	for (i = strlen (id); i < align; i++)
		g_print (" ");
	g_print (" %s\n", msg);
}

static gboolean
as_util_merge_appstream (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i, j;
	g_autoptr(GFile) file_new = NULL;
	g_autoptr(AsStore) store_new = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: output.xml source1.xml ...");
		return FALSE;
	}

	/* load each source store and add to master store */
	store_new = as_store_new ();
	as_store_set_api_version (store_new, 0.9);
	for (j = 1; values[j] != NULL; j++) {
		GPtrArray *apps;
		g_autoptr(GFile) file = NULL;
		g_autoptr(AsStore) store = NULL;

		file = g_file_new_for_path (values[j]);
		store = as_store_new ();
		if (!as_store_from_file (store, file, NULL, NULL, error))
			return FALSE;
		apps = as_store_get_apps (store);
		for (i = 0; i < apps->len; i++) {
			AsApp *app = g_ptr_array_index (apps, i);
			as_store_add_app (store_new, app);
		}

		/* adopt the origin from the first source */
		if (j == 1) {
			as_store_set_origin (store_new,
					     as_store_get_origin (store));
		}
	}

	/* save new store */
	file_new = g_file_new_for_path (values[0]);
	if (!as_store_to_file (store_new, file_new,
			       AS_NODE_TO_XML_FLAG_ADD_HEADER |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
			       NULL, error))
		return FALSE;
	return TRUE;
}

static gboolean
as_util_split_appstream (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps;
	const gchar *destdir;
	const gchar *id;
	guint i;
	g_autoptr(GFile) file = NULL;
	g_autoptr(AsStore) store = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: appstream.xml");
		return FALSE;
	}

	/* load store */
	file = g_file_new_for_path (values[0]);
	store = as_store_new ();
	if (!as_store_from_file (store, file, NULL, NULL, error))
		return FALSE;

	/* support building in rpmbuild */
	destdir = g_getenv ("DESTDIR");
	if (destdir == NULL)
		destdir = "/";

	/* save each file */
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		g_autofree gchar *fn = NULL;
		g_autofree gchar *path = NULL;
		g_autoptr(GFile) file_app = NULL;

		/* use AppData for desktop files, metainfo otherwise */
		app = g_ptr_array_index (apps, i);
		id = as_app_get_id (app);
		if (as_app_get_kind (app) == AS_APP_KIND_DESKTOP) {
			fn = g_strdup_printf ("%s.appdata.xml", id);
		} else {
			fn = g_strdup_printf ("%s.metainfo.xml", id);
		}

		/* save to a file */
		path = g_build_filename (destdir, "usr", "share", "metainfo", fn, NULL);
		g_debug ("saving %s as %s", id, path);
		file_app = g_file_new_for_path (path);
		if (!as_app_to_file (app, file_app, NULL, error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
as_util_modify (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsNode *node_app;
	AsNode *node_val;
	g_autoptr(GFile) file = NULL;
	g_autoptr(AsNode) root = NULL;

	/* check args */
	if (g_strv_length (values) != 3) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected FILENAME KEY VALUE");
		return FALSE;
	}

	/* load file */
	file = g_file_new_for_path (values[0]);
	root = as_node_from_file (file,
				  AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS |
				  AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				  NULL,
				  error);
	if (root == NULL)
		return FALSE;

	/* get application node */
	node_app = as_node_find (root, "component");
	if (node_app == NULL)
		node_app = as_node_find (root, "application");
	if (node_app == NULL) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "invalid AppData file");
		return FALSE;
	}

	/* find a key with this exact name */
	node_val = as_node_find (node_app, values[1]);
	if (node_val != NULL) {
		as_node_set_data (node_val, values[2], AS_NODE_INSERT_FLAG_NONE);
	} else {
		AsNode *n;
		n = as_node_insert (node_app,
				    values[1], values[2],
				    AS_NODE_INSERT_FLAG_NONE,
				    NULL);

		/* special case some tags with default values */
		if (g_strcmp0 (values[1], "translation") == 0)
			as_node_add_attribute (n, "type", "gettext");
	}

	/* save to file */
	return as_node_to_file (root, file,
				AS_NODE_TO_XML_FLAG_ADD_HEADER |
				AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
				NULL, error);
}

static gboolean
as_util_generate_guid (AsUtilPrivate *priv, gchar **values, GError **error)
{
	g_autofree gchar *guid = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected STRING");
		return FALSE;
	}
	guid = as_utils_guid_from_string (values[0]);
	g_print ("%s\n", guid);
	return TRUE;
}

static gboolean
as_util_compare (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps;
	const gchar *id;
	const guint align = 50;
	guint i;
	g_autoptr(GFile) file_new = NULL;
	g_autoptr(GFile) file_old= NULL;
	g_autoptr(AsStore) store_new = NULL;
	g_autoptr(AsStore) store_old = NULL;

	/* check args */
	if (g_strv_length (values) != 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: old.xml new.xml");
		return FALSE;
	}

	/* load old data */
	file_old = g_file_new_for_path (values[0]);
	store_old = as_store_new ();
	if (!as_store_from_file (store_old, file_old, NULL, NULL, error))
		return FALSE;

	/* load new data */
	file_new = g_file_new_for_path (values[1]);
	store_new = as_store_new ();
	if (!as_store_from_file (store_new, file_new, NULL, NULL, error))
		return FALSE;

	/* find apps in old that are not in new */
	apps = as_store_get_apps (store_old);
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) == AS_APP_KIND_WEB_APP)
			continue;
		id = as_app_get_id (app);
		if (as_store_get_app_by_id_with_fallbacks (store_new, id) != NULL)
			continue;
		/* TRANSLATORS: application was removed */
		as_util_pad_strings (id, _("Removed"), align);
	}

	/* find apps in new that are not in old */
	apps = as_store_get_apps (store_new);
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_kind (app) == AS_APP_KIND_WEB_APP)
			continue;
		id = as_app_get_id (app);
		if (as_store_get_app_by_id_with_fallbacks (store_old, id) != NULL)
			continue;
		/* TRANSLATORS: application was added */
		as_util_pad_strings (id, _("Added"), align);
	}

	return TRUE;
}

static gboolean
as_util_incorporate (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	AsApp *app_source;
	GPtrArray *apps;
	const gchar *id;
	const guint align = 50;
	guint i;
	guint j;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(AsStore) helper = NULL;
	g_autoptr(GFile) file_new = NULL;
	g_autoptr(GFile) file_old= NULL;
	g_autoptr(GFile) file_helper = NULL;

	/* check args */
	if (g_strv_length (values) < 3) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Not enough arguments, expected: old.xml helper.xml new.xml");
		return FALSE;
	}

	/* load old data */
	file_old = g_file_new_for_path (values[0]);
	store = as_store_new ();
	if (!as_store_from_file (store, file_old, NULL, NULL, error))
		return FALSE;

	/* load as_util_helper data */
	file_helper = g_file_new_for_path (values[1]);
	helper = as_store_new ();
	if (!as_store_from_file (helper, file_helper, NULL, NULL, error))
		return FALSE;

	/* try to incorporate apps using the application ID */
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		id = as_app_get_id (app);
		if (as_app_get_description_size (app) > 0) {
			as_util_pad_strings (id, "Already has AppData", align);
			continue;
		}
		app_source = as_store_get_app_by_id_with_fallbacks (helper, id);
		if (app_source == NULL) {
			as_util_pad_strings (id, "Not found", align);
			continue;
		}
		if (as_app_get_description_size (app_source) == 0) {
			as_util_pad_strings (id, "No source AppData", align);
			continue;
		}
		as_util_pad_strings (id, "Incorporating...", align);
		as_app_subsume_full (app, app_source,
				     AS_APP_SUBSUME_FLAG_NO_OVERWRITE);

		/* good enough for us now */
		as_app_remove_veto (app, "Required AppData");
	}

	/* try to incorporate apps using the package name */
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		GPtrArray *pkgnames;
		g_auto(GStrv) tmp = NULL;

		app = g_ptr_array_index (apps, i);
		id = as_app_get_id (app);
		if (as_app_get_description_size (app) > 0) {
			as_util_pad_strings (id, "Already has AppData", align);
			continue;
		}
		pkgnames = as_app_get_pkgnames (app);
		if (pkgnames->len == 0)
			continue;

		/* copy to a GStrv */
		tmp = g_new0 (gchar *, pkgnames->len + 1);
		for (j = 0; j < pkgnames->len; j++)
			tmp[j] = g_strdup (g_ptr_array_index (pkgnames, j));
		app_source = as_store_get_app_by_pkgnames (helper, tmp);
		if (app_source == NULL) {
			as_util_pad_strings (id, "Not found", align);
			continue;
		}
		if (as_app_get_description_size (app_source) == 0) {
			as_util_pad_strings (id, "No source AppData", align);
			continue;
		}
		as_util_pad_strings (id, "Incorporating...", align);
		as_app_subsume_full (app, app_source,
				     AS_APP_SUBSUME_FLAG_NO_OVERWRITE);

		/* good enough for us now */
		as_app_remove_veto (app, "Required AppData");
	}

	/* save new store */
	file_new = g_file_new_for_path (values[2]);
	if (!as_store_to_file (store, file_new,
			       AS_NODE_TO_XML_FLAG_ADD_HEADER |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
			       NULL, error))
		return FALSE;
	return TRUE;
}

/**
 * as_util_check_root:
 *
 * What kind of errors this will detect:
 *
 * - No applications found
 * if not application in blacklist:
 *   - Application icon too small
 *   - Application icon not present
 *   - Application has no comment
 **/
static gboolean
as_util_check_root (AsUtilPrivate *priv, gchar **values, GError **error)
{
	GPtrArray *apps;
	const gchar *tmp;
	guint i;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GPtrArray) problems = NULL;

	/* check args */
	if (g_strv_length (values) != 0) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "No arguments expected");
		return FALSE;
	}

	/* load root */
	store = as_store_new ();
	as_store_set_add_flags (store, AS_STORE_ADD_FLAG_PREFER_LOCAL);
	as_store_set_destdir (store, g_getenv ("DESTDIR"));
	if (!as_store_load (store,
			    AS_STORE_LOAD_FLAG_DESKTOP |
			    AS_STORE_LOAD_FLAG_APPDATA |
			    AS_STORE_LOAD_FLAG_ALLOW_VETO,
			    NULL,
			    error))
		return FALSE;

	/* no apps found */
	if (as_store_get_size (store) == 0) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "No applications found");
		return FALSE;
	}

	/* sanity check each */
	problems = g_ptr_array_new_with_free_func (g_free);
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		AsApp *app;
		app = g_ptr_array_index (apps, i);
		as_util_check_root_app (app, problems);
	}

	/* show problems */
	if (problems->len) {
		for (i = 0; i < problems->len; i++) {
			tmp = g_ptr_array_index (problems, i);
			g_printerr ("• %s\n", tmp);
		}
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Failed to check root, %u problems detected",
			     problems->len);
		return FALSE;
	}

	return TRUE;
}

static gboolean
as_util_markup_import (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsMarkupConvertFormat format;
	guint i;
	g_autofree gchar *data = NULL;
	g_autofree gchar *tmp = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "expected type filename");
		return FALSE;
	}

	/* get type */
	if (g_strcmp0 (values[0], "simple") == 0) {
		format = AS_MARKUP_CONVERT_FORMAT_SIMPLE;
	} else if (g_strcmp0 (values[0], "html") == 0) {
		format = AS_MARKUP_CONVERT_FORMAT_HTML;
	} else {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_INVALID_ARGUMENTS,
			     "invalid type %s",
			     values[0]);
		return FALSE;
	}

	/* read and convert */
	for (i = 1; values[i] != NULL; i++) {
		if (!g_file_get_contents (values[i], &data, NULL, error))
			return FALSE;
		tmp = as_markup_import (data, format, error);
		if (tmp == NULL) {
			g_prefix_error (error, "Failed to parse %s: ", values[i]);
			return FALSE;
		}
		g_print ("%s\n", tmp);
	}
	return TRUE;
}

static gboolean
as_util_vercmp (AsUtilPrivate *priv, gchar **values, GError **error)
{
	gint rc;

	/* check args */
	if (g_strv_length (values) != 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "expected VERSION1 VERSION2");
		return FALSE;
	}

	/* compare */
	rc = as_utils_vercmp (values[0], values[1]);
	if (rc == G_MAXINT) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "failed to compare version numbers");
		return FALSE;
	}

	/* print results */
	if (rc == 0)
		g_print ("%s = %s\n", values[0], values[1]);
	else if (rc < 0)
		g_print ("%s < %s\n", values[0], values[1]);
	else if (rc > 0)
		g_print ("%s > %s\n", values[0], values[1]);
	return TRUE;
}

static void
as_util_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		   const gchar *message, gpointer user_data)
{
}

static void
as_util_watch_cancelled_cb (GCancellable *cancellable, gpointer user_data)
{
	AsUtilPrivate *priv = (AsUtilPrivate *) user_data;
	/* TRANSLATORS: this is when a device ctrl+c's a watch */
	g_print ("%s\n", _("Cancelled"));
	g_main_loop_quit (priv->loop);
}

static gboolean
as_util_sigint_cb (gpointer user_data)
{
	AsUtilPrivate *priv = (AsUtilPrivate *) user_data;
	g_debug ("Handling SIGINT");
	g_cancellable_cancel (priv->cancellable);
	return FALSE;
}

static gboolean
as_util_regex (AsUtilPrivate *priv, gchar **values, GError **error)
{
	/* check args */
	if (g_strv_length (values) != 2) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "expected PATTERN STRING");
		return FALSE;
	}

	/* test */
	if (!g_regex_match_simple (values[0], values[1], 0, 0)) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "Failed to match");
		return FALSE;
	}
	return TRUE;
}

int
main (int argc, char *argv[])
{
	AsProfileTask *ptask;
	AsUtilPrivate *priv = NULL;
	gboolean ret;
	gboolean enable_profiling = FALSE;
	gboolean nonet = FALSE;
	gboolean verbose = FALSE;
	gboolean version = FALSE;
	GError *error = NULL;
	gint retval = 1;
	g_autofree gchar *cmd_descriptions = NULL;
	const GOptionEntry options[] = {
		{ "nonet", '\0', 0, G_OPTION_ARG_NONE, &nonet,
			/* TRANSLATORS: this is the --nonet argument */
			_("Do not use network access"), NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
			/* TRANSLATORS: command line option */
			_("Show version"), NULL },
		{ "profile", '\0', 0, G_OPTION_ARG_NONE, &enable_profiling,
			/* TRANSLATORS: command line option */
			_("Enable profiling"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* create helper object */
	priv = g_new0 (AsUtilPrivate, 1);
	priv->profile = as_profile_new ();

	/* do stuff on ctrl+c */
	priv->loop = g_main_loop_new (NULL, FALSE);
	priv->cancellable = g_cancellable_new ();
	g_unix_signal_add_full (G_PRIORITY_DEFAULT,
				SIGINT,
				as_util_sigint_cb,
				priv,
				NULL);
	g_signal_connect (priv->cancellable, "cancelled",
			  G_CALLBACK (as_util_watch_cancelled_cb), priv);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) as_util_item_free);
	as_util_add (priv->cmd_array,
		     "convert",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Converts AppStream metadata from one version to another"),
		     as_util_convert);
	as_util_add (priv->cmd_array,
		     "upgrade",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Upgrade AppData metadata to the latest version"),
		     as_util_upgrade);
	as_util_add (priv->cmd_array,
		     "appdata-from-desktop",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Creates an example Appdata file from a .desktop file"),
		     as_util_appdata_from_desktop);
	as_util_add (priv->cmd_array,
		     "dump",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Dumps the applications in the AppStream metadata"),
		     as_util_dump);
	as_util_add (priv->cmd_array,
		     "search",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Search for AppStream applications"),
		     as_util_search);
	as_util_add (priv->cmd_array,
		     "search-pkgname",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Search for AppStream applications by package name"),
		     as_util_search_pkgname);
	as_util_add (priv->cmd_array,
		     "query-installed",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Show all installed AppStream applications"),
		     as_util_query_installed);
	as_util_add (priv->cmd_array,
		     "search-category",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Search for AppStream applications by category name"),
		     as_util_search_category);
	as_util_add (priv->cmd_array,
		     "show-search-tokens",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Display application search tokens"),
		     as_util_show_search_tokens);
	as_util_add (priv->cmd_array,
		     "install",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Installs AppStream metadata"),
		     as_util_install);
	as_util_add (priv->cmd_array,
		     "install-origin",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Installs AppStream metadata with new origin"),
		     as_util_install_origin);
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
		     "status-csv",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Create an CSV status document"),
		     as_util_status_csv);
	as_util_add (priv->cmd_array,
		     "matrix-html",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Create an HTML matrix page"),
		     as_util_matrix_html);
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
		     "agreement-export",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Exports the agreement to text"),
		     as_util_agreement_export);
	as_util_add (priv->cmd_array,
		     "validate-strict",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Validate an AppData or AppStream file (strict)"),
		     as_util_validate_strict);
	as_util_add (priv->cmd_array,
		     "appdata-to-news",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Convert an AppData file to NEWS format"),
		     as_util_appdata_to_news);
	as_util_add (priv->cmd_array,
		     "news-to-appdata",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Convert an NEWS file to AppData format"),
		     as_util_news_to_appdata);
	as_util_add (priv->cmd_array,
		     "check-root",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Check installed application data"),
		     as_util_check_root);
	as_util_add (priv->cmd_array,
		     "check-component",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("check an installed application"),
		     as_util_check_component);
	as_util_add (priv->cmd_array,
		     "replace-screenshots",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Replace screenshots in source file"),
		     as_util_replace_screenshots);
	as_util_add (priv->cmd_array,
		     "add-provide",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Add a provide to a source file"),
		     as_util_add_provide);
	as_util_add (priv->cmd_array,
		     "add-language",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Add a language to a source file"),
		     as_util_add_language);
	as_util_add (priv->cmd_array,
		     "mirror-screenshots",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Mirror upstream screenshots"),
		     as_util_mirror_screenshots);
	as_util_add (priv->cmd_array,
		     "mirror-local-firmware",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Mirror local firmware files"),
		     as_util_mirror_local_firmware);
	as_util_add (priv->cmd_array,
		     "incorporate",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Incorporate extra metadata from an external file"),
		     as_util_incorporate);
	as_util_add (priv->cmd_array,
		     "compare",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Compare the contents of two AppStream files"),
		     as_util_compare);
	as_util_add (priv->cmd_array,
		     "generate-guid",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Generate a GUID from an input string"),
		     as_util_generate_guid);
	as_util_add (priv->cmd_array,
		     "modify",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Modify an AppData file"),
		     as_util_modify);
	as_util_add (priv->cmd_array,
		     "split-appstream",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Split an AppStream file to AppData and Metainfo files"),
		     as_util_split_appstream);
	as_util_add (priv->cmd_array,
		     "merge-appstream",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Merge several files to an AppStream file"),
		     as_util_merge_appstream);
	as_util_add (priv->cmd_array,
		     "markup-import",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Import a file to AppStream markup"),
		     as_util_markup_import);
	as_util_add (priv->cmd_array,
		     "watch",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Watch AppStream locations for changes"),
		     as_util_watch);
	as_util_add (priv->cmd_array,
		     "vercmp",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Compare version numbers"),
		     as_util_vercmp);
	as_util_add (priv->cmd_array,
		     "regex",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Test a regular expression"),
		     as_util_regex);

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
	priv->nonet = nonet;

	/* set verbose? */
	if (verbose) {
		priv->verbose = TRUE;
		g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
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
	ptask = as_profile_start (priv->profile, "%s: %s", argv[0], argv[1]);
	ret = as_util_run (priv, argv[1], (gchar**) &argv[2], &error);
	as_profile_task_free (ptask);
	if (!ret) {
		if (g_error_matches (error, AS_ERROR, AS_ERROR_NO_SUCH_CMD)) {
			gchar *tmp;
			tmp = g_option_context_get_help (priv->context, TRUE, NULL);
			g_printerr ("%s", tmp);
			g_free (tmp);
		} else {
			g_printerr ("%s\n", error->message);
		}
		g_error_free (error);
		goto out;
	}

	/* profile */
	if (enable_profiling)
		as_profile_dump (priv->profile);

	/* success */
	retval = 0;
out:
	if (priv != NULL) {
		if (priv->cmd_array != NULL)
			g_ptr_array_unref (priv->cmd_array);
		g_object_unref (priv->profile);
		g_object_unref (priv->cancellable);
		g_main_loop_unref (priv->loop);
		g_option_context_free (priv->context);
		g_free (priv);
	}
	return retval;
}
