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
#include "as-node-private.h"
#include "as-problem.h"
#include "as-utils.h"

typedef struct {
	AsApp			*app;
	AsAppValidateFlags	 flags;
	GPtrArray		*screenshot_urls;
	GPtrArray		*probs;
	SoupSession		*session;
	gboolean		 previous_para_was_short;
	gchar			*previous_para_was_short_str;
	guint			 para_chars_before_list;
	guint			 number_paragraphs;
} AsAppValidateHelper;

G_GNUC_PRINTF (3, 4) static void
ai_app_validate_add (AsAppValidateHelper *helper,
		     AsProblemKind kind,
		     const gchar *fmt, ...)
{
	AsProblem *problem;
	guint i;
	va_list args;
	g_autofree gchar *str = NULL;

	va_start (args, fmt);
	str = g_strdup_vprintf (fmt, args);
	va_end (args);

	/* don't care about style when relaxed */
	if (helper->flags & AS_APP_VALIDATE_FLAG_RELAX &&
	    kind == AS_PROBLEM_KIND_STYLE_INCORRECT)
		return;

	/* already added */
	for (i = 0; i < helper->probs->len; i++) {
		problem = g_ptr_array_index (helper->probs, i);
		if (g_strcmp0 (as_problem_get_message (problem), str) == 0)
			return;
	}

	/* add new problem to list */
	problem = as_problem_new ();
	as_problem_set_kind (problem, kind);
	as_problem_set_message (problem, str);
	g_debug ("Adding %s '%s'", as_problem_kind_to_string (kind), str);
	g_ptr_array_add (helper->probs, problem);
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
	str_len = (guint) strlen (tmp);
	if (str_len == 0)
		return FALSE;
	return tmp[str_len - 1] == '.';
}

static gboolean
as_app_validate_has_hyperlink (const gchar *text)
{
	if (g_strstr_len (text, -1, "http://") != NULL)
		return TRUE;
	if (g_strstr_len (text, -1, "https://") != NULL)
		return TRUE;
	if (g_strstr_len (text, -1, "ftp://") != NULL)
		return TRUE;
	return FALSE;
}

static gboolean
as_app_validate_has_email (const gchar *text)
{
	if (g_strstr_len (text, -1, "@") != NULL)
		return TRUE;
	if (g_strstr_len (text, -1, "_at_") != NULL)
		return TRUE;
	return FALSE;
}

static gboolean
as_app_validate_has_first_word_capital (AsAppValidateHelper *helper, const gchar *text)
{
	g_autofree gchar *first_word = NULL;
	gchar *tmp;
	guint i;

	if (text == NULL || text[0] == '\0')
		return TRUE;

	/* text starts with a number */
	if (g_ascii_isdigit (text[0]))
		return TRUE;

	/* get the first word */
	first_word = g_strdup (text);
	tmp = g_strstr_len (first_word, -1, " ");
	if (tmp != NULL)
		*tmp = '\0';

	/* does the word have caps anywhere? */
	for (i = 0; first_word[i] != '\0'; i++) {
		if (first_word[i] >= 'A' && first_word[i] <= 'Z')
			return TRUE;
	}

	/* is the first word the project name */
	if (g_strcmp0 (first_word, as_app_get_name (helper->app, NULL)) == 0)
		return TRUE;

	return FALSE;
}

static void
as_app_validate_description_li (const gchar *text, AsAppValidateHelper *helper)
{
	gboolean require_sentence_case = TRUE;
	guint str_len;
	guint length_li_max = 100;
	guint length_li_min = 20;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_li_max = 1000;
		length_li_min = 3;
		require_sentence_case = FALSE;
	}

	/* empty */
	if (text == NULL) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> is empty");
		return;
	}

	str_len = (guint) strlen (text);
	if (str_len < length_li_min) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> is too short [%s]", text);
	}
	if (str_len > length_li_max) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> is too long [%s]", text);
	}
	if (ai_app_validate_fullstop_ending (text)) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> cannot end in '.' [%s]", text);
	}
	if (as_app_validate_has_hyperlink (text)) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> cannot contain a hyperlink [%s]",
				     text);
	}
	if (require_sentence_case &&
	    !as_app_validate_has_first_word_capital (helper, text)) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<li> requires sentence case [%s]", text);
	}
}

