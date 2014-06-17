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

/**
 * SECTION:asb-plugin
 * @short_description: Generic plugin helpers.
 * @stability: Unstable
 *
 * Utilities for plugins.
 */

#include "config.h"

#include <glib.h>

#include "asb-plugin.h"
#include "asb-utils.h"

/**
 * asb_plugin_set_enabled:
 * @plugin: A #AsbPlugin
 * @enabled: boolean
 *
 * Enables or disables a plugin.
 *
 * Since: 0.1.0
 **/
void
asb_plugin_set_enabled (AsbPlugin *plugin, gboolean enabled)
{
	plugin->enabled = enabled;
}

/**
 * asb_plugin_process:
 * @plugin: A #AsbPlugin
 * @pkg: A #AsbPackage
 * @tmpdir: the temporary location
 * @error: A #GError or %NULL
 *
 * Runs a function on a specific plugin.
 *
 * Returns: (transfer none) (element-type AsbApp): applications
 *
 * Since: 0.1.0
 **/
GList *
asb_plugin_process (AsbPlugin *plugin,
		    AsbPackage *pkg,
		    const gchar *tmpdir,
		    GError **error)
{
	AsbPluginProcessFunc plugin_func = NULL;
	gboolean ret;

	/* run each plugin */
	asb_package_log (pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Running asb_plugin_process() from %s",
			 plugin->name);
	ret = g_module_symbol (plugin->module,
			       "asb_plugin_process",
			       (gpointer *) &plugin_func);
	if (!ret) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "no asb_plugin_process");
		return NULL;
	}
	return plugin_func (plugin, pkg, tmpdir, error);
}

/**
 * asb_plugin_add_app:
 * @list: (element-type AsbApp): A list of #AsbApp's
 * @app: A #AsbApp
 *
 * Adds an application to a list.
 *
 * Since: 0.1.0
 **/
void
asb_plugin_add_app (GList **list, AsbApp *app)
{
	*list = g_list_prepend (*list, g_object_ref (app));
}

/**
 * asb_plugin_add_glob:
 * @array: (element-type utf8): A #GPtrArray
 * @glob: a filename glob
 *
 * Adds a glob from the plugin.
 *
 * Since: 0.1.0
 **/
void
asb_plugin_add_glob (GPtrArray *array, const gchar *glob)
{
	g_ptr_array_add (array, asb_glob_value_new (glob, ""));
}
