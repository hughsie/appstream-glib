/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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
 * SECTION:as-inf
 * @short_description: Helper functions for parsing .inf files
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * These functions are used internally to libappstream-glib, and some may be
 * useful to user-applications.
 */

#include "config.h"

#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>

#include "as-inf.h"

/**
 * as_inf_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.3.7
 **/
GQuark
as_inf_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsInfError");
	return quark;
}

typedef struct {
	GKeyFile		*keyfile;
	GHashTable		*dict;
	gboolean		 last_line_continuation;
	gboolean		 last_line_continuation_ignore;
	gboolean		 require_2nd_pass;
	gchar			*group;
	gchar			*last_key;
	gchar			*comment;
	gchar			*comment_group;
	guint			 nokey_idx;
	AsInfLoadFlags		 flags;
} AsInfHelper;

/**
 * as_inf_make_case_insensitive:
 */
static gchar *
as_inf_make_case_insensitive (AsInfHelper *helper, const gchar *text)
{
	if (helper->flags & AS_INF_LOAD_FLAG_CASE_INSENSITIVE)
		return g_ascii_strdown (text, -1);
	return g_strdup (text);
}

/**
 * as_inf_string_isdigits:
 */
static gboolean
as_inf_string_isdigits (const gchar *str)
{
	guint i;
	for (i = 0; str[i] != '\0'; i++) {
		if (!g_ascii_isdigit (str[i]))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_inf_replace_variable:
 *
 * Replaces any instance of %val% with the 'val' key value found in @dict.
 */
static gchar *
as_inf_replace_variable (AsInfHelper *helper, const gchar *line, GError **error)
{
	GString *new;
	const gchar *tmp;
	guint i;
	g_auto(GStrv) split = NULL;

	/* split up into sections of the delimiter */
	new = g_string_sized_new (strlen (line));
	split = g_strsplit (line, "%", -1);
	for (i = 0; split[i] != NULL; i++) {
		g_autofree gchar *lower = NULL;

		/* the text between the substitutions */
		if (i % 2 == 0) {
			g_strdelimit (split[i], "\\", '/');
			g_string_append (new, split[i]);
			continue;
		}

		/* a Dirid */
		if (as_inf_string_isdigits (split[i])) {
			g_string_append (new, "/tmp");
			continue;
		}

		/* replace or ignore things not (yet) in the dictionary */
		lower = as_inf_make_case_insensitive (helper, split[i]);
		tmp = g_hash_table_lookup (helper->dict, lower);
		if (tmp == NULL) {
			if (helper->flags & AS_INF_LOAD_FLAG_STRICT) {
				g_set_error (error,
					     AS_INF_ERROR,
					     AS_INF_ERROR_FAILED,
					     "No replacement for '%s' in [Strings]",
					     lower);
				g_string_free (new, TRUE);
				return NULL;
			}
			g_string_append_printf (new, "%%%s%%", split[i]);
			continue;
		}
		g_string_append (new, tmp);
	}
	return g_string_free (new, FALSE);
}

/**
 * as_inf_get_dict:
 *
 * Puts all the strings in [Strings] into a hash table.
 */
static GHashTable *
as_inf_get_dict (AsInfHelper *helper, GError **error)
{
	GHashTable *dict = NULL;
	gchar *val;
	guint i;
	g_autofree gchar *lower = NULL;
	g_autoptr(GHashTable) dict_tmp = NULL;
	g_auto(GStrv) keys = NULL;

	dict_tmp = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	lower = as_inf_make_case_insensitive (helper, "Strings");
	keys = g_key_file_get_keys (helper->keyfile, lower, NULL, NULL);
	for (i = 0; keys != NULL && keys[i] != NULL; i++) {
		val = g_key_file_get_string (helper->keyfile, lower, keys[i], error);
		if (val == NULL)
			goto out;
		g_hash_table_insert (dict_tmp, g_strdup (keys[i]), val);
	}
	dict = g_hash_table_ref (dict_tmp);
out:
	return dict;
}

/**
 * as_inf_replace_variables:
 *
 * Replaces any key file value with it's substitution from [Strings]
 */
static gboolean
as_inf_replace_variables (AsInfHelper *helper, GError **error)
{
	guint i;
	guint j;
	g_auto(GStrv) groups = NULL;

	groups = g_key_file_get_groups (helper->keyfile, NULL);
	for (i = 0; groups[i] != NULL; i++) {
		g_auto(GStrv) keys = NULL;

		/* ignore the source */
		if (g_strcmp0 (groups[i], "Strings") == 0)
			continue;

		/* fix up keys */
		keys = g_key_file_get_keys (helper->keyfile, groups[i], NULL, NULL);
		if (keys == NULL)
			continue;
		for (j = 0; keys[j] != NULL; j++) {
			g_autofree gchar *data_old = NULL;
			g_autofree gchar *data_new = NULL;

			/* get the old data for this [group] key */
			data_old = g_key_file_get_string (helper->keyfile,
							  groups[i],
							  keys[j],
							  NULL);
			if (data_old == NULL || data_old[0] == '\0')
				continue;

			/* optimise a little */
			if (g_strstr_len (data_old, -1, "%") == NULL)
				continue;

			/* do string replacement */
			data_new = as_inf_replace_variable (helper,
							    data_old,
							    error);
			if (data_new == NULL)
				return FALSE;
			if (g_strcmp0 (data_old, data_new) == 0)
				continue;
			g_key_file_set_string (helper->keyfile,
					       groups[i],
					       keys[j],
					       data_new);
		}
	}
	return TRUE;
}

/**
 * as_inf_set_last_key:
 */
static void
as_inf_set_last_key (AsInfHelper *helper, const gchar *key)
{
	/* same value */
	if (key == helper->last_key)
		return;
	g_free (helper->last_key);
	helper->last_key = g_strdup (key);
}

/**
 * as_inf_set_comment:
 */
static void
as_inf_set_comment (AsInfHelper *helper, const gchar *comment)
{
	/* same value */
	if (comment == helper->comment)
		return;

	g_free (helper->comment);
	helper->comment = g_strdup (comment);
	if (helper->comment != NULL)
		g_strchug (helper->comment);
}

/**
 * as_inf_strcheckchars:
 */
static gboolean
as_inf_strcheckchars (const gchar *str, const gchar *chars)
{
	guint i;
	guint j;
	for (i = 0; str[i] != '\0'; i++) {
		for (j = 0; chars[j] != '\0'; j++) {
			if (str[i] == chars[j])
				return TRUE;
		}
	}
	return FALSE;
}

/**
 * as_inf_strip_value:
 *
 * Strips off any comments and quotes from the .inf key value.
 */
static gboolean
as_inf_strip_value (AsInfHelper *helper, gchar *value, GError **error)
{
	gchar *comment;
	gchar *last_quote;

	/* trivial case; no quotes */
	if (value[0] != '"') {
		comment = g_strrstr (value + 1, ";");
		if (comment != NULL) {
			*comment = '\0';
			if (helper->comment == NULL)
				as_inf_set_comment (helper, comment + 1);
			g_strchomp (value + 1);
		}
		return TRUE;
	}

	/* is there only one quote? */
	last_quote = g_strrstr (value, "\"");
	if (last_quote == value) {
		if (helper->flags & AS_INF_LOAD_FLAG_STRICT) {
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "mismatched quotes %s", value);
			return FALSE;
		}
		last_quote = NULL;
	}

	/* shift the string down */
	memmove (value, value + 1, strlen (value));

	/* there's a comment past the last quote */
	comment = g_strrstr (value, ";");
	if (comment != NULL && comment > last_quote) {
		*comment = '\0';
		if (helper->comment == NULL)
			as_inf_set_comment (helper, comment + 1);
		g_strchomp (value);
	}

	/* remove the last quote */
	if (last_quote != NULL)
		*(last_quote-1) = '\0';

	return TRUE;
}

/**
 * as_inf_set_group:
 */
static gboolean
as_inf_set_group (AsInfHelper *helper, const gchar *group, GError **error)
{
	/* same value */
	if (group == helper->group)
		return TRUE;

	/* maximum permitted length is 255 chars */
	if (strlen (group) > 255) {
		if (helper->flags & AS_INF_LOAD_FLAG_STRICT) {
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "section name is too long: %s",
				     group);
			return FALSE;
		}
	}

	/* save new value */
	g_free (helper->group);
	helper->group = as_inf_make_case_insensitive (helper, group);

	/* section names can contain double quotes */
	if (!as_inf_strip_value (helper, helper->group, error))
		return FALSE;

	/* does this section name have any banned chars */
	if (helper->flags & AS_INF_LOAD_FLAG_STRICT) {
		if (g_str_has_prefix (helper->group, " ")) {
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "section name '%s' has leading spaces",
				     helper->group);
			return FALSE;
		}
		if (g_str_has_suffix (helper->group, " ")) {
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "section name '%s' has trailing spaces",
				     helper->group);
			return FALSE;
		}
		if (as_inf_strcheckchars (helper->group, "\t\n\r;\"")) {
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "section name '%s' has invalid chars",
				     helper->group);
			return FALSE;
		}
	} else {
		g_strstrip (helper->group);
		g_strdelimit (helper->group, "\t\n\r;\"", '_');
	}

	/* start with new values */
	helper->last_line_continuation_ignore = FALSE;
	helper->nokey_idx = 0;

	/* no longer valid */
	as_inf_set_last_key (helper, NULL);

	/* FIXME: refusing to add a comment to a group that does not exist
	 * is a GLib bug! */
	if (helper->comment != NULL) {
		g_key_file_set_comment (helper->keyfile,
					helper->group,
					NULL,
					helper->comment,
					NULL);
		as_inf_set_comment (helper, NULL);
	}
	return TRUE;
}