static void
as_app_validate_description_para (const gchar *text, AsAppValidateHelper *helper)
{
	gboolean require_sentence_case = TRUE;
	guint length_para_max = 600;
	guint length_para_min = 50;
	guint str_len;

	/* empty */
	if (text == NULL) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> was empty");
		return;
	}

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_para_max = 1000;
		length_para_min = 10;
		require_sentence_case = FALSE;
	}

	/* previous was short */
	if (helper->previous_para_was_short) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> is too short [%s]", text);
	}
	helper->previous_para_was_short = FALSE;

	str_len = (guint) strlen (text);
	if (str_len < length_para_min) {
		/* we don't add the problem now, as we allow a short
		 * paragraph as an introduction to a list */
		helper->previous_para_was_short = TRUE;
		g_free (helper->previous_para_was_short_str);
		helper->previous_para_was_short_str = g_strdup (text);
	}
	if (str_len > length_para_max) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> is too long [%s]", text);
	}
	if (g_str_has_prefix (text, "This application")) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> should not start with 'This application'");
	}
	if (as_app_validate_has_hyperlink (text)) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> cannot contain a hyperlink [%s]",
				     text);
	}
	if (require_sentence_case &&
	    !as_app_validate_has_first_word_capital (helper, text)) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> requires sentence case [%s]", text);
	}
	if (text[str_len - 1] != '.' &&
	    text[str_len - 1] != '!' &&
	    text[str_len - 1] != ':') {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> does not end in '.|:|!' [%s]", text);
	}
	helper->number_paragraphs++;
	helper->para_chars_before_list += str_len;
}

static void
as_app_validate_description_list (const gchar *text,
				  gboolean allow_short_para,
				  AsAppValidateHelper *helper)
{
	guint length_para_before_list = 300;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_para_before_list = 100;
	}

	/* ul without a leading para */
	if (helper->number_paragraphs < 1) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<ul> cannot start a description [%s]",
				     text);
	}
	if (!allow_short_para &&
	    helper->para_chars_before_list != 0 &&
	    helper->para_chars_before_list < (guint) length_para_before_list) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Content before <ul> is too short [%u], at least %u characters required",
				     helper->para_chars_before_list,
				     length_para_before_list);
	}

	/* we allow the previous paragraph to be short to
	 * introduce the list */
	helper->previous_para_was_short = FALSE;
	helper->para_chars_before_list = 0;
}

static gboolean
as_app_validate_description (const gchar *xml,
			     AsAppValidateHelper *helper,
			     guint number_para_min,
			     guint number_para_max,
			     gboolean allow_short_para,
			     GError **error)
{
	GNode *l;
	GNode *l2;
	g_autoptr(AsNode) node = NULL;

	/* parse xml */
	node = as_node_from_xml (xml, AS_NODE_FROM_XML_FLAG_NONE, error);
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
							  allow_short_para,
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
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<p> is too short [%s]",
				     helper->previous_para_was_short_str);
	}
	if (helper->number_paragraphs < number_para_min) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Not enough <p> tags for a good description [%u/%u]",
				     helper->number_paragraphs,
				     number_para_min);
	}
	if (helper->number_paragraphs > number_para_max) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Too many <p> tags for a good description [%u/%u]",
				     helper->number_paragraphs,
				     number_para_max);
	}
	return TRUE;
}

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

