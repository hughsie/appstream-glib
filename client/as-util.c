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

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-app-private.h>

#include "as-cleanup.h"

#define AS_ERROR			1
#define AS_ERROR_INVALID_ARGUMENTS	0
#define AS_ERROR_NO_SUCH_CMD		1
#define AS_ERROR_FAILED			2

typedef struct {
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	gboolean		 nonet;
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
 * as_util_convert_appdata:
 **/
static gboolean
as_util_convert_appdata (GFile *file_input,
			 GFile *file_output,
			 gdouble new_version,
			 GError **error)
{
	AsNodeInsertFlags flags_translate = AS_NODE_INSERT_FLAG_NONE;
	GNode *n;
	GNode *n2;
	GNode *n3;
	const gchar *tmp;
	const gchar *project_group = NULL;
	gboolean action_required = FALSE;
	_cleanup_node_unref_ GNode *root = NULL;

	/* load to GNode */
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
				as_node_add_attribute (n, "type", tmp, -1);
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
		n2 = as_node_insert (n, "metadata_license", "FIXME: Insert SPDX ID Here",
				     AS_NODE_INSERT_FLAG_NONE, NULL);
	} else {
		tmp = as_node_get_data (n2);

		/* convert the old license defines */
		if (g_strcmp0 (tmp, "CC0") == 0)
			as_node_set_data (n2, "CC0-1.0", -1,
					  AS_NODE_INSERT_FLAG_NONE);
		else if (g_strcmp0 (tmp, "CC-BY") == 0)
			as_node_set_data (n2, "CC-BY-3.0", -1,
					  AS_NODE_INSERT_FLAG_NONE);
		else if (g_strcmp0 (tmp, "CC-BY-SA") == 0)
			as_node_set_data (n2, "CC-BY-SA-3.0", -1,
					  AS_NODE_INSERT_FLAG_NONE);
		else if (g_strcmp0 (tmp, "GFDL") == 0)
			as_node_set_data (n2, "GFDL-1.3", -1,
					  AS_NODE_INSERT_FLAG_NONE);

		/* ensure in SPDX format */
		if (!as_utils_is_spdx_license_id (as_node_get_data (n2))) {
			action_required = TRUE;
			as_node_set_comment (n2, "FIXME: convert to an SPDX ID", -1);
		}
	}

	/* ensure this exists */
	n2 = as_node_find (n, "project_license");
	if (n2 == NULL) {
		action_required = TRUE;
		n2 = as_node_insert (n, "project_license", "FIXME: Insert SPDX ID Here",
				     AS_NODE_INSERT_FLAG_NONE, NULL);
	} else {
		/* ensure in SPDX format */
		if (!as_utils_is_spdx_license_id (as_node_get_data (n2))) {
			action_required = TRUE;
			as_node_set_comment (n2, "FIXME: convert to an SPDX ID", -1);
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
	if (n2 == NULL) {
		n3 = as_node_find (n, "project_group");
		if (n3 != NULL) {
			if (g_strcmp0 (as_node_get_data (n3), "GNOME") == 0) {
				as_node_insert (n, "developer_name", "The GNOME Project",
						flags_translate, NULL);
			} else if (g_strcmp0 (as_node_get_data (n3), "KDE") == 0) {
				as_node_insert (n, "developer_name", "KDE e.V.",
						AS_NODE_INSERT_FLAG_NONE, NULL);
			} else if (g_strcmp0 (as_node_get_data (n3), "XFCE") == 0) {
				as_node_insert (n, "developer_name", "Xfce Development Team",
						flags_translate, NULL);
			} else if (g_strcmp0 (as_node_get_data (n3), "MATE") == 0) {
				as_node_insert (n, "developer_name", "The MATE Project",
						flags_translate, NULL);
			} else {
				action_required = TRUE;
				n3 = as_node_insert (n, "developer_name", "FIXME: Company Name",
						     AS_NODE_INSERT_FLAG_NONE, NULL);
				as_node_set_comment (n3, "FIXME: compulsory_for_desktop was not recognised", -1);
			}
		} else {
			action_required = TRUE;
			n3 = as_node_insert (n, "developer_name", "FIXME: Company Name",
					     AS_NODE_INSERT_FLAG_NONE, NULL);
			as_node_set_comment (n3, "FIXME: You can use a project or "
					     "developer name if there's no company", -1);
		}
	}

	n3 = as_node_find (n, "project_group");
	if (n3 != NULL)
		project_group = as_node_get_data (n3);

	/* ensure each URL type exists */
	if (as_node_find_with_attribute (n, "url", "type", "homepage") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "FIXME: https://wiki.gnome.org/Apps/FIXME",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "homepage", NULL);
		} else {
			n3 = as_node_insert (n, "url", "FIXME: http://www.homepage.com/",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "homepage", NULL);
			as_node_set_comment (n3, "FIXME: homepage for the application", -1);
		}
	}
	if (as_node_find_with_attribute (n, "url", "type", "bugtracker") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "FIXME: https://bugzilla.gnome.org/enter_bug.cgi?product=FIXME",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "bugtracker", NULL);
		} else if (g_strcmp0 (project_group, "KDE") == 0) {
			n3 = as_node_insert (n, "url", "FIXME: https://bugs.kde.org/enter_bug.cgi?format=guided",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "bugtracker", NULL);
		} else if (g_strcmp0 (project_group, "XFCE") == 0) {
			n3 = as_node_insert (n, "url", "FIXME: https://bugzilla.xfce.org/enter_bug.cgi",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "bugtracker", NULL);
		} else {
			n3 = as_node_insert (n, "url", "FIXME: http://www.homepage.com/where-to-report_bug.html",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "bugtracker", NULL);
			as_node_set_comment (n3, "FIXME: where to report bugs for "
					     "the application, or empty for none", -1);
		}
	}
	if (as_node_find_with_attribute (n, "url", "type", "donation") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "http://www.gnome.org/friends/",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "donation", NULL);
			as_node_set_comment (n3, "GNOME Projects usually have no per-app donation page", -1);
		} else {
			n3 = as_node_insert (n, "url", "FIXME: http://www.homepage.com/donation.html",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "donation", NULL);
			as_node_set_comment (n3, "FIXME: where to donate to the application", -1);
		}
	}
	if (as_node_find_with_attribute (n, "url", "type", "help") == NULL) {
		if (g_strcmp0 (project_group, "GNOME") == 0) {
			n3 = as_node_insert (n, "url", "FIXME: https://help.gnome.org/users/FIXME/stable/",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "help", NULL);
			as_node_set_comment (n3, "FIXME: where on the internet users "
					     "can find help or empty for none", -1);
		} else {
			n3 = as_node_insert (n, "url", "FIXME: http://www.homepage.com/docs/",
					     AS_NODE_INSERT_FLAG_NONE,
					     "type", "help", NULL);
			as_node_set_comment (n3, "FIXME: where to report bugs for "
					     "the application, or empty for none", -1);
		}
	}

	/* add <updatecontact> */
	n2 = as_node_find (n, "updatecontact");
	if (n2 == NULL) {
		action_required = TRUE;
		n3 = as_node_insert (n, "updatecontact", "FIXME: upstream_contact_at_email.com",
				     AS_NODE_INSERT_FLAG_NONE, NULL);
		as_node_set_comment (n3, "FIXME: this is optional, but recommended", -1);
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
			as_node_set_data (n3, tmp, -1, AS_NODE_INSERT_FLAG_NONE);
			as_node_set_data (n2, NULL, -1, AS_NODE_INSERT_FLAG_NONE);
			action_required = TRUE;
			as_node_insert (n2, "caption", "FIXME: Describe this "
					"screenshot in less than ~10 words",
					flags_translate, NULL);
		}
	}

	/* save to file */
	if (!as_node_to_file (root, file_output,
			      AS_NODE_TO_XML_FLAG_ADD_HEADER |
			      AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			      AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
			      NULL, error))
		return FALSE;

	/* TRANSLATORS: any manual changes required? */
	if (action_required) {
		g_print (_("Please review the file and fix any '%s' items"), "FIXME");
		g_print ("\n");
	}

	return TRUE;
}

