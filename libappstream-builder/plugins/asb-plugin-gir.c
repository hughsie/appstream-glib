/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>
#include <fnmatch.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "gir";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/*/*.gir");
}

/**
 * _asb_plugin_check_filename:
 */
static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (asb_plugin_match_glob ("/usr/share/*/*.gir", filename))
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_process_gir:
 */
static gboolean
asb_plugin_process_gir (AsbApp *app,
			const gchar *tmpdir,
			const gchar *filename,
			GError **error)
{
	GNode *l;
	GNode *node = NULL;
	const gchar *name;
	const gchar *version;
	gboolean ret = TRUE;
	g_autofree gchar *filename_full = NULL;
	g_autoptr(GFile) file = NULL;

	/* load file */
	filename_full = g_build_filename (tmpdir, filename, NULL);
	file = g_file_new_for_path (filename_full);
	node = as_node_from_file (file, AS_NODE_FROM_XML_FLAG_NONE, NULL, error);
	if (node == NULL) {
		ret = FALSE;
		goto out;
	}

	/* look for includes */
	l = as_node_find (node, "repository");
	if (l == NULL)
		goto out;
	for (l = l->children; l != NULL; l = l->next) {
		if (g_strcmp0 (as_node_get_name (l), "include") != 0)
			continue;
		name = as_node_get_attribute (l, "name");
		version = as_node_get_attribute (l, "version");
		if (g_strcmp0 (name, "Gtk") == 0 &&
		    g_strcmp0 (version, "3.0") == 0) {
			asb_package_log (asb_app_get_package (app),
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "Auto-adding kudo ModernToolkit for %s",
					 as_app_get_id (AS_APP (app)));
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_MODERN_TOOLKIT);
		}
	}
out:
	if (node != NULL)
		as_node_unref (node);
	return ret;
}

/**
 * asb_plugin_process_app:
 */
gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	gchar **filelist;
	guint i;

	/* already set */
	if (as_app_has_kudo_kind (AS_APP (app), AS_KUDO_KIND_MODERN_TOOLKIT))
		return TRUE;

	/* look for any GIR files */
	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (!_asb_plugin_check_filename (filelist[i]))
			continue;
		if (!asb_plugin_process_gir (app, tmpdir, filelist[i], error))
			return FALSE;
	}
	return TRUE;
}
