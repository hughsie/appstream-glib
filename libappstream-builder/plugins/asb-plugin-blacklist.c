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

#include <asb-plugin.h>

struct AsbPluginPrivate {
	GPtrArray	*vetos;
};

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "blacklist";
}

/**
 * asb_plugin_initialize:
 */
void
asb_plugin_initialize (AsbPlugin *plugin)
{
	guint i;
	struct {
		const gchar	*id;
		const gchar	*reason;
	} blacklist[] =  {
		{ "ailurus",			"Upstream abandoned" },
		{ "bareftp",			"Upstream abandoned, see: http://bareftp.eide-itc.no/news/?p=228" },
		{ "birdie",			"Upstream abandoned, see: http://birdieapp.github.io/2014/02/26/time-to-say-goodbye.html" },
		{ "chmsee",			"Upstream abandoned, see: https://code.google.com/p/chmsee/" },
		{ "chromium-bsu",		"Upstream abandoned, see: http://chromium-bsu.sourceforge.net/faq.htm#q11" },
		{ "coccinella",			"Upstream abandoned" },
		{ "conduit",			"Upstream abandoned" },
		{ "doom-shareware",		"Upstream abandoned, see: https://www.ohloh.net/p/8278" },
		{ "emesene",			"Upstream abandoned, see: https://github.com/emesene/emesene/issues/1588" },
		{ "fusion-icon",		"Upstream abandoned" },
		{ "gnome-dasher",		"Upstream abandoned" },
		{ "kupfer",			"Upstream abandoned" },
		{ "listen",			"Upstream abandoned" },
		{ "logjam",			"Upstream abandoned, see: http://andy-shev.github.io/LogJam/dev/" },
		{ "mana",			"Upstream abandoned, private email" },
		{ "mm3d",			"Upstream abandoned, see: http://www.misfitcode.com/misfitmodel3d/" },
		{ "nekobee",			"Upstream abandoned" },
		{ "nicotine",			"Upstream abandoned" },
		{ "postler",			"Upstream abandoned, see: https://launchpad.net/postler" },
		{ "rasterview",			"Upstream abandoned" },
		{ "resapplet",			"Obsolete, see: https://mail.gnome.org/archives/gnome-bugsquad/2011-June/msg00000.html" },
		{ "scantailor",			"Upstream abandoned, see: http://www.diybookscanner.org/forum/viewtopic.php?f=21&t=2979" },
		{ "schismtracker",		"Upstream abandoned, see: http://www.nimh.org/" },
		{ "sigil",			"Upstream abandoned, see: http://sigildev.blogspot.co.uk/2014/02/sigils-spiritual-successor.html" },
		{ "spacefm*",			"Upstream abandoned, see: http://igurublog.wordpress.com/2014/04/28/ignorantgurus-hiatus/" },
		{ "specto",			"Upstream abandoned, see: http://jeff.ecchi.ca/blog/2013/03/21/a-programs-obsolescence/" },
		{ "vkeybd",			"Upstream abandoned" },
		{ "rott-registered",		"Requires purchase of original game: http://icculus.org/rott/" },
		{ NULL, NULL } };

	plugin->priv = ASB_PLUGIN_GET_PRIVATE (AsbPluginPrivate);
	plugin->priv->vetos = asb_glob_value_array_new ();

	/* add each entry */
	for (i = 0; blacklist[i].id != NULL; i++) {
		g_ptr_array_add (plugin->priv->vetos,
				 asb_glob_value_new (blacklist[i].id,
						     blacklist[i].reason));
	}
}

/**
 * asb_plugin_destroy:
 */
void
asb_plugin_destroy (AsbPlugin *plugin)
{
	g_ptr_array_unref (plugin->priv->vetos);
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
	const gchar *tmp;
	if (as_utils_is_blacklisted_id (as_app_get_id (AS_APP (app))))
		asb_app_add_veto (app, "Not an application");
	tmp = asb_glob_value_search (plugin->priv->vetos,
				     as_app_get_id (AS_APP (app)));
	if (tmp != NULL)
		asb_app_add_veto (app, "%s", tmp);
	return TRUE;
}
