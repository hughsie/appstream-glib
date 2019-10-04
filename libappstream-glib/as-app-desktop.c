/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <string.h>

#include "as-app-private.h"
#include "as-utils.h"

static gchar *
as_app_desktop_key_get_locale (const gchar *key)
{
	gchar *tmp1;
	gchar *tmp2;
	gchar *locale = NULL;

	tmp1 = g_strstr_len (key, -1, "[");
	if (tmp1 == NULL)
		return NULL;
	tmp2 = g_strstr_len (tmp1, -1, "]");
	if (tmp2 == NULL)
		return NULL;
	locale = g_strdup (tmp1 + 1);
	locale[tmp2 - tmp1 - 1] = '\0';
	return locale;
}

static gboolean
as_app_infer_kudos (AsApp *app, GKeyFile *kf, const gchar *key, GError **error)
{
	if (g_strcmp0 (key, "X-GNOME-UsesNotifications") == 0) {
		as_app_add_kudo_kind (AS_APP (app),
				      AS_KUDO_KIND_NOTIFICATIONS);
	}
	return TRUE;
}

static gboolean
as_app_infer_project_group (AsApp *app, GKeyFile *kf, const gchar *key, GError **error)
{
	g_autofree gchar *tmp = NULL;
	if (g_strcmp0 (key, "X-GNOME-Bugzilla-Bugzilla") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (g_strcmp0 (tmp, "GNOME") == 0)
			as_app_set_project_group (app, "GNOME");

	} else if (g_strcmp0 (key, "X-MATE-Bugzilla-Product") == 0) {
		as_app_set_project_group (app, "MATE");

	} else if (g_strcmp0 (key, "X-KDE-StartupNotify") == 0) {
		as_app_set_project_group (app, "KDE");

	} else if (g_strcmp0 (key, "X-DocPath") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (g_str_has_prefix (tmp, "http://userbase.kde.org/"))
			as_app_set_project_group (app, "KDE");

	/* Exec */
	} else if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_EXEC) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (g_str_has_prefix (tmp, "xfce4-"))
			as_app_set_project_group (app, "XFCE");
	}

	return TRUE;
}

static gboolean
_as_utils_is_stock_icon_name_fallback (const gchar *name)
{
	guint i;
	const gchar *names[] = {
		"fedora-logo-sprite",
		"gtk-preferences",
		"hwinfo",
		"trash-empty",
		"utilities-log-viewer",
		NULL };
	for (i = 0; names[i] != NULL; i++) {
		if (g_strcmp0 (name, names[i]) == 0)
			return TRUE;
	}
	return FALSE;
}

static void
as_app_parse_file_metadata (AsApp *app, GKeyFile *kf, const gchar *key)
{
	guint i;
	g_autofree gchar *value = NULL;
	const gchar *blacklist[] = {
		"X-AppInstall-",
		"X-Desktop-File-Install-Version",
		"X-Geoclue-Reason",
		"X-GNOME-Bugzilla-",
		"X-GNOME-FullName",
		"X-GNOME-Gettext-Domain",
		"X-GNOME-UsesNotifications",
		NULL };

	if (!g_str_has_prefix (key, "X-"))
		return;

	/* anything blacklisted */
	for (i = 0; blacklist[i] != NULL; i++) {
		if (g_str_has_prefix (blacklist[i], key))
			return;
	}
	value = g_key_file_get_string (kf,
				       G_KEY_FILE_DESKTOP_GROUP,
				       key,
				       NULL);
	as_app_add_metadata (app, key, value);
}

static AsIcon *
as_app_desktop_create_icon (AsApp *app, const gchar *name, AsAppParseFlags flags)
{
	AsIcon *icon = as_icon_new ();
	gchar *dot;
	g_autofree gchar *name_fixed = NULL;

	/* local */
	if (g_path_is_absolute (name)) {
		as_icon_set_kind (icon, AS_ICON_KIND_LOCAL);
		as_icon_set_filename (icon, name);
		return icon;
	}

	/* work around a common mistake in desktop files */
	name_fixed = g_strdup (name);
	dot = g_strstr_len (name_fixed, -1, ".");
	if (dot != NULL &&
	    (g_strcmp0 (dot, ".png") == 0 ||
	     g_strcmp0 (dot, ".xpm") == 0 ||
	     g_strcmp0 (dot, ".svg") == 0)) {
		*dot = '\0';
	}

	/* stock */
	if (as_utils_is_stock_icon_name (name_fixed)) {
		as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
		as_icon_set_name (icon, name_fixed);
		return icon;
	}

	/* stock, but kinda sneaky */
	if ((flags & AS_APP_PARSE_FLAG_USE_FALLBACKS) > 0 &&
	    _as_utils_is_stock_icon_name_fallback (name_fixed)) {
		as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
		as_icon_set_name (icon, name_fixed);
		return icon;
	}

	/* just use default of UNKNOWN */
	as_icon_set_name (icon, name_fixed);
	return icon;
}

