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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__APPSTREAM_GLIB_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_APP_H
#define __AS_APP_H

#include <glib-object.h>

#include "as-enums.h"
#include "as-provide.h"
#include "as-release.h"
#include "as-screenshot.h"

#define AS_TYPE_APP		(as_app_get_type())
#define AS_APP(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_APP, AsApp))
#define AS_APP_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_APP, AsAppClass))
#define AS_IS_APP(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_APP))
#define AS_IS_APP_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_APP))
#define AS_APP_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_APP, AsAppClass))

G_BEGIN_DECLS

typedef struct _AsApp		AsApp;
typedef struct _AsAppClass	AsAppClass;

struct _AsApp
{
	GObject			parent;
};

struct _AsAppClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

/**
 * AsAppParseFlags:
 * @AS_APP_PARSE_FLAG_NONE:		No special actions to use
 * @AS_APP_PARSE_FLAG_USE_HEURISTICS:	Use heuristic to infer properties
 * @AS_APP_PARSE_FLAG_KEEP_COMMENTS:	Save comments from the file
 * @AS_APP_PARSE_FLAG_CONVERT_TRANSLATABLE:	Allow translatable flags like <_p>
 * @AS_APP_PARSE_FLAG_APPEND_DATA:	Append new data rather than replacing
 * @AS_APP_PARSE_FLAG_ALLOW_VETO:	Do not return errors for vetoed apps
 *
 * The flags to use when parsing resources.
 **/
typedef enum {
	AS_APP_PARSE_FLAG_NONE,
	AS_APP_PARSE_FLAG_USE_HEURISTICS	= 1,	/* Since: 0.1.2 */
	AS_APP_PARSE_FLAG_KEEP_COMMENTS		= 2,	/* Since: 0.1.6 */
	AS_APP_PARSE_FLAG_CONVERT_TRANSLATABLE	= 4,	/* Since: 0.1.6 */
	AS_APP_PARSE_FLAG_APPEND_DATA		= 8,	/* Since: 0.1.8 */
	AS_APP_PARSE_FLAG_ALLOW_VETO		= 16,	/* Since: 0.2.5 */
	/*< private >*/
	AS_APP_PARSE_FLAG_LAST,
} AsAppParseFlags;

/**
 * AsAppSubsumeFlags:
 * @AS_APP_SUBSUME_FLAG_NONE:		No special actions to use
 * @AS_APP_SUBSUME_FLAG_NO_OVERWRITE:	Do not overwrite already set properties
 * @AS_APP_SUBSUME_FLAG_BOTH_WAYS:	Copy unset properties both ways
 * @AS_APP_SUBSUME_FLAG_PARTIAL:	Only subsume a safe subset
 *
 * The flags to use when subsuming applications.
 **/
typedef enum {
	AS_APP_SUBSUME_FLAG_NONE,
	AS_APP_SUBSUME_FLAG_NO_OVERWRITE = 1,	/* Since: 0.1.4 */
	AS_APP_SUBSUME_FLAG_BOTH_WAYS	 = 2,	/* Since: 0.1.4 */
	AS_APP_SUBSUME_FLAG_PARTIAL	 = 4,	/* Since: 0.2.2 */
	/*< private >*/
	AS_APP_SUBSUME_FLAG_LAST,
} AsAppSubsumeFlags;

/**
 * AsAppError:
 * @AS_APP_ERROR_FAILED:			Generic failure
 * @AS_APP_ERROR_INVALID_TYPE:			Invalid type
 *
 * The error type.
 **/
typedef enum {
	AS_APP_ERROR_FAILED,
	AS_APP_ERROR_INVALID_TYPE,
	/*< private >*/
	AS_APP_ERROR_LAST
} AsAppError;

/**
 * AsAppValidateFlags:
 * @AS_APP_VALIDATE_FLAG_NONE:			No extra flags to use
 * @AS_APP_VALIDATE_FLAG_RELAX:			Relax the checks
 * @AS_APP_VALIDATE_FLAG_STRICT:		Make the checks more strict
 * @AS_APP_VALIDATE_FLAG_NO_NETWORK:		Do not use the network
 *
 * The flags to use when validating.
 **/
typedef enum {
	AS_APP_VALIDATE_FLAG_NONE		= 0,	/* Since: 0.1.4 */
	AS_APP_VALIDATE_FLAG_RELAX		= 1,	/* Since: 0.1.4 */
	AS_APP_VALIDATE_FLAG_STRICT		= 2,	/* Since: 0.1.4 */
	AS_APP_VALIDATE_FLAG_NO_NETWORK		= 4,	/* Since: 0.1.4 */
	/*< private >*/
	AS_APP_VALIDATE_FLAG_LAST
} AsAppValidateFlags;

