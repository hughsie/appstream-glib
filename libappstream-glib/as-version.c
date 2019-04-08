/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:as-version
 * @short_description: Helpers for the current runtime version.
 * @include: appstream-glib.h
 * @stability: Unstable
 *
 * The version number can be different between build-time and runtime.
 */

#include "config.h"

#include <glib.h>

#include "as-version.h"

/**
 * as_version_string:
 *
 * Gets the libappstream-glib installed runtime version.
 *
 * Returns: a version number, e.g. "0.7.8"
 *
 * Since: 0.7.8
 **/
const gchar *
as_version_string (void)
{
	return G_STRINGIFY(AS_MAJOR_VERSION) "."
		G_STRINGIFY(AS_MINOR_VERSION) "."
		G_STRINGIFY(AS_MICRO_VERSION);
}
