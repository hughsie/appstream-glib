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
		{ "ailurus.desktop",		"Upstream abandoned" },
		{ "alltray.desktop",		"Upstream abandoned and homepage dead" },
		{ "ayttm.desktop",		"Upstream abandoned, see: https://www.openhub.net/p/ayttm" },
		{ "bareftp.desktop",		"Upstream abandoned, see: http://bareftp.eide-itc.no/news/?p=228" },
		{ "birdie.desktop",		"Upstream abandoned, see: http://birdieapp.github.io/2014/02/26/time-to-say-goodbye.html" },
		{ "chmsee.desktop",		"Upstream abandoned, see: https://code.google.com/p/chmsee/" },
		{ "chromium-bsu.desktop",	"Upstream abandoned, see: http://chromium-bsu.sourceforge.net/faq.htm#q11" },
		{ "coccinella.desktop",		"Upstream abandoned" },
		{ "conduit.desktop",		"Upstream abandoned" },
		{ "coriander.desktop",		"Upstream abandoned, see: https://www.openhub.net/p/dv4linux" },
		{ "diffpdf.desktop",		"Upstream abandoned, see: http://www.qtrac.eu/diffpdf-foss.html" },
		{ "dissy.desktop",		"Upstream abandoned, see: https://code.google.com/p/dissy/" },
		{ "doom-shareware.desktop",	"Upstream abandoned, see: https://www.ohloh.net/p/8278" },
		{ "emesene.desktop",		"Upstream abandoned, see: https://github.com/emesene/emesene/issues/1588" },
		{ "flightdeck.desktop",		"Upstream abandoned" },
		{ "fotowall.desktop",		"Upstream abandoned, see: https://www.openhub.net/p/fotowall" },
		{ "fusion-icon.desktop",	"Upstream abandoned" },
		{ "gnome-dasher.desktop",	"Upstream abandoned" },
		{ "hotwire.desktop",		"Upstream abandoned" },
		{ "jigdo.desktop",		"Upstream abandoned, see http://atterer.org/jigdo/" },
		{ "kupfer.desktop",		"Upstream abandoned" },
		{ "listen.desktop",		"Upstream abandoned" },
		{ "logjam.desktop",		"Upstream abandoned, see: http://andy-shev.github.io/LogJam/dev/" },
		{ "mana.desktop",		"Upstream abandoned, private email" },
		{ "mm3d.desktop",		"Upstream abandoned, see: http://www.misfitcode.com/misfitmodel3d/" },
		{ "nekobee.desktop",		"Upstream abandoned" },
		{ "nicotine.desktop",		"Upstream abandoned" },
		{ "postler.desktop",		"Upstream abandoned, see: https://launchpad.net/postler" },
		{ "qmpdclient.desktop",		"Upstream abandoned, private email" },
		{ "rasterview.desktop",		"Upstream abandoned" },
		{ "resapplet.desktop",		"Obsolete, see: https://mail.gnome.org/archives/gnome-bugsquad/2011-June/msg00000.html" },
		{ "rott-registered.desktop",	"Requires purchase of original game: http://icculus.org/rott/" },
		{ "scantailor.desktop",		"Upstream abandoned, see: http://www.diybookscanner.org/forum/viewtopic.php?f=21&t=2979" },
		{ "schismtracker.desktop",	"Upstream abandoned, see: http://www.nimh.org/" },
		{ "sigil.desktop",		"Upstream abandoned, see: http://sigildev.blogspot.co.uk/2014/02/sigils-spiritual-successor.html" },
		{ "spacefm*.desktop",		"Upstream abandoned, see: http://igurublog.wordpress.com/2014/04/28/ignorantgurus-hiatus/" },
		{ "specto.desktop",		"Upstream abandoned, see: http://jeff.ecchi.ca/blog/2013/03/21/a-programs-obsolescence/" },
		{ "vkeybd.desktop",		"Upstream abandoned" },
		{ "rott-registered.desktop",	"Requires purchase of original game: http://icculus.org/rott/" },
		{ "xwrits.desktop",		"Upstream abandoned, see: http://www.lcdf.org/xwrits/changes.html" },
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
		as_app_add_veto (AS_APP (app), "Not an application");
	tmp = asb_glob_value_search (plugin->priv->vetos,
				     as_app_get_id (AS_APP (app)));
	if (tmp != NULL)
		asb_app_add_requires_appdata (app, "%s", tmp);
	return TRUE;
}
