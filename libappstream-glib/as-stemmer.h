/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "as-ref-string.h"

G_BEGIN_DECLS

#define AS_TYPE_STEMMER	(as_stemmer_get_type ())

G_DECLARE_FINAL_TYPE (AsStemmer, as_stemmer, AS, STEMMER, GObject)

AsStemmer	*as_stemmer_new			(void);
AsRefString	*as_stemmer_process		(AsStemmer	*stemmer,
						 const gchar	*value);

G_END_DECLS
