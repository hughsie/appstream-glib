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

#include "config.h"

#include <stdlib.h>

#include "as-node.h"
#include "as-release.h"

typedef struct _AsReleasePrivate	AsReleasePrivate;
struct _AsReleasePrivate
{
	gchar		*version;
	gchar		*description;
	guint64		 timestamp;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsRelease, as_release, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_release_get_instance_private (o))

/**
 * as_release_finalize:
 **/
static void
as_release_finalize (GObject *object)
{
	AsRelease *release = AS_RELEASE (object);
	AsReleasePrivate *priv = GET_PRIVATE (release);

	g_free (priv->version);
	g_free (priv->description);

	G_OBJECT_CLASS (as_release_parent_class)->finalize (object);
}

/**
 * as_release_init:
 **/
static void
as_release_init (AsRelease *release)
{
}

/**
 * as_release_class_init:
 **/
static void
as_release_class_init (AsReleaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_release_finalize;
}

/**
 * as_release_get_version:
 **/
const gchar *
as_release_get_version (AsRelease *release)
{
	AsReleasePrivate *priv = GET_PRIVATE (release);
	return priv->version;
}

/**
 * as_release_get_timestamp:
 **/
guint64
as_release_get_timestamp (AsRelease *release)
{
	AsReleasePrivate *priv = GET_PRIVATE (release);
	return priv->timestamp;
}

/**
 * as_release_get_description:
 **/
const gchar *
as_release_get_description (AsRelease *release)
{
	AsReleasePrivate *priv = GET_PRIVATE (release);
	return priv->description;
}

/**
 * as_release_set_version:
 **/
void
as_release_set_version (AsRelease *release, const gchar *version)
{
	AsReleasePrivate *priv = GET_PRIVATE (release);
	priv->version = g_strdup (version);
}

/**
 * as_release_set_timestamp:
 **/
void
as_release_set_timestamp (AsRelease *release, guint64 timestamp)
{
	AsReleasePrivate *priv = GET_PRIVATE (release);
	priv->timestamp = timestamp;
}

/**
 * as_release_set_description:
 **/
void
as_release_set_description (AsRelease *release, const gchar *description)
{
	AsReleasePrivate *priv = GET_PRIVATE (release);
	priv->description = g_strdup (description);
}

/**
 * as_release_node_insert:
 **/
GNode *
as_release_node_insert (AsRelease *release, GNode *parent)
{
	AsReleasePrivate *priv = GET_PRIVATE (release);
	GNode *n;
	gchar *timestamp_str;

	timestamp_str = g_strdup_printf ("%" G_GUINT64_FORMAT,
					 priv->timestamp);
	n = as_node_insert (parent, "release", priv->description,
			    AS_NODE_INSERT_FLAG_NONE,
			    "timestamp", timestamp_str,
			    "version", priv->version,
			    NULL);
	g_free (timestamp_str);
	return n;
}

/**
 * as_release_node_parse:
 **/
gboolean
as_release_node_parse (AsRelease *release, GNode *node, GError **error)
{
	const gchar *tmp;
	tmp = as_node_get_attribute (node, "timestamp");
	if (tmp != NULL)
		as_release_set_timestamp (release, atol (tmp));
	tmp = as_node_get_attribute (node, "version");
	if (tmp != NULL)
		as_release_set_version (release, tmp);
	tmp = as_node_get_data (node);
	if (tmp != NULL)
		as_release_set_description (release, tmp);
	return TRUE;
}

/**
 * as_release_new:
 **/
AsRelease *
as_release_new (void)
{
	AsRelease *release;
	release = g_object_new (AS_TYPE_RELEASE, NULL);
	return AS_RELEASE (release);
}