/**
 * AsAppTrustFlags:
 * @AS_APP_TRUST_FLAG_COMPLETE:			Trusted data with no validation
 * @AS_APP_TRUST_FLAG_CHECK_DUPLICATES:		Check for duplicates
 * @AS_APP_TRUST_FLAG_CHECK_VALID_UTF8:		Check for valid UTF-8
 *
 * The flags to use when checking input.
 **/
typedef enum {
	AS_APP_TRUST_FLAG_COMPLETE		= 0,	/* Since: 0.2.2 */
	AS_APP_TRUST_FLAG_CHECK_DUPLICATES	= 1,	/* Since: 0.2.2 */
	AS_APP_TRUST_FLAG_CHECK_VALID_UTF8	= 2,	/* Since: 0.2.2 */
	/*< private >*/
	AS_APP_TRUST_FLAG_LAST
} AsAppTrustFlags;

/**
 * AsAppSourceKind:
 * @AS_APP_SOURCE_KIND_UNKNOWN:			Not sourced from a file
 * @AS_APP_SOURCE_KIND_APPSTREAM:		Sourced from a AppStream file
 * @AS_APP_SOURCE_KIND_DESKTOP:			Sourced from a desktop file
 * @AS_APP_SOURCE_KIND_APPDATA:			Sourced from a AppData file
 * @AS_APP_SOURCE_KIND_METAINFO:		Sourced from a MetaInfo file
 *
 * The source kind.
 **/
typedef enum {
	AS_APP_SOURCE_KIND_UNKNOWN,			/* Since: 0.1.4 */
	AS_APP_SOURCE_KIND_APPSTREAM,			/* Since: 0.1.4 */
	AS_APP_SOURCE_KIND_DESKTOP,			/* Since: 0.1.4 */
	AS_APP_SOURCE_KIND_APPDATA,			/* Since: 0.1.4 */
	AS_APP_SOURCE_KIND_METAINFO,			/* Since: 0.1.7 */
	/*< private >*/
	AS_APP_SOURCE_KIND_LAST
} AsAppSourceKind;

/**
 * AsAppState:
 * @AS_APP_STATE_UNKNOWN:			Unknown state
 * @AS_APP_STATE_INSTALLED:			Application is installed
 * @AS_APP_STATE_AVAILABLE:			Application is available
 * @AS_APP_STATE_AVAILABLE_LOCAL:		Application is locally available as a file
 * @AS_APP_STATE_UPDATABLE:			Application is installed and updatable
 * @AS_APP_STATE_UNAVAILABLE:			Application is referenced, but not available
 * @AS_APP_STATE_QUEUED_FOR_INSTALL:		Application is queued for install
 * @AS_APP_STATE_INSTALLING:			Application is being installed
 * @AS_APP_STATE_REMOVING:			Application is being removed
 *
 * The application state.
 **/
typedef enum {
	AS_APP_STATE_UNKNOWN,				/* Since: 0.2.2 */
	AS_APP_STATE_INSTALLED,				/* Since: 0.2.2 */
	AS_APP_STATE_AVAILABLE,				/* Since: 0.2.2 */
	AS_APP_STATE_AVAILABLE_LOCAL,			/* Since: 0.2.2 */
	AS_APP_STATE_UPDATABLE,				/* Since: 0.2.2 */
	AS_APP_STATE_UNAVAILABLE,			/* Since: 0.2.2 */
	AS_APP_STATE_QUEUED_FOR_INSTALL,		/* Since: 0.2.2 */
	AS_APP_STATE_INSTALLING,			/* Since: 0.2.2 */
	AS_APP_STATE_REMOVING,				/* Since: 0.2.2 */
	/*< private >*/
	AS_APP_STATE_LAST
} AsAppState;

#define	AS_APP_ERROR				as_app_error_quark ()

GType		 as_app_get_type		(void);
AsApp		*as_app_new			(void);
GQuark		 as_app_error_quark		(void);
AsAppSourceKind	 as_app_guess_source_kind	(const gchar	*filename);
AsAppSourceKind	 as_app_source_kind_from_string	(const gchar	*source_kind);
const gchar	*as_app_source_kind_to_string	(AsAppSourceKind source_kind);
const gchar	*as_app_state_to_string		(AsAppState	 state);

