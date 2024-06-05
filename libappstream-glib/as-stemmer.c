/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <glib/gi18n.h>

#include "as-stemmer.h"
#include "as-ref-string.h"

struct _AsStemmer
{
	GObject			 parent_instance;
};

G_DEFINE_TYPE (AsStemmer, as_stemmer, G_TYPE_OBJECT)

/**
 * as_stemmer_process:
 * @stemmer: A #AsStemmer
 * @value: The input string
 *
 * Stems a string using the Porter algorithm.
 *
 * Since: 0.2.2
 *
 * Returns: A new refcounted string
 **/
AsRefString *
as_stemmer_process (AsStemmer *stemmer, const gchar *value)
{
	g_autofree gchar *value_casefold = NULL;
	value_casefold = g_utf8_casefold (value, -1);
	return as_ref_string_new (value_casefold);
}

static void
as_stemmer_class_init (AsStemmerClass *klass)
{
}

static void
as_stemmer_init (AsStemmer *stemmer)
{
}

/**
 * as_stemmer_new:
 *
 * Creates a new #AsStemmer.
 *
 * Returns: (transfer full): a #AsStemmer
 *
 * Since: 0.2.2
 **/
AsStemmer *
as_stemmer_new (void)
{
	AsStemmer *stemmer = g_object_new (AS_TYPE_STEMMER, NULL);
	return AS_STEMMER (stemmer);
}