/**
 * as_inf_set_key:
 */
static void
as_inf_set_key (AsInfHelper *helper, const gchar *key, const gchar *value)
{
	helper->last_line_continuation_ignore = FALSE;
	g_key_file_set_string (helper->keyfile, helper->group, key, value);
	if (helper->comment != NULL) {
		g_key_file_set_comment (helper->keyfile,
					helper->group,
					key,
					helper->comment,
					NULL);
		as_inf_set_comment (helper, NULL);
	}
	as_inf_set_last_key (helper, key);
}

/**
 * as_inf_convert_key:
 */
static void
as_inf_convert_key (gchar *key)
{
	guint i;
	for (i = 0; key[i] != '\0'; i++) {
		if (g_ascii_isalnum (key[i]))
			continue;
		if (key[i] == '.' || key[i] == '%')
			continue;
		key[i] = '_';
	}
}

/**
 * as_inf_repair_utf8:
 */
static gboolean
as_inf_repair_utf8 (AsInfHelper *helper, gchar *line, GError **error)
{
	guint i;
	guint8 val;

	/* Microsoft keeps using 0x99 as UTF-8 (R), and it's not valid */
	for (i = 0; line[i] != '\0'; i++) {
		val = (guint8) line[i];
		if (val == 0x99) {
			if (helper->flags & AS_INF_LOAD_FLAG_STRICT) {
				g_set_error (error,
					     AS_INF_ERROR,
					     AS_INF_ERROR_FAILED,
					     "Invalid UTF-8: 0x%02x", val);
				return FALSE;

			}
			line[i] = '?';
			continue;
		}
	}
	return TRUE;
}

