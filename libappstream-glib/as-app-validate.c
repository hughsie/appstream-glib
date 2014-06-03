/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <fnmatch.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libsoup/soup.h>
#include <string.h>

#include "as-app-private.h"
#include "as-cleanup.h"
#include "as-node-private.h"
#include "as-problem.h"
#include "as-utils.h"

typedef struct {
	AsAppValidateFlags	 flags;
	GPtrArray		*screenshot_urls;
	GPtrArray		*probs;
	SoupSession		*session;
	gboolean		 previous_para_was_short;
	guint			 para_chars_before_list;
	guint			 number_paragraphs;
} AsAppValidateHelper;

/**
 * ai_app_validate_add:
 */
static void
ai_app_validate_add (GPtrArray *problems,
		     AsProblemKind kind,
		     const gchar *str)
{
	AsProblem *problem;
	guint i;

	/* already added */
	for (i = 0; i < problems->len; i++) {
		problem = g_ptr_array_index (problems, i);
		if (g_strcmp0 (as_problem_get_message (problem), str) == 0)
			return;
	}

	/* add new problem to list */
	problem = as_problem_new ();
	as_problem_set_kind (problem, kind);
	as_problem_set_message (problem, str);
	g_debug ("Adding %s '%s'", as_problem_kind_to_string (kind), str);
	g_ptr_array_add (problems, problem);
}

/**
 * ai_app_validate_fullstop_ending:
 *
 * Returns %TRUE if the string ends in a full stop, unless the string contains
 * multiple dots. This allows names such as "0 A.D." and summaries to end
 * with "..."
 */
static gboolean
ai_app_validate_fullstop_ending (const gchar *tmp)
{
	guint cnt = 0;
	guint i;
	guint str_len;

	for (i = 0; tmp[i] != '\0'; i++)
		if (tmp[i] == '.')
			cnt++;
	if (cnt++ > 1)
		return FALSE;
	str_len = strlen (tmp);
	if (str_len == 0)
		return FALSE;
	return tmp[str_len - 1] == '.';
}

/**
 * as_app_validate_description_li:
 **/
static void
as_app_validate_description_li (const gchar *text, AsAppValidateHelper *helper)
{
	guint str_len;
	guint length_li_max = 100;
	guint length_li_min = 20;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_li_max = 1000;
		length_li_min = 4;
	}

	str_len = strlen (text);
	if (str_len < length_li_min) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> is too short");
	}
	if (str_len > length_li_max) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> is too long");
	}
	if (ai_app_validate_fullstop_ending (text)) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> cannot end in '.'");
	}
}

/**
 * as_app_validate_description_para:
 **/
static void
as_app_validate_description_para (const gchar *text, AsAppValidateHelper *helper)
{
	guint length_para_max = 600;
	guint length_para_min = 50;
	guint str_len;

	/* empty */
	if (text == NULL) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> was empty");
		return;
	}

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_para_max = 1000;
		length_para_min = 10;
	}

	/* previous was short */
	if (helper->previous_para_was_short) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> is too short [p]");
	}
	helper->previous_para_was_short = FALSE;

	str_len = strlen (text);
	if (str_len < length_para_min) {
		/* we don't add the problem now, as we allow a short
		 * paragraph as an introduction to a list */
		helper->previous_para_was_short = TRUE;
	}
	if (str_len > length_para_max) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> is too long");
	}
	if (g_str_has_prefix (text, "This application")) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> should not start with 'This application'");
	}
	if (text[str_len - 1] != '.' &&
	    text[str_len - 1] != '!' &&
	    text[str_len - 1] != ':') {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> does not end in '.|:|!'");
	}
	helper->number_paragraphs++;
	helper->para_chars_before_list += str_len;
}

/**
 * as_app_validate_description_list:
 **/
static void
as_app_validate_description_list (const gchar *text, AsAppValidateHelper *helper)
{
	guint length_para_before_list = 300;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_para_before_list = 100;
	}

	/* ul without a leading para */
	if (helper->number_paragraphs < 1) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<ul> cannot start a description");
	}
	if (helper->para_chars_before_list != 0 &&
	    helper->para_chars_before_list < (guint) length_para_before_list) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Not enough <p> content before <ul>");
	}

	/* we allow the previous paragraph to be short to
	 * introduce the list */
	helper->previous_para_was_short = FALSE;
	helper->para_chars_before_list = 0;
}

