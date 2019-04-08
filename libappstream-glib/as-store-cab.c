/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

//#include <glib/gstdio.h>
//#include <gio/gunixinputstream.h>

#include "as-store-cab.h"
//#include "as-utils.h"

gboolean
as_store_cab_from_bytes (AsStore *store,
			 GBytes *bytes,
			 GCancellable *cancellable,
			 GError **error)
{
	g_set_error (error,
		     AS_STORE_ERROR,
		     AS_STORE_ERROR_FAILED,
		     "Loading firmware is no longer supported, see fwupd");
	return FALSE;
}

gboolean
as_store_cab_from_file (AsStore *store,
			GFile *file,
			GCancellable *cancellable,
			GError **error)
{
	g_set_error (error,
		     AS_STORE_ERROR,
		     AS_STORE_ERROR_FAILED,
		     "Loading firmware is no longer supported, see fwupd");
	return FALSE;
}
