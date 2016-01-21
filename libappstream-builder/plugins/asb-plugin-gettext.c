/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "gettext";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/locale/*/LC_MESSAGES/*.mo");
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
	g_autofree gchar *root = NULL;
	g_auto(GStrv) intl_domains = NULL;

	/* search for .mo files in the prefix */
	root = g_build_filename (tmpdir, "/usr/share/locale", NULL);
	if (!g_file_test (root, G_FILE_TEST_EXISTS)) {
		g_free (root);
		root = g_build_filename (tmpdir, "/files/share/locale", NULL);
	}
	if (!g_file_test (root, G_FILE_TEST_EXISTS))
		return TRUE;

	/* generate */
	intl_domains = g_strsplit (asb_package_get_name (pkg), ",", -1);
	return as_app_gettext_search_path (AS_APP (app),
					   root,
					   intl_domains,
					   25,
					   NULL,
					   error);
}