/**
 * as_app_validate_description:
 **/
static gboolean
as_app_validate_description (const gchar *xml,
			     AsAppValidateHelper *helper,
			     guint number_para_min,
			     guint number_para_max,
			     GError **error)
{
	GNode *l;
	GNode *l2;
	_cleanup_unref_node GNode *node;

	/* parse xml */
	node = as_node_from_xml (xml, -1,
				 AS_NODE_FROM_XML_FLAG_NONE,
				 error);
	if (node == NULL)
		return FALSE;
	helper->number_paragraphs = 0;
	helper->previous_para_was_short = FALSE;
	for (l = node->children; l != NULL; l = l->next) {
		if (g_strcmp0 (as_node_get_name (l), "p") == 0) {
			if (as_node_get_attribute (l, "xml:lang") != NULL)
				continue;
			as_app_validate_description_para (as_node_get_data (l),
							  helper);
		} else if (g_strcmp0 (as_node_get_name (l), "ul") == 0 ||
			   g_strcmp0 (as_node_get_name (l), "ol") == 0) {
			as_app_validate_description_list (as_node_get_data (l),
							  helper);
			for (l2 = l->children; l2 != NULL; l2 = l2->next) {
				if (g_strcmp0 (as_node_get_name (l2), "li") == 0) {
					if (as_node_get_attribute (l2, "xml:lang") != NULL)
						continue;
					as_app_validate_description_li (as_node_get_data (l2),
									helper);
				} else {
					/* only <li> supported */
					g_set_error (error,
						     AS_APP_ERROR,
						     AS_APP_ERROR_FAILED,
						     "invalid markup: <%s> follows <%s>",
						     as_node_get_name (l2),
						     as_node_get_name (l));
					return FALSE;
				}
			}
		} else {
			/* only <p>, <ol> and <ul> supported */
			g_set_error (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "invalid markup: tag <%s> invalid here",
				     as_node_get_name (l));
			return FALSE;
		}
	}

	/* previous paragraph wasn't long enough */
	if (helper->previous_para_was_short) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> is too short");
	}
	if (helper->number_paragraphs < number_para_min) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Not enough <p> tags for a good description");
	}
	if (helper->number_paragraphs > number_para_max) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Too many <p> tags for a good description");
	}
	return TRUE;
}

/**
 * as_app_validate_image_url_already_exists:
 */
