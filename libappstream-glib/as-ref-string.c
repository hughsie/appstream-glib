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

static GHashTable	*as_ref_string_hash = NULL;
static GMutex		 as_ref_string_mutex;

static GHashTable *
as_ref_string_get_hash_safe (void)
{
	if (as_ref_string_hash == NULL) {
		/* gpointer to AsRefStringHeader */
		as_ref_string_hash = g_hash_table_new_full (g_direct_hash,
							    g_direct_equal,
							    (GDestroyNotify) as_ref_string_unref_from_str,
							    NULL);
	}
	return as_ref_string_hash;
}

static void __attribute__ ((destructor))
as_ref_string_destructor (void)
{
	g_clear_pointer (&as_ref_string_hash, g_hash_table_unref);
}

/**
 * as_ref_string_new_static:
 * @str: a string
 *
 * Returns a refcounted string from a static string. The static string cannot
 * be unloaded and freed.
 *
 * Returns: a %AsRefString
 *
 * Since: 0.6.6
 */

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
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&as_ref_string_mutex);

	/* create object */
	hdr = g_malloc (len + sizeof (AsRefStringHeader) + 1);
	hdr->refcnt = 1;
	rstr_new = AS_REFPTR_FROM_HEADER (hdr);
	memcpy (rstr_new, str, len);
	rstr_new[len] = '\0';

	/* add */
	g_hash_table_add (as_ref_string_get_hash_safe (), rstr_new);

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
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&as_ref_string_mutex);

	g_return_val_if_fail (str != NULL, NULL);

	/* already in hash */
	if (g_hash_table_contains (as_ref_string_get_hash_safe (), str)) {
		hdr = AS_REFPTR_TO_HEADER (str);
		if (hdr->refcnt < 0)
			return str;
		g_atomic_int_inc (&hdr->refcnt);
		return str;
	}

	g_clear_pointer (&locker, g_mutex_locker_free);
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
	if (hdr->refcnt < 0)
		return rstr;
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
	if (hdr->refcnt < 0)
		return rstr;
	if (g_atomic_int_dec_and_test (&hdr->refcnt)) {
		g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&as_ref_string_mutex);
		g_hash_table_remove (as_ref_string_get_hash_safe (), rstr);
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

static gint
as_ref_string_sort_by_refcnt_cb (gconstpointer a, gconstpointer b)
{
	AsRefStringHeader *hdr1 = AS_REFPTR_TO_HEADER (a);
	AsRefStringHeader *hdr2 = AS_REFPTR_TO_HEADER (b);
	if (hdr1->refcnt > hdr2->refcnt)
		return -1;
	if (hdr1->refcnt < hdr2->refcnt)
		return 1;
	return 0;
}

/**
 * as_ref_string_debug:
 * @flags: some #AsRefStringDebugFlags, e.g. %AS_REF_STRING_DEBUG_DUPES
 *
 * This function outputs some debugging information to a string.
 *
 * Returns: a string describing the current state of the dedupe hash.
 *
 * Since: 0.6.6
 */
gchar *
as_ref_string_debug (AsRefStringDebugFlags flags)
{
	GHashTable *hash = NULL;
	GString *tmp = g_string_new (NULL);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&as_ref_string_mutex);

	/* overview */
	hash = as_ref_string_get_hash_safe ();
	g_string_append_printf (tmp, "Size of hash table: %u\n",
				g_hash_table_size (hash));

	/* success: deduped */
	if (flags & AS_REF_STRING_DEBUG_DEDUPED) {
		GList *l;
		g_autoptr(GList) keys = g_hash_table_get_keys (hash);

		/* split up sections */
		if (tmp->len > 0)
			g_string_append (tmp, "\n\n");

		/* sort by refcount number */
		keys = g_list_sort (keys, as_ref_string_sort_by_refcnt_cb);
		g_string_append (tmp, "Deduplicated strings:\n");
		for (l = keys; l != NULL; l = l->next) {
			const gchar *str = l->data;
			AsRefStringHeader *hdr = AS_REFPTR_TO_HEADER (str);
			if (hdr->refcnt <= 1)
				continue;
			g_string_append_printf (tmp, "%i\t%s\n", hdr->refcnt, str);
		}
	}

	/* failed: duplicate */
	if (flags & AS_REF_STRING_DEBUG_DUPES) {
		GList *l;
		GList *l2;
		g_autoptr(GHashTable) dupes = g_hash_table_new (g_direct_hash, g_direct_equal);
		g_autoptr(GList) keys = g_hash_table_get_keys (hash);

		/* split up sections */
		if (tmp->len > 0)
			g_string_append (tmp, "\n\n");

		g_string_append (tmp, "Duplicated strings:\n");
		for (l = keys; l != NULL; l = l->next) {
			const gchar *str = l->data;
			AsRefStringHeader *hdr = AS_REFPTR_TO_HEADER (str);
			guint dupe_cnt = 0;

			if (g_hash_table_contains (dupes, hdr))
				continue;
			g_hash_table_add (dupes, (gpointer) hdr);

			for (l2 = l; l2 != NULL; l2 = l2->next) {
				const gchar *str2 = l2->data;
				AsRefStringHeader *hdr2 = AS_REFPTR_TO_HEADER (str2);
				if (g_hash_table_contains (dupes, hdr2))
					continue;
				if (l == l2)
					continue;
				if (g_strcmp0 (str, str2) != 0)
					continue;
				g_hash_table_add (dupes, (gpointer) hdr2);
				dupe_cnt += 1;
			}
			if (dupe_cnt > 0) {
				g_string_append_printf (tmp, "%u\t%s\n",
							dupe_cnt, str);
			}
		}
	}
	return g_string_free (tmp, FALSE);
}
