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

#include <glib.h>
#include <stdlib.h>

#include "as-app-private.h"
#include "as-cleanup.h"
#include "as-enums.h"
#include "as-image-private.h"
#include "as-node-private.h"
#include "as-problem.h"
#include "as-provide-private.h"
#include "as-release-private.h"
#include "as-screenshot-private.h"
#include "as-store.h"
#include "as-tag.h"
#include "as-utils-private.h"
#include "as-yaml.h"

/**
 * cd_test_get_filename:
 **/
static gchar *
as_test_get_filename (const gchar *filename)
{
	char full_tmp[PATH_MAX];
	gchar *tmp;
	_cleanup_free_ gchar *path = NULL;

	path = g_build_filename (TESTDATADIR, filename, NULL);
	tmp = realpath (path, full_tmp);
	if (tmp == NULL)
		return NULL;
	return g_strdup (full_tmp);
}

static void
as_test_tag_func (void)
{
	guint i;

	/* simple test */
	g_assert_cmpstr (as_tag_to_string (AS_TAG_URL), ==, "url");
	g_assert_cmpstr (as_tag_to_string (AS_TAG_UNKNOWN), ==, "unknown");
	g_assert_cmpint (as_tag_from_string ("url"), ==, AS_TAG_URL);
	g_assert_cmpint (as_tag_from_string ("xxx"), ==, AS_TAG_UNKNOWN);

	/* deprecated names */
	g_assert_cmpint (as_tag_from_string_full ("appcategories",
						  AS_TAG_FLAG_USE_FALLBACKS),
						  ==, AS_TAG_CATEGORIES);

	/* test we can go back and forth */
	for (i = 0; i < AS_TAG_LAST; i++)
		g_assert_cmpint (as_tag_from_string (as_tag_to_string (i)), ==, i);
}