static gboolean
as_app_validate_image_url_already_exists (AsAppValidateHelper *helper,
					  const gchar *search)
{
	const gchar *tmp;
	guint i;

	for (i = 0; i < helper->screenshot_urls->len; i++) {
		tmp = g_ptr_array_index (helper->screenshot_urls, i);
		if (g_strcmp0 (tmp, search) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * ai_app_validate_image_check:
 */
static gboolean
ai_app_validate_image_check (AsImage *im, AsAppValidateHelper *helper)
{
	const gchar *url;
	gboolean require_correct_aspect_ratio = FALSE;
	gdouble desired_aspect = 1.777777778;
	gdouble screenshot_aspect;
	gint status_code;
	guint screenshot_height;
	guint screenshot_width;
	guint ss_size_height_max = 900;
	guint ss_size_height_min = 351;
	guint ss_size_width_max = 1600;
	guint ss_size_width_min = 624;
	_cleanup_unref_object GdkPixbuf *pixbuf = NULL;
	_cleanup_unref_object GInputStream *stream = NULL;
	_cleanup_unref_object SoupMessage *msg = NULL;
	_cleanup_unref_uri SoupURI *base_uri = NULL;

	/* make the requirements more strict */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_STRICT) > 0) {
		require_correct_aspect_ratio = TRUE;
	}

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		ss_size_height_max = 1800;
		ss_size_height_min = 150;
		ss_size_width_max = 3200;
		ss_size_width_min = 300;
	}

	/* have we got network access */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_NO_NETWORK) > 0)
		return TRUE;

	/* GET file */
	url = as_image_get_url (im);
	g_debug ("checking %s", url);
	base_uri = soup_uri_new (url);
	if (base_uri == NULL) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_URL_NOT_FOUND,
				     "<screenshot> url not valid");
		return FALSE;
	}
	msg = soup_message_new_from_uri (SOUP_METHOD_GET, base_uri);
	if (msg == NULL) {
		g_warning ("Failed to setup message");
		return FALSE;
	}

	/* send sync */
	status_code = soup_session_send_message (helper->session, msg);
	if (status_code != SOUP_STATUS_OK) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_URL_NOT_FOUND,
				     "<screenshot> url not found");
		return FALSE;
	}

	/* check if it's a zero sized file */
	if (msg->response_body->length == 0) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_FILE_INVALID,
				     "<screenshot> url is a zero length file");
		return FALSE;
	}

	/* create a buffer with the data */
	stream = g_memory_input_stream_new_from_data (msg->response_body->data,
						      msg->response_body->length,
						      NULL);
	if (stream == NULL) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_URL_NOT_FOUND,
				     "<screenshot> failed to load data");
		return FALSE;
	}

	/* load the image */
	pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
	if (pixbuf == NULL) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_FILE_INVALID,
				     "<screenshot> failed to load image");
		return FALSE;
	}

	/* check width matches */
	screenshot_width = gdk_pixbuf_get_width (pixbuf);
	screenshot_height = gdk_pixbuf_get_height (pixbuf);
	if (as_image_get_width (im) != 0 &&
	    as_image_get_width (im) != screenshot_width) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> width did not match specified");
	}

	/* check height matches */
	if (as_image_get_height (im) != 0 &&
	    as_image_get_height (im) != screenshot_height) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> height did not match specified");
	}

	/* check size is reasonable */
	if (screenshot_width < ss_size_width_min) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> width was too small");
	}
	if (screenshot_height < ss_size_height_min) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> height was too small");
	}
	if (screenshot_width > ss_size_width_max) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> width was too large");
	}
	if (screenshot_height > ss_size_height_max) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> height was too large");
	}

	/* check aspect ratio */
	if (require_correct_aspect_ratio) {
		screenshot_aspect = (gdouble) screenshot_width / (gdouble) screenshot_height;
		if (ABS (screenshot_aspect - 1.777777777) > 0.1) {
			g_debug ("got aspect %.2f, wanted %.2f",
				 screenshot_aspect, desired_aspect);
			ai_app_validate_add (helper->probs,
					     AS_PROBLEM_KIND_ASPECT_RATIO_INCORRECT,
					     "<screenshot> aspect ratio was not 16:9");
		}
	}
	return TRUE;
}

/**
 * as_app_validate_image:
 **/
static void
as_app_validate_image (AsImage *im, AsAppValidateHelper *helper)
{
	const gchar *url;
	gboolean ret;

	/* blank */
	url = as_image_get_url (im);
	if (strlen (url) == 0) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_VALUE_MISSING,
				     "<screenshot> has no content");
		return;
	}

	/* check for duplicates */
	ret = as_app_validate_image_url_already_exists (helper, url);
	if (ret) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_DUPLICATE_DATA,
				     "<screenshot> has duplicated data");
		return;
	}

	/* validate the URL */
	ret = ai_app_validate_image_check (im, helper);
	if (ret)
		g_ptr_array_add (helper->screenshot_urls, g_strdup (url));
}

/**
 * as_app_validate_screenshot:
 **/
static void
as_app_validate_screenshot (AsScreenshot *ss, AsAppValidateHelper *helper)
{
	AsImage *im;
	GPtrArray *images;
	guint i;

	if (as_screenshot_get_kind (ss) == AS_SCREENSHOT_KIND_UNKNOWN) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> has unknown type");
	}
	images = as_screenshot_get_images (ss);
	for (i = 0; i < images->len; i++) {
		im = g_ptr_array_index (images, i);
		as_app_validate_image (im, helper);
	}
}

/**
 * as_app_validate_screenshots:
 **/