static gboolean
as_app_parse_file_key (AsApp *app,
		       GKeyFile *kf,
		       const gchar *key,
		       AsAppParseFlags flags,
		       GError **error)
{
	guint i;
	guint j;
	g_autofree gchar *locale = NULL;
	g_autofree gchar *tmp = NULL;
	g_auto(GStrv) list = NULL;

	/* NoDisplay */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && g_ascii_strcasecmp (tmp, "True") == 0)
			as_app_add_veto (app, "NoDisplay=true");

	/* Type */
	} else if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_TYPE) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (g_strcmp0 (tmp, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) != 0) {
			g_set_error_literal (error,
					     AS_APP_ERROR,
					     AS_APP_ERROR_INVALID_TYPE,
					     "not an application");
			return FALSE;
		}

	/* Icon */
	} else if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_ICON) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0') {
			g_autoptr(AsIcon) icon = NULL;
			icon = as_app_desktop_create_icon (app, tmp, flags);
			as_app_add_icon (app, icon);
		}

	/* Categories */
	} else if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_CATEGORIES) == 0) {
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		for (i = 0; list != NULL && list[i] != NULL; i++) {
			const gchar *category_blacklist[] = {
				"X-GNOME-Settings-Panel",
				"X-Unity-Settings-Panel",
				NULL };

			/* we have to veto these */
			for (j = 0; category_blacklist[j] != NULL; j++) {
				if (g_strcmp0 (list[i], category_blacklist[j]) == 0) {
					as_app_add_veto (app, "Has category %s",
							 category_blacklist[j]);
				}
			}

			/* check the category is valid */
			if (!as_utils_is_category_id (list[i]))
				continue;

			/* ignore some useless keys */
			if (g_strcmp0 (list[i], "GTK") == 0)
				continue;
			if (g_strcmp0 (list[i], "Qt") == 0)
				continue;
			if (g_strcmp0 (list[i], "KDE") == 0)
				continue;
			if (g_strcmp0 (list[i], "GNOME") == 0)
				continue;
			as_app_add_category (app, list[i]);
		}

	} else if (g_strcmp0 (key, "Keywords") == 0) {
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		for (i = 0; list != NULL && list[i] != NULL; i++) {
			g_auto(GStrv) kw_split = NULL;
			kw_split = g_strsplit (list[i], ",", -1);
			for (j = 0; kw_split[j] != NULL; j++) {
				if (kw_split[j][0] == '\0')
					continue;
				as_app_add_keyword (app, "C", kw_split[j]);
			}
		}

	} else if (g_str_has_prefix (key, "Keywords")) {
		locale = as_app_desktop_key_get_locale (key);
		if (flags & AS_APP_PARSE_FLAG_ONLY_NATIVE_LANGS &&
		    !g_strv_contains (g_get_language_names (), locale))
			return TRUE;
		list = g_key_file_get_locale_string_list (kf,
							  G_KEY_FILE_DESKTOP_GROUP,
							  key,
							  locale,
							  NULL, NULL);
		for (i = 0; list != NULL && list[i] != NULL; i++) {
			g_auto(GStrv) kw_split = NULL;
			kw_split = g_strsplit (list[i], ",", -1);
			for (j = 0; kw_split[j] != NULL; j++) {
				if (kw_split[j][0] == '\0')
					continue;
				as_app_add_keyword (app, locale, kw_split[j]);
			}
		}

	} else if (g_strcmp0 (key, "MimeType") == 0) {
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		for (i = 0; list != NULL && list[i] != NULL; i++)
			as_app_add_mimetype (app, list[i]);

	} else if (g_strcmp0 (key, "X-Flatpak") == 0) {
		tmp = g_key_file_get_string (kf, G_KEY_FILE_DESKTOP_GROUP, key, NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_id (app, tmp);

	} else if (g_strcmp0 (key, "X-Flatpak-RenamedFrom") == 0) {
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		for (i = 0; list != NULL && list[i] != NULL; i++) {
			g_autoptr(AsProvide) prov = as_provide_new ();
			as_provide_set_kind (prov, AS_PROVIDE_KIND_ID);
			as_provide_set_value (prov, list[i]);
			as_app_add_provide (app, prov);
		}

	} else if (g_strcmp0 (key, "X-AppInstall-Package") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_add_pkgname (app, tmp);

	/* OnlyShowIn */
	} else if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN) == 0) {
		/* if an app has only one entry, it's that desktop */
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		/* "OnlyShowIn=" is the same as "NoDisplay=True" */
		if (g_strv_length (list) == 0)
			as_app_add_veto (app, "Empty OnlyShowIn");
		else if (g_strv_length (list) == 1)
			as_app_set_project_group (app, list[0]);

	/* Name */
	} else if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NAME) == 0 ||
	           g_strcmp0 (key, "_Name") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_name (app, "C", tmp);

	/* Name[] */
	} else if (g_str_has_prefix (key, G_KEY_FILE_DESKTOP_KEY_NAME)) {
		locale = as_app_desktop_key_get_locale (key);
		if (flags & AS_APP_PARSE_FLAG_ONLY_NATIVE_LANGS &&
		    !g_strv_contains (g_get_language_names (), locale))
			return TRUE;
		tmp = g_key_file_get_locale_string (kf,
						    G_KEY_FILE_DESKTOP_GROUP,
						    G_KEY_FILE_DESKTOP_KEY_NAME,
						    locale,
						    NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_name (app, locale, tmp);

	/* Comment */
	} else if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_COMMENT) == 0 ||
	           g_strcmp0 (key, "_Comment") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_comment (app, "C", tmp);

	/* Comment[] */
	} else if (g_str_has_prefix (key, G_KEY_FILE_DESKTOP_KEY_COMMENT)) {
		locale = as_app_desktop_key_get_locale (key);
		if (flags & AS_APP_PARSE_FLAG_ONLY_NATIVE_LANGS &&
		    !g_strv_contains (g_get_language_names (), locale))
			return TRUE;
		tmp = g_key_file_get_locale_string (kf,
						    G_KEY_FILE_DESKTOP_GROUP,
						    G_KEY_FILE_DESKTOP_KEY_COMMENT,
						    locale,
						    NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_comment (app, locale, tmp);

	/* non-standard */
	} else if (g_strcmp0 (key, "X-Ubuntu-Software-Center-Name") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_name (app, "C", tmp);
	} else if (g_str_has_prefix (key, "X-Ubuntu-Software-Center-Name")) {
		locale = as_app_desktop_key_get_locale (key);
		if (flags & AS_APP_PARSE_FLAG_ONLY_NATIVE_LANGS &&
		    !g_strv_contains (g_get_language_names (), locale))
			return TRUE;
		tmp = g_key_file_get_locale_string (kf,
						    G_KEY_FILE_DESKTOP_GROUP,
						    "X-Ubuntu-Software-Center-Name",
						    locale,
						    NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_name (app, locale, tmp);

	/* for Ubuntu */
	} else if (g_strcmp0 (key, "X-AppStream-Ignore") == 0) {
		gboolean ret;
		ret = g_key_file_get_boolean (kf,
					      G_KEY_FILE_DESKTOP_GROUP,
					      key,
					      NULL);
		if (ret)
			as_app_add_veto (app, "X-AppStream-Ignore");
	}

	/* add any external attribute as metadata to the application */
	if (flags & AS_APP_PARSE_FLAG_ADD_ALL_METADATA)
		as_app_parse_file_metadata (app, kf, key);

	return TRUE;
}

static gboolean
as_app_parse_file_key_fallback_comment (AsApp *app,
					GKeyFile *kf,
					const gchar *key,
					GError **error)
{
	g_autofree gchar *locale = NULL;
	g_autofree gchar *tmp = NULL;

	/* GenericName */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME) == 0 ||
	           g_strcmp0 (key, "_GenericName") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_comment (app, "C", tmp);

	/* GenericName[] */
	} else if (g_str_has_prefix (key, G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME)) {
		locale = as_app_desktop_key_get_locale (key);
		tmp = g_key_file_get_locale_string (kf,
						    G_KEY_FILE_DESKTOP_GROUP,
						    G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME,
						    locale,
						    NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_comment (app, locale, tmp);
	}

	return TRUE;
}