static void
as_test_release_func (void)
{
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	const gchar *src = "<release version=\"0.1.2\" timestamp=\"123\"/>";
	gboolean ret;
	_cleanup_object_unref_ AsRelease *release = NULL;

	release = as_release_new ();

	/* to object */
	root = as_node_from_xml (src, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "release");
	g_assert (n != NULL);
	ret = as_release_node_parse (release, n, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_release_get_timestamp (release), ==, 123);
	g_assert_cmpstr (as_release_get_version (release), ==, "0.1.2");

	/* back to node */
	root = as_node_new ();
	n = as_release_node_insert (release, root, 0.4);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_provide_func (void)
{
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	const gchar *src = "<binary>/usr/bin/gnome-shell</binary>";
	gboolean ret;
	_cleanup_object_unref_ AsProvide *provide = NULL;

	provide = as_provide_new ();

	/* to object */
	root = as_node_from_xml (src, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "binary");
	g_assert (n != NULL);
	ret = as_provide_node_parse (provide, n, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_provide_get_kind (provide), ==, AS_PROVIDE_KIND_BINARY);
	g_assert_cmpstr (as_provide_get_value (provide), ==, "/usr/bin/gnome-shell");

	/* back to node */
	root = as_node_new ();
	n = as_provide_node_insert (provide, root, 0.4);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_release_desc_func (void)
{
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<release version=\"0.1.2\" timestamp=\"123\">"
		"<description><p>This is a new release</p></description>"
		"<description xml:lang=\"pl\"><p>Oprogramowanie</p></description>"
		"</release>";
	_cleanup_object_unref_ AsRelease *release;

	release = as_release_new ();

	/* to object */
	root = as_node_from_xml (src, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "release");
	g_assert (n != NULL);
	ret = as_release_node_parse (release, n, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_release_get_timestamp (release), ==, 123);
	g_assert_cmpstr (as_release_get_version (release), ==, "0.1.2");
	g_assert_cmpstr (as_release_get_description (release, "pl"), ==,
				"<p>Oprogramowanie</p>");

	/* back to node */
	root = as_node_new ();
	n = as_release_node_insert (release, root, 0.6);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

typedef enum {
	AS_TEST_RESIZE_NEAREST,
	AS_TEST_RESIZE_TILES,
	AS_TEST_RESIZE_BILINEAR,
	AS_TEST_RESIZE_HYPER,
	AS_TEST_RESIZE_BILINEAR_SHARP,
	AS_TEST_RESIZE_HYPER_SHARP,
	AS_TEST_RESIZE_LAST,
} AsTestResize;

static const gchar *
as_test_resize_to_string (AsTestResize rz)
{
	if (rz == AS_TEST_RESIZE_NEAREST)
		return "nearest";
	if (rz == AS_TEST_RESIZE_TILES)
		return "tiles";
	if (rz == AS_TEST_RESIZE_BILINEAR)
		return "bilinear";
	if (rz == AS_TEST_RESIZE_HYPER)
		return "hyper";
	if (rz == AS_TEST_RESIZE_BILINEAR_SHARP)
		return "bilinear-sharp";
	if (rz == AS_TEST_RESIZE_HYPER_SHARP)
		return "hyper-sharp";
	return NULL;
}

static void
as_test_image_resize_filename (AsTestResize rz, const gchar *in, const gchar *out)
{
	gboolean ret;
	_cleanup_object_unref_ GdkPixbuf *pb = NULL;
	_cleanup_object_unref_ GdkPixbuf *pb2 = NULL;

	pb = gdk_pixbuf_new_from_file (in, NULL);
	g_assert (pb != NULL);

	switch (rz) {
	case AS_TEST_RESIZE_NEAREST:
		pb2 = gdk_pixbuf_scale_simple (pb,
					       AS_IMAGE_LARGE_WIDTH,
					       AS_IMAGE_LARGE_HEIGHT,
					       GDK_INTERP_NEAREST);
		break;
	case AS_TEST_RESIZE_TILES:
		pb2 = gdk_pixbuf_scale_simple (pb,
					       AS_IMAGE_LARGE_WIDTH,
					       AS_IMAGE_LARGE_HEIGHT,
					       GDK_INTERP_TILES);
		break;
	case AS_TEST_RESIZE_BILINEAR:
		pb2 = gdk_pixbuf_scale_simple (pb,
					       AS_IMAGE_LARGE_WIDTH,
					       AS_IMAGE_LARGE_HEIGHT,
					       GDK_INTERP_BILINEAR);
		break;
	case AS_TEST_RESIZE_HYPER:
		pb2 = gdk_pixbuf_scale_simple (pb,
					       AS_IMAGE_LARGE_WIDTH,
					       AS_IMAGE_LARGE_HEIGHT,
					       GDK_INTERP_HYPER);
		break;
	case AS_TEST_RESIZE_BILINEAR_SHARP:
		pb2 = gdk_pixbuf_scale_simple (pb,
					       AS_IMAGE_LARGE_WIDTH,
					       AS_IMAGE_LARGE_HEIGHT,
					       GDK_INTERP_BILINEAR);
		as_pixbuf_sharpen (pb2, 1, -0.5);
		break;
	case AS_TEST_RESIZE_HYPER_SHARP:
		pb2 = gdk_pixbuf_scale_simple (pb,
					       AS_IMAGE_LARGE_WIDTH,
					       AS_IMAGE_LARGE_HEIGHT,
					       GDK_INTERP_HYPER);
		as_pixbuf_sharpen (pb2, 1, -0.5);
		break;
	default:
		g_assert_not_reached ();
	}

	ret = gdk_pixbuf_save (pb2, out, "png", NULL, NULL);
	g_assert (ret);
}

static void
as_test_image_alpha_func (void)
{
	gboolean ret;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_free_ gchar *fn_both = NULL;
	_cleanup_free_ gchar *fn_horiz = NULL;
	_cleanup_free_ gchar *fn_internal1 = NULL;
	_cleanup_free_ gchar *fn_internal2 = NULL;
	_cleanup_free_ gchar *fn_none = NULL;
	_cleanup_free_ gchar *fn_vert = NULL;
	_cleanup_object_unref_ AsImage *im;

	/* horiz */
	fn_horiz = as_test_get_filename ("alpha-horiz.png");
	im = as_image_new ();
	ret = as_image_load_filename (im, fn_horiz, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_image_get_alpha_flags (im), ==,
			 AS_IMAGE_ALPHA_FLAG_LEFT |
			 AS_IMAGE_ALPHA_FLAG_RIGHT);

	/* vert */
	fn_vert = as_test_get_filename ("alpha-vert.png");
	ret = as_image_load_filename (im, fn_vert, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_image_get_alpha_flags (im), ==,
			 AS_IMAGE_ALPHA_FLAG_TOP |
			 AS_IMAGE_ALPHA_FLAG_BOTTOM);

	/* both */
	fn_both = as_test_get_filename ("alpha-both.png");
	ret = as_image_load_filename (im, fn_both, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_image_get_alpha_flags (im), ==,
			 AS_IMAGE_ALPHA_FLAG_LEFT |
			 AS_IMAGE_ALPHA_FLAG_RIGHT |
			 AS_IMAGE_ALPHA_FLAG_TOP |
			 AS_IMAGE_ALPHA_FLAG_BOTTOM);

	/* internal */
	fn_internal1 = as_test_get_filename ("alpha-internal1.png");
	ret = as_image_load_filename (im, fn_internal1, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_image_get_alpha_flags (im), ==,
			 AS_IMAGE_ALPHA_FLAG_INTERNAL);

	fn_internal2 = as_test_get_filename ("alpha-internal2.png");
	ret = as_image_load_filename (im, fn_internal2, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_image_get_alpha_flags (im), ==,
			 AS_IMAGE_ALPHA_FLAG_INTERNAL);

	fn_none = as_test_get_filename ("ss-small.png");
	ret = as_image_load_filename (im, fn_none, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_image_get_alpha_flags (im), ==,
			 AS_IMAGE_ALPHA_FLAG_NONE);
}

static void
as_test_image_resize_func (void)
{
	GError *error = NULL;
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_free_ gchar *output_dir = NULL;

	/* only do this test if an "output" directory exists */
	output_dir = g_build_filename (TESTDATADIR, "output", NULL);
	if (!g_file_test (output_dir, G_FILE_TEST_EXISTS))
		return;

	/* look for test screenshots */
	dir = g_dir_open (TESTDATADIR, 0, &error);
	g_assert_no_error (error);
	g_assert (dir != NULL);
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		guint i;
		_cleanup_free_ gchar *path = NULL;

		if (!g_str_has_prefix (tmp, "ss-"))
			continue;
		path = g_build_filename (TESTDATADIR, tmp, NULL);

		for (i = 0; i < AS_TEST_RESIZE_LAST; i++) {
			_cleanup_free_ gchar *new_path = NULL;
			_cleanup_string_free_ GString *basename = NULL;

			basename = g_string_new (tmp);
			g_string_truncate (basename, basename->len - 4);
			g_string_append_printf (basename, "-%s.png",
						as_test_resize_to_string (i));
			new_path = g_build_filename (output_dir, basename->str, NULL);
			as_test_image_resize_filename (i, path, new_path);
		}
	}
}

static void
as_test_image_func (void)
{
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	const gchar *src =
		"<image type=\"thumbnail\" height=\"12\" width=\"34\">"
		"http://www.hughsie.com/a.jpg</image>";
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsImage *image = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;

	image = as_image_new ();

	/* to object */
	root = as_node_from_xml (src, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "image");
	g_assert (n != NULL);
	ret = as_image_node_parse (image, n, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_image_get_kind (image), ==, AS_IMAGE_KIND_THUMBNAIL);
	g_assert_cmpint (as_image_get_height (image), ==, 12);
	g_assert_cmpint (as_image_get_width (image), ==, 34);
	g_assert_cmpstr (as_image_get_url (image), ==, "http://www.hughsie.com/a.jpg");

	/* back to node */
	root = as_node_new ();
	n = as_image_node_insert (image, root, 0.4);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* read from image */
	filename = as_test_get_filename ("screenshot.png");
	ret = as_image_load_filename (image, filename, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_image_get_width (image), ==, 800);
	g_assert_cmpint (as_image_get_height (image), ==, 600);
	g_assert_cmpstr (as_image_get_basename (image), ==, "screenshot.png");
	g_assert_cmpstr (as_image_get_md5 (image), ==, "9de72240c27a6f8f2eaab692795cdafc");

	/* resample */
	pixbuf = as_image_save_pixbuf (image,
				       752, 423,
				       AS_IMAGE_SAVE_FLAG_PAD_16_9);
	g_assert_cmpint (gdk_pixbuf_get_width (pixbuf), ==, 752);
	g_assert_cmpint (gdk_pixbuf_get_height (pixbuf), ==, 423);

	/* save */
	ret = as_image_save_filename (image,
				      "/tmp/foo.png",
				      0, 0,
				      AS_IMAGE_SAVE_FLAG_NONE,
				      &error);
	g_assert_no_error (error);
	g_assert (ret);
}

static void
as_test_screenshot_func (void)
{
	GPtrArray *images;
	AsImage *im;
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	const gchar *src =
		"<screenshot>"
		"<caption>Hello</caption>"
		"<image type=\"source\" height=\"800\" width=\"600\">http://1.png</image>"
		"<image type=\"thumbnail\" height=\"100\" width=\"100\">http://2.png</image>"
		"</screenshot>";
	gboolean ret;
	_cleanup_object_unref_ AsScreenshot *screenshot = NULL;

	screenshot = as_screenshot_new ();

	/* to object */
	root = as_node_from_xml (src, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "screenshot");
	g_assert (n != NULL);
	ret = as_screenshot_node_parse (screenshot, n, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpint (as_screenshot_get_kind (screenshot), ==, AS_SCREENSHOT_KIND_NORMAL);
	g_assert_cmpstr (as_screenshot_get_caption (screenshot, "C"), ==, "Hello");
	images = as_screenshot_get_images (screenshot);
	g_assert_cmpint (images->len, ==, 2);
	im = as_screenshot_get_source (screenshot);
	g_assert (im != NULL);
	g_assert_cmpstr (as_image_get_url (im), ==, "http://1.png");
	as_node_unref (root);

	/* get closest */
	im = as_screenshot_get_image (screenshot, 120, 120);
	g_assert (im != NULL);
	g_assert_cmpstr (as_image_get_url (im), ==, "http://2.png");
	im = as_screenshot_get_image (screenshot, 800, 560);
	g_assert (im != NULL);
	g_assert_cmpstr (as_image_get_url (im), ==, "http://1.png");

	/* back to node */
	root = as_node_new ();
	n = as_screenshot_node_insert (screenshot, root, 0.6);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_app_func (void)
{
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<component priority=\"-4\" type=\"desktop\">"
		"<id>org.gnome.Software.desktop</id>"
		"<pkgname>gnome-software</pkgname>"
		"<source_pkgname>gnome-software-src</source_pkgname>"
		"<name>Software</name>"
		"<name xml:lang=\"pl\">Oprogramowanie</name>"
		"<summary>Application manager</summary>"
		"<developer_name>GNOME Foundation</developer_name>"
		"<description><p>Software allows you to find stuff</p></description>"
		"<description xml:lang=\"pt_BR\"><p>O aplicativo Software.</p></description>"
		"<icon type=\"cached\">org.gnome.Software.png</icon>"
		"<categories>"
		"<category>System</category>"
		"</categories>"
		"<architectures>"
		"<arch>i386</arch>"
		"</architectures>"
		"<keywords>"
		"<keyword>Installing</keyword>"
		"</keywords>"
		"<kudos>"
		"<kudo>SearchProvider</kudo>"
		"</kudos>"
		"<vetos>"
		"<veto>Required AppData: ConsoleOnly</veto>"
		"</vetos>"
		"<mimetypes>"
		"<mimetype>application/vnd.oasis.opendocument.spreadsheet</mimetype>"
		"</mimetypes>"
		"<project_license>GPLv2+</project_license>"
		"<url type=\"homepage\">https://wiki.gnome.org/Design/Apps/Software</url>"
		"<project_group>GNOME</project_group>"
		"<compulsory_for_desktop>GNOME</compulsory_for_desktop>"
		"<screenshots>"
		"<screenshot type=\"default\">"
		"<image type=\"thumbnail\" height=\"351\" width=\"624\">http://a.png</image>"
		"</screenshot>"
		"<screenshot>"
		"<image type=\"thumbnail\">http://b.png</image>"
		"</screenshot>"
		"</screenshots>"
		"<releases>"
		"<release version=\"3.11.90\" timestamp=\"1392724800\"/>"
		"</releases>"
		"<provides>"
		"<binary>/usr/bin/gnome-shell</binary>"
		"<dbus type=\"session\">org.gnome.Software</dbus>"
		"<dbus type=\"system\">org.gnome.Software2</dbus>"
		"</provides>"
		"<languages>"
		"<lang percentage=\"90\">en_GB</lang>"
		"<lang>pl</lang>"
		"</languages>"
		"<metadata>"
		"<value key=\"SomethingRandom\"/>"
		"</metadata>"
		"</component>";
	_cleanup_object_unref_ AsApp *app = NULL;

	app = as_app_new ();

	/* to object */
	root = as_node_from_xml (src, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "component");
	g_assert (n != NULL);
	ret = as_app_node_parse (app, n, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_id (app), ==, "org.gnome.Software.desktop");
	g_assert_cmpstr (as_app_get_id_filename (app), ==, "org.gnome.Software");
	g_assert_cmpstr (as_app_get_name (app, "pl"), ==, "Oprogramowanie");
	g_assert_cmpstr (as_app_get_comment (app, NULL), ==, "Application manager");
	g_assert_cmpstr (as_app_get_developer_name (app, NULL), ==, "GNOME Foundation");
	g_assert_cmpstr (as_app_get_icon (app), ==, "org.gnome.Software.png");
	g_assert_cmpstr (as_app_get_source_pkgname (app), ==, "gnome-software-src");
	g_assert_cmpint (as_app_get_icon_kind (app), ==, AS_ICON_KIND_CACHED);
	g_assert_cmpint (as_app_get_source_kind (app), ==, AS_APP_SOURCE_KIND_UNKNOWN);
	g_assert_cmpstr (as_app_get_project_group (app), ==, "GNOME");
	g_assert_cmpstr (as_app_get_project_license (app), ==, "GPLv2+");
	g_assert_cmpint (as_app_get_categories(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_priority (app), ==, -4);
	g_assert_cmpint (as_app_get_screenshots(app)->len, ==, 2);
	g_assert_cmpint (as_app_get_releases(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_provides(app)->len, ==, 3);
	g_assert_cmpint (as_app_get_kudos(app)->len, ==, 1);
	g_assert_cmpstr (as_app_get_metadata_item (app, "SomethingRandom"), ==, "");
	g_assert_cmpint (as_app_get_language (app, "en_GB"), ==, 90);
	g_assert_cmpint (as_app_get_language (app, "pl"), ==, 0);
	g_assert_cmpint (as_app_get_language (app, "xx_XX"), ==, -1);
	g_assert (as_app_has_kudo (app, "SearchProvider"));
	g_assert (as_app_has_kudo_kind (app, AS_KUDO_KIND_SEARCH_PROVIDER));
	g_assert (!as_app_has_kudo (app, "MagicValue"));
	g_assert (!as_app_has_kudo_kind (app, AS_KUDO_KIND_USER_DOCS));
	as_node_unref (root);

	/* back to node */
	root = as_node_new ();
	n = as_app_node_insert (app, root, 0.8);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* test contact demunging */
	as_app_set_update_contact (app, "richard_at_hughsie_dot_co_dot_uk", -1);
	g_assert_cmpstr (as_app_get_update_contact (app), ==, "richard@hughsie.co.uk");
}

static void
as_test_app_validate_check (GPtrArray *array,
			    AsProblemKind kind,
			    const gchar *message)
{
	AsProblem *problem;
	guint i;

	for (i = 0; i < array->len; i++) {
		problem = g_ptr_array_index (array, i);
		if (as_problem_get_kind (problem) == kind &&
		    g_strcmp0 (as_problem_get_message (problem), message) == 0)
			return;
	}
	for (i = 0; i < array->len; i++) {
		problem = g_ptr_array_index (array, i);
		g_print ("%i\t%s\n",
			 as_problem_get_kind (problem),
			 as_problem_get_message (problem));
	}
	g_assert_cmpstr (message, ==, "not-found");
}

static void
as_test_app_validate_appdata_good_func (void)
{
	AsImage *im;
	AsProblem *problem;
	AsScreenshot *ss;
	GError *error = NULL;
	GPtrArray *images;
	GPtrArray *probs;
	GPtrArray *screenshots;
	gboolean ret;
	guint i;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("success.appdata.xml");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check success */
	g_assert_cmpint (as_app_get_id_kind (app), ==, AS_ID_KIND_DESKTOP);
	g_assert_cmpstr (as_app_get_id (app), ==, "gnome-power-statistics.desktop");
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "0 A.D.");
	g_assert_cmpstr (as_app_get_comment (app, "C"), ==, "Observe power management");
	g_assert_cmpstr (as_app_get_metadata_license (app), ==, "CC0-1.0 and CC-BY-3.0");
	g_assert_cmpstr (as_app_get_update_contact (app), ==, "richard@hughsie.com");
	g_assert_cmpstr (as_app_get_project_group (app), ==, "GNOME");
	g_assert_cmpstr (as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE), ==,
			 "http://www.gnome.org/projects/gnome-power-manager/");
	g_assert_cmpstr (as_app_get_description (app, "C"), !=, NULL);
	g_assert_cmpint (as_app_get_description_size (app), ==, 1);
	probs = as_app_validate (app, AS_APP_VALIDATE_FLAG_NO_NETWORK, &error);
	g_assert_no_error (error);
	g_assert (probs != NULL);
	for (i = 0; i < probs->len; i++) {
		problem = g_ptr_array_index (probs, i);
		g_warning ("%s", as_problem_get_message (problem));
	}
	g_assert_cmpint (probs->len, ==, 0);
	g_ptr_array_unref (probs);

	/* check screenshots were loaded */
	screenshots = as_app_get_screenshots (app);
	g_assert_cmpint (screenshots->len, ==, 1);
	ss = g_ptr_array_index (screenshots, 0);
	g_assert_cmpint (as_screenshot_get_kind (ss), ==, AS_SCREENSHOT_KIND_DEFAULT);
	images = as_screenshot_get_images (ss);
	g_assert_cmpint (images->len, ==, 1);
	im = g_ptr_array_index (images, 0);
	g_assert_cmpstr (as_image_get_url (im), ==, "https://projects.gnome.org/gnome-power-manager/images/gpm-low-batt.png");
	g_assert_cmpint (as_image_get_width (im), ==, 355);
	g_assert_cmpint (as_image_get_height (im), ==, 134);
	g_assert_cmpint (as_image_get_kind (im), ==, AS_IMAGE_KIND_SOURCE);
}

static void
as_test_app_validate_metainfo_good_func (void)
{
	AsProblem *problem;
	GError *error = NULL;
	GPtrArray *probs;
	gboolean ret;
	guint i;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("example.metainfo.xml");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check success */
	g_assert_cmpint (as_app_get_id_kind (app), ==, AS_ID_KIND_ADDON);
	g_assert_cmpstr (as_app_get_id (app), ==, "gedit-code-assistance");
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Code assistance");
	g_assert_cmpstr (as_app_get_comment (app, "C"), ==, "Code assistance for C, C++ and Objective-C");
	g_assert_cmpstr (as_app_get_metadata_license (app), ==, "CC0-1.0");
	g_assert_cmpstr (as_app_get_project_license (app), ==, "GPL-3.0+");
	g_assert_cmpstr (as_app_get_update_contact (app), ==, "richard@hughsie.com");
	g_assert_cmpstr (as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE), ==,
			 "http://projects.gnome.org/gedit");
	g_assert_cmpstr (as_app_get_description (app, "C"), ==, NULL);

	/* validate */
	probs = as_app_validate (app, AS_APP_VALIDATE_FLAG_NO_NETWORK, &error);
	g_assert_no_error (error);
	g_assert (probs != NULL);
	for (i = 0; i < probs->len; i++) {
		problem = g_ptr_array_index (probs, i);
		g_warning ("%s", as_problem_get_message (problem));
	}
	g_assert_cmpint (probs->len, ==, 0);
	g_ptr_array_unref (probs);
}

static void
as_test_app_validate_intltool_func (void)
{
	AsProblem *problem;
	GError *error = NULL;
	GPtrArray *probs;
	gboolean ret;
	guint i;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("intltool.appdata.xml.in");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check success */
	g_assert_cmpint (as_app_get_id_kind (app), ==, AS_ID_KIND_DESKTOP);
	g_assert_cmpstr (as_app_get_id (app), ==, "gnome-power-statistics.desktop");
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "0 A.D.");
	g_assert_cmpstr (as_app_get_comment (app, "C"), ==, "Observe power management");
	probs = as_app_validate (app, AS_APP_VALIDATE_FLAG_NO_NETWORK, &error);
	g_assert_no_error (error);
	g_assert (probs != NULL);
	for (i = 0; i < probs->len; i++) {
		problem = g_ptr_array_index (probs, i);
		g_warning ("%s", as_problem_get_message (problem));
	}
	g_assert_cmpint (probs->len, ==, 0);
	g_ptr_array_unref (probs);
}

static void
as_test_app_translated_func (void)
{
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("translated.appdata.xml");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check success */
	g_assert_cmpstr (as_app_get_description (app, "C"), ==, "<p>Awesome</p>");
	g_assert_cmpstr (as_app_get_description (app, "pl"), ==, "<p>Asomeski</p>");
	g_assert_cmpint (as_app_get_description_size (app), ==, 2);
}

static void
as_test_app_validate_file_bad_func (void)
{
	AsProblem *problem;
	GError *error = NULL;
	gboolean ret;
	guint i;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *probs = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("broken.appdata.xml");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (as_app_get_description (app, "C"), !=, NULL);
	g_assert_cmpint (as_app_get_description_size (app), ==, 1);

	probs = as_app_validate (app, AS_APP_VALIDATE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (probs != NULL);
	for (i = 0; i < probs->len; i++) {
		problem = g_ptr_array_index (probs, i);
		g_debug ("%s", as_problem_get_message (problem));
	}
	g_assert_cmpint (probs->len, ==, 26);

	as_test_app_validate_check (probs, AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				    "<id> has invalid type attribute");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_MARKUP_INVALID,
				    "<id> does not have correct extension for kind");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<metadata_license> is not valid");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<project_license> is not valid: SPDX ID 'CC1' unknown");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<updatecontact> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<url> does not start with 'http://'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_MARKUP_INVALID,
				    "<?xml> header not found");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<name> cannot end in '.'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<summary> cannot end in '.'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "Not enough <screenshot> tags");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<li> is too short");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<li> cannot end in '.'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<ul> cannot start a description");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<ul> cannot start a description");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<p> should not start with 'This application'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<p> does not end in '.|:|!'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<p> is too short");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<p> cannot contain a hyperlink");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<release> description should be "
				    "prose and not contain hyperlinks");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				    "<release> timestamp should be a UNIX time");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
				    "<release> has no version");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
				    "<release> has no timestamp");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<p> requires sentence case");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<li> requires sentence case");
}