/**
 * as_inf_parse_line:
 */
static gboolean
as_inf_parse_line (AsInfHelper *helper, gchar *line, GError **error)
{
	gchar *kvsplit;
	gchar *comment;
	gchar *tmp;
	guint len;
	gboolean ret = TRUE;
	gboolean continuation = FALSE;
	g_autofree gchar *key = NULL;

	/* line too long */
	if ((helper->flags & AS_INF_LOAD_FLAG_STRICT) > 0 &&
	    strlen (line) > 4096) {
		ret = FALSE;
		g_set_error (error,
			     AS_INF_ERROR,
			     AS_INF_ERROR_FAILED,
			     "line too long: %s", line);
		goto out;
	}

	/* fix up invalid UTF-8 */
	if (!as_inf_repair_utf8 (helper, line, error)) {
		ret = FALSE;
		goto out;
	}

	/* remove leading and trailing whitespace */
	g_strstrip (line);
	if (line[0] == '\0')
		goto out;

	/* remove comments */
	if (line[0] == ';') {
		as_inf_set_comment (helper, line + 1);
		goto out;
	}

	/* ignore pragmas */
	if (g_str_has_prefix (line, "#pragma"))
		goto out;
	if (g_str_has_prefix (line, "#define"))
		goto out;

	/* [group] */
	if (line[0] == '[') {

		/* remove comments */
		comment = g_strstr_len (line, -1, ";");
		if (comment != NULL) {
			*comment = '\0';
			as_inf_set_comment (helper, comment + 1);
			g_strchomp (line);
		}

		/* find group end */
		tmp = g_strrstr (line, "]");
		if (tmp == NULL) {
			ret = FALSE;
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "[ but no ] : '%s'", line);
			goto out;
		}
		*tmp = '\0';
		ret = as_inf_set_group (helper, line + 1, error);
		goto out;
	}

	/* last char continuation */
	len = strlen (line);
	if (line[len-1] == '\\') {
		line[len-1] = '\0';
		continuation = TRUE;
	}

	/* is a multiline key value */
	if (line[len-1] == '|') {
		line[len-1] = '\0';
		continuation = TRUE;
	}

	/* if the key-value split is before the first quote */
	kvsplit = g_strstr_len (line, -1, "=");
	tmp = g_strstr_len (line, -1, "\"");
	if ((tmp == NULL && kvsplit != NULL) ||
	    (kvsplit != NULL && kvsplit < tmp)) {
		g_autofree gchar *key_new = NULL;

		/* key=value before [group] */
		if (helper->group == NULL) {
			ret = FALSE;
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "key not in group: '%s'", line);
			goto out;
		}

		*kvsplit = '\0';
		key = as_inf_make_case_insensitive (helper, line);
		g_strchomp (key);

		/* remove invalid chars as GKeyFile is more strict than .ini */
		as_inf_convert_key (key);

		/* convert key names with variables */
		if (key[0] == '%') {
			g_autofree gchar *key_tmp = NULL;
			if (helper->dict == NULL) {
				helper->require_2nd_pass = TRUE;
				helper->last_line_continuation_ignore = TRUE;
				goto out;
			}
			key_tmp = as_inf_replace_variable (helper, key, error);
			if (key_tmp == NULL)
				return FALSE;
			if (key_tmp[0] == '%') {
				key_new = g_strdup (key + 1);
				g_strdelimit (key_new, "%", '\0');
				g_debug ("No replacement for '%s' in [Strings] "
					 "using '%s'", key, key_new);
				goto out;
			}

			/* this has to be lowercase */
			key_new = as_inf_make_case_insensitive (helper, key_tmp);
			as_inf_convert_key (key_new);
		} else {
			key_new = g_strdup (key);
		}

		/* remove leading and trailing quote */
		g_strchug (kvsplit + 1);
		if (!as_inf_strip_value (helper, kvsplit + 1, error)) {
			ret = FALSE;
			goto out;
		}

		/* add to keyfile */
		as_inf_set_key (helper, key_new, kvsplit + 1);
		goto out;
	}

	/* last_line_continuation from the last line */
	if (helper->last_line_continuation) {
		g_autofree gchar *old = NULL;
		g_autofree gchar *new = NULL;

		/* this is the 1st pass, and we have no key */
		if (helper->last_line_continuation_ignore)
			goto out;

		/* eek, no key at all */
		if (helper->last_key == NULL) {
			ret = FALSE;
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "continued last but no key for %s", line);
			goto out;
		}

		/* get the old value of this */
		old = g_key_file_get_string (helper->keyfile,
					     helper->group,
					     helper->last_key,
					     NULL);
		if (old == NULL) {
			ret = FALSE;
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_FAILED,
				     "can't continue [%s] '%s': %s",
				     helper->group, helper->last_key, line);
			goto out;
		}

		/* remove leading and trailing quote */
		if (!as_inf_strip_value (helper, line, error)) {
			ret = FALSE;
			goto out;
		}

		/* re-set key with new value */
		new = g_strdup_printf ("%s%s", old, line);
		as_inf_set_key (helper, helper->last_key, new);
		goto out;
	}

	/* there was left-over data but with no group defined */
	if (helper->group == NULL) {
		ret = FALSE;
		g_set_error (error,
			     AS_INF_ERROR,
			     AS_INF_ERROR_FAILED,
			     "fake key %s without [group]", line);
		goto out;
	}

	/* remove leading and trailing quote */
	if (!as_inf_strip_value (helper, line, error)) {
		ret = FALSE;
		goto out;
	}

	/* convert registry entries. e.g.
	 * "HKR,,FirmwareFilename,,firmware.bin"
	 *   -> "HKR_FirmwareFilename"="firmware.bin"
	 * "HKR,,FirmwareVersion,%REG_DWORD%,0x0000000"
	 *   -> "HKR_FirmwareVersion_0x00010001"="0x0000000" */
	if (g_strcmp0 (helper->group, "Firmware_AddReg") == 0 &&
	    g_str_has_prefix (line, "HK")) {
		guint i;
		g_auto(GStrv) reg_split = NULL;
		g_autoptr(GString) str = NULL;
		str = g_string_new ("");
		reg_split = g_strsplit (line, ",", -1);
		for (i = 0; reg_split[i+1] != NULL; i++) {
			g_strstrip (reg_split[i]);
			if (reg_split[i][0] == '\0')
				continue;
			g_string_append_printf (str, "%s_", reg_split[i]);
		}
		if (str->len > 0) {
			g_autofree gchar *key_tmp = NULL;

			/* remove trailing '_' */
			g_string_truncate (str, str->len - 1);

			/* remove leading and trailing quote */
			g_strchug (reg_split[i]);
			if (!as_inf_strip_value (helper, reg_split[i], error)) {
				ret = FALSE;
				goto out;
			}
			if (helper->dict == NULL) {
				helper->require_2nd_pass = TRUE;
				helper->last_line_continuation_ignore = TRUE;
				goto out;
			}
			key_tmp = as_inf_replace_variable (helper, str->str, error);
			if (key_tmp == NULL)
				return FALSE;
			as_inf_set_key (helper, key_tmp, reg_split[i]);
			goto out;
		}
	}

	/* add fake key */
	key = g_strdup_printf ("value%03i", helper->nokey_idx++);
	as_inf_set_key (helper, key, line);
