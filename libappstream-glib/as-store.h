/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include "as-app.h"
#include "as-node.h"

G_BEGIN_DECLS

#define AS_TYPE_STORE (as_store_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsStore, as_store, AS, STORE, GObject)

struct _AsStoreClass
{
	GObjectClass		parent_class;
	void			(*changed)	(AsStore	*store);
	void			(*app_added)	(AsStore	*store,
						 AsApp		*app);
	void			(*app_removed)	(AsStore	*store,
						 AsApp		*app);
	void			(*app_changed)	(AsStore	*store,
						 AsApp		*app);
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
};

/**
 * AsStoreLoadFlags:
 * @AS_STORE_LOAD_FLAG_NONE:			No extra flags to use
 * @AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM:		The system app-info AppStream data
 * @AS_STORE_LOAD_FLAG_APP_INFO_USER:		The per-user app-info AppStream data
 * @AS_STORE_LOAD_FLAG_APP_INSTALL:		The ubuntu-specific app-install data (obsolete)
 * @AS_STORE_LOAD_FLAG_APPDATA:			The installed AppData files
 * @AS_STORE_LOAD_FLAG_DESKTOP:			The installed desktop files
 * @AS_STORE_LOAD_FLAG_ALLOW_VETO:		Add vetoed applications
 * @AS_STORE_LOAD_FLAG_FLATPAK_USER:		Add flatpak user applications (obsolete)
 * @AS_STORE_LOAD_FLAG_FLATPAK_SYSTEM:		Add flatpak system applications (obsolete)
 * @AS_STORE_LOAD_FLAG_IGNORE_INVALID:		Ignore invalid files
 * @AS_STORE_LOAD_FLAG_ONLY_UNCOMPRESSED:	Ignore compressed files
 * @AS_STORE_LOAD_FLAG_ONLY_MERGE_APPS:		Ignore non-wildcard matches
 *
 * The flags to use when loading the store.
 **/
typedef enum {
	AS_STORE_LOAD_FLAG_NONE			= 0,		/* Since: 0.1.2 */
	AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM	= 1 << 0,	/* Since: 0.1.2 */
	AS_STORE_LOAD_FLAG_APP_INFO_USER	= 1 << 1,	/* Since: 0.1.2 */
	AS_STORE_LOAD_FLAG_APP_INSTALL		= 1 << 2,	/* Since: 0.1.2 */
	AS_STORE_LOAD_FLAG_APPDATA		= 1 << 3,	/* Since: 0.2.2 */
	AS_STORE_LOAD_FLAG_DESKTOP		= 1 << 4,	/* Since: 0.2.2 */
	AS_STORE_LOAD_FLAG_ALLOW_VETO		= 1 << 5,	/* Since: 0.2.5 */
	AS_STORE_LOAD_FLAG_FLATPAK_USER		= 1 << 6,	/* Since: 0.5.7 */
	AS_STORE_LOAD_FLAG_FLATPAK_SYSTEM	= 1 << 7,	/* Since: 0.5.7 */
	AS_STORE_LOAD_FLAG_IGNORE_INVALID	= 1 << 8,	/* Since: 0.5.8 */
	AS_STORE_LOAD_FLAG_ONLY_UNCOMPRESSED	= 1 << 9,	/* Since: 0.6.4 */
	AS_STORE_LOAD_FLAG_ONLY_MERGE_APPS	= 1 << 10,	/* Since: 0.6.4 */
	/*< private >*/
	AS_STORE_LOAD_FLAG_LAST
} AsStoreLoadFlags;

/* DEPRECATED */
#define AS_STORE_LOAD_FLAG_XDG_APP_USER		AS_STORE_LOAD_FLAG_FLATPAK_USER
#define AS_STORE_LOAD_FLAG_XDG_APP_SYSTEM	AS_STORE_LOAD_FLAG_FLATPAK_SYSTEM

/**
 * AsStoreAddFlags:
 * @AS_STORE_ADD_FLAG_NONE:				No extra flags to use
 * @AS_STORE_ADD_FLAG_PREFER_LOCAL:			Local files will be used by default
 * @AS_STORE_ADD_FLAG_USE_UNIQUE_ID:			Allow multiple apps with the same AppStream ID
 * @AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC:		Use a heuristic when adding merge components
 * @AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS:		Only load native languages
 *
 * The flags to use when adding applications to the store.
 **/