static void
as_test_app_validate_meta_bad_func (void)
{
	AsProblem *problem;
	GError *error = NULL;
	gboolean ret;
	guint i;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *probs = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("broken.metainfo.xml");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	probs = as_app_validate (app, AS_APP_VALIDATE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (probs != NULL);
	for (i = 0; i < probs->len; i++) {
		problem = g_ptr_array_index (probs, i);
		g_debug ("%s", as_problem_get_message (problem));
	}
	g_assert_cmpint (probs->len, ==, 7);
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<name> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<summary> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<url> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<updatecontact> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<extends> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<metadata_license> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<pkgname> not allowed in metainfo");
}

static void
as_test_store_local_app_install_func (void)
{
	AsApp *app;
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_free_ gchar *source_file = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;

	/* open test store */
	store = as_store_new ();
	filename = as_test_get_filename (".");
	as_store_set_destdir (store, filename);
	ret = as_store_load (store, AS_STORE_LOAD_FLAG_APP_INSTALL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_store_get_size (store), ==, 1);

	/* make sure app is valid */
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (app != NULL);
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Test");
	g_assert_cmpstr (as_app_get_comment (app, "C"), ==, "A test program");
	g_assert_cmpstr (as_app_get_icon (app), ==, "test");
	g_assert_cmpint (as_app_get_icon_kind (app), ==, AS_ICON_KIND_CACHED);
	g_assert_cmpint (as_app_get_source_kind (app), ==, AS_APP_SOURCE_KIND_APPSTREAM);

	/* ensure we reference the correct file */
	source_file = g_build_filename (filename, "/usr", "share", "app-install",
					"desktop", "test.desktop", NULL);
	g_assert_cmpstr (as_app_get_source_file (app), ==, source_file);
}

static void
as_test_store_local_appdata_func (void)
{
	AsApp *app;
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;

	/* this are the warnings expected */
	g_test_expect_message (G_LOG_DOMAIN,
			       G_LOG_LEVEL_WARNING,
			       "ignoring description '*' from */broken.appdata.xml: Unknown tag '_p'");

	/* open test store */
	store = as_store_new ();
	filename = as_test_get_filename (".");
	as_store_set_destdir (store, filename);
	ret = as_store_load (store, AS_STORE_LOAD_FLAG_APPDATA, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_store_get_size (store), ==, 1);

	/* make sure app is valid */
	app = as_store_get_app_by_id (store, "broken.desktop");
	g_assert (app != NULL);
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Broken");
	g_assert_cmpint (as_app_get_source_kind (app), ==, AS_APP_SOURCE_KIND_APPDATA);
}

static void
as_test_store_validate_func (void)
{
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *probs = NULL;

	/* open file */
	store = as_store_new ();
	filename = as_test_get_filename ("validate.xml.gz");
	file = g_file_new_for_path (filename);
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_store_get_size (store), ==, 1);

	probs = as_store_validate (store, AS_APP_VALIDATE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (probs != NULL);
	g_assert_cmpint (probs->len, ==, 4);
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "metdata version is v0.1 and "
				    "<screenshots> only introduced in v0.4");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "metdata version is v0.1 and "
				    "<compulsory_for_desktop> only introduced in v0.4");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "metdata version is v0.1 and "
				    "<project_group> only introduced in v0.4");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "metdata version is v0.1 and "
				    "<description> markup was introduced in v0.6");
}

static void
as_test_app_validate_style_func (void)
{
	AsProblem *problem;
	GError *error = NULL;
	guint i;
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *probs = NULL;

	app = as_app_new ();
	as_app_add_url (app, AS_URL_KIND_UNKNOWN, "dave.com", -1);
	as_app_set_id (app, "dave.exe", -1);
	as_app_set_id_kind (app, AS_ID_KIND_DESKTOP);
	as_app_set_source_kind (app, AS_APP_SOURCE_KIND_APPDATA);
	as_app_set_metadata_license (app, "BSD", -1);
	as_app_set_project_license (app, "GPL-2.0+", -1);
	as_app_set_name (app, "C", "Test app name that is very log indeed.", -1);
	as_app_set_comment (app, "C", "Awesome", -1);
	as_app_set_update_contact (app, "someone_who_cares@upstream_project.org", -1);

	probs = as_app_validate (app, AS_APP_VALIDATE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (probs != NULL);
	for (i = 0; i < probs->len; i++) {
		problem = g_ptr_array_index (probs, i);
		g_debug ("%s", as_problem_get_message (problem));
	}
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<update_contact> is still set to a dummy value");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<url> type invalid");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<url> does not start with 'http://'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<metadata_license> is not valid");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<name> is too long");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<name> cannot end in '.'");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<summary> is too short");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_MARKUP_INVALID,
				    "<id> does not have correct extension for kind");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "Not enough <screenshot> tags");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<summary> is shorter than <name>");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<url> is not present");
	g_assert_cmpint (probs->len, ==, 11);
}