static gboolean
as_app_parse_desktop_kf (AsApp *app, GKeyFile *kf, AsAppParseFlags flags, GError **error)
{
	g_auto(GStrv) keys = NULL;

	/* check this is a valid desktop file */
	if (!g_key_file_has_group (kf, G_KEY_FILE_DESKTOP_GROUP)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Not a desktop file: no [%s]",
			     G_KEY_FILE_DESKTOP_GROUP);
		return FALSE;
	}

	/* look at all the keys */
	keys = g_key_file_get_keys (kf, G_KEY_FILE_DESKTOP_GROUP, NULL, error);
	if (keys == NULL)
		return FALSE;
	as_app_set_kind (app, AS_APP_KIND_DESKTOP);
	for (guint i = 0; keys[i] != NULL; i++) {
		if (!as_app_parse_file_key (app, kf, keys[i], flags, error))
			return FALSE;
		if ((flags & AS_APP_PARSE_FLAG_USE_HEURISTICS) > 0) {
			if (!as_app_infer_kudos (app, kf, keys[i], error))
				return FALSE;
			if (as_app_get_project_group (app) == NULL) {
				if (!as_app_infer_project_group (app, kf, keys[i], error))
					return FALSE;
			}
		}
	}

	/* perform any fallbacks */
	if ((flags & AS_APP_PARSE_FLAG_USE_FALLBACKS) > 0 &&
	    as_app_get_comment_size (app) == 0) {
		for (guint i = 0; keys[i] != NULL; i++) {
			if (!as_app_parse_file_key_fallback_comment (app,
								     kf,
								     keys[i],
								     error))
				return FALSE;
		}
	}

	/* all applications require icons */
	if (as_app_get_icons(app)->len == 0)
		as_app_add_veto (app, "has no icon");
	return TRUE;
}