typedef enum {
	AS_STORE_ADD_FLAG_NONE			= 0,		/* Since: 0.2.2 */
	AS_STORE_ADD_FLAG_PREFER_LOCAL		= 1 << 0,	/* Since: 0.2.2 */
	AS_STORE_ADD_FLAG_USE_UNIQUE_ID		= 1 << 1,	/* Since: 0.6.1 */
	AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC	= 1 << 2,	/* Since: 0.6.1 */
	AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS	= 1 << 3,	/* Since: 0.6.5 */
	/*< private >*/
	AS_STORE_ADD_FLAG_LAST
} AsStoreAddFlags;

/**
 * AsStoreWatchFlags:
 * @AS_STORE_WATCH_FLAG_NONE:			No extra flags to use
 * @AS_STORE_WATCH_FLAG_ADDED:			Add applications if files change or are added
 * @AS_STORE_WATCH_FLAG_REMOVED:		Remove applications if files are changed or deleted
 *
 * The flags to use when local files are added or removed from the store.
 **/
typedef enum {
	AS_STORE_WATCH_FLAG_NONE			= 0,	/* Since: 0.4.2 */
	AS_STORE_WATCH_FLAG_ADDED			= 1,	/* Since: 0.4.2 */
	AS_STORE_WATCH_FLAG_REMOVED			= 2,	/* Since: 0.4.2 */
	/*< private >*/
	AS_STORE_WATCH_FLAG_LAST
} AsStoreWatchFlags;

/**
 * AsStoreSearchFlags:
 * @AS_STORE_SEARCH_FLAG_NONE:			No extra flags to use
 * @AS_STORE_SEARCH_FLAG_USE_WILDCARDS:		Process the globs
 *
 * The flags to use when searching in the store.
 **/
typedef enum {
	AS_STORE_SEARCH_FLAG_NONE			= 0,	/* Since: 0.6.1 */
	AS_STORE_SEARCH_FLAG_USE_WILDCARDS		= 1,	/* Since: 0.6.1 */
	/*< private >*/
	AS_STORE_SEARCH_FLAG_LAST
} AsStoreSearchFlags;

/**
 * AsStoreError:
 * @AS_STORE_ERROR_FAILED:			Generic failure
 *
 * The error type.
 **/
typedef enum {
	AS_STORE_ERROR_FAILED,
	/*< private >*/
	AS_STORE_ERROR_LAST
} AsStoreError;

#define	AS_STORE_ERROR				as_store_error_quark ()

AsStore		*as_store_new			(void);
GQuark		 as_store_error_quark		(void);

/* getters */
guint		 as_store_get_size		(AsStore	*store);