/**
 * as_util_convert_appstream:
 **/
static gboolean
as_util_convert_appstream (GFile *file_input,
			   GFile *file_output,
			   gdouble new_version,
			   GError **error)
{
	_cleanup_object_unref_ AsStore *store = NULL;

	store = as_store_new ();
	if (!as_store_from_file (store, file_input, NULL, NULL, error))
		return FALSE;
	/* TRANSLATORS: information message */
	g_print ("Old API version: %.2f\n", as_store_get_api_version (store));

	/* save file */
	as_store_set_api_version (store, new_version);
	if (!as_store_to_file (store, file_output,
				AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
				AS_NODE_TO_XML_FLAG_ADD_HEADER,
				NULL, error))
		return FALSE;
	/* TRANSLATORS: information message */
	g_print (_("New API version: %.2f\n"), as_store_get_api_version (store));
	return TRUE;
}

/**
 * as_util_convert:
 **/
static gboolean
as_util_convert (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsAppSourceKind input_kind;
	AsAppSourceKind output_kind;
	gdouble new_version;
	_cleanup_object_unref_ GFile *file_input = NULL;
	_cleanup_object_unref_ GFile *file_output = NULL;

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
	input_kind = as_app_guess_source_kind (values[0]);
	output_kind = as_app_guess_source_kind (values[1]);
	file_input = g_file_new_for_path (values[0]);
	file_output = g_file_new_for_path (values[1]);
	new_version = g_ascii_strtod (values[2], NULL);

	/* AppData -> AppData */
	if (input_kind == AS_APP_SOURCE_KIND_APPDATA &&
	    output_kind == AS_APP_SOURCE_KIND_APPDATA) {
		return as_util_convert_appdata (file_input,
						file_output,
						new_version,
						error);
	}

	/* AppStream -> AppStream */
	if (input_kind == AS_APP_SOURCE_KIND_APPSTREAM &&
	    output_kind == AS_APP_SOURCE_KIND_APPSTREAM) {
		return as_util_convert_appstream (file_input,
						  file_output,
						  new_version,
						  error);
	}

	/* don't know what to do */
	g_set_error_literal (error,
			     AS_ERROR,
			     AS_ERROR_INVALID_ARGUMENTS,
			     "Format not recognised");
	return FALSE;
}

