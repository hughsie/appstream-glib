/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#ifdef HAVE_LIBSTEMMER
  #include "libstemmer.h"
#endif

#include "as-stemmer.h"

struct _AsStemmer
{
	GObject			 parent_instance;
	gboolean		 enabled;
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
 * Returns: A new string
 **/
gchar *
as_stemmer_process (AsStemmer *stemmer, const gchar *value)
{
#ifdef HAVE_LIBSTEMMER
	gchar *new;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&stemmer->ctx_mutex);
	if (stemmer->ctx == NULL || !stemmer->enabled)
		return g_strdup (value);
	new = g_strdup ((gchar *) sb_stemmer_stem (stemmer->ctx,
						   (guchar *) value,
						   strlen (value)));
//	if (g_strcmp0 (value, new) != 0)
//		g_debug ("stemmed %s->%s", value, new);
	return new;
#else
	return g_strdup (value);
#endif
}

static void
as_stemmer_finalize (GObject *object)
{
#ifdef HAVE_LIBSTEMMER
	AsStemmer *stemmer = AS_STEMMER (object);
	sb_stemmer_delete (stemmer->ctx);
	g_mutex_clear (&stemmer->ctx_mutex);
#endif
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
