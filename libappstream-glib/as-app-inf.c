/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <string.h>

#include "as-app-private.h"
#include "as-inf.h"

gboolean
as_app_parse_inf_file (AsApp *app,
		       const gchar *filename,
		       AsAppParseFlags flags,
		       GError **error)
{
	g_set_error (error,
		     AS_INF_ERROR,
		     AS_INF_ERROR_FAILED,
		     "Loading .inf data is no longer supported, see libginf");
	return FALSE;
}
