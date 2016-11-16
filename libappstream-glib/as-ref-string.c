/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:as-ref-string
 * @short_description: Reference counted strings
 * @include: appstream-glib.h
 * @stability: Unstable
 *
 * These functions are used to implement refcounted C strings.
 */

#include "config.h"

#include <string.h>

#include "as-ref-string.h"

typedef struct {
	volatile gint	refcnt;
} AsRefStringHeader;

#define AS_REFPTR_TO_HEADER(o)		((AsRefStringHeader *) ((void *) ((guint8 *) o - sizeof (AsRefStringHeader))))
#define AS_REFPTR_FROM_HEADER(o)	((gpointer) (((guint8 *) o) + sizeof (AsRefStringHeader)))

static void
as_ref_string_unref_from_str (gchar *str)
{
	AsRefStringHeader *hdr = AS_REFPTR_TO_HEADER (str);
	g_free (hdr);
}

static GHashTable *
as_ref_string_get_hash (void)
{
	static GHashTable *hash = NULL;
	if (hash == NULL) {
		/* gpointer to AsRefStringHeader */
		hash = g_hash_table_new_full (g_direct_hash,
					      g_direct_equal,
					      (GDestroyNotify) as_ref_string_unref_from_str,
					      NULL);
	}
	return hash;
}

/**
 * as_ref_string_new_copy_with_length:
 * @str: a string
 * @len: length of @str, not including the NUL byte
 *
 * Returns a deep copied refcounted string. The returned string can be modified
 * without affecting other refcounted versions.
 *
 * Returns: a %AsRefString
 *
 * Since: 0.6.6
 */
AsRefString *
as_ref_string_new_copy_with_length (const gchar *str, gsize len)
{
	AsRefStringHeader *hdr;
	AsRefString *rstr_new;
	GHashTable *hash = as_ref_string_get_hash ();

	/* create object */
	hdr = g_malloc (len + sizeof (AsRefStringHeader) + 1);
	hdr->refcnt = 1;
	rstr_new = AS_REFPTR_FROM_HEADER (hdr);
	memcpy (rstr_new, str, len);
	rstr_new[len] = '\0';

	/* add */
	g_hash_table_add (hash, rstr_new);

	/* return to data, not the header */
	return rstr_new;
}

/**
 * as_ref_string_new_copy:
 * @str: a string
 *
 * Returns a deep copied refcounted string. The returned string can be modified
 * without affecting other refcounted versions.
 *
 * Returns: a %AsRefString
 *
 * Since: 0.6.6
 */
AsRefString *
as_ref_string_new_copy (const gchar *str)
{
	g_return_val_if_fail (str != NULL, NULL);
	return as_ref_string_new_copy_with_length (str, strlen (str));
}

/**
 * as_ref_string_new_with_length:
 * @str: a string
 * @len: length of @str, not including the NUL byte
 *
 * Returns a immutable refcounted string. The returned string cannot be modified
 * without affecting other refcounted versions.
 *
 * Returns: a %AsRefString
 *
 * Since: 0.6.6
 */
AsRefString *
as_ref_string_new_with_length (const gchar *str, gsize len)
{
	AsRefStringHeader *hdr;
	GHashTable *hash = as_ref_string_get_hash ();

	g_return_val_if_fail (str != NULL, NULL);

	/* already in hash */
	if (g_hash_table_contains (hash, str)) {
		hdr = AS_REFPTR_TO_HEADER (str);
		g_atomic_int_inc (&hdr->refcnt);
		return str;
	}
	return as_ref_string_new_copy_with_length (str, len);
}

/**
 * as_ref_string_new:
 * @str: a string
 *
 * Returns a immutable refcounted string. The returned string cannot be modified
 * without affecting other refcounted versions.
 *
 * Returns: a %AsRefString
 *
 * Since: 0.6.6
 */
AsRefString *
as_ref_string_new (const gchar *str)
{
	g_return_val_if_fail (str != NULL, NULL);
	return as_ref_string_new_with_length (str, strlen (str));
}

/**
 * as_ref_string_ref:
 * @rstr: a #AsRefString
 *
 * Adds a reference to the string.
 *
 * Returns: the same %AsRefString
 *
 * Since: 0.6.6
 */
AsRefString *
as_ref_string_ref (AsRefString *rstr)
{
	AsRefStringHeader *hdr;
	g_return_val_if_fail (rstr != NULL, NULL);
	hdr = AS_REFPTR_TO_HEADER (rstr);
	g_atomic_int_inc (&hdr->refcnt);
	return rstr;
}

/**
 * as_ref_string_unref:
 * @rstr: a #AsRefString
 *
 * Removes a reference to the string.
 *
 * Returns: the same %AsRefString, or %NULL if the refcount dropped to zero
 *
 * Since: 0.6.6
 */
AsRefString *
as_ref_string_unref (AsRefString *rstr)
{
	AsRefStringHeader *hdr;

	g_return_val_if_fail (rstr != NULL, NULL);

	hdr = AS_REFPTR_TO_HEADER (rstr);
	if (g_atomic_int_dec_and_test (&hdr->refcnt)) {
		GHashTable *hash = as_ref_string_get_hash ();
		g_hash_table_remove (hash, rstr);
		return NULL;
	}
	return rstr;
}

/**
 * as_ref_string_assign:
 * @rstr_ptr: (out): a #AsRefString
 * @rstr: a #AsRefString
 *
 * This function unrefs and clears @rstr_ptr if set, then sets @rstr if
 * non-NULL. If @rstr and @rstr_ptr are the same string the action is ignored.
 *
 * This function can ONLY be used when @str is guaranteed to be a
 * refcounted string and is suitable for use when getting strings from
 * methods without a fixed API.
 *
 * This function is slightly faster than as_ref_string_assign_safe() as no
 * hash table lookup is done on the @rstr pointer.
 *
 * Since: 0.6.6
 */
void
as_ref_string_assign (AsRefString **rstr_ptr, AsRefString *rstr)
{
	g_return_if_fail (rstr_ptr != NULL);
	if (*rstr_ptr == rstr)
		return;
	if (*rstr_ptr != NULL) {
		as_ref_string_unref (*rstr_ptr);
		*rstr_ptr = NULL;
	}
	if (rstr != NULL)
		*rstr_ptr = as_ref_string_ref (rstr);
}

/**
 * as_ref_string_assign_safe:
 * @rstr_ptr: (out): a #AsRefString
 * @str: a string, or a #AsRefString
 *
 * This function unrefs and clears @rstr_ptr if set, then sets @rstr if
 * non-NULL. If @rstr and @rstr_ptr are the same string the action is ignored.
 *
 * This function should be used when @str cannot be guaranteed to be a
 * refcounted string and is suitable for use in existing object setters.
 *
 * Since: 0.6.6
 */
void
as_ref_string_assign_safe (AsRefString **rstr_ptr, const gchar *str)
{
	g_return_if_fail (rstr_ptr != NULL);
	if (*rstr_ptr != NULL) {
		as_ref_string_unref (*rstr_ptr);
		*rstr_ptr = NULL;
	}
	if (str != NULL)
		*rstr_ptr = as_ref_string_new (str);
}