static void
as_app_validate_screenshots (AsApp *app, AsAppValidateHelper *helper)
{
	AsScreenshot *ss;
	GPtrArray *screenshots;
	gboolean screenshot_has_default = FALSE;
	guint number_screenshots_max = 5;
	guint number_screenshots_min = 1;
	guint i;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		number_screenshots_max = 10;
		number_screenshots_min = 0;
	}

	/* only for AppData and AppStream */
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_DESKTOP)
		return;

	screenshots = as_app_get_screenshots (app);
	if (screenshots->len < number_screenshots_min) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Not enough <screenshot> tags");
	}
	if (screenshots->len > number_screenshots_max) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Too many <screenshot> tags");
	}
	for (i = 0; i < screenshots->len; i++) {
		ss = g_ptr_array_index (screenshots, i);
		as_app_validate_screenshot (ss, helper);
		if (as_screenshot_get_kind (ss) == AS_SCREENSHOT_KIND_DEFAULT) {
			if (screenshot_has_default) {
				ai_app_validate_add (helper->probs,
						     AS_PROBLEM_KIND_MARKUP_INVALID,
						     "<screenshot> has more than one default");
			}
			screenshot_has_default = TRUE;
			continue;
		}
	}
	if (screenshots->len > 0 && !screenshot_has_default) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_MARKUP_INVALID,
				     "<screenshots> has no default <screenshot>");
	}
}

/**
 * as_app_validate_release:
 **/