/**
 * as_util_upgrade:
 **/
static gboolean
as_util_upgrade (AsUtilPrivate *priv, gchar **values, GError **error)
{
	/* check args */
	if (g_strv_length (values) != 1) {
		/* TRANSLATORS: error message */
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     _("Not enough arguments, "
				     "expected file.xml"));
		return FALSE;
	}

	/* AppData */
	if (as_app_guess_source_kind (values[0]) == AS_APP_SOURCE_KIND_APPDATA) {
		_cleanup_object_unref_ GFile *file = NULL;
		file = g_file_new_for_path (values[0]);
		return as_util_convert_appdata (file, file, 1.0, error);
	}

	/* don't know what to do */
	g_set_error_literal (error,
			     AS_ERROR,
			     AS_ERROR_INVALID_ARGUMENTS,
			     "Format not recognised");
	return FALSE;
}

/**
 * as_util_appdata_from_desktop:
 **/
static gboolean
as_util_appdata_from_desktop (AsUtilPrivate *priv, gchar **values, GError **error)
{
	gchar *instr = NULL;
	_cleanup_free_ gchar *id_new = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_object_unref_ AsImage *im1 = NULL;
	_cleanup_object_unref_ AsImage *im2 = NULL;
	_cleanup_object_unref_ AsScreenshot *ss1 = NULL;
	_cleanup_object_unref_ AsScreenshot *ss2 = NULL;
	_cleanup_object_unref_ GFile *file = NULL;

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
	if (as_app_guess_source_kind (values[0]) != AS_APP_SOURCE_KIND_DESKTOP ||
	    as_app_guess_source_kind (values[1]) != AS_APP_SOURCE_KIND_APPDATA) {
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Format not recognised");
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
				"  <p>\n   This should a second paragraph.\n  </p>\n", -1);
	as_app_set_developer_name (app, NULL, "XXX: Insert Company or Developer Name", -1);
	as_app_set_project_group (app, "XXX: Values valid are none, GNOME, KDE or XFCE", -1);

	/* fix the ID */
	id_new = g_strdup (as_app_get_id (app));
	instr = g_strstr_len (id_new, -1, ".desktop.in");
	if (instr != NULL) {
		instr[8] = '\0';
		as_app_set_id (app, id_new, -1);
	}

	/* set things that don't belong in the AppData file */
	as_app_set_icon (app, NULL, -1);
	g_ptr_array_set_size (as_app_get_keywords (app, NULL), 0);
	g_ptr_array_set_size (as_app_get_categories (app), 0);
	g_ptr_array_set_size (as_app_get_mimetypes (app), 0);

	/* add urls */
	as_app_add_url (app, AS_URL_KIND_HOMEPAGE,
			"XXX: http://www.homepage.com/", -1);
	as_app_add_url (app, AS_URL_KIND_BUGTRACKER,
			"XXX: http://www.homepage.com/where-to-report_bug.html", -1);
	as_app_add_url (app, AS_URL_KIND_FAQ,
			"XXX: http://www.homepage.com/faq.html", -1);
	as_app_add_url (app, AS_URL_KIND_DONATION,
			"XXX: http://www.homepage.com/donation.html", -1);
	as_app_add_url (app, AS_URL_KIND_HELP,
			"XXX: http://www.homepage.com/docs/", -1);
	as_app_set_project_license (app, "XXX: Insert SPDX value here", -1);
	as_app_set_metadata_license (app, "XXX: Insert SPDX value here", -1);

	/* add first screenshot */
	ss1 = as_screenshot_new ();
	as_screenshot_set_kind (ss1, AS_SCREENSHOT_KIND_DEFAULT);
	as_screenshot_set_caption (ss1, NULL, "XXX: Describe the default screenshot", -1);
	im1 = as_image_new ();
	as_image_set_url (im1, "XXX: http://www.my-screenshot-default.png", -1);
	as_image_set_width (im1, 1120);
	as_image_set_height (im1, 630);
	as_screenshot_add_image (ss1, im1);
	as_app_add_screenshot (app, ss1);

	/* add second screenshot */
	ss2 = as_screenshot_new ();
	as_screenshot_set_kind (ss2, AS_SCREENSHOT_KIND_NORMAL);
	as_screenshot_set_caption (ss2, NULL, "XXX: Describe another screenshot", -1);
	im2 = as_image_new ();
	as_image_set_url (im2, "XXX: http://www.my-screenshot.png", -1);
	as_image_set_width (im2, 1120);
	as_image_set_height (im2, 630);
	as_screenshot_add_image (ss2, im2);
	as_app_add_screenshot (app, ss2);

	/* save to disk */
	file = g_file_new_for_path (values[1]);
	return as_app_to_file (app, file, NULL, error);
}