static void
as_test_app_parse_file_func (void)
{
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;

	/* create an AsApp from a desktop file */
	app = as_app_new ();
	filename = as_test_get_filename ("example.desktop");
	ret = as_app_parse_file (app,
				 filename,
				 AS_APP_PARSE_FLAG_ALLOW_VETO,
				 &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test things we found */
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Color Profile Viewer");
	g_assert_cmpstr (as_app_get_name (app, "pl"), ==, "Podgląd profilu kolorów");
	g_assert_cmpstr (as_app_get_comment (app, "C"), ==,
		"Inspect and compare installed color profiles");
	g_assert_cmpstr (as_app_get_comment (app, "pl"), ==,
		"Badanie i porównywanie zainstalowanych profilów kolorów");
	g_assert_cmpstr (as_app_get_icon (app), ==, "audio-input-microphone");
	g_assert_cmpint (as_app_get_icon_kind (app), ==, AS_ICON_KIND_STOCK);
	g_assert_cmpint (as_app_get_vetos(app)->len, ==, 1);
	g_assert_cmpstr (as_app_get_project_group (app), ==, NULL);
	g_assert_cmpstr (as_app_get_source_file (app), ==, filename);
	g_assert_cmpint (as_app_get_categories(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_keywords(app, NULL)->len, ==, 2);
	g_assert_cmpint (as_app_get_keywords(app, "pl")->len, ==, 1);
	g_assert (as_app_has_category (app, "System"));
	g_assert (!as_app_has_category (app, "NotGoingToExist"));

	/* reparse with heuristics */
	ret = as_app_parse_file (app,
				 filename,
				 AS_APP_PARSE_FLAG_ALLOW_VETO |
				 AS_APP_PARSE_FLAG_USE_HEURISTICS,
				 &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (as_app_get_project_group (app), ==, "GNOME");
	g_free (filename);

	/* reparse with invalid file */
	filename = as_test_get_filename ("settings-panel.desktop");
	ret = as_app_parse_file (app, filename, 0, &error);
	g_assert_error (error, AS_APP_ERROR, AS_APP_ERROR_INVALID_TYPE);
	g_assert (!ret);
	g_clear_error (&error);
}

static void
as_test_app_no_markup_func (void)
{
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<application>"
		"<id type=\"desktop\">org.gnome.Software.desktop</id>"
		"<description>Software is awesome:\n\n * Bada\n * Boom!</description>"
		"</application>";
	_cleanup_object_unref_ AsApp *app = NULL;

	app = as_app_new ();

	/* to object */
	root = as_node_from_xml (src, -1,
				 AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				 &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "application");
	g_assert (n != NULL);
	ret = as_app_node_parse (app, n, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_id (app), ==, "org.gnome.Software.desktop");
	g_assert_cmpstr (as_app_get_description (app, "C"), ==,
		"Software is awesome:\n\n * Bada\n * Boom!");
	as_node_unref (root);

	/* back to node */
	root = as_node_new ();
	n = as_app_node_insert (app, root, 0.4);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_node_reflow_text_func (void)
{
	gchar *tmp;

	/* plain text */
	tmp = as_node_reflow_text ("Dave", -1);
	g_assert_cmpstr (tmp, ==, "Dave");
	g_free (tmp);

	/* stripping */
	tmp = as_node_reflow_text ("    Dave    ", -1);
	g_assert_cmpstr (tmp, ==, "Dave");
	g_free (tmp);

	/* paragraph */
	tmp = as_node_reflow_text ("Dave\n\nSoftware", -1);
	g_assert_cmpstr (tmp, ==, "Dave\n\nSoftware");
	g_free (tmp);

	/* pathological */
	tmp = as_node_reflow_text (
		"\n"
		"  Dave: \n"
		"  Software is \n"
		"  awesome.\n\n\n"
		"  Okay!\n", -1);
	g_assert_cmpstr (tmp, ==, "Dave: Software is awesome.\n\nOkay!");
	g_free (tmp);
}

static void
as_test_node_sort_func (void)
{
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_node_unref_ GNode *root = NULL;
	_cleanup_string_free_ GString *str;

	root = as_node_from_xml ("<d>ddd</d><c>ccc</c><b>bbb</b><a>aaa</a>", -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);

	/* verify that the tags are sorted */
	str = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_SORT_CHILDREN);
	g_assert_cmpstr (str->str, ==, "<a>aaa</a><b>bbb</b><c>ccc</c><d>ddd</d>");
}

static void
as_test_node_func (void)
{
	GNode *n1;
	GNode *n2;
	_cleanup_node_unref_ GNode *root = NULL;

	/* create a simple tree */
	root = as_node_new ();
	n1 = as_node_insert (root, "apps", NULL, 0,
			     "version", "2",
			     NULL);
	g_assert (n1 != NULL);
	g_assert_cmpstr (as_node_get_name (n1), ==, "apps");
	g_assert_cmpstr (as_node_get_data (n1), ==, NULL);
	g_assert_cmpstr (as_node_get_attribute (n1, "version"), ==, "2");
	g_assert_cmpint (as_node_get_attribute_as_int (n1, "version"), ==, 2);
	g_assert_cmpstr (as_node_get_attribute (n1, "xxx"), ==, NULL);
	n2 = as_node_insert (n1, "id", "hal", 0, NULL);
	g_assert (n2 != NULL);
	g_assert_cmpint (as_node_get_tag (n2), ==, AS_TAG_ID);
	g_assert_cmpstr (as_node_get_data (n2), ==, "hal");
	g_assert_cmpstr (as_node_get_attribute (n2, "xxx"), ==, NULL);

	/* remove an attribute */
	as_node_remove_attribute (n1, "version");
	g_assert_cmpstr (as_node_get_attribute (n1, "version"), ==, NULL);

	/* replace some node data */
	as_node_set_data (n2, "udev", -1, 0);
	g_assert_cmpstr (as_node_get_data (n2), ==, "udev");
	as_node_add_attribute (n2, "enabled", "true", -1);
	g_assert_cmpstr (as_node_get_attribute (n2, "enabled"), ==, "true");

	/* find the n2 node */
	n2 = as_node_find (root, "apps/id");
	g_assert (n2 != NULL);
	g_assert_cmpint (as_node_get_tag (n2), ==, AS_TAG_ID);

	/* don't find invalid nodes */
	n2 = as_node_find (root, "apps/id/xxx");
	g_assert (n2 == NULL);
	n2 = as_node_find (root, "apps/xxx");
	g_assert (n2 == NULL);
	n2 = as_node_find (root, "apps//id");
	g_assert (n2 == NULL);
}

static void
as_test_node_xml_func (void)
{
	const gchar *valid = "<!-- this documents foo --><foo><!-- this documents bar --><bar key=\"value\">baz</bar></foo>";
	GError *error = NULL;
	GNode *n2;
	GNode *root;
	GString *xml;

	/* invalid XML */
	root = as_node_from_xml ("<moo>", -1, 0, &error);
	g_assert (root == NULL);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_clear_error (&error);
	root = as_node_from_xml ("<foo></bar>", -1, 0, &error);
	g_assert (root == NULL);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_clear_error (&error);

	/* valid XML */
	root = as_node_from_xml (valid, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);

	n2 = as_node_find (root, "foo/bar");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_data (n2), ==, "baz");
	g_assert_cmpstr (as_node_get_comment (n2), ==, NULL);
	g_assert_cmpstr (as_node_get_attribute (n2, "key"), ==, "value");

	/* convert back */
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==, "<foo><bar key=\"value\">baz</bar></foo>");
	g_string_free (xml, TRUE);

	/* with newlines */
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==,
		"<foo>\n<bar key=\"value\">baz</bar>\n</foo>\n");
	g_string_free (xml, TRUE);

	/* fully formatted */
	xml = as_node_to_xml (root,
			      AS_NODE_TO_XML_FLAG_ADD_HEADER |
			      AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			      AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<foo>\n  <bar key=\"value\">baz</bar>\n</foo>\n");
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* convert all the children to XML */
	root = as_node_from_xml ("<p>One</p><p>Two</p>", -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_ALL), ==, 3);
	xml = as_node_to_xml (root->children, AS_NODE_TO_XML_FLAG_INCLUDE_SIBLINGS);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==, "<p>One</p><p>Two</p>");
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* keep comments */
	root = as_node_from_xml (valid, -1,
				 AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS,
				 &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n2 = as_node_find (root, "foo/bar");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_comment (n2), ==, "this documents bar");
	n2 = as_node_find (root, "foo");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_comment (n2), ==, "this documents foo");

	/* check comments were preserved */
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==, valid);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_node_hash_func (void)
{
	GHashTable *hash;
	GNode *n1;
	GNode *root;
	GString *xml;

	/* test un-swapped hash */
	root = as_node_new ();
	n1 = as_node_insert (root, "app", NULL, 0, NULL);
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (hash, (gpointer) "a", (gpointer) "1");
	g_hash_table_insert (hash, (gpointer) "b", (gpointer) "2");
	as_node_insert_hash (n1, "md1", "key", hash, 0);
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==,
		"<app><md1 key=\"a\">1</md1><md1 key=\"b\">2</md1></app>");
	g_string_free (xml, TRUE);
	g_hash_table_unref (hash);
	as_node_unref (root);

	/* test swapped hash */
	root = as_node_new ();
	n1 = as_node_insert (root, "app", NULL, AS_NODE_INSERT_FLAG_NONE, NULL);
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (hash, (gpointer) "a", (gpointer) "1");
	g_hash_table_insert (hash, (gpointer) "b", (gpointer) "2");
	as_node_insert_hash (n1, "md1", "key", hash, AS_NODE_INSERT_FLAG_SWAPPED);
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==,
		"<app><md1 key=\"1\">a</md1><md1 key=\"2\">b</md1></app>");
	g_string_free (xml, TRUE);
	g_hash_table_unref (hash);
	as_node_unref (root);
}

static void
as_test_node_localized_func (void)
{
	GHashTable *hash;
	GNode *n1;
	GNode *root;
	GString *xml;

	/* writing localized values */
	root = as_node_new ();
	n1 = as_node_insert (root, "app", NULL, 0, NULL);
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (hash, (gpointer) "C", (gpointer) "color");
	g_hash_table_insert (hash, (gpointer) "en_XX", (gpointer) "colour");
	as_node_insert_localized (n1, "name", hash, AS_NODE_INSERT_FLAG_NONE);
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==,
		"<app><name>color</name>"
		"<name xml:lang=\"en_XX\">colour</name></app>");
	g_string_free (xml, TRUE);
	g_hash_table_unref (hash);

	/* get the best locale */
	g_assert_cmpstr (as_node_get_localized_best (n1, "name"), ==, "color");

	/* get something that isn't there */
	hash = as_node_get_localized (n1, "comment");
	g_assert (hash == NULL);

	/* read them back */
	hash = as_node_get_localized (n1, "name");
	g_assert (hash != NULL);
	g_assert_cmpint (g_hash_table_size (hash), ==, 2);
	g_assert_cmpstr (g_hash_table_lookup (hash, "C"), ==, "color");
	g_assert_cmpstr (g_hash_table_lookup (hash, "en_XX"), ==, "colour");
	g_hash_table_unref (hash);

	as_node_unref (root);
}

static void
as_test_node_localized_wrap_func (void)
{
	GError *error = NULL;
	GNode *n1;
	const gchar *xml =
		"<description>"
		" <p>Hi</p>"
		" <p xml:lang=\"pl\">Czesc</p>"
		" <ul>"
		"  <li>First</li>"
		"  <li xml:lang=\"pl\">Pierwszy</li>"
		"  <li xml:lang=\"en_GB\">Hi</li>"
		" </ul>"
		"</description>";
	_cleanup_hashtable_unref_ GHashTable *hash = NULL;
	_cleanup_node_unref_ GNode *root = NULL;

	root = as_node_from_xml (xml, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);

	/* unwrap the locale data */
	n1 = as_node_find (root, "description");
	g_assert (n1 != NULL);
	hash = as_node_get_localized_unwrap (n1, &error);
	g_assert_no_error (error);
	g_assert (hash != NULL);
	g_assert_cmpint (g_hash_table_size (hash), ==, 3);
	g_assert_cmpstr (g_hash_table_lookup (hash, "C"), ==,
		"<p>Hi</p><ul><li>First</li></ul>");
	g_assert_cmpstr (g_hash_table_lookup (hash, "pl"), ==,
		"<p>Czesc</p><ul><li>Pierwszy</li></ul>");
	g_assert_cmpstr (g_hash_table_lookup (hash, "en_GB"), ==,
		"<ul><li>Hi</li></ul>");
}

static void
as_test_node_intltool_func (void)
{
	GNode *n;
	_cleanup_node_unref_ GNode *root;
	_cleanup_string_free_ GString *str;

	root = as_node_new ();
	n = as_node_insert (root, "description", NULL, AS_NODE_INSERT_FLAG_NONE, NULL);
	as_node_insert (n, "name", "Hello",
			AS_NODE_INSERT_FLAG_MARK_TRANSLATABLE, NULL);

	/* verify that the tags get prefixed with '_' */
	str = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (str->str, ==, "<description><_name>Hello</_name></description>");
}

