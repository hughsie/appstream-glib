/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <glib/gi18n.h>

#ifdef HAVE_LIBSTEMMER
  #include "libstemmer.h"
#endif

#include "as-stemmer.h"
#include "as-ref-string.h"

struct _AsStemmer
{
	GObject			 parent_instance;
	gboolean		 enabled;
	GHashTable		*hash;
	struct sb_stemmer	*ctx;
	GMutex			 ctx_mutex;
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
#ifdef HAVE_LIBSTEMMER
	AsRefString *new;
	const gchar *tmp;
	gsize value_len;
	g_autofree gchar *value_casefold = NULL;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&stemmer->ctx_mutex);

	/* look for word in the cache */
	new = g_hash_table_lookup (stemmer->hash, value);
	if (new != NULL)
		return as_ref_string_ref (new);

	/* not enabled */
	value_casefold = g_utf8_casefold (value, -1);
	if (stemmer->ctx == NULL || !stemmer->enabled)
		return as_ref_string_new (value_casefold);

	/* stem, then add to the cache */
	value_len = strlen (value_casefold);
	tmp = (const gchar *) sb_stemmer_stem (stemmer->ctx,
					       (guchar *) value_casefold,
					       (gint) value_len);
	if (value_len == (gsize) sb_stemmer_length (stemmer->ctx)) {
		new = as_ref_string_new_with_length (value_casefold, value_len);
	} else {
		new = as_ref_string_new (tmp);
	}
	g_hash_table_insert (stemmer->hash,
			     as_ref_string_new (value_casefold),
			     as_ref_string_ref (new));
	return new;
#else
	g_autofree gchar *value_casefold = NULL;
	value_casefold = g_utf8_casefold (value, -1);
	return as_ref_string_new (value_casefold);
#endif
}

static void
as_stemmer_finalize (GObject *object)
{
	AsStemmer *stemmer = AS_STEMMER (object);
#ifdef HAVE_LIBSTEMMER
	sb_stemmer_delete (stemmer->ctx);
	g_mutex_clear (&stemmer->ctx_mutex);
#endif
	g_hash_table_unref (stemmer->hash);
	G_OBJECT_CLASS (as_stemmer_parent_class)->finalize (object);
}

static void
as_stemmer_class_init (AsStemmerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_stemmer_finalize;
}

static void
as_stemmer_init (AsStemmer *stemmer)
{
	/* FIXME: use as_utils_locale_to_language()? */
#ifdef HAVE_LIBSTEMMER
	stemmer->ctx = sb_stemmer_new ("en", NULL);
	g_mutex_init (&stemmer->ctx_mutex);
#endif
	stemmer->enabled = g_getenv ("APPSTREAM_GLIB_DISABLE_STEMMER") == NULL;
	stemmer->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) as_ref_string_unref,
					       (GDestroyNotify) as_ref_string_unref);
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