gboolean
as_app_parse_desktop_data (AsApp *app, GBytes *data, AsAppParseFlags flags, GError **error)
{
	GKeyFileFlags kf_flags = G_KEY_FILE_KEEP_TRANSLATIONS;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GKeyFile) kf = NULL;

	kf = g_key_file_new ();
	if (flags & AS_APP_PARSE_FLAG_KEEP_COMMENTS)
		kf_flags |= G_KEY_FILE_KEEP_COMMENTS;
	if (!g_key_file_load_from_bytes (kf, data, kf_flags, &error_local)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Failed to parse data: %s",
			     error_local->message);
		return FALSE;
	}
	return as_app_parse_desktop_kf (app, kf, flags, error);
}

gboolean
as_app_parse_desktop_file (AsApp *app,
			   const gchar *desktop_file,
			   AsAppParseFlags flags,
			   GError **error)
{
	GKeyFileFlags kf_flags = G_KEY_FILE_KEEP_TRANSLATIONS;
	gchar *tmp;
	g_autofree gchar *app_id = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GKeyFile) kf = NULL;

	/* load file */
	kf = g_key_file_new ();
	if (flags & AS_APP_PARSE_FLAG_KEEP_COMMENTS)
		kf_flags |= G_KEY_FILE_KEEP_COMMENTS;
	if (!g_key_file_load_from_file (kf, desktop_file, kf_flags, &error_local)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Failed to parse %s: %s",
			     desktop_file, error_local->message);
		return FALSE;
	}

	/* is this really a web-app? */
	if ((flags & AS_APP_PARSE_FLAG_USE_HEURISTICS) > 0) {
		g_autofree gchar *exec = NULL;
		exec = g_key_file_get_string (kf,
					      G_KEY_FILE_DESKTOP_GROUP,
					      G_KEY_FILE_DESKTOP_KEY_EXEC,
					      NULL);
		if (exec != NULL) {
			if (g_str_has_prefix (exec, "epiphany --application-mode"))
				as_app_set_kind (app, AS_APP_KIND_WEB_APP);
		}
	}

	/* Ubuntu helpfully put the package name in the desktop file name */
	app_id = g_path_get_basename (desktop_file);
	tmp = g_strstr_len (app_id, -1, ":");
	if (tmp != NULL)
		as_app_set_id (app, tmp + 1);
	else
		as_app_set_id (app, app_id);
	return as_app_parse_desktop_kf (app, kf, flags, error);
}