/* getters */
AsIconKind	 as_app_get_icon_kind		(AsApp		*app);
AsIdKind	 as_app_get_id_kind		(AsApp		*app);
AsAppSourceKind	 as_app_get_source_kind		(AsApp		*app);
AsAppState	 as_app_get_state		(AsApp		*app);
AsAppTrustFlags	 as_app_get_trust_flags		(AsApp		*app);
GList		*as_app_get_languages		(AsApp		*app);
GPtrArray	*as_app_get_addons		(AsApp		*app);
GPtrArray	*as_app_get_categories		(AsApp		*app);
GPtrArray	*as_app_get_compulsory_for_desktops (AsApp	*app);
GPtrArray	*as_app_get_extends		(AsApp		*app);
GPtrArray	*as_app_get_keywords		(AsApp		*app,
						 const gchar	*locale);
GPtrArray	*as_app_get_kudos		(AsApp		*app);
GPtrArray	*as_app_get_mimetypes		(AsApp		*app);
GPtrArray	*as_app_get_pkgnames		(AsApp		*app);
GPtrArray	*as_app_get_architectures	(AsApp		*app);
GPtrArray	*as_app_get_releases		(AsApp		*app);
GPtrArray	*as_app_get_provides		(AsApp		*app);
GPtrArray	*as_app_get_screenshots		(AsApp		*app);
GHashTable	*as_app_get_names		(AsApp		*app);
GHashTable	*as_app_get_comments		(AsApp		*app);
GHashTable	*as_app_get_developer_names	(AsApp		*app);
GHashTable	*as_app_get_metadata		(AsApp		*app);
GHashTable	*as_app_get_descriptions	(AsApp		*app);
GHashTable	*as_app_get_urls		(AsApp		*app);
GPtrArray	*as_app_get_vetos		(AsApp		*app);
const gchar	*as_app_get_icon		(AsApp		*app);
const gchar	*as_app_get_icon_path		(AsApp		*app);
const gchar	*as_app_get_id_filename		(AsApp		*app);
const gchar	*as_app_get_id			(AsApp		*app);
const gchar	*as_app_get_pkgname_default	(AsApp		*app);
const gchar	*as_app_get_source_pkgname	(AsApp		*app);
const gchar	*as_app_get_project_group	(AsApp		*app);
const gchar	*as_app_get_project_license	(AsApp		*app);
const gchar	*as_app_get_metadata_license	(AsApp		*app);
const gchar	*as_app_get_update_contact	(AsApp		*app);
const gchar	*as_app_get_source_file		(AsApp		*app);
const gchar	*as_app_get_name		(AsApp		*app,
						 const gchar	*locale);
const gchar	*as_app_get_comment		(AsApp		*app,
						 const gchar	*locale);
const gchar	*as_app_get_developer_name	(AsApp		*app,
						 const gchar	*locale);
const gchar	*as_app_get_description		(AsApp		*app,
						 const gchar	*locale);
gint		 as_app_get_priority		(AsApp		*app);
gint		 as_app_get_language		(AsApp		*app,
						 const gchar	*locale);
const gchar	*as_app_get_metadata_item	(AsApp		*app,
						 const gchar	*key);
const gchar	*as_app_get_url_item		(AsApp		*app,
						 AsUrlKind	 url_kind);
gboolean	 as_app_has_category		(AsApp		*app,
						 const gchar	*category);
gboolean	 as_app_has_kudo		(AsApp		*app,
						 const gchar	*kudo);
gboolean	 as_app_has_kudo_kind		(AsApp		*app,
						 AsKudoKind	 kudo);

/* setters */
void		 as_app_set_id			(AsApp		*app,
						 const gchar	*id,
						 gssize		 id_len);
void		 as_app_set_id_kind		(AsApp		*app,
						 AsIdKind	 id_kind);
void		 as_app_set_source_kind		(AsApp		*app,
						 AsAppSourceKind source_kind);
void		 as_app_set_state		(AsApp		*app,
						 AsAppState	 state);
void		 as_app_set_trust_flags		(AsApp		*app,
						 AsAppTrustFlags trust_flags);
void		 as_app_set_project_group	(AsApp		*app,
						 const gchar	*project_group,
						 gssize		 project_group_len);
void		 as_app_set_project_license	(AsApp		*app,
						 const gchar	*project_license,
						 gssize		 project_license_len);
void		 as_app_set_metadata_license	(AsApp		*app,
						 const gchar	*metadata_license,
						 gssize		 metadata_license_len);