static void
as_test_node_localized_wrap2_func (void)
{
	GError *error = NULL;
	GNode *n1;
	const gchar *xml =
		"<description>"
		" <p>Hi</p>"
		" <p xml:lang=\"pl\">Czesc</p>"
		" <ul>"
		"  <li>First</li>"
		"  <li>Second</li>"
		" </ul>"
		" <ul xml:lang=\"pl\">"
		"  <li>Pierwszy</li>"
		"  <li>Secondski</li>"
		" </ul>"
		"</description>";
	_cleanup_hashtable_unref_ GHashTable *hash = NULL;
	_cleanup_node_unref_ GNode *root = NULL;

	root = as_node_from_xml (xml, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);

	/* unwrap the locale data */
	n1 = as_node_find (root, "description");
	g_assert (n1 != NULL);
	hash = as_node_get_localized_unwrap (n1, &error);
	g_assert_no_error (error);
	g_assert (hash != NULL);
	g_assert_cmpint (g_hash_table_size (hash), ==, 2);
	g_assert_cmpstr (g_hash_table_lookup (hash, "C"), ==,
		"<p>Hi</p><ul><li>First</li><li>Second</li></ul>");
	g_assert_cmpstr (g_hash_table_lookup (hash, "pl"), ==,
		"<p>Czesc</p><ul><li>Pierwszy</li><li>Secondski</li></ul>");

	/* find the Polish first paragraph */
	n1 = as_node_find_with_attribute (root, "description/p", "xml:lang", "pl");
	g_assert (n1 != NULL);
	g_assert_cmpstr (as_node_get_data (n1), ==, "Czesc");
}

static void
as_test_app_subsume_func (void)
{
	GList *list;
	_cleanup_object_unref_ AsApp *app = NULL;
	_cleanup_object_unref_ AsApp *donor = NULL;
	_cleanup_object_unref_ AsScreenshot *ss = NULL;

	donor = as_app_new ();
	as_app_set_state (donor, AS_APP_STATE_INSTALLED);
	as_app_set_icon (donor, "gtk-find", -1);
	as_app_add_pkgname (donor, "hal", -1);
	as_app_add_language (donor, -1, "en_GB", -1);
	as_app_add_metadata (donor, "donor", "true", -1);
	as_app_add_metadata (donor, "overwrite", "1111", -1);
	as_app_add_keyword (donor, "C", "klass", -1);
	as_app_add_keyword (donor, "pl", "klaski", -1);
	ss = as_screenshot_new ();
	as_app_add_screenshot (donor, ss);

	/* copy all useful properties */
	app = as_app_new ();
	as_app_add_metadata (app, "overwrite", "2222", -1);
	as_app_add_metadata (app, "recipient", "true", -1);
	as_app_subsume_full (app, donor, AS_APP_SUBSUME_FLAG_NO_OVERWRITE);
	as_app_add_screenshot (app, ss);

	g_assert_cmpstr (as_app_get_icon (app), ==, "gtk-find");
	g_assert_cmpstr (as_app_get_metadata_item (app, "donor"), ==, "true");
	g_assert_cmpstr (as_app_get_metadata_item (app, "overwrite"), ==, "2222");
	g_assert_cmpstr (as_app_get_metadata_item (donor, "recipient"), ==, NULL);
	g_assert_cmpint (as_app_get_pkgnames(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_state (app), ==, AS_APP_STATE_INSTALLED);
	g_assert_cmpint (as_app_get_keywords(app, "C")->len, ==, 1);
	g_assert_cmpint (as_app_get_keywords(app, "pl")->len, ==, 1);
	list = as_app_get_languages (app);
	g_assert_cmpint (g_list_length (list), ==, 1);
	g_list_free (list);

	/* test both ways */
	as_app_subsume_full (app, donor, AS_APP_SUBSUME_FLAG_BOTH_WAYS);
	g_assert_cmpstr (as_app_get_metadata_item (app, "donor"), ==, "true");
	g_assert_cmpstr (as_app_get_metadata_item (app, "recipient"), ==, "true");
	g_assert_cmpstr (as_app_get_metadata_item (donor, "donor"), ==, "true");
	g_assert_cmpstr (as_app_get_metadata_item (donor, "recipient"), ==, "true");
	g_assert_cmpint (as_app_get_screenshots(app)->len, ==, 1);
}

static void
as_test_app_search_func (void)
{
	const gchar *all[] = { "gnome", "install", "software", NULL };
	const gchar *none[] = { "gnome", "xxx", "software", NULL };
	const gchar *mime[] = { "application", "vnd", "oasis", "opendocument","text", NULL };
	_cleanup_object_unref_ AsApp *app = NULL;

	app = as_app_new ();
	as_app_set_name (app, NULL, "GNOME Software", -1);
	as_app_set_comment (app, NULL, "Install and remove software", -1);
	as_app_add_mimetype (app, "application/vnd.oasis.opendocument.text", -1);
	as_app_add_keyword (app, NULL, "awesome", -1);

	g_assert_cmpint (as_app_search_matches (app, "software"), ==, 80);
	g_assert_cmpint (as_app_search_matches (app, "soft"), ==, 80);
	g_assert_cmpint (as_app_search_matches (app, "install"), ==, 60);
	g_assert_cmpint (as_app_search_matches (app, "awesome"), ==, 90);
	g_assert_cmpint (as_app_search_matches_all (app, (gchar**) all), ==, 220);
	g_assert_cmpint (as_app_search_matches_all (app, (gchar**) none), ==, 0);
	g_assert_cmpint (as_app_search_matches_all (app, (gchar**) mime), ==, 5);
}

/* demote the .desktop "application" to an addon */
static void
as_test_store_demote_func (void)
{
	AsApp *app;
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename1 = NULL;
	_cleanup_free_ gchar *filename2 = NULL;
	_cleanup_object_unref_ AsApp *app_appdata = NULL;
	_cleanup_object_unref_ AsApp *app_desktop = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_string_free_ GString *xml = NULL;

	/* load example desktop file */
	app_desktop = as_app_new ();
	filename1 = as_test_get_filename ("example.desktop");
	ret = as_app_parse_file (app_desktop, filename1,
				 AS_APP_PARSE_FLAG_ALLOW_VETO, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_app_get_id_kind (app_desktop), ==, AS_ID_KIND_DESKTOP);

	/* load example appdata file */
	app_appdata = as_app_new ();
	filename2 = as_test_get_filename ("example.appdata.xml");
	ret = as_app_parse_file (app_appdata, filename2,
				 AS_APP_PARSE_FLAG_ALLOW_VETO, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_app_get_id_kind (app_appdata), ==, AS_ID_KIND_ADDON);

	/* add apps */
	store = as_store_new ();
	as_store_set_api_version (store, 0.8);
	as_store_add_app (store, app_desktop);
	as_store_add_app (store, app_appdata);

	/* check we demoted */
	g_assert_cmpint (as_store_get_size (store), ==, 1);
	app = as_store_get_app_by_id (store, "example.desktop");
	g_assert (app != NULL);
	g_assert_cmpint (as_app_get_id_kind (app), ==, AS_ID_KIND_ADDON);
	g_assert_cmpint (as_app_get_extends(app)->len, >, 0);

	/* dump */
	xml = as_store_to_xml (store,
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE |
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT);
	g_debug ("%s", xml->str);
}

static void
as_test_store_merges_func (void)
{
	AsApp *app_tmp;
	_cleanup_object_unref_ AsApp *app_appdata = NULL;
	_cleanup_object_unref_ AsApp *app_appinfo = NULL;
	_cleanup_object_unref_ AsApp *app_desktop = NULL;
	_cleanup_object_unref_ AsStore *store_all = NULL;
	_cleanup_object_unref_ AsStore *store_desktop_appdata = NULL;

	/* test desktop + appdata */
	store_desktop_appdata = as_store_new ();

	app_desktop = as_app_new ();
	as_app_set_id (app_desktop, "gimp.desktop", -1);
	as_app_set_source_kind (app_desktop, AS_APP_SOURCE_KIND_DESKTOP);
	as_app_set_name (app_desktop, NULL, "GIMP", -1);
	as_app_set_comment (app_desktop, NULL, "GNU Bla Bla", -1);
	as_app_set_priority (app_desktop, -1);
	as_app_set_state (app_desktop, AS_APP_STATE_INSTALLED);

	app_appdata = as_app_new ();
	as_app_set_id (app_appdata, "gimp.desktop", -1);
	as_app_set_source_kind (app_appdata, AS_APP_SOURCE_KIND_APPDATA);
	as_app_set_description (app_appdata, NULL, "<p>Gimp is awesome</p>", -1);
	as_app_add_pkgname (app_appdata, "gimp", -1);
	as_app_set_priority (app_appdata, -1);
	as_app_set_state (app_appdata, AS_APP_STATE_INSTALLED);

	as_store_add_app (store_desktop_appdata, app_desktop);
	as_store_add_app (store_desktop_appdata, app_appdata);

	app_tmp = as_store_get_app_by_id (store_desktop_appdata, "gimp.desktop");
	g_assert (app_tmp != NULL);
	g_assert_cmpstr (as_app_get_name (app_tmp, NULL), ==, "GIMP");
	g_assert_cmpstr (as_app_get_comment (app_tmp, NULL), ==, "GNU Bla Bla");
	g_assert_cmpstr (as_app_get_description (app_tmp, NULL), ==, "<p>Gimp is awesome</p>");
	g_assert_cmpstr (as_app_get_pkgname_default (app_tmp), ==, "gimp");
	g_assert_cmpint (as_app_get_source_kind (app_tmp), ==, AS_APP_SOURCE_KIND_APPDATA);
	g_assert_cmpint (as_app_get_state (app_tmp), ==, AS_APP_STATE_INSTALLED);

	/* test desktop + appdata + appstream */
	store_all = as_store_new ();

	app_appinfo = as_app_new ();
	as_app_set_id (app_appinfo, "gimp.desktop", -1);
	as_app_set_source_kind (app_appinfo, AS_APP_SOURCE_KIND_APPSTREAM);
	as_app_set_name (app_appinfo, NULL, "GIMP", -1);
	as_app_set_comment (app_appinfo, NULL, "GNU Bla Bla", -1);
	as_app_set_description (app_appinfo, NULL, "<p>Gimp is Distro</p>", -1);
	as_app_add_pkgname (app_appinfo, "gimp", -1);
	as_app_set_priority (app_appinfo, 0);

	as_store_add_app (store_all, app_appinfo);
	as_store_add_app (store_all, app_desktop);
	as_store_add_app (store_all, app_appdata);

	/* ensure the AppStream entry 'wins' */
	app_tmp = as_store_get_app_by_id (store_all, "gimp.desktop");
	g_assert (app_tmp != NULL);
	g_assert_cmpstr (as_app_get_name (app_tmp, NULL), ==, "GIMP");
	g_assert_cmpstr (as_app_get_comment (app_tmp, NULL), ==, "GNU Bla Bla");
	g_assert_cmpstr (as_app_get_description (app_tmp, NULL), ==, "<p>Gimp is Distro</p>");
	g_assert_cmpstr (as_app_get_pkgname_default (app_tmp), ==, "gimp");
	g_assert_cmpint (as_app_get_source_kind (app_tmp), ==, AS_APP_SOURCE_KIND_APPSTREAM);
	g_assert_cmpint (as_app_get_state (app_tmp), ==, AS_APP_STATE_INSTALLED);
}

static void
as_test_store_merges_local_func (void)
{
	AsApp *app_tmp;
	_cleanup_object_unref_ AsApp *app_appdata = NULL;
	_cleanup_object_unref_ AsApp *app_appinfo = NULL;
	_cleanup_object_unref_ AsApp *app_desktop = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;

	/* test desktop + appdata + appstream */
	store = as_store_new ();
	as_store_set_add_flags (store, AS_STORE_ADD_FLAG_PREFER_LOCAL);

	app_desktop = as_app_new ();
	as_app_set_id (app_desktop, "gimp.desktop", -1);
	as_app_set_source_kind (app_desktop, AS_APP_SOURCE_KIND_DESKTOP);
	as_app_set_name (app_desktop, NULL, "GIMP", -1);
	as_app_set_comment (app_desktop, NULL, "GNU Bla Bla", -1);
	as_app_set_priority (app_desktop, -1);
	as_app_set_state (app_desktop, AS_APP_STATE_INSTALLED);

	app_appdata = as_app_new ();
	as_app_set_id (app_appdata, "gimp.desktop", -1);
	as_app_set_source_kind (app_appdata, AS_APP_SOURCE_KIND_APPDATA);
	as_app_set_description (app_appdata, NULL, "<p>Gimp is awesome</p>", -1);
	as_app_add_pkgname (app_appdata, "gimp", -1);
	as_app_set_priority (app_appdata, -1);
	as_app_set_state (app_appdata, AS_APP_STATE_INSTALLED);

	app_appinfo = as_app_new ();
	as_app_set_id (app_appinfo, "gimp.desktop", -1);
	as_app_set_source_kind (app_appinfo, AS_APP_SOURCE_KIND_APPSTREAM);
	as_app_set_name (app_appinfo, NULL, "GIMP", -1);
	as_app_set_comment (app_appinfo, NULL, "Fedora GNU Bla Bla", -1);
	as_app_set_description (app_appinfo, NULL, "<p>Gimp is Distro</p>", -1);
	as_app_add_pkgname (app_appinfo, "gimp", -1);
	as_app_set_priority (app_appinfo, 0);

	/* this is actually the install order we get at startup */
	as_store_add_app (store, app_appinfo);
	as_store_add_app (store, app_desktop);
	as_store_add_app (store, app_appdata);

	/* ensure the local entry 'wins' */
	app_tmp = as_store_get_app_by_id (store, "gimp.desktop");
	g_assert (app_tmp != NULL);
	g_assert_cmpstr (as_app_get_name (app_tmp, NULL), ==, "GIMP");
	g_assert_cmpstr (as_app_get_comment (app_tmp, NULL), ==, "GNU Bla Bla");
	g_assert_cmpstr (as_app_get_description (app_tmp, NULL), ==, "<p>Gimp is awesome</p>");
	g_assert_cmpstr (as_app_get_pkgname_default (app_tmp), ==, "gimp");
	g_assert_cmpint (as_app_get_source_kind (app_tmp), ==, AS_APP_SOURCE_KIND_APPDATA);
	g_assert_cmpint (as_app_get_state (app_tmp), ==, AS_APP_STATE_INSTALLED);
}

static void
as_test_store_func (void)
{
	AsApp *app;
	GString *xml;
	_cleanup_object_unref_ AsStore *store = NULL;

	/* create a store and add a single app */
	store = as_store_new ();
	g_assert_cmpfloat (as_store_get_api_version (store), <, 1.f);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.f);
	app = as_app_new ();
	as_app_set_id (app, "gnome-software.desktop", -1);
	as_app_set_id_kind (app, AS_ID_KIND_DESKTOP);
	as_store_add_app (store, app);
	g_object_unref (app);
	g_assert_cmpstr (as_store_get_origin (store), ==, NULL);

	/* add and then remove another app */
	app = as_app_new ();
	as_app_set_id (app, "junk.desktop", -1);
	as_app_set_id_kind (app, AS_ID_KIND_FONT);
	as_store_add_app (store, app);
	g_object_unref (app);
	as_store_remove_app (store, app);

	/* check string output */
	as_store_set_api_version (store, 0.4);
	xml = as_store_to_xml (store, 0);
	g_assert_cmpstr (xml->str, ==,
		"<applications version=\"0.4\">"
		"<application>"
		"<id type=\"desktop\">gnome-software.desktop</id>"
		"</application>"
		"</applications>");
	g_string_free (xml, TRUE);

	/* add another app and ensure it's sorted */
	app = as_app_new ();
	as_app_set_id (app, "aaa.desktop", -1);
	as_app_set_id_kind (app, AS_ID_KIND_FONT);
	as_store_add_app (store, app);
	g_object_unref (app);
	xml = as_store_to_xml (store, 0);
	g_assert_cmpstr (xml->str, ==,
		"<applications version=\"0.4\">"
		"<application>"
		"<id type=\"font\">aaa.desktop</id>"
		"</application>"
		"<application>"
		"<id type=\"desktop\">gnome-software.desktop</id>"
		"</application>"
		"</applications>");
	g_string_free (xml, TRUE);

	/* empty the store */
	as_store_remove_all (store);
	g_assert_cmpint (as_store_get_size (store), ==, 0);
	g_assert (as_store_get_app_by_id (store, "aaa.desktop") == NULL);
	g_assert (as_store_get_app_by_id (store, "gnome-software.desktop") == NULL);
	xml = as_store_to_xml (store, 0);
	g_assert_cmpstr (xml->str, ==,
		"<applications version=\"0.4\"/>");
	g_string_free (xml, TRUE);
}