/* object methods */
gboolean	 as_store_from_file		(AsStore	*store,
						 GFile		*file,
						 const gchar	*icon_root,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 as_store_from_bytes		(AsStore	*store,
						 GBytes		*bytes,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 as_store_from_xml		(AsStore	*store,
						 const gchar	*data,
						 const gchar	*icon_root,
						 GError		**error);

gboolean	 as_store_load			(AsStore	*store,
						 guint32	 flags,
						 GCancellable	*cancellable,
						 GError		**error);
void 		 as_store_load_async		(AsStore	*store,
						 AsStoreLoadFlags flags,
						 GCancellable	*cancellable,
						 GAsyncReadyCallback callback,
						 gpointer        user_data);
gboolean	 as_store_load_finish		(AsStore	*store,
						 GAsyncResult	*result,
						 GError		**error);

gboolean	 as_store_load_path		(AsStore	*store,
						 const gchar	*path,
						 GCancellable	*cancellable,
						 GError		**error);
void		 as_store_load_path_async	(AsStore	*store,
						 const gchar	*path,
						 GCancellable	*cancellable,
						 GAsyncReadyCallback callback,
						 gpointer	 user_data);
gboolean	 as_store_load_path_finish	(AsStore	*store,
						 GAsyncResult	*result,
						 GError		**error);

void		 as_store_load_search_cache	(AsStore	*store);
void		 as_store_set_search_match	(AsStore	*store,
						 guint16	 search_match);
guint16		 as_store_get_search_match	(AsStore	*store);
void		 as_store_remove_all		(AsStore	*store);
GPtrArray	*as_store_get_apps		(AsStore	*store);
GPtrArray	*as_store_dup_apps		(AsStore	*store);
GPtrArray	*as_store_get_apps_by_id	(AsStore	*store,
						 const gchar	*id);
GPtrArray	*as_store_get_apps_by_id_merge	(AsStore	*store,
						 const gchar	*id);
GPtrArray	*as_store_dup_apps_by_id_merge	(AsStore	*store,
						 const gchar	*id);
GPtrArray	*as_store_get_apps_by_metadata	(AsStore	*store,
						 const gchar	*key,
						 const gchar	*value);
AsApp		*as_store_get_app_by_id		(AsStore	*store,
						 const gchar	*id);
AsApp		*as_store_get_app_by_unique_id	(AsStore	*store,
						 const gchar	*unique_id,
						 guint32	 search_flags);
AsApp		*as_store_get_app_by_id_ignore_prefix
						(AsStore	*store,
						 const gchar	*id);
AsApp		*as_store_get_app_by_id_with_fallbacks (AsStore	*store,
						 const gchar	*id);
AsApp		*as_store_get_app_by_pkgname	(AsStore	*store,
						 const gchar	*pkgname);
AsApp		*as_store_get_app_by_pkgnames	(AsStore	*store,
						 gchar		**pkgnames);
AsApp		*as_store_get_app_by_provide	(AsStore	*store,
						 AsProvideKind	 kind,
						 const gchar	*value);
AsApp		*as_store_get_app_by_launchable	(AsStore	*store,
						 AsLaunchableKind kind,
						 const gchar	*value);
GPtrArray	*as_store_get_apps_by_provide	(AsStore	*store,
						 AsProvideKind	 kind,
						 const gchar	*value);
void		 as_store_add_app		(AsStore	*store,
						 AsApp		*app);
void		 as_store_add_apps		(AsStore	*store,
						 GPtrArray	*apps);
void		 as_store_remove_app		(AsStore	*store,
						 AsApp		*app);
void		 as_store_remove_app_by_id	(AsStore	*store,
						 const gchar	*id);
void		 as_store_remove_apps_with_veto	(AsStore	*store);
GString		*as_store_to_xml		(AsStore	*store,
						 guint32	 flags);
gboolean	 as_store_to_file		(AsStore	*store,
						 GFile		*file,
						 guint32	 flags,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 as_store_convert_icons		(AsStore	*store,
						 AsIconKind	 kind,
						 GError		**error);
const gchar	*as_store_get_origin		(AsStore	*store);
void		 as_store_set_origin		(AsStore	*store,
						 const gchar	*origin);
const gchar	*as_store_get_builder_id	(AsStore	*store);
void		 as_store_set_builder_id	(AsStore	*store,
						 const gchar	*builder_id);
const gchar	*as_store_get_destdir		(AsStore	*store);
void		 as_store_set_destdir		(AsStore	*store,
						 const gchar	*destdir);
gdouble		 as_store_get_api_version	(AsStore	*store);
void		 as_store_set_api_version	(AsStore	*store,
						 gdouble	 api_version);
guint32		 as_store_get_add_flags		(AsStore	*store);
void		 as_store_set_add_flags		(AsStore	*store,
						 guint32	 add_flags);
guint32		 as_store_get_watch_flags	(AsStore	*store);
void		 as_store_set_watch_flags	(AsStore	*store,
						 guint32	 watch_flags);
GPtrArray	*as_store_validate		(AsStore	*store,
						 guint32	 flags,
						 GError		**error);
void		 as_store_add_metadata_index	(AsStore	*store,
						 const gchar	*key);
void		 as_store_add_filter		(AsStore	*store,
						 AsAppKind	 kind);
void		 as_store_remove_filter		(AsStore	*store,
						 AsAppKind	 kind);

G_END_DECLS