out:
	helper->last_line_continuation = continuation;
	return ret;
}

/**
 * as_inf_helper_new:
 */
static AsInfHelper *
as_inf_helper_new (void)
{
	AsInfHelper *helper;
	helper = g_new0 (AsInfHelper, 1);
	return helper;
}

/**
 * as_inf_helper_free:
 */
static void
as_inf_helper_free (AsInfHelper *helper)
{
	if (helper->dict != NULL)
		g_hash_table_unref (helper->dict);
	g_key_file_unref (helper->keyfile);
	g_free (helper->comment);
	g_free (helper->group);
	g_free (helper->last_key);
	g_free (helper);
}

/**
 * as_inf_load_data:
 * @keyfile: a #GKeyFile
 * @data: the .inf file date to parse
 * @flags: #AsInfLoadFlags, e.g. %AS_INF_LOAD_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Repairs .inf file data and opens it as a keyfile.
 *
 * Important: The group and keynames are all forced to lower case as INF files
 * are specified as case insensitve and GKeyFile *is* case sensitive.
 * Any backslashes or spaces in the key name are also converted to '_'.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.5
 */
gboolean
as_inf_load_data (GKeyFile *keyfile,
		  const gchar *data,
		  AsInfLoadFlags flags,
		  GError **error)
{
	AsInfHelper *helper;
	gboolean ret = TRUE;
	guint i;
	g_auto(GStrv) lines = NULL;

	/* initialize helper */
	helper = as_inf_helper_new ();
	helper->flags = flags;
	helper->keyfile = g_key_file_ref (keyfile);

	/* verify each line, and make sane */
	lines = g_strsplit (data, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		if (!as_inf_parse_line (helper, lines[i], error)) {
			g_prefix_error (error,
					"Failed to parse line %i: ",
					i + 1);
			ret = FALSE;
			goto out;
		}
	}

	/* process every key in each group for strings we can substitute */
	helper->dict = as_inf_get_dict (helper, error);
	if (helper->dict == NULL) {
		ret = FALSE;
		goto out;
	}

	/* lets do this all over again */
	if (helper->require_2nd_pass) {
		g_auto(GStrv) lines2 = NULL;
		lines2 = g_strsplit (data, "\n", -1);
		for (i = 0; lines2[i] != NULL; i++) {
			if (!as_inf_parse_line (helper, lines2[i], error)) {
				g_prefix_error (error,
						"Failed to parse line %i: ",
						i + 1);
				ret = FALSE;
				goto out;
			}
		}
	}

	/* replace key values */
	if (!as_inf_replace_variables (helper, error)) {
		ret = FALSE;
		goto out;
	}
out:
	as_inf_helper_free (helper);
	return ret;
}