static void
as_test_store_versions_func (void)
{
	AsApp *app;
	AsStore *store;
	GError *error = NULL;
	gboolean ret;
	GString *str;

	/* load a file to the store */
	store = as_store_new ();
	ret = as_store_from_xml (store,
		"<applications version=\"0.4\">"
		"<application>"
		"<id type=\"desktop\">test.desktop</id>"
		"<description><p>Hello world</p></description>"
		"<architectures><arch>i386</arch></architectures>"
		"<releases>"
		"<release version=\"0.1.2\" timestamp=\"123\">"
		"<description><p>Hello</p></description>"
		"</release>"
		"</releases>"
		"</application>"
		"</applications>", -1, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpfloat (as_store_get_api_version (store), <, 0.4 + 0.01);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.4 - 0.01);

	/* verify source kind */
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert_cmpint (as_app_get_source_kind (app), ==, AS_APP_SOURCE_KIND_APPSTREAM);

	/* test with latest features */
	as_store_set_api_version (store, 0.6);
	g_assert_cmpfloat (as_store_get_api_version (store), <, 0.6 + 0.01);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.6 - 0.01);
	str = as_store_to_xml (store, 0);
	g_assert_cmpstr (str->str, ==,
		"<components version=\"0.6\">"
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"<description><p>Hello world</p></description>"
		"<architectures><arch>i386</arch></architectures>"
		"<releases>"
		"<release version=\"0.1.2\" timestamp=\"123\">"
		"<description><p>Hello</p></description>"
		"</release>"
		"</releases>"
		"</component>"
		"</components>");
	g_string_free (str, TRUE);

	/* test with legacy options */
	as_store_set_api_version (store, 0.3);
	str = as_store_to_xml (store, 0);
	g_assert_cmpstr (str->str, ==,
		"<applications version=\"0.3\">"
		"<application>"
		"<id type=\"desktop\">test.desktop</id>"
		"<description>Hello world</description>"
		"</application>"
		"</applications>");
	g_string_free (str, TRUE);

	g_object_unref (store);

	/* load a version 0.6 file to the store */
	store = as_store_new ();
	ret = as_store_from_xml (store,
		"<components version=\"0.6\">"
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"</component>"
		"</components>", -1, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test latest spec version */
	str = as_store_to_xml (store, 0);
	g_assert_cmpstr (str->str, ==,
		"<components version=\"0.6\">"
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"</component>"
		"</components>");
	g_string_free (str, TRUE);

	g_object_unref (store);
}

static void
as_test_store_addons_func (void)
{
	AsApp *app;
	GError *error = NULL;
	GPtrArray *data;
	gboolean ret;
	const gchar *xml =
		"<components version=\"0.7\">"
		"<component type=\"addon\">"
		"<id>eclipse-php.jar</id>"
		"<mimetypes>"
		"<mimetype>xtest</mimetype>"
		"</mimetypes>"
		"<extends>eclipse.desktop</extends>"
		"</component>"
		"<component type=\"desktop\">"
		"<id>eclipse.desktop</id>"
		"</component>"
		"</components>";
	_cleanup_object_unref_ AsStore *store;
	_cleanup_string_free_ GString *str;

	/* load a file to the store */
	store = as_store_new ();
	ret = as_store_from_xml (store, xml, -1, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check the addon references the main application */
	app = as_store_get_app_by_id (store, "eclipse-php.jar");
	g_assert (app != NULL);
	data = as_app_get_extends (app);
	g_assert_cmpint (data->len, ==, 1);
	g_assert_cmpstr (g_ptr_array_index (data, 0), ==, "eclipse.desktop");

	/* check the main application has a ref to the addon */
	app = as_store_get_app_by_id (store, "eclipse.desktop");
	data = as_app_get_addons (app);
	g_assert_cmpint (data->len, ==, 1);
	app = g_ptr_array_index (data, 0);
	g_assert_cmpstr (as_app_get_id (app), ==, "eclipse-php.jar");

	/* check we can search for token from the addon */
	g_assert_cmpint (as_app_search_matches (app, "xtest"), >, 0);

	/* check it marshals back to the same XML */
	str = as_store_to_xml (store, 0);
	if (g_strcmp0 (str->str, xml) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", xml, str->str);
	g_assert_cmpstr (str->str, ==, xml);
}

/*
 * test that we don't save the same translated data as C back to the file
 */
static void
as_test_node_no_dup_c_func (void)
{
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<application>"
		"<id type=\"desktop\">test.desktop</id>"
		"<name>Krita</name>"
		"<name xml:lang=\"pl\">Krita</name>"
		"</application>";
	_cleanup_object_unref_ AsApp *app = NULL;

	/* to object */
	app = as_app_new ();
	root = as_node_from_xml (src, -1, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "application");
	g_assert (n != NULL);
	ret = as_app_node_parse (app, n, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Krita");
	g_assert_cmpstr (as_app_get_name (app, "pl"), ==, "Krita");
	as_node_unref (root);

	/* back to node */
	root = as_node_new ();
	n = as_app_node_insert (app, root, 0.4);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==,
		"<application>"
		"<id type=\"desktop\">test.desktop</id>"
		"<name>Krita</name>"
		"</application>");
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_store_origin_func (void)
{
	AsApp *app;
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file = NULL;

	/* load a file to the store */
	store = as_store_new ();
	filename = as_test_get_filename ("origin.xml");
	file = g_file_new_for_path (filename);
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test icon path */
	g_assert_cmpstr (as_store_get_origin (store), ==, "fedora-21");
	g_assert_cmpint (as_store_get_size (store), ==, 1);
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (app != NULL);
	g_assert_cmpstr (as_app_get_icon_path (app), ==,
		"/usr/share/app-info/icons/fedora-21");
}

static void
as_test_store_speed_appstream_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint loops = 10;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_timer_destroy_ GTimer *timer = NULL;

	filename = as_test_get_filename ("example-v04.xml.gz");
	file = g_file_new_for_path (filename);
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		_cleanup_object_unref_ AsStore *store;
		store = as_store_new ();
		ret = as_store_from_file (store, file, NULL, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);
		g_assert_cmpint (as_store_get_apps (store)->len, >=, 1415);
		g_assert (as_store_get_app_by_id (store, "org.gnome.Software.desktop") != NULL);
		g_assert (as_store_get_app_by_pkgname (store, "gnome-software") != NULL);
	}
	g_print ("%.0f ms: ", g_timer_elapsed (timer, NULL) * 1000 / loops);
}

static void
as_test_store_speed_appdata_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint loops = 10;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_timer_destroy_ GTimer *timer = NULL;

	filename = as_test_get_filename (".");
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		_cleanup_object_unref_ AsStore *store;
		store = as_store_new ();
		as_store_set_destdir (store, filename);
		g_test_expect_message (G_LOG_DOMAIN,
				       G_LOG_LEVEL_WARNING,
				       "ignoring description '*' from */broken.appdata.xml: Unknown tag '_p'");
		ret = as_store_load (store, AS_STORE_LOAD_FLAG_APPDATA, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);
		g_assert_cmpint (as_store_get_apps (store)->len, >, 0);
	}
	g_print ("%.0f ms: ", g_timer_elapsed (timer, NULL) * 1000 / loops);
}