static gboolean
as_app_validate_release (AsRelease *release, AsAppValidateHelper *helper, GError **error)
{
	const gchar *tmp;
	guint64 timestamp;
	guint number_para_max = 2;
	guint number_para_min = 1;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		number_para_max = 4;
	}

	/* check version */
	tmp = as_release_get_version (release);
	if (tmp == NULL) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
				     "<release> has no version");
	}

	/* check timestamp */
	timestamp = as_release_get_timestamp (release);
	if (timestamp == 0) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
				     "<release> has no timestamp");
	}
	if (timestamp > 20120101 && timestamp < 20151231) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<release> timestamp should be a UNIX time");
	}

	/* check description */
	tmp = as_release_get_description (release, "C");
	if (tmp == NULL) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
				     "<release> has no description");
	} else {
		if (g_strstr_len (tmp, -1, "http://") != NULL ||
		    g_strstr_len (tmp, -1, "https://") != NULL ||
		    g_strstr_len (tmp, -1, "ftp://") != NULL) {
			ai_app_validate_add (helper->probs,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<release> description should be "
					     "prose and not contain hyperlinks");
		}
		if (!as_app_validate_description (tmp,
						  helper,
						  number_para_min,
						  number_para_max,
						  error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_app_validate_releases:
 **/
static gboolean
as_app_validate_releases (AsApp *app, AsAppValidateHelper *helper, GError **error)
{
	AsRelease *release;
	GPtrArray *releases;
	guint i;

	/* only for AppData */
	if (as_app_get_source_kind (app) != AS_APP_SOURCE_KIND_APPDATA)
		return TRUE;

	releases = as_app_get_releases (app);
	if (releases->len > 10) {
		ai_app_validate_add (helper->probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Too many <release> tags");
	}
	for (i = 0; i < releases->len; i++) {
		release = g_ptr_array_index (releases, i);
		if (!as_app_validate_release (release, helper, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_app_validate_setup_networking:
 **/
static gboolean
as_app_validate_setup_networking (AsAppValidateHelper *helper, GError **error)
{
	helper->session = soup_session_sync_new_with_options (SOUP_SESSION_USER_AGENT,
							      "libappstream-glib",
							      SOUP_SESSION_TIMEOUT,
							      5000,
							      NULL);
	if (helper->session == NULL) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "Failed to set up networking");
		return FALSE;
	}
	soup_session_add_feature_by_type (helper->session,
					  SOUP_TYPE_PROXY_RESOLVER_DEFAULT);
	return TRUE;
}

/**
 * as_app_validate_license:
 **/
static gboolean
as_app_validate_license (const gchar *license_text, GError **error)
{
	guint i;
	_cleanup_free_strv gchar **licenses = NULL;

	licenses = as_utils_spdx_license_tokenize (license_text);
	for (i = 0; licenses[i] != NULL; i++) {
		if (g_str_has_prefix (licenses[i], "#"))
			continue;
		if (!as_utils_is_spdx_license_id (licenses[i])) {
			g_set_error (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "SPDX ID '%s' unknown",
				     licenses[i]);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * as_app_validate:
 * @app: a #AsApp instance.
 * @flags: the #AsAppValidateFlags to use, e.g. %AS_APP_VALIDATE_FLAG_NONE
 * @error: A #GError or %NULL.
 *
 * Validates data in the instance for style and consitency.
 *
 * Returns: (transfer container) (element-type AsProblem): A list of problems, or %NULL
 *
 * Since: 0.1.4
 **/
GPtrArray *
as_app_validate (AsApp *app, AsAppValidateFlags flags, GError **error)
{
	AsAppProblems problems;
	AsAppValidateHelper helper;
	GError *error_local = NULL;
	GHashTable *urls;
	GList *l;
	GPtrArray *probs = NULL;
	const gchar *description;
	const gchar *id_full;
	const gchar *key;
	const gchar *license;
	const gchar *name;
	const gchar *summary;
	const gchar *tmp;
	const gchar *update_contact;
	gboolean deprectated_failure = FALSE;
	gboolean require_contactdetails = TRUE;
	gboolean require_copyright = FALSE;
	gboolean require_project_license = FALSE;
	gboolean require_translations = FALSE;
	gboolean require_url = TRUE;
	gboolean ret;
	guint length_name_max = 30;
	guint length_name_min = 3;
	guint length_summary_max = 100;
	guint length_summary_min = 8;
	guint number_para_max = 4;
	guint number_para_min = 2;
	guint str_len;
	_cleanup_free_list GList *keys = NULL;

	/* relax the requirements a bit */
	if ((flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_name_max = 100;
		length_summary_max = 200;
		require_contactdetails = FALSE;
		require_url = FALSE;
		number_para_max = 10;
		number_para_min = 1;
	}

	/* make the requirements more strict */
	if ((flags & AS_APP_VALIDATE_FLAG_STRICT) > 0) {
		deprectated_failure = TRUE;
		require_copyright = TRUE;
		require_translations = TRUE;
		require_project_license = TRUE;
	}

	/* set up networking */
	helper.probs = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	helper.screenshot_urls = g_ptr_array_new_with_free_func (g_free);
	helper.flags = flags;
	helper.previous_para_was_short = FALSE;
	helper.para_chars_before_list = 0;
	helper.number_paragraphs = 0;
	ret = as_app_validate_setup_networking (&helper, error);
	if (!ret)
		goto out;

	/* success, enough */
	probs = helper.probs;

	/* id */
	ret = FALSE;
	id_full = as_app_get_id_full (app);
	switch (as_app_get_id_kind (app)) {
	case AS_ID_KIND_DESKTOP:
		if (g_str_has_suffix (id_full, ".desktop"))
			ret = TRUE;
		break;
	case AS_ID_KIND_FONT:
		if (g_str_has_suffix (id_full, ".ttf"))
			ret = TRUE;
		else if (g_str_has_suffix (id_full, ".otf"))
			ret = TRUE;
		break;
	case AS_ID_KIND_INPUT_METHOD:
		if (g_str_has_suffix (id_full, ".xml"))
			ret = TRUE;
		else if (g_str_has_suffix (id_full, ".db"))
			ret = TRUE;
		break;
	case AS_ID_KIND_CODEC:
		if (g_str_has_prefix (id_full, "gstreamer"))
			ret = TRUE;
		break;
	case AS_ID_KIND_UNKNOWN:
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<id> has invalid type attribute");

		break;
	default:
		break;
	}
	if (!ret) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_MARKUP_INVALID,
				     "<id> does not have correct extension for kind");
	}

	/* metadata_license */
	license = as_app_get_metadata_license (app);
	if (license != NULL) {
		if (g_strcmp0 (license, "CC0-1.0") != 0 &&
		    g_strcmp0 (license, "CC-BY-3.0") != 0 &&
		    g_strcmp0 (license, "CC-BY-SA-3.0") != 0 &&
		    g_strcmp0 (license, "GFDL-1.3") != 0) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "<metadata_license> is not valid");
		}
	}
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA &&
	    license == NULL) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<metadata_license> is not present");
	}

	/* project_license */
	license = as_app_get_project_license (app);
	if (license != NULL) {
		ret = as_app_validate_license (license, &error_local);
		if (!ret) {
			g_prefix_error (&error_local,
					"<project_license> is not valid: ");
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     error_local->message);
			g_clear_error (&error_local);
		}
	}
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA &&
	    require_project_license && license == NULL) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<project_license> is not present");
	}

	/* updatecontact */
	update_contact = as_app_get_update_contact (app);
	if (g_strcmp0 (update_contact,
		       "someone_who_cares@upstream_project.org") == 0) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "<update_contact> is still set to a dummy value");
	}
	if (update_contact != NULL && strlen (update_contact) < 6) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<update_contact> is too short");
	}
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA &&
	    require_contactdetails &&
	    update_contact == NULL) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<updatecontact> is not present");
	}

	/* only found for files */
	problems = as_app_get_problems (app);
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA) {
		if ((problems & AS_APP_PROBLEM_NO_XML_HEADER) > 0) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_MARKUP_INVALID,
					     "<?xml> header not found");
		}
		if (require_copyright &&
		    (problems & AS_APP_PROBLEM_NO_COPYRIGHT_INFO) > 0) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_VALUE_MISSING,
					     "<!-- Copyright [year] [name] --> is not present");
		}
	}

	/* check for things that have to exist */
	if (as_app_get_id_full (app) == NULL) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<id> is not present");
	}

	/* url */
	urls = as_app_get_urls (app);
	keys = g_hash_table_get_keys (urls);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		if (g_strcmp0 (key, "unknown") == 0) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "<url> type invalid");
		}
		tmp = g_hash_table_lookup (urls, key);
		if (!g_str_has_prefix (tmp, "http://") &&
		    !g_str_has_prefix (tmp, "https://")) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "<url> does not start with 'http://'");
		}
	}

	/* screenshots */
	as_app_validate_screenshots (app, &helper);

	/* releases */
	ret = as_app_validate_releases (app, &helper, error);
	if (!ret)
		goto out;

	/* name */
	name = as_app_get_name (app, "C");
	if (name != NULL) {
		str_len = strlen (name);
		if (str_len < length_name_min) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> is too short");
		}
		if (str_len > length_name_max) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> is too long");
		}
		if (ai_app_validate_fullstop_ending (name)) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> cannot end in '.'");
		}
	}

	/* comment */
	summary = as_app_get_comment (app, "C");
	if (summary != NULL) {
		str_len = strlen (summary);
		if (str_len < length_summary_min) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> is too short");
		}
		if (str_len > length_summary_max) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> is too long");
		}
		if (ai_app_validate_fullstop_ending (summary)) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> cannot end in '.'");
		}
	}
	if (summary != NULL && name != NULL &&
	    strlen (summary) < strlen (name)) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<summary> is shorter than <name>");
	}
	description = as_app_get_description (app, "C");
	if (description != NULL) {
		ret = as_app_validate_description (description,
						   &helper,
						   number_para_min,
						   number_para_max,
						   &error_local);
		if (!ret) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_MARKUP_INVALID,
					     error_local->message);
			g_error_free (error_local);
		}
	}
	if (require_translations) {
		if (name != NULL && as_app_get_name_size (app) == 1) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
					     "<name> has no translations");
		}
		if (summary != NULL && as_app_get_comment_size (app) == 1) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
					     "<summary> has no translations");
		}
		if (description != NULL && as_app_get_description_size (app) == 1) {
			ai_app_validate_add (probs,
					     AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
					     "<description> has no translations");
		}
	}

	/* using deprecated names */
	if (deprectated_failure && (problems & AS_APP_PROBLEM_DEPRECATED_LICENCE) > 0) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<licence> is deprecated, use "
				     "<metadata_license> instead");
	}
	if ((problems & AS_APP_PROBLEM_MULTIPLE_ENTRIES) > 0) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_MARKUP_INVALID,
				     "<application> used more than once");
	}

	/* require homepage */
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA &&
	    require_url &&
	    as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE) == NULL) {
		ai_app_validate_add (probs,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<url> is not present");
	}
out:
	g_ptr_array_unref (helper.screenshot_urls);
	if (helper.session != NULL)
		g_object_unref (helper.session);
	return probs;
}
