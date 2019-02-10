/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2018 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2018 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>

#include "as-checksum.h"

G_BEGIN_DECLS

#define AS_TYPE_RELEASE	(as_release_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsRelease, as_release, AS, RELEASE, GObject)

struct _AsReleaseClass
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
 * AsReleaseKind:
 * @AS_RELEASE_KIND_UNKNOWN:		Unknown release type
 * @AS_RELEASE_KIND_STABLE:		A stable release
 * @AS_RELEASE_KIND_DEVELOPMENT:	A development release
 *
 * The release size kind.
 *
 * Since: 0.7.6
 **/
typedef enum {
	AS_RELEASE_KIND_UNKNOWN,		/* Since: 0.7.6 */
	AS_RELEASE_KIND_STABLE,			/* Since: 0.7.6 */
	AS_RELEASE_KIND_DEVELOPMENT,		/* Since: 0.7.6 */
	/*< private >*/
	AS_RELEASE_KIND_LAST
} AsReleaseKind;

/**
 * AsReleaseState:
 * @AS_RELEASE_STATE_UNKNOWN:			Unknown state
 * @AS_RELEASE_STATE_INSTALLED:			Release is installed
 * @AS_RELEASE_STATE_AVAILABLE:			Release is available
 *
 * The release state.
 **/
typedef enum {
	AS_RELEASE_STATE_UNKNOWN,		/* Since: 0.5.8 */
	AS_RELEASE_STATE_INSTALLED,		/* Since: 0.5.8 */
	AS_RELEASE_STATE_AVAILABLE,		/* Since: 0.5.8 */
	/*< private >*/
	AS_RELEASE_STATE_LAST
} AsReleaseState;

AsRelease	*as_release_new			(void);
gint		 as_release_vercmp		(AsRelease	*rel1,
						 AsRelease	*rel2);

AsReleaseKind	 as_release_kind_from_string	(const gchar	*kind_str);
const gchar	*as_release_kind_to_string	(AsReleaseKind	 kind);
AsReleaseState	 as_release_state_from_string	(const gchar	*state);
const gchar	*as_release_state_to_string	(AsReleaseState	 state);

/* getters */
const gchar	*as_release_get_version		(AsRelease	*release);
GBytes		*as_release_get_blob		(AsRelease	*release,
						 const gchar	*filename);
guint64		 as_release_get_timestamp	(AsRelease	*release);
guint64		 as_release_get_install_duration(AsRelease	*release);
const gchar	*as_release_get_description	(AsRelease	*release,
						 const gchar	*locale);
GPtrArray	*as_release_get_locations	(AsRelease	*release);
const gchar	*as_release_get_location_default (AsRelease	*release);
AsChecksum	*as_release_get_checksum_by_fn	(AsRelease	*release,
						 const gchar	*fn);
AsChecksum	*as_release_get_checksum_by_target (AsRelease	*release,
						 AsChecksumTarget target);
GPtrArray	*as_release_get_checksums	(AsRelease	*release);
AsUrgencyKind	 as_release_get_urgency		(AsRelease	*release);
AsReleaseKind	 as_release_get_kind		(AsRelease	*release);
AsReleaseState	 as_release_get_state		(AsRelease	*release);
guint64		 as_release_get_size		(AsRelease	*release,
						 AsSizeKind	 kind);
const gchar	*as_release_get_url		(AsRelease	*release,
						 AsUrlKind	 url_kind);

/* setters */
void		 as_release_set_version		(AsRelease	*release,
						 const gchar	*version);
void		 as_release_set_blob		(AsRelease	*release,
						 const gchar	*filename,
						 GBytes		*blob);
void		 as_release_set_timestamp	(AsRelease	*release,
						 guint64	 timestamp);
void		 as_release_set_install_duration(AsRelease	*release,
						 guint64	 install_duration);
void		 as_release_set_description	(AsRelease	*release,
						 const gchar	*locale,
						 const gchar	*description);
void		 as_release_add_location	(AsRelease	*release,
						 const gchar	*location);
void		 as_release_add_checksum	(AsRelease	*release,
						 AsChecksum	*checksum);
void		 as_release_set_urgency		(AsRelease	*release,
						 AsUrgencyKind	 urgency);
void		 as_release_set_kind		(AsRelease	*release,
						 AsReleaseKind	 kind);
void		 as_release_set_state		(AsRelease	*release,
						 AsReleaseState	 state);
void		 as_release_set_size		(AsRelease	*release,
						 AsSizeKind	 kind,
						 guint64	 size);
void		 as_release_set_url		(AsRelease	*release,
						 AsUrlKind	 url_kind,
						 const gchar	*url);

G_END_DECLS
