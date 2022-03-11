/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:asb-app
 * @short_description: Application object.
 * @stability: Unstable
 *
 * This is an application object that wraps AsApp and provides further features.
 */

#include "config.h"

#include <stdlib.h>
#include <appstream-glib.h>

#include "asb-app.h"
#include "asb-utils.h"

typedef struct
{
	GPtrArray	*requires_appdata;
	AsbPackage	*pkg;
	gboolean	 ignore_requires_appdata;
} AsbAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsbApp, asb_app, AS_TYPE_APP)

#define GET_PRIVATE(o) (asb_app_get_instance_private (o))

static void
asb_app_finalize (GObject *object)
{
	AsbApp *app = ASB_APP (object);
	AsbAppPrivate *priv = GET_PRIVATE (app);

	g_ptr_array_unref (priv->requires_appdata);
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);

	G_OBJECT_CLASS (asb_app_parent_class)->finalize (object);
}

static void
asb_app_init (AsbApp *app)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	priv->requires_appdata = g_ptr_array_new_with_free_func (g_free);

	/* all untrusted */
	as_app_set_trust_flags (AS_APP (app),
				AS_APP_TRUST_FLAG_CHECK_DUPLICATES |
				AS_APP_TRUST_FLAG_CHECK_VALID_UTF8);
}

static void
asb_app_class_init (AsbAppClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asb_app_finalize;
}

/**
 * asb_app_get_package:
 * @app: A #AsbApp
 *
 * Gets the package that backs the application.
 *
 * Returns: (transfer none): package
 *
 * Since: 0.1.0
 **/
AsbPackage *
asb_app_get_package (AsbApp *app)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	return priv->pkg;
}

/**
 * asb_app_set_package:
 * @app: A #AsbApp
 * @pkg: A #AsbPackage
 *
 * Sets the package that backs the application.
 *
 * Since: 0.5.1
 **/
void
asb_app_set_package (AsbApp *app, AsbPackage *pkg)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);

	g_set_object (&priv->pkg, pkg);

	/* be helpful */
	if (asb_package_get_kind (pkg) == ASB_PACKAGE_KIND_DEFAULT)
		as_app_add_pkgname (AS_APP (app), asb_package_get_name (pkg));
}

/**
 * asb_app_save_resources:
 * @app: A #AsbApp
 * @save_flags: #AsbAppSaveFlags, e.g. %ASB_APP_SAVE_FLAG_SCREENSHOTS
 * @error: A #GError or %NULL
 *
 * Saves to disk any resources set for the application.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_app_save_resources (AsbApp *app, AsbAppSaveFlags save_flags, GError **error)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	AsIcon *icon;
	GdkPixbuf *pixbuf;
	GPtrArray *icons = NULL;
	guint i;

	/* any non-stock icon set */
	if (save_flags & ASB_APP_SAVE_FLAG_ICONS)
		icons = as_app_get_icons (AS_APP (app));
	for (i = 0; icons != NULL && i < icons->len; i++) {
		const gchar *tmpdir;
		g_autofree gchar *dir = NULL;
		g_autofree gchar *filename = NULL;
		g_autofree gchar *size_str = NULL;
		g_autoptr(GError) error_local = NULL;

		/* don't save some types of icons */
		icon = g_ptr_array_index (icons, i);
		if (as_icon_get_kind (icon) == AS_ICON_KIND_UNKNOWN ||
		    as_icon_get_kind (icon) == AS_ICON_KIND_STOCK ||
		    as_icon_get_kind (icon) == AS_ICON_KIND_EMBEDDED ||
		    as_icon_get_kind (icon) == AS_ICON_KIND_LOCAL ||
		    as_icon_get_kind (icon) == AS_ICON_KIND_REMOTE)
			continue;

		/* missing cached icons */
		pixbuf = as_icon_get_pixbuf (icon);
		if (pixbuf == NULL)
			continue;

		/* save to disk */
		tmpdir = asb_package_get_config (priv->pkg, "IconsDir");
		dir = g_strdup_printf ("%ix%i", as_icon_get_width (icon),
				       as_icon_get_height (icon));
		filename = g_build_filename (tmpdir, dir,
					     as_icon_get_name (icon),
					     NULL);
		if (!gdk_pixbuf_save (pixbuf, filename, "png", error, NULL))
			return FALSE;

		/* optimize the icon */
		if (!asb_utils_optimize_png (filename, &error_local)) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to optimize icon: %s",
					 error_local->message);
		}

		/* set new AppStream compatible icon name */
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Saved icon %s", filename);
	}
	return TRUE;
}

/**
 * asb_app_new:
 * @pkg: A #AsbPackage, or %NULL
 * @id: The ID for the package, or %NULL
 *
 * Creates a new application object.
 *
 * Returns: a #AsbApp
 *
 * Since: 0.1.0
 **/
AsbApp *
asb_app_new (AsbPackage *pkg, const gchar *id)
{
	AsbApp *app;
	app = g_object_new (ASB_TYPE_APP, NULL);
	if (pkg != NULL)
		asb_app_set_package (app, pkg);
	if (id != NULL)
		as_app_set_id (AS_APP (app), id);
	return ASB_APP (app);
}