typedef struct {
	const gchar	*id;
	const gchar	*name;
	guint		 id_length;
} AsInfBOM;

/**
 * as_inf_load_file:
 * @keyfile: a #GKeyFile
 * @filename: the .inf file to open
 * @flags: #AsInfLoadFlags, e.g. %AS_INF_LOAD_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Repairs an .inf file and opens it as a keyfile.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.5
 */
gboolean
as_inf_load_file (GKeyFile *keyfile,
		  const gchar *filename,
		  AsInfLoadFlags flags,
		  GError **error)
{
	const gchar *data_no_bom;
	gsize len;
	guint i;
	g_autofree gchar *data = NULL;
	AsInfBOM boms[] = { { "\x00\x00\xfe\xff",	"UTF-32BE",	4 },
			    { "\xff\xfe\x00\x00",	"UTF-32LE",	4 },
			    { "\xfe\xff",		"UTF-16BE",	2 },
			    { "\xff\xfe",		"UTF-16LE",	2 },
			    { "\xef\xbb\xbf",		"UTF-8",	3 },
			    { NULL,			NULL,		0 } };

	if (!g_file_get_contents (filename, &data, &len, error))
		return FALSE;

	/* detect BOM */
	data_no_bom = data;
	for (i = 0; boms[i].id != NULL; i++) {

		/* BOM matches */
		if (len < boms[i].id_length)
			continue;
		if (memcmp (data, boms[i].id, boms[i].id_length) != 0)
			continue;

		/* ignore UTF-8 BOM */
		if (g_strcmp0 (boms[i].name, "UTF-8") == 0) {
			data_no_bom += boms[i].id_length;
			break;
		}
		g_set_error (error,
			     AS_INF_ERROR,
			     AS_INF_ERROR_INVALID_TYPE,
			     "File is encoded with %s and not supported",
			     boms[i].name);
		return FALSE;
	}

	/* load data stripped of BOM */
	return as_inf_load_data (keyfile, data_no_bom, flags, error);
}