static gboolean
ai_app_validate_image_check (AsImage *im, AsAppValidateHelper *helper)
{
	AsImageAlphaFlags alpha_flags;
	const gchar *url;
	gboolean require_correct_aspect_ratio = FALSE;
	gdouble desired_aspect = 1.777777778;
	gdouble screenshot_aspect;
	guint status_code;
	guint screenshot_height;
	guint screenshot_width;
	guint ss_size_height_max = 900;
	guint ss_size_height_min = 351;
	guint ss_size_width_max = 1600;
	guint ss_size_width_min = 624;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	g_autoptr(GInputStream) stream = NULL;
	g_autoptr(SoupMessage) msg = NULL;
	g_autoptr(SoupURI) base_uri = NULL;

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
	if (!SOUP_URI_VALID_FOR_HTTP (base_uri)) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_URL_NOT_FOUND,
				     "<screenshot> url not valid [%s]", url);
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
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_URL_NOT_FOUND,
				     "<screenshot> url not found [%s]", url);
		return FALSE;
	}

	/* check if it's a zero sized file */
	if (msg->response_body->length == 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_FILE_INVALID,
				     "<screenshot> url is a zero length file [%s]",
				     url);
		return FALSE;
	}

	/* create a buffer with the data */
	stream = g_memory_input_stream_new_from_data (msg->response_body->data,
						      (gssize) msg->response_body->length,
						      NULL);
	if (stream == NULL) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_URL_NOT_FOUND,
				     "<screenshot> failed to load data [%s]",
				     url);
		return FALSE;
	}

	/* load the image */
	pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
	if (pixbuf == NULL) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_FILE_INVALID,
				     "<screenshot> failed to load [%s]",
				     url);
		return FALSE;
	}

	/* check width matches */
	screenshot_width = (guint) gdk_pixbuf_get_width (pixbuf);
	screenshot_height = (guint) gdk_pixbuf_get_height (pixbuf);
	if (as_image_get_width (im) != 0 &&
	    as_image_get_width (im) != screenshot_width) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> width did not match specified [%s]",
				     url);
	}

	/* check height matches */
	if (as_image_get_height (im) != 0 &&
	    as_image_get_height (im) != screenshot_height) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> height did not match specified [%s]",
				     url);
	}

	/* check size is reasonable */
	if (screenshot_width < ss_size_width_min) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> width too small [%s]",
				     url);
	}
	if (screenshot_height < ss_size_height_min) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> height too small [%s]",
				     url);
	}
	if (screenshot_width > ss_size_width_max) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> width too large [%s]",
				     url);
	}
	if (screenshot_height > ss_size_height_max) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> height too large [%s]",
				     url);
	}

	/* check padding */
	as_image_set_pixbuf (im, pixbuf);
	alpha_flags = as_image_get_alpha_flags (im);
	if ((alpha_flags & AS_IMAGE_ALPHA_FLAG_TOP) > 0||
	    (alpha_flags & AS_IMAGE_ALPHA_FLAG_BOTTOM) > 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<image> has vertical padding [%s]",
				     url);
	}
	if ((alpha_flags & AS_IMAGE_ALPHA_FLAG_LEFT) > 0||
	    (alpha_flags & AS_IMAGE_ALPHA_FLAG_RIGHT) > 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<image> has horizontal padding [%s]",
				     url);
	}

	/* check aspect ratio */
	if (require_correct_aspect_ratio) {
		screenshot_aspect = (gdouble) screenshot_width / (gdouble) screenshot_height;
		if (ABS (screenshot_aspect - 1.777777777) > 0.1) {
			g_debug ("got aspect %.2f, wanted %.2f",
				 screenshot_aspect, desired_aspect);
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_ASPECT_RATIO_INCORRECT,
					     "<screenshot> aspect ratio not 16:9 [%s]",
					     url);
		}
	}
	return TRUE;
}

static void
as_app_validate_image (AsImage *im, AsAppValidateHelper *helper)
{
	const gchar *url;
	gboolean ret;

	/* blank */
	url = as_image_get_url (im);
	if ((guint) strlen (url) == 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_VALUE_MISSING,
				     "<screenshot> has no content");
		return;
	}

	/* check for duplicates */
	ret = as_app_validate_image_url_already_exists (helper, url);
	if (ret) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_DUPLICATE_DATA,
				     "<screenshot> has duplicated data");
		return;
	}

	/* validate the URL */
	ret = ai_app_validate_image_check (im, helper);
	if (ret)
		g_ptr_array_add (helper->screenshot_urls, g_strdup (url));
}