static void
as_test_store_speed_desktop_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint loops = 10;
	_cleanup_timer_destroy_ GTimer *timer = NULL;

	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		_cleanup_object_unref_ AsStore *store;
		store = as_store_new ();
		ret = as_store_load (store, AS_STORE_LOAD_FLAG_DESKTOP, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);
		g_assert_cmpint (as_store_get_apps (store)->len, >, 0);
	}
	g_print ("%.0f ms: ", g_timer_elapsed (timer, NULL) * 1000 / loops);
}

static void
as_test_utils_icons_func (void)
{
	gchar *tmp;
	GError *error = NULL;
	_cleanup_free_ gchar *destdir = NULL;

	destdir = as_test_get_filename (".");

	/* full path */
	tmp = as_utils_find_icon_filename (destdir, "/usr/share/pixmaps/test.png", &error);
	g_assert_cmpstr (tmp, !=, NULL);
	g_assert_no_error (error);
	g_free (tmp);

	/* full pixmaps name */
	tmp = as_utils_find_icon_filename (destdir, "test.png", &error);
	g_assert_cmpstr (tmp, !=, NULL);
	g_assert_no_error (error);
	g_free (tmp);

	/* pixmaps name */
	tmp = as_utils_find_icon_filename (destdir, "test", &error);
	g_assert_cmpstr (tmp, !=, NULL);
	g_assert_no_error (error);
	g_free (tmp);

	/* full theme name */
	tmp = as_utils_find_icon_filename (destdir, "test2.png", &error);
	g_assert_cmpstr (tmp, !=, NULL);
	g_assert_no_error (error);
	g_free (tmp);

	/* theme name */
	tmp = as_utils_find_icon_filename (destdir, "test2", &error);
	g_assert_cmpstr (tmp, !=, NULL);
	g_assert_no_error (error);
	g_free (tmp);

	/* full pixmaps invalid */
	tmp = as_utils_find_icon_filename (destdir, "/usr/share/pixmaps/not-going-to-exist.png", &error);
	g_assert_cmpstr (tmp, ==, NULL);
	g_assert_error (error, AS_APP_ERROR, AS_APP_ERROR_FAILED);
	g_free (tmp);
	g_clear_error (&error);

	/* all invalid */
	tmp = as_utils_find_icon_filename (destdir, "not-going-to-exist.png", &error);
	g_assert_cmpstr (tmp, ==, NULL);
	g_assert_error (error, AS_APP_ERROR, AS_APP_ERROR_FAILED);
	g_free (tmp);
	g_clear_error (&error);
}

static void
as_test_utils_spdx_token_func (void)
{
	gchar **tok;
	gchar *tmp;

	/* simple */
	tok = as_utils_spdx_license_tokenize ("GPL");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "GPL");
	g_strfreev (tok);
	g_free (tmp);

	/* empty */
	tok = as_utils_spdx_license_tokenize ("");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences */
	tok = as_utils_spdx_license_tokenize ("GPL and MPL and CDL");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "GPL|# and |MPL|# and |CDL");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences */
	tok = as_utils_spdx_license_tokenize ("GPL and MPL or BSD and MPL");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "GPL|# and |MPL|# or |BSD|# and |MPL");
	g_strfreev (tok);
	g_free (tmp);

	/* brackets */
	tok = as_utils_spdx_license_tokenize ("LGPLv2+ and (QPL or GPLv2) and MIT");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "LGPLv2+|# and (|QPL|# or |GPLv2|#) and |MIT");
	g_strfreev (tok);
	g_free (tmp);

	/* detokenisation */
	tok = as_utils_spdx_license_tokenize ("LGPLv2+ and (QPL or GPLv2) and MIT");
	tmp = as_utils_spdx_license_detokenize (tok);
	g_assert_cmpstr (tmp, ==, "LGPLv2+ and (QPL or GPLv2) and MIT");
	g_strfreev (tok);
	g_free (tmp);

	/* leading brackets */
	tok = as_utils_spdx_license_tokenize ("(MPLv1.1 or LGPLv3+) and LGPLv3");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "#(|MPLv1.1|# or |LGPLv3+|#) and |LGPLv3");
	g_strfreev (tok);
	g_free (tmp);

	/*  trailing brackets */
	tok = as_utils_spdx_license_tokenize ("MPLv1.1 and (LGPLv3 or GPLv3)");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "MPLv1.1|# and (|LGPLv3|# or |GPLv3|#)");
	g_strfreev (tok);
	g_free (tmp);

	/*  deprecated names */
	tok = as_utils_spdx_license_tokenize ("CC0 and (CC0 or CC0)");
	tmp = g_strjoinv ("|", tok);
	g_assert_cmpstr (tmp, ==, "CC0-1.0|# and (|CC0-1.0|# or |CC0-1.0|#)");
	g_strfreev (tok);
	g_free (tmp);

	/* SPDX strings */
	g_assert (as_utils_is_spdx_license ("CC0"));
	g_assert (as_utils_is_spdx_license ("CC0 and GFDL-1.3"));
	g_assert (!as_utils_is_spdx_license ("CC0 dave"));
}

static void
as_test_utils_func (void)
{
	gboolean ret;
	gchar *tmp;
	GError *error = NULL;

	/* as_strndup */
	tmp = as_strndup ("dave", 2);
	g_assert_cmpstr (tmp, ==, "da");
	g_free (tmp);
	tmp = as_strndup ("dave", 4);
	g_assert_cmpstr (tmp, ==, "dave");
	g_free (tmp);
	tmp = as_strndup ("dave", -1);
	g_assert_cmpstr (tmp, ==, "dave");
	g_free (tmp);

	/* as_utils_is_stock_icon_name */
	g_assert (!as_utils_is_stock_icon_name (NULL));
	g_assert (!as_utils_is_stock_icon_name (""));
	g_assert (!as_utils_is_stock_icon_name ("indigo-blue"));
	g_assert (as_utils_is_stock_icon_name ("accessories-calculator"));
	g_assert (as_utils_is_stock_icon_name ("insert-image"));
	g_assert (as_utils_is_stock_icon_name ("zoom-out"));

	/* environments */
	g_assert (as_utils_is_environment_id ("GNOME"));
	g_assert (!as_utils_is_environment_id ("RandomDE"));

	/* categories */
	g_assert (as_utils_is_category_id ("AudioVideoEditing"));
	g_assert (!as_utils_is_category_id ("SpellEditing"));

	/* blacklist */
	g_assert (as_utils_is_blacklisted_id ("gnome-system-monitor-kde.desktop"));
	g_assert (as_utils_is_blacklisted_id ("doom-*-demo.desktop"));
	g_assert (!as_utils_is_blacklisted_id ("gimp.desktop"));

	/* valid description markup */
	tmp = as_markup_convert_simple ("<p>Hello world!</p>", -1, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "Hello world!");
	g_free (tmp);
	tmp = as_markup_convert_simple ("<p>Hello world</p>"
					"<ul><li>Item</li></ul>",
					-1, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "Hello world\n • Item");
	g_free (tmp);

	/* valid description markup */
	tmp = as_markup_convert_simple ("bare text", -1, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "bare text");
	g_free (tmp);

	/* invalid XML */
	tmp = as_markup_convert_simple ("<p>Hello world</dave>",
					-1, &error);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_assert_cmpstr (tmp, ==, NULL);
	g_clear_error (&error);

	/* invalid URLs */
	ret = as_utils_check_url_exists ("hello dave", 1, &error);
	g_assert (!ret);
	g_assert (error != NULL);
	g_clear_error (&error);
	ret = as_utils_check_url_exists ("http://www.bbc.co.uk/notgoingtoexist", 1, &error);
	g_assert (!ret);
	g_assert (error != NULL);
	g_clear_error (&error);

	/* valid URLs */
	//ret = as_utils_check_url_exists ("http://www.bbc.co.uk/", &error);
	//g_assert (ret);
	//g_assert_no_error (error);
}

static void
as_test_store_app_install_func (void)
{
	GError *error = NULL;
	gboolean ret;
	_cleanup_object_unref_ AsStore *store = NULL;

	store = as_store_new ();
	ret = as_store_load (store,
			     AS_STORE_LOAD_FLAG_APP_INSTALL,
			     NULL,
			     &error);
	g_assert_no_error (error);
	g_assert (ret);
}