void		 as_app_set_source_pkgname	(AsApp		*app,
						 const gchar	*source_pkgname,
						 gssize		 source_pkgname_len);
void		 as_app_set_update_contact	(AsApp		*app,
						 const gchar	*update_contact,
						 gssize		 update_contact_len);
void		 as_app_set_icon		(AsApp		*app,
						 const gchar	*icon,
						 gssize		 icon_len);
void		 as_app_set_icon_path		(AsApp		*app,
						 const gchar	*icon_path,
						 gssize		 icon_path_len);
void		 as_app_set_icon_kind		(AsApp		*app,
						 AsIconKind	 icon_kind);
void		 as_app_set_name		(AsApp		*app,
						 const gchar	*locale,
						 const gchar	*name,
						 gssize		 name_len);
void		 as_app_set_comment		(AsApp		*app,
						 const gchar	*locale,
						 const gchar	*comment,
						 gssize		 comment_len);
void		 as_app_set_developer_name	(AsApp		*app,
						 const gchar	*locale,
						 const gchar	*developer_name,
						 gssize		 developer_name_len);
void		 as_app_set_description		(AsApp		*app,
						 const gchar	*locale,
						 const gchar	*description,
						 gssize		 description_len);
void		 as_app_set_source_file		(AsApp		*app,
						 const gchar	*source_file);
void		 as_app_set_priority		(AsApp		*app,
						 gint		 priority);
void		 as_app_add_category		(AsApp		*app,
						 const gchar	*category,
						 gssize		 category_len);
void		 as_app_add_keyword		(AsApp		*app,
						 const gchar	*locale,
						 const gchar	*keyword,
						 gssize		 keyword_len);
void		 as_app_add_kudo		(AsApp		*app,
						 const gchar	*kudo,
						 gssize		 kudo_len);
void		 as_app_add_kudo_kind		(AsApp		*app,
						 AsKudoKind	 kudo_kind);
void		 as_app_add_mimetype		(AsApp		*app,
						 const gchar	*mimetype,
						 gssize		 mimetype_len);
void		 as_app_add_pkgname		(AsApp		*app,
						 const gchar	*pkgname,
						 gssize		 pkgname_len);
void		 as_app_add_arch		(AsApp		*app,
						 const gchar	*arch,
						 gssize		 arch_len);
void		 as_app_add_release		(AsApp		*app,
						 AsRelease	*release);
void		 as_app_add_provide		(AsApp		*app,
						 AsProvide	*provide);
void		 as_app_add_screenshot		(AsApp		*app,
						 AsScreenshot	*screenshot);
void		 as_app_add_language		(AsApp		*app,
						 gint		 percentage,
						 const gchar	*locale,
						 gssize		 locale_len);
void		 as_app_add_compulsory_for_desktop (AsApp	*app,
						 const gchar	*compulsory_for_desktop,
						 gssize		 compulsory_for_desktop_len);
void		 as_app_add_url			(AsApp		*app,
						 AsUrlKind	 url_kind,
						 const gchar	*url,
						 gssize		 url_len);
void		 as_app_add_metadata		(AsApp		*app,
						 const gchar	*key,
						 const gchar	*value,
						 gssize		 value_len);
void		 as_app_remove_metadata		(AsApp		*app,
						 const gchar	*key);
void		 as_app_add_addon		(AsApp		*app,
						 AsApp		*addon);
void		 as_app_add_extends		(AsApp		*app,
						 const gchar	*extends,
						 gssize		 extends_len);

/* object methods */
GPtrArray	*as_app_validate		(AsApp		*app,
						 AsAppValidateFlags flags,
						 GError		**error);
void		 as_app_subsume			(AsApp		*app,
						 AsApp		*donor);
void		 as_app_subsume_full		(AsApp		*app,
						 AsApp		*donor,
						 AsAppSubsumeFlags flags);
void		 as_app_add_veto		(AsApp		*app,
						 const gchar	*fmt,
						 ...)
						 G_GNUC_PRINTF(2,3);
guint		 as_app_search_matches_all	(AsApp		*app,
						 gchar		**search);
guint		 as_app_search_matches		(AsApp		*app,
						 const gchar	*search);
gboolean	 as_app_parse_file		(AsApp		*app,
						 const gchar	*filename,
						 AsAppParseFlags flags,
						 GError		**error);
gboolean	 as_app_to_file			(AsApp		*app,
						 GFile		*file,
						 GCancellable	*cancellable,
						 GError		**error);

G_END_DECLS

#endif /* __AS_APP_H */