/**
 * as_util_add_file_to_store:
 **/
static gboolean
as_util_add_file_to_store (AsStore *store, const gchar *filename, GError **error)
{
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_object_unref_ GFile *file_input = NULL;

	switch (as_app_guess_source_kind (filename)) {
	case AS_APP_SOURCE_KIND_APPDATA:
	case AS_APP_SOURCE_KIND_METAINFO:
	case AS_APP_SOURCE_KIND_DESKTOP:
		app = as_app_new ();
		if (!as_app_parse_file (app, filename,
					AS_APP_PARSE_FLAG_USE_HEURISTICS, error))
			return FALSE;
		as_store_add_app (store, app);
		break;
	case AS_APP_SOURCE_KIND_APPSTREAM:
		/* load file */
		file_input = g_file_new_for_path (filename);
		if (!as_store_from_file (store, file_input, NULL, NULL, error))
			return FALSE;
		break;
	default:
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_INVALID_ARGUMENTS,
				     "Format not recognised");
		return FALSE;
	}
	return TRUE;
}

/**
 * as_util_dump:
 **/
static gboolean
as_util_dump (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;
	_cleanup_string_free_ GString *xml = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;

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
as_util_install_xml (const gchar *filename,
		     const gchar *origin,
		     const gchar *dir,
		     GError **error)
{
	const gchar *destdir;
	gchar *tmp;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *path_dest = NULL;
	_cleanup_free_ gchar *path_parent = NULL;
	_cleanup_object_unref_ GFile *file_dest = NULL;
	_cleanup_object_unref_ GFile *file_src = NULL;

	/* create directory structure */
	destdir = g_getenv ("DESTDIR");
	path_parent = g_strdup_printf ("%s%s", destdir != NULL ? destdir : "", dir);
	if (g_mkdir_with_parents (path_parent, 0777) != 0) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "Failed to create %s", path_parent);
		return FALSE;
	}

	/* calculate the new destination */
	file_src = g_file_new_for_path (filename);
	basename = g_path_get_basename (filename);
	if (origin != NULL) {
		_cleanup_free_ gchar *basename_new = NULL;
		tmp = g_strstr_len (basename, -1, ".");
		if (tmp == NULL) {
			g_set_error (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "Name of XML file invalid %s",
				     basename);
			return FALSE;
		}
		basename_new = g_strdup_printf ("%s%s", origin, tmp);
		/* replace the fedora.xml.gz into %{origin}.xml.gz */
		path_dest = g_build_filename (path_parent, basename_new, NULL);
	} else {
		path_dest = g_build_filename (path_parent, basename, NULL);
	}

	/* actually copy file */
	file_dest = g_file_new_for_path (path_dest);
	if (!g_file_copy (file_src, file_dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error))
		return FALSE;

	/* fix the origin */
	if (origin != NULL) {
		_cleanup_object_unref_ AsStore *store = NULL;
		store = as_store_new ();
		if (!as_store_from_file (store, file_dest, NULL, NULL, error))
			return FALSE;
		as_store_set_origin (store, origin);
		if (!as_store_to_file (store, file_dest,
				       AS_NODE_TO_XML_FLAG_ADD_HEADER |
				       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
				       NULL, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_util_install_filename:
 **/
static gboolean
as_util_install_filename (const gchar *filename,
			  const gchar *origin,
			  GError **error)
{
	gboolean ret = FALSE;
	gchar *tmp;
	_cleanup_free_ gchar *basename = NULL;

	switch (as_app_guess_source_kind (filename)) {
	case AS_APP_SOURCE_KIND_APPSTREAM:
		if (g_strstr_len (filename, -1, ".yml.gz") != NULL) {
			ret = as_util_install_xml (filename, origin,
						   "/usr/share/app-info/yaml", error);
		} else {
			ret = as_util_install_xml (filename, origin,
						   "/usr/share/app-info/xmls", error);
		}
		break;
	case AS_APP_SOURCE_KIND_APPDATA:
	case AS_APP_SOURCE_KIND_METAINFO:
		ret = as_util_install_xml (filename, NULL,
					   "/usr/share/appdata", error);
		break;
	default:
		/* icons */
		if (origin != NULL) {
			ret = as_util_install_icons (filename, origin, error);
			break;
		}
		basename = g_path_get_basename (filename);
		tmp = g_strstr_len (basename, -1, "-icons.tar.gz");
		if (tmp != NULL) {
			*tmp = '\0';
			ret = as_util_install_icons (filename, basename, error);
			break;
		}

		/* unrecognised */
		g_set_error_literal (error,
				     AS_ERROR,
				     AS_ERROR_FAILED,
				     "No idea how to process files of this type");
		break;
	}
	return ret;
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
		if (!as_util_install_filename (values[i], NULL, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_util_install_origin:
 **/
static gboolean
as_util_install_origin (AsUtilPrivate *priv, gchar **values, GError **error)
{
	guint i;

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
	for (i = 1; values[i] != NULL; i++) {
		if (!as_util_install_filename (values[i], values[0], error))
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

typedef enum {
	AS_UTIL_DISTRO_UNKNOWN,
	AS_UTIL_DISTRO_FEDORA,
	AS_UTIL_DISTRO_LAST
} AsUtilDistro;

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
	_cleanup_string_free_ GString *classes = NULL;

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
				"Type", as_id_kind_to_string (as_app_get_id_kind (app)));

	/* extends */
	tmp = as_util_status_html_join (as_app_get_extends (app));
	if (tmp != NULL) {
		tmp2 = g_strrstr (tmp, ".desktop");
		if (tmp2 != NULL)
			*tmp2 = '\0';
		g_string_append_printf (html, "<tr><td class=\"alt\">%s</td>"
					"<td><a href=\"#%s\">%s</a></td></tr>\n",
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
	if (as_app_get_id_kind (app) == AS_ID_KIND_DESKTOP) {
		tmp = as_util_status_html_join (as_app_get_kudos (app));
		if (tmp != NULL) {
			g_string_append_printf (html, "<tr><td class=\"alt\">%s</td><td>%s</td></tr>\n",
						"Kudos", tmp);
		}
		g_free (tmp);
	}

	/* vetos */
	if (as_app_get_id_kind (app) == AS_ID_KIND_DESKTOP) {
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

	g_string_append (html, "<h1>Executive Summary</h1>\n");

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
	g_string_append (html, "<table class=\"summary\">\n");

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
	g_string_append_printf (html, "<tr><td class=\"alt\">Descriptions</td>"
				"<td>%i/%i</td>"
				"<td class=\"thin\">%.1f%%</td></tr>\n",
				cnt, total, perc);

	/* keywords */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) != AS_ID_KIND_DESKTOP)
			continue;
		if (as_app_get_keywords(app, NULL) != NULL)
			cnt++;
	}
	perc = 100.f * (gdouble) cnt / (gdouble) total;
	g_string_append_printf (html, "<tr><td class=\"alt\">Keywords</td>"
				"<td>%i/%i</td><td class=\"thin\">%.1f%%</td></tr>\n",
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
	g_string_append_printf (html, "<tr><td class=\"alt\">Screenshots</td>"
				"<td>%i/%i</td><td class=\"thin\">%.1f%%</td></tr>\n",
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
		g_string_append_printf (html, "<tr><td class=\"alt\">%s "
					"AppData</td><td>%i/%i</td>"
					"<td class=\"thin\">%.1f%%</td></tr>\n",
					project_groups[j], cnt,
					total, perc);
	}

	/* addons with MetaInfo */
	cnt = 0;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) == AS_ID_KIND_ADDON)
			cnt++;
	}
	g_string_append_printf (html, "<tr><td class=\"alt\">MetaInfo</td>"
				"<td>%i</td><td class=\"thin\"></td></tr>\n", cnt);


	g_string_append (html, "</table>\n");
	g_string_append (html, "<br/>\n");
	return TRUE;
}

/**
 * as_util_status_html_write_javascript:
 */
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
	"	var class_names = [ 'description', 'screenshots', 'keywords', 'gnome', 'kde', 'xfce' ];\n"
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

/**
 * as_util_status_html_write_css:
 */
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

/**
 * as_util_status_html_write_filter_section:
 */
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
	"<td colspan=\"3\"><button id=\"button-reset\" onclick=\"statusReset('description');\">Reset Filters</button></td>\n"
	"</tr>\n"
	"</table>\n");
}

/**
 * as_util_status_html:
 **/
static gboolean
as_util_status_html (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	AsUtilDistro distro = AS_UTIL_DISTRO_UNKNOWN;
	GPtrArray *apps = NULL;
	guint i;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_string_free_ GString *html = NULL;

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
		if (as_app_get_id_kind (app) == AS_ID_KIND_FONT)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_INPUT_METHOD)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_CODEC)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_SOURCE)
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