static void
as_app_validate_screenshot (AsScreenshot *ss, AsAppValidateHelper *helper)
{
	AsImage *im;
	GPtrArray *images;
	const gchar *tmp;
	gboolean require_sentence_case = TRUE;
	guint i;
	guint length_caption_max = 50;
	guint length_caption_min = 10;
	guint str_len;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_caption_max = 100;
		length_caption_min = 5;
		require_sentence_case = FALSE;
	}

	if (as_screenshot_get_kind (ss) == AS_SCREENSHOT_KIND_UNKNOWN) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<screenshot> has unknown type");
	}
	images = as_screenshot_get_images (ss);
	for (i = 0; i < images->len; i++) {
		im = g_ptr_array_index (images, i);
		as_app_validate_image (im, helper);
	}
	tmp = as_screenshot_get_caption (ss, NULL);
	if (tmp != NULL) {
		str_len = (guint) strlen (tmp);
		if (str_len < length_caption_min) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<caption> is too short [%s];"
					     "shortest allowed is %u chars",
					     tmp, length_caption_min);
		}
		if (str_len > length_caption_max) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<caption> is too long [%s];"
					     "longest allowed is %u chars",
					     tmp, length_caption_max);
		}
		if (ai_app_validate_fullstop_ending (tmp)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<caption> cannot end in '.' [%s]",
					     tmp);
		}
		if (as_app_validate_has_hyperlink (tmp)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<caption> cannot contain a hyperlink [%s]",
					     tmp);
		}
		if (require_sentence_case &&
		    !as_app_validate_has_first_word_capital (helper, tmp)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<caption> requires sentence case [%s]",
					     tmp);
		}
	}
}

static void
as_app_validate_icons (AsApp *app, AsAppValidateHelper *helper)
{
	AsIcon *icon;
	AsIconKind icon_kind;
	const gchar *icon_name;

	/* just check the default icon */
	icon = as_app_get_icon_default (app);
	if (icon == NULL)
		return;

	/* check the content is correct */
	icon_kind = as_icon_get_kind (icon);
	switch (icon_kind) {
	case AS_ICON_KIND_STOCK:
		icon_name = as_icon_get_name (icon);
		if (!as_utils_is_stock_icon_name (icon_name)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "stock icon is not valid [%s]",
					     icon_name);
		}
		break;
	case AS_ICON_KIND_LOCAL:
		icon_name = as_icon_get_filename (icon);
		if (icon_name == NULL ||
		    !g_str_has_prefix (icon_name, "/")) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "local icon is not a filename [%s]",
					     icon_name);
		}
		break;
	case AS_ICON_KIND_CACHED:
		icon_name = as_icon_get_name (icon);
		if (icon_name == NULL ||
		    g_str_has_prefix (icon_name, "/")) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "cached icon is a filename [%s]",
					     icon_name);
		}
		break;
	case AS_ICON_KIND_REMOTE:
		icon_name = as_icon_get_url (icon);
		if (!g_str_has_prefix (icon_name, "http://") &&
		    !g_str_has_prefix (icon_name, "https://")) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "remote icon is not a url [%s]",
					     icon_name);
		}
		break;
	default:
		break;
	}
}

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

	/* firmware does not need screenshots */
	if (as_app_get_kind (app) == AS_APP_KIND_FIRMWARE ||
	    as_app_get_kind (app) == AS_APP_KIND_LOCALIZATION)
		number_screenshots_min = 0;

	/* metainfo and inf do not require any screenshots */
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_METAINFO ||
	    as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_INF)
		number_screenshots_min = 0;

	/* only for AppData and AppStream */
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_DESKTOP)
		return;

	screenshots = as_app_get_screenshots (app);
	if (screenshots->len < number_screenshots_min) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Not enough <screenshot> tags");
	}
	if (screenshots->len > number_screenshots_max) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "Too many <screenshot> tags");
	}
	for (i = 0; i < screenshots->len; i++) {
		ss = g_ptr_array_index (screenshots, i);
		as_app_validate_screenshot (ss, helper);
		if (as_screenshot_get_kind (ss) == AS_SCREENSHOT_KIND_DEFAULT) {
			if (screenshot_has_default) {
				ai_app_validate_add (helper,
						     AS_PROBLEM_KIND_MARKUP_INVALID,
						     "<screenshot> has more than one default");
			}
			screenshot_has_default = TRUE;
			continue;
		}
	}
	if (screenshots->len > 0 && !screenshot_has_default) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_MARKUP_INVALID,
				     "<screenshots> has no default <screenshot>");
	}
}