/**
 * as_inf_get_driver_version:
 * @keyfile: a #GKeyFile
 * @timestamp: the returned driverver timestamp, or %NULL
 * @error: A #GError or %NULL
 *
 * Parses the DriverVer string into a recognisable version and timestamp;
 *
 * Returns: the version string, or %NULL for error.
 *
 * Since: 0.3.5
 */
gchar *
as_inf_get_driver_version (GKeyFile *keyfile, guint64 *timestamp, GError **error)
{
	g_autoptr(GDateTime) dt = NULL;
	g_auto(GStrv) split = NULL;
	g_auto(GStrv) dv_split = NULL;
	g_autofree gchar *driver_ver = NULL;

	/* get the release date and the version in case there's no metainfo */
	driver_ver = g_key_file_get_string (keyfile, "Version", "DriverVer", NULL);
	if (driver_ver == NULL) {
		g_set_error_literal (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_NOT_FOUND,
				     "DriverVer is missing");
		return FALSE;
	}

	/* split into driver date and version */
	dv_split = g_strsplit (driver_ver, ",", -1);
	if (g_strv_length (dv_split) != 2) {
		g_set_error (error,
			     AS_INF_ERROR,
			     AS_INF_ERROR_INVALID_TYPE,
			     "DriverVer is invalid: %s", driver_ver);
		return NULL;
	}

	/* split up into MM/DD/YYYY, because America */
	if (timestamp != NULL) {
		split = g_strsplit (dv_split[0], "/", -1);
		if (g_strv_length (split) != 3) {
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_INVALID_TYPE,
				     "DriverVer date invalid: %s",
				     dv_split[0]);
			return NULL;
		}
		dt = g_date_time_new_utc (atoi (split[2]),
					  atoi (split[0]),
					  atoi (split[1]),
					  0, 0, 0);
		if (dt == NULL) {
			g_set_error (error,
				     AS_INF_ERROR,
				     AS_INF_ERROR_INVALID_TYPE,
				     "DriverVer date invalid: %s",
				     dv_split[0]);
			return NULL;
		}
		*timestamp = g_date_time_to_unix (dt);
	}
	return g_strdup (dv_split[1]);
}