/**
 * as_util_status_csv:
 **/
static gboolean
as_util_status_csv (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsApp *app;
	GPtrArray *apps = NULL;
	guint i;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_string_free_ GString *data = NULL;

	/* check args */
	if (g_strv_length (values) != 2) {
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
		_cleanup_free_ gchar *description = NULL;
		app = g_ptr_array_index (apps, i);
		if (as_app_get_id_kind (app) == AS_ID_KIND_FONT)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_INPUT_METHOD)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_CODEC)
			continue;
		if (as_app_get_id_kind (app) == AS_ID_KIND_SOURCE)
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
		g_string_append_printf (data, "\"%s\",", description);
		g_string_append_printf (data, "\"%s\",", as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE));
		g_string_truncate (data, data->len - 1);
		g_string_append (data, "\n");
	}

	/* save file */
	if (!g_file_set_contents (values[1], data->str, -1, error))
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

/**
 * as_util_validate_output_text:
 **/
static void
as_util_validate_output_text (const gchar *filename, GPtrArray *probs)
{
	AsProblem *problem;
	const gchar *tmp;
	guint i;
	guint j;

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
			g_print (" : %s [ln:%i]\n",
				 as_problem_get_message (problem),
				 as_problem_get_line_number (problem));
		} else {
			g_print (" : %s\n", as_problem_get_message (problem));
		}
	}
}