static void
as_test_store_metadata_func (void)
{
	GError *error = NULL;
	GPtrArray *apps;
	gboolean ret;
	const gchar *xml =
		"<applications version=\"0.3\">"
		"<application>"
		"<id type=\"desktop\">test.desktop</id>"
		"<metadata>"
		"<value key=\"foo\">bar</value>"
		"</metadata>"
		"</application>"
		"<application>"
		"<id type=\"desktop\">tested.desktop</id>"
		"<metadata>"
		"<value key=\"foo\">bar</value>"
		"</metadata>"
		"</application>"
		"</applications>";
	_cleanup_object_unref_ AsStore *store = NULL;

	store = as_store_new ();
	ret = as_store_from_xml (store, xml, -1, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	apps = as_store_get_apps_by_metadata (store, "foo", "bar");
	g_assert_cmpint (apps->len, ==, 2);
	g_ptr_array_unref (apps);
}

static void
as_test_store_metadata_index_func (void)
{
	GPtrArray *apps;
	const guint repeats = 10000;
	guint i;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_timer_destroy_ GTimer *timer = NULL;

	/* create lots of applications in the store */
	store = as_store_new ();
	as_store_add_metadata_index (store, "X-CacheID");
	for (i = 0; i < repeats; i++) {
		_cleanup_free_ gchar *id = g_strdup_printf ("app-%05i", i);
		_cleanup_object_unref_ AsApp *app = as_app_new ();
		as_app_set_id (app, id, -1);
		as_app_add_metadata (app, "X-CacheID", "dave.i386", -1);
		as_app_add_metadata (app, "baz", "dave", -1);
		as_store_add_app (store, app);
	}

	/* find out how long this takes with an index */
	timer = g_timer_new ();
	for (i = 0; i < repeats; i++) {
		apps = as_store_get_apps_by_metadata (store, "X-CacheID", "dave.i386");
		g_assert_cmpint (apps->len, ==, repeats);
		g_ptr_array_unref (apps);
		apps = as_store_get_apps_by_metadata (store, "X-CacheID", "notgoingtoexist");
		g_assert_cmpint (apps->len, ==, 0);
		g_ptr_array_unref (apps);
	}
	g_assert_cmpfloat (g_timer_elapsed (timer, NULL), <, 0.5);
	g_print ("%.0fms: ", g_timer_elapsed (timer, NULL) * 1000);
}

static void
as_test_yaml_func (void)
{
	GNode *node;
	GError *error = NULL;
	GString *str;
	const gchar *expected;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ GFile *file = NULL;

	/* simple header */
	node = as_yaml_from_data (
		"File: DEP-11\n"
		"Origin: aequorea\n"
		"Version: '0.6'\n",
		-1, &error);
	g_assert_no_error (error);
	g_assert (node != NULL);
	str = as_yaml_to_string (node);
	expected =
		"[MAP]{\n"
		" [KVL]File=DEP-11\n"
		" [KVL]Origin=aequorea\n"
		" [KVL]Version=0.6\n";
	if (g_strcmp0 (str->str, expected) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", expected, str->str);
	g_assert_cmpstr (str->str, ==, expected);
	g_string_free (str, TRUE);
	as_yaml_unref (node);

	/* simple list */
	node = as_yaml_from_data (
		"Mimetypes:\n"
		"  - text/html\n"
		"  - text/xml\n"
		"  - application/xhtml+xml\n"
		"Kudos:\n"
		"  - AppMenu\n"
		"  - SearchProvider\n"
		"  - Notifications\n",
		-1, &error);
	g_assert_no_error (error);
	g_assert (node != NULL);
	str = as_yaml_to_string (node);
	expected =
		"[MAP]{\n"
		" [SEQ]Mimetypes\n"
		"  [KEY]text/html\n"
		"  [KEY]text/xml\n"
		"  [KEY]application/xhtml+xml\n"
		" [SEQ]Kudos\n"
		"  [KEY]AppMenu\n"
		"  [KEY]SearchProvider\n"
		"  [KEY]Notifications\n";
	if (g_strcmp0 (str->str, expected) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", expected, str->str);
	g_assert_cmpstr (str->str, ==, expected);
	g_string_free (str, TRUE);
	as_yaml_unref (node);

	/* dummy application */
	filename = as_test_get_filename ("example.yml");
	g_assert (filename != NULL);
	file = g_file_new_for_path (filename);
	node = as_yaml_from_file (file, NULL, &error);
	g_assert_no_error (error);
	g_assert (node != NULL);
	str = as_yaml_to_string (node);
	expected =
		"[MAP]{\n"
		" [KVL]File=DEP-11\n"
		" [KVL]Origin=aequorea\n"
		" [KVL]Version=0.6\n"
		"[MAP]{\n"
		" [KVL]Type=desktop-app\n"
		" [KVL]ID=iceweasel.desktop\n"
		" [MAP]Name\n"
		"  [KVL]C=Iceweasel\n"
		" [SEQ]Packages\n"
		"  [KEY]iceweasel\n"
		" [MAP]Icon\n"
		"  [KVL]cached=iceweasel.png\n"
		" [MAP]Keywords\n"
		"  [SEQ]C\n"
		"   [KEY]browser\n"
		" [SEQ]Screenshots\n"
		"  [MAP]{\n"
		"   [KVL]default=true\n"
		"   [MAP]source-image\n"
		"    [KVL]height=770\n"
		"    [KVL]url=http://localhost/source/screenshot.png\n"
		"    [KVL]width=1026\n"
		"   [SEQ]thumbnails\n"
		"    [MAP]{\n"
		"     [KVL]height=423\n"
		"     [KVL]url=http://localhost/752x423/screenshot.png\n"
		"     [KVL]width=752\n"
		"[MAP]{\n"
		" [KVL]Type=desktop-app\n"
		" [KVL]ID=dave.desktop\n"
		" [MAP]Name\n"
		"  [KVL]C=dave\n";
	if (g_strcmp0 (str->str, expected) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", expected, str->str);
	g_assert_cmpstr (str->str, ==, expected);
	g_string_free (str, TRUE);
	as_yaml_unref (node);

}

static void
as_test_store_yaml_func (void)
{
	AsApp *app;
	GError *error = NULL;
	gboolean ret;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_string_free_ GString *str = NULL;
	const gchar *xml =
		"<components version=\"0.6\" origin=\"aequorea\">\n"
		"<component type=\"desktop\">\n"
		"<id>dave.desktop</id>\n"
		"<name>dave</name>\n"
		"</component>\n"
		"<component type=\"desktop\">\n"
		"<id>iceweasel.desktop</id>\n"
		"<pkgname>iceweasel</pkgname>\n"
		"<name>Iceweasel</name>\n"
		"<icon type=\"cached\">iceweasel.png</icon>\n"
		"<keywords>\n"
		"<keyword>browser</keyword>\n"
		"</keywords>\n"
		"<screenshots>\n"
		"<screenshot type=\"default\">\n"
		"<image type=\"source\" height=\"770\" width=\"1026\">http://localhost/source/screenshot.png</image>\n"
		"<image type=\"thumbnail\" height=\"423\" width=\"752\">http://localhost/752x423/screenshot.png</image>\n"
		"</screenshot>\n"
		"</screenshots>\n"
		"</component>\n"
		"</components>\n";

	/* load store */
	store = as_store_new ();
	filename = as_test_get_filename ("example.yml");
	file = g_file_new_for_path (filename);
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test it matches expected XML */
	str = as_store_to_xml (store, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	if (g_strcmp0 (str->str, xml) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", xml, str->str);
	g_assert_cmpstr (str->str, ==, xml);

	/* test store properties */
	g_assert_cmpstr (as_store_get_origin (store), ==, "aequorea");
	g_assert_cmpfloat (as_store_get_api_version (store), <, 0.6 + 0.01);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.6 - 0.01);
	g_assert_cmpint (as_store_get_size (store), ==, 2);
	g_assert (as_store_get_app_by_id (store, "iceweasel.desktop") != NULL);
	g_assert (as_store_get_app_by_id (store, "dave.desktop") != NULL);

	/* test application properties */
	app = as_store_get_app_by_id (store, "iceweasel.desktop");
	g_assert_cmpint (as_app_get_id_kind (app), ==, AS_ID_KIND_DESKTOP);
	g_assert_cmpstr (as_app_get_pkgname_default (app), ==, "iceweasel");
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Iceweasel");
}

static void
as_test_store_speed_yaml_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint loops = 10;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_timer_destroy_ GTimer *timer = NULL;

	filename = as_test_get_filename ("example-v06.yml.gz");
	g_assert (filename != NULL);
	file = g_file_new_for_path (filename);
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		_cleanup_object_unref_ AsStore *store;
		store = as_store_new ();
		ret = as_store_from_file (store, file, NULL, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);

		/* test store properties */
		g_assert_cmpstr (as_store_get_origin (store), ==, "bartholomea");
		g_assert_cmpfloat (as_store_get_api_version (store), <, 0.6 + 0.01);
		g_assert_cmpfloat (as_store_get_api_version (store), >, 0.6 - 0.01);
		g_assert_cmpint (as_store_get_size (store), ==, 85);
		g_assert (as_store_get_app_by_id (store, "blobwars.desktop") != NULL);
	}
	g_print ("%.0f ms: ", g_timer_elapsed (timer, NULL) * 1000 / loops);

}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/AppStream/tag", as_test_tag_func);
	g_test_add_func ("/AppStream/provide", as_test_provide_func);
	g_test_add_func ("/AppStream/release", as_test_release_func);
	g_test_add_func ("/AppStream/release{description}", as_test_release_desc_func);
	g_test_add_func ("/AppStream/image", as_test_image_func);
	g_test_add_func ("/AppStream/image{resize}", as_test_image_resize_func);
	g_test_add_func ("/AppStream/image{alpha}", as_test_image_alpha_func);
	g_test_add_func ("/AppStream/screenshot", as_test_screenshot_func);
	g_test_add_func ("/AppStream/app", as_test_app_func);
	g_test_add_func ("/AppStream/app{translated}", as_test_app_translated_func);
	g_test_add_func ("/AppStream/app{validate-style}", as_test_app_validate_style_func);
	g_test_add_func ("/AppStream/app{validate-appdata-good}", as_test_app_validate_appdata_good_func);
	g_test_add_func ("/AppStream/app{validate-metainfo-good}", as_test_app_validate_metainfo_good_func);
	g_test_add_func ("/AppStream/app{validate-file-bad}", as_test_app_validate_file_bad_func);
	g_test_add_func ("/AppStream/app{validate-meta-bad}", as_test_app_validate_meta_bad_func);
	g_test_add_func ("/AppStream/app{validate-intltool}", as_test_app_validate_intltool_func);
	g_test_add_func ("/AppStream/app{parse-file}", as_test_app_parse_file_func);
	g_test_add_func ("/AppStream/app{no-markup}", as_test_app_no_markup_func);
	g_test_add_func ("/AppStream/app{subsume}", as_test_app_subsume_func);
	g_test_add_func ("/AppStream/app{search}", as_test_app_search_func);
	g_test_add_func ("/AppStream/node", as_test_node_func);
	g_test_add_func ("/AppStream/node{reflow}", as_test_node_reflow_text_func);
	g_test_add_func ("/AppStream/node{xml}", as_test_node_xml_func);
	g_test_add_func ("/AppStream/node{hash}", as_test_node_hash_func);
	g_test_add_func ("/AppStream/node{no-dup-c}", as_test_node_no_dup_c_func);
	g_test_add_func ("/AppStream/node{localized}", as_test_node_localized_func);
	g_test_add_func ("/AppStream/node{localized-wrap}", as_test_node_localized_wrap_func);
	g_test_add_func ("/AppStream/node{localized-wrap2}", as_test_node_localized_wrap2_func);
	g_test_add_func ("/AppStream/node{intltool}", as_test_node_intltool_func);
	g_test_add_func ("/AppStream/node{sort}", as_test_node_sort_func);
	g_test_add_func ("/AppStream/utils", as_test_utils_func);
	g_test_add_func ("/AppStream/utils{icons}", as_test_utils_icons_func);
	g_test_add_func ("/AppStream/utils{spdx-token}", as_test_utils_spdx_token_func);
	g_test_add_func ("/AppStream/yaml", as_test_yaml_func);
	g_test_add_func ("/AppStream/store", as_test_store_func);
	g_test_add_func ("/AppStream/store{demote}", as_test_store_demote_func);
	g_test_add_func ("/AppStream/store{merges}", as_test_store_merges_func);
	g_test_add_func ("/AppStream/store{merges-local}", as_test_store_merges_local_func);
	g_test_add_func ("/AppStream/store{addons}", as_test_store_addons_func);
	g_test_add_func ("/AppStream/store{versions}", as_test_store_versions_func);
	g_test_add_func ("/AppStream/store{origin}", as_test_store_origin_func);
	g_test_add_func ("/AppStream/store{app-install}", as_test_store_app_install_func);
	g_test_add_func ("/AppStream/store{yaml}", as_test_store_yaml_func);
	g_test_add_func ("/AppStream/store{metadata}", as_test_store_metadata_func);
	g_test_add_func ("/AppStream/store{metadata-index}", as_test_store_metadata_index_func);
	g_test_add_func ("/AppStream/store{validate}", as_test_store_validate_func);
	g_test_add_func ("/AppStream/store{local-app-install}", as_test_store_local_app_install_func);
	g_test_add_func ("/AppStream/store{local-appdata}", as_test_store_local_appdata_func);
	g_test_add_func ("/AppStream/store{speed-appstream}", as_test_store_speed_appstream_func);
	g_test_add_func ("/AppStream/store{speed-appdata}", as_test_store_speed_appdata_func);
	g_test_add_func ("/AppStream/store{speed-desktop}", as_test_store_speed_desktop_func);
	g_test_add_func ("/AppStream/store{speed-yaml}", as_test_store_speed_yaml_func);

	return g_test_run ();
}