static gboolean
as_app_validate_release (AsApp *app,
			 AsRelease *release,
			 AsAppValidateHelper *helper,
			 GError **error)
{
	const gchar *tmp;
	guint64 timestamp;
	guint number_para_max = 3;
	guint number_para_min = 1;

	/* relax the requirements a bit */
	if ((helper->flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		number_para_max = 4;
	}

	/* check version */
	tmp = as_release_get_version (release);
	if (tmp == NULL) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
				     "<release> has no version");
	}

	/* check timestamp */
	timestamp = as_release_get_timestamp (release);
	if (timestamp == 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
				     "<release> has no timestamp");
	}
	if (timestamp > 20120101 && timestamp < 20251231) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<release> timestamp should be a UNIX time");
	}

	/* for firmware, check urgency */
	if (as_app_get_kind (app) == AS_APP_KIND_FIRMWARE &&
	    as_release_get_urgency (release) == AS_URGENCY_KIND_UNKNOWN) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<release> urgency is required for firmware");
	}

	/* check description */
	tmp = as_release_get_description (release, "C");
	if (tmp != NULL) {
		if (as_app_validate_has_hyperlink (tmp)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<release> description should be "
					     "prose and not contain hyperlinks [%s]",
					     tmp);
		}
		if (!as_app_validate_description (tmp,
						  helper,
						  number_para_min,
						  number_para_max,
						  TRUE,
						  error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
as_app_validate_releases (AsApp *app, AsAppValidateHelper *helper, GError **error)
{
	AsRelease *release;
	GPtrArray *releases;
	guint i;

	/* only for AppData */
	if (as_app_get_source_kind (app) != AS_APP_SOURCE_KIND_APPDATA &&
	    as_app_get_source_kind (app) != AS_APP_SOURCE_KIND_METAINFO)
		return TRUE;

	releases = as_app_get_releases (app);
	for (i = 0; i < releases->len; i++) {
		release = g_ptr_array_index (releases, i);
		if (!as_app_validate_release (app, release, helper, error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
as_app_validate_setup_networking (AsAppValidateHelper *helper, GError **error)
{
	helper->session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT,
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

static gboolean
as_app_validate_license (const gchar *license_text, GError **error)
{
	guint i;
	g_auto(GStrv) licenses = NULL;

	licenses = as_utils_spdx_license_tokenize (license_text);
	if (licenses == NULL) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "SPDX license text '%s' could not be parsed",
			     license_text);
		return FALSE;
	}
	for (i = 0; licenses[i] != NULL; i++) {
		if (g_strcmp0 (licenses[i], "&") == 0 ||
		    g_strcmp0 (licenses[i], "|") == 0 ||
		    g_strcmp0 (licenses[i], "(") == 0 ||
		    g_strcmp0 (licenses[i], ")") == 0)
			continue;
		if (licenses[i][0] != '@' ||
		    !as_utils_is_spdx_license_id (licenses[i] + 1)) {
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

static gboolean
as_app_validate_is_content_license_id (const gchar *license_id)
{
	if (g_strcmp0 (license_id, "@CC0-1.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-3.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-3.0+") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-4.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-4.0+") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-SA-3.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-SA-3.0+") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-SA-4.0") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@CC-BY-SA-4.0+") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@GFDL-1.1") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@GFDL-1.2") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@GFDL-1.3") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "@FSFAP") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "&") == 0)
		return TRUE;
	if (g_strcmp0 (license_id, "|") == 0)
		return TRUE;
	return FALSE;
}

static gboolean
as_app_validate_is_content_license (const gchar *license)
{
	guint i;
	g_auto(GStrv) tokens = NULL;
	tokens = as_utils_spdx_license_tokenize (license);
	if (tokens == NULL)
		return FALSE;
	for (i = 0; tokens[i] != NULL; i++) {
		if (!as_app_validate_is_content_license_id (tokens[i]))
			return FALSE;
	}
	return TRUE;
}

static void
as_app_validate_helper_free (AsAppValidateHelper *helper)
{
	g_ptr_array_unref (helper->screenshot_urls);
	g_free (helper->previous_para_was_short_str);
	if (helper->session != NULL)
		g_object_unref (helper->session);
	g_free (helper);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsAppValidateHelper, as_app_validate_helper_free);

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
	GError *error_local = NULL;
	GHashTable *urls;
	GList *l;
	const gchar *description;
	const gchar *id;
	const gchar *key;
	const gchar *license;
	const gchar *name;
	const gchar *summary;
	const gchar *tmp;
	const gchar *update_contact;
	gboolean deprectated_failure = FALSE;
	gboolean require_appstream_spec_only = FALSE;
	gboolean require_contactdetails = TRUE;
	gboolean require_copyright = FALSE;
	gboolean require_project_license = FALSE;
	gboolean require_sentence_case = TRUE;
	gboolean require_translations = FALSE;
	gboolean require_url = TRUE;
	gboolean require_content_license = TRUE;
	gboolean require_name = TRUE;
	gboolean require_translation = TRUE;
	gboolean validate_license = TRUE;
	gboolean ret;
	guint length_name_max = 30;
	guint length_name_min = 3;
	guint length_summary_max = 100;
	guint length_summary_min = 8;
	guint number_para_max = 4;
	guint number_para_min = 2;
	guint str_len;
	g_autoptr(GList) keys = NULL;
	g_autoptr(AsAppValidateHelper) helper = g_new0 (AsAppValidateHelper, 1);

	/* relax the requirements a bit */
	if ((flags & AS_APP_VALIDATE_FLAG_RELAX) > 0) {
		length_name_max = 100;
		length_summary_max = 200;
		require_contactdetails = FALSE;
		require_content_license = FALSE;
		validate_license = FALSE;
		require_url = FALSE;
		number_para_max = 10;
		number_para_min = 1;
		require_sentence_case = FALSE;
		require_translation = FALSE;
		switch (as_app_get_source_kind (app)) {
		case AS_APP_SOURCE_KIND_METAINFO:
		case AS_APP_SOURCE_KIND_APPDATA:
			require_name = FALSE;
			break;
		default:
			break;
		}
	}

	/* make the requirements more strict */
	if ((flags & AS_APP_VALIDATE_FLAG_STRICT) > 0) {
		deprectated_failure = TRUE;
		require_copyright = TRUE;
		require_translations = TRUE;
		require_project_license = TRUE;
		require_content_license = TRUE;
		require_appstream_spec_only = TRUE;
	}

	/* addons don't need such a long description */
	switch (as_app_get_source_kind (app)) {
	case AS_APP_SOURCE_KIND_METAINFO:
	case AS_APP_SOURCE_KIND_APPDATA:
		number_para_min = 1;
		break;
	default:
		break;
	}

	/* set up networking */
	helper->app = app;
	helper->probs = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	helper->screenshot_urls = g_ptr_array_new_with_free_func (g_free);
	helper->flags = flags;
	if (!as_app_validate_setup_networking (helper, error))
		return NULL;

	/* id */
	ret = FALSE;
	id = as_app_get_id (app);
	switch (as_app_get_kind (app)) {
	case AS_APP_KIND_DESKTOP:
	case AS_APP_KIND_WEB_APP:
		if (g_str_has_suffix (id, ".desktop"))
			ret = TRUE;
		break;
	case AS_APP_KIND_INPUT_METHOD:
		if (g_str_has_suffix (id, ".xml"))
			ret = TRUE;
		else if (g_str_has_suffix (id, ".db"))
			ret = TRUE;
		break;
	case AS_APP_KIND_CODEC:
		if (g_str_has_prefix (id, "gstreamer"))
			ret = TRUE;
		break;
	case AS_APP_KIND_UNKNOWN:
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<id> has invalid type attribute");

		break;
	case AS_APP_KIND_FIRMWARE:
		if (g_str_has_suffix (id, ".firmware"))
			ret = TRUE;
		break;
	default:
		/* anything goes */
		ret = TRUE;
		break;
	}
	if (!ret) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_MARKUP_INVALID,
				     "<id> does not have correct extension for kind");
	}

	/* metadata_license */
	license = as_app_get_metadata_license (app);
	if (license != NULL) {
		if (require_content_license &&
		    !as_app_validate_is_content_license (license)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "<metadata_license> is not valid [%s]",
					     license);
		} else if (validate_license) {
			ret = as_app_validate_license (license, &error_local);
			if (!ret) {
				g_prefix_error (&error_local,
						"<metadata_license> is not valid [%s]",
						license);
				ai_app_validate_add (helper,
						     AS_PROBLEM_KIND_TAG_INVALID,
						     "%s", error_local->message);
				g_clear_error (&error_local);
			}
		}
	}
	if (license == NULL) {
		switch (as_app_get_source_kind (app)) {
		case AS_APP_SOURCE_KIND_APPDATA:
		case AS_APP_SOURCE_KIND_METAINFO:
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_MISSING,
					     "<metadata_license> is not present");
			break;
		default:
			break;
		}
	}

	/* project_license */
	license = as_app_get_project_license (app);
	if (license != NULL && validate_license) {
		ret = as_app_validate_license (license, &error_local);
		if (!ret) {
			g_prefix_error (&error_local,
					"<project_license> is not valid [%s]",
					license);
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "%s", error_local->message);
			g_clear_error (&error_local);
		}
	}
	if (require_project_license && license == NULL) {
		switch (as_app_get_source_kind (app)) {
		case AS_APP_SOURCE_KIND_APPDATA:
		case AS_APP_SOURCE_KIND_METAINFO:
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_MISSING,
					     "<project_license> is not present");
			break;
		default:
			break;
		}
	}

	/* translation */
	if (require_translation &&
	    as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA &&
	    as_app_get_translations (app)->len == 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<translation> not specified");
	}

	/* pkgname */
	if (as_app_get_pkgname_default (app) != NULL &&
	    as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_METAINFO) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "<pkgname> not allowed in metainfo");
	}

	/* appdata */
	if (as_app_get_icon_default (app) != NULL &&
	    as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "<icon> not allowed in appdata");
	}

	/* extends */
	if (as_app_get_extends(app)->len == 0 &&
	    as_app_get_kind (app) == AS_APP_KIND_ADDON &&
	    as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_METAINFO) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<extends> is not present");
	}

	/* update_contact */
	update_contact = as_app_get_update_contact (app);
	if (g_strcmp0 (update_contact,
		       "someone_who_cares@upstream_project.org") == 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "<update_contact> is still set to a dummy value");
	}
	if (update_contact != NULL && strlen (update_contact) < 6) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<update_contact> is too short [%s]",
				     update_contact);
	}
	if (require_contactdetails && update_contact == NULL) {
		switch (as_app_get_source_kind (app)) {
		case AS_APP_SOURCE_KIND_APPDATA:
		case AS_APP_SOURCE_KIND_METAINFO:
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_MISSING,
					     "<update_contact> is not present");
			break;
		default:
			break;
		}
	}

	/* only found for files */
	problems = as_app_get_problems (app);
	if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA ||
	    as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_METAINFO) {
		if ((problems & AS_APP_PROBLEM_NO_XML_HEADER) > 0) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_MARKUP_INVALID,
					     "<?xml> header not found");
		}
		if (require_copyright &&
		    (problems & AS_APP_PROBLEM_NO_COPYRIGHT_INFO) > 0) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_VALUE_MISSING,
					     "<!-- Copyright [year] [name] --> is not present");
		}
		if (deprectated_failure &&
		    (problems & AS_APP_PROBLEM_UPDATECONTACT_FALLBACK) > 0) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "<updatecontact> should be <update_contact>");
		}
	}

	/* check invalid values */
	if ((problems & AS_APP_PROBLEM_INVALID_PROJECT_GROUP) > 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "<project_group> is not valid");
	}

	/* only allow XML in the specification */
	if (require_appstream_spec_only &&
	    (problems & AS_APP_PROBLEM_INVALID_XML_TAG) > 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "XML data contains unknown tag");
	}

	/* only allow XML in the specification */
	if (problems & AS_APP_PROBLEM_EXPECTED_CHILDREN) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "Expected children for tag");
	}

	/* only allow XML in the specification */
	if (problems & AS_APP_PROBLEM_INVALID_KEYWORDS) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_INVALID,
				     "<keyword> invalid contents");
	}

	/* check for things that have to exist */
	if (as_app_get_id (app) == NULL) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<id> is not present");
	}

	/* url */
	urls = as_app_get_urls (app);
	keys = g_hash_table_get_keys (urls);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		if (g_strcmp0 (key, "unknown") == 0) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "<url> type invalid [%s]", key);
		}
		tmp = g_hash_table_lookup (urls, key);
		if (tmp == NULL || tmp[0] == '\0')
			continue;
		if (!g_str_has_prefix (tmp, "http://") &&
		    !g_str_has_prefix (tmp, "https://")) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_INVALID,
					     "<url> does not start with 'http://' [%s]",
					     tmp);
		}
	}

	/* screenshots */
	as_app_validate_screenshots (app, helper);

	/* icons */
	as_app_validate_icons (app, helper);

	/* releases */
	if (!as_app_validate_releases (app, helper, error))
		return NULL;

	/* name */
	name = as_app_get_name (app, "C");
	if (name != NULL) {
		str_len = (guint) strlen (name);
		if (str_len < length_name_min) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> is too short [%s]", name);
		}
		if (str_len > length_name_max) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> is too long [%s]", name);
		}
		if (ai_app_validate_fullstop_ending (name)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> cannot end in '.' [%s]",
					     name);
		}
		if (as_app_validate_has_hyperlink (name)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> cannot contain a hyperlink [%s]",
					     name);
		}
		if (require_sentence_case &&
		    !as_app_validate_has_first_word_capital (helper, name)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<name> requires sentence case [%s]",
					     name);
		}
	} else if (require_name) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<name> is not present");
	}

	/* comment */
	summary = as_app_get_comment (app, "C");
	if (summary != NULL) {
		str_len = (guint) strlen (summary);
		if (str_len < length_summary_min) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> is too short [%s]",
					     summary);
		}
		if (str_len > length_summary_max) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> is too long [%s]",
					     summary);
		}
		if (ai_app_validate_fullstop_ending (summary)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> cannot end in '.' [%s]",
					     summary);
		}
		if (as_app_validate_has_hyperlink (summary)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> cannot contain a hyperlink [%s]",
					     summary);
		}
		if (require_sentence_case &&
		    !as_app_validate_has_first_word_capital (helper, summary)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<summary> requires sentence case [%s]",
					     summary);
		}
	} else if (require_name) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_TAG_MISSING,
				     "<summary> is not present");
	}
	if (summary != NULL && name != NULL &&
	    strlen (summary) < strlen (name)) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_STYLE_INCORRECT,
				     "<summary> is shorter than <name>");
	}
	description = as_app_get_description (app, "C");
	if (description != NULL) {
		ret = as_app_validate_description (description,
						   helper,
						   number_para_min,
						   number_para_max,
						   FALSE,
						   &error_local);
		if (!ret) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_MARKUP_INVALID,
					     "%s", error_local->message);
			g_error_free (error_local);
		}
	}
	if (require_translations) {
		if (name != NULL &&
		    as_app_get_name_size (app) == 1 &&
		    (problems & AS_APP_PROBLEM_INTLTOOL_NAME) == 0) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
					     "<name> has no translations");
		}
		if (summary != NULL &&
		    as_app_get_comment_size (app) == 1 &&
		    (problems & AS_APP_PROBLEM_INTLTOOL_SUMMARY) == 0) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
					     "<summary> has no translations");
		}
		if (description != NULL &&
		    as_app_get_description_size (app) == 1 &&
		    (problems & AS_APP_PROBLEM_INTLTOOL_DESCRIPTION) == 0) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
					     "<description> has no translations");
		}
	}

	/* developer_name */
	name = as_app_get_developer_name (app, NULL);
	if (name != NULL) {
		str_len = (guint) strlen (name);
		if (str_len < length_name_min) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<developer_name> is too short [%s]",
					     name);
		}
		if (str_len > length_name_max) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<developer_name> is too long [%s]",
					     name);
		}
		if (as_app_validate_has_hyperlink (name)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<developer_name> cannot contain a hyperlink [%s]",
					     name);
		}
		if (as_app_validate_has_email (name)) {
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_STYLE_INCORRECT,
					     "<developer_name> cannot contain an email address [%s]",
					     name);
		}
	}

	/* using deprecated names */
	if (deprectated_failure && (problems & AS_APP_PROBLEM_DEPRECATED_LICENCE) > 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				     "<licence> is deprecated, use "
				     "<metadata_license> instead");
	}
	if ((problems & AS_APP_PROBLEM_MULTIPLE_ENTRIES) > 0) {
		ai_app_validate_add (helper,
				     AS_PROBLEM_KIND_MARKUP_INVALID,
				     "<application> used more than once");
	}

	/* require homepage */
	if (require_url && as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE) == NULL) {
		switch (as_app_get_source_kind (app)) {
		case AS_APP_SOURCE_KIND_APPDATA:
		case AS_APP_SOURCE_KIND_METAINFO:
			ai_app_validate_add (helper,
					     AS_PROBLEM_KIND_TAG_MISSING,
					     "<url> is not present");
			break;
		default:
			break;
		}
	}
	return helper->probs;
}