/**
 * as_util_validate_output_html:
 **/
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
			_cleanup_free_ gchar *tmp = NULL;
			problem = g_ptr_array_index (probs, i);
			tmp = g_markup_escape_text (as_problem_get_message (problem), -1);
			g_print ("<li>");
			g_print ("%s\n", tmp);
			if (as_problem_get_line_number (problem) > 0) {
				g_print (" (line %i)",
					 as_problem_get_line_number (problem));
			}
			g_print ("</li>\n");
		}
		g_print ("</ul>\n");
	}
	g_print ("</body>\n");
	g_print ("</html>\n");
}

/**
 * as_util_validate_file:
 **/
static gboolean
as_util_validate_file (const gchar *filename,
		       AsAppValidateFlags flags,
		       GError **error)
{
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *probs = NULL;

	/* is AppStream */
	g_print ("%s: ", filename);
	if (as_app_guess_source_kind (filename) == AS_APP_SOURCE_KIND_APPSTREAM) {
		gboolean ret;
		_cleanup_object_unref_ AsStore *store;
		_cleanup_object_unref_ GFile *file;
		file = g_file_new_for_path (filename);
		store = as_store_new ();
		ret = as_store_from_file (store, file, NULL, NULL, error);
		if (!ret)
			return FALSE;
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
		/* TRANSLATORS: error message */
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
	AsAppValidateFlags flags = AS_APP_VALIDATE_FLAG_NONE;
	if (priv->nonet)
		flags |= AS_APP_VALIDATE_FLAG_NO_NETWORK;
	return as_util_validate_files (values, flags, error);
}

/**
 * as_util_validate_relax:
 **/
static gboolean
as_util_validate_relax (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsAppValidateFlags flags = AS_APP_VALIDATE_FLAG_RELAX;
	if (priv->nonet)
		flags |= AS_APP_VALIDATE_FLAG_NO_NETWORK;
	return as_util_validate_files (values, flags, error);
}

/**
 * as_util_validate_strict:
 **/
static gboolean
as_util_validate_strict (AsUtilPrivate *priv, gchar **values, GError **error)
{
	AsAppValidateFlags flags = AS_APP_VALIDATE_FLAG_STRICT;
	if (priv->nonet)
		flags |= AS_APP_VALIDATE_FLAG_NO_NETWORK;
	return as_util_validate_files (values, flags, error);
}

/**
 * as_util_check_root_app_icon:
 **/
static gboolean
as_util_check_root_app_icon (AsApp *app, GError **error)
{
	_cleanup_free_ gchar *icon = NULL;
	_cleanup_object_unref_ GdkPixbuf *pb = NULL;

	/* nothing found */
	if (as_app_get_icon (app) == NULL) {
		g_set_error (error,
			     AS_ERROR,
			     AS_ERROR_FAILED,
			     "%s has no Icon",
			     as_app_get_id (app));
		return FALSE;
	}

	/* is stock icon */
	if (as_utils_is_stock_icon_name (as_app_get_icon (app)))
		return TRUE;

	/* can we find it */
	icon = as_utils_find_icon_filename (g_getenv ("DESTDIR"),
					    as_app_get_icon (app),
					    error);
	if (icon == NULL) {
		g_prefix_error (error,
				"%s missing icon %s: ",
				as_app_get_id (app),
				as_app_get_icon (app));
		return FALSE;
	}

	/* can we can load it */
	pb = gdk_pixbuf_new_from_file (icon, error);
	if (pb == NULL) {
		g_prefix_error (error,
				"%s invalid icon %s: ",
				as_app_get_id (app),
				as_app_get_icon (app));
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

/**
 * as_util_check_root_app:
 **/
static void
as_util_check_root_app (AsApp *app, GPtrArray *problems)
{
	GError *error_local = NULL;

	/* skip */
	if (as_app_get_metadata_item (app, "NoDisplay") != NULL)
		return;
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_METAINFO)
		return;

	/* relax this for now */
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_DESKTOP)
		return;

	/* check one line summary */
	if (as_app_get_comment (app, NULL) == NULL) {
		g_ptr_array_add (problems,
				 g_strdup_printf ("%s has no Comment",
						  as_app_get_id (app)));
	}

	/* check icon exists and is large enough */
	if (!as_util_check_root_app_icon (app, &error_local)) {
		g_ptr_array_add (problems, g_strdup (error_local->message));
		g_clear_error (&error_local);
	}
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
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *problems = NULL;

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
			     "Failed to check root, %i problems detected",
			     problems->len);
		return FALSE;
	}

	return TRUE;
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
	gboolean nonet = FALSE;
	gboolean verbose = FALSE;
	gboolean version = FALSE;
	GError *error = NULL;
	guint retval = 1;
	_cleanup_free_ gchar *cmd_descriptions = NULL;
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
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

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
	as_util_add (priv->cmd_array,
		     "check-root",
		     NULL,
		     /* TRANSLATORS: command description */
		     _("Check installed application data"),
		     as_util_check_root);

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
			g_printerr ("%s", tmp);
			g_free (tmp);
		} else {
			g_printerr ("%s\n", error->message);
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
