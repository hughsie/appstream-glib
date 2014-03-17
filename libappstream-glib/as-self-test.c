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

#include "as-app.h"
#include "as-image.h"
#include "as-node.h"
#include "as-release.h"
#include "as-screenshot.h"
#include "as-store.h"

static void
ch_test_release_func (void)
{
	AsRelease *release;
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	const gchar *src = "<release version=\"0.1.2\" timestamp=\"123\"/>";
	gboolean ret;

	release = as_release_new ();

	/* to object */
	root = as_node_from_xml (src, -1, &error);
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
	n = as_release_node_insert (release, root);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	g_object_unref (release);
}

static void
ch_test_image_func (void)
{
	AsImage *image;
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	const gchar *src =
		"<image type=\"thumbnail\" height=\"12\" width=\"34\">"
		"http://www.hughsie.com/a.jpg</image>";
	gboolean ret;

	image = as_image_new ();

	/* to object */
	root = as_node_from_xml (src, -1, &error);
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
	n = as_image_node_insert (image, root);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	g_object_unref (image);
}

static void
ch_test_screenshot_func (void)
{
	GPtrArray *images;
	AsScreenshot *screenshot;
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	const gchar *src =
		"<screenshot type=\"normal\">"
		"<caption>Hello</caption>"
		"<image type=\"source\">http://1.png</image>"
		"<image type=\"thumbnail\">http://2.png</image>"
		"</screenshot>";
	gboolean ret;

	screenshot = as_screenshot_new ();

	/* to object */
	root = as_node_from_xml (src, -1, &error);
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
	as_node_unref (root);

	/* back to node */
	root = as_node_new ();
	n = as_screenshot_node_insert (screenshot, root);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	g_object_unref (screenshot);
}

static void
ch_test_app_func (void)
{
	AsApp *app;
	GError *error = NULL;
	GNode *n;
	GNode *root;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<application>"
		"<id type=\"desktop\">org.gnome.Software.desktop</id>"
		"<priority>-4</priority>"
		"<pkgname>gnome-software</pkgname>"
		"<name>Software</name>"
		"<name xml:lang=\"pl\">Oprogramowanie</name>"
		"<summary>Application manager</summary>"
		"<description><p>Software allows you to find stuff</p></description>"
		"<description xml:lang=\"pt_BR\"><p>O aplicativo Software.</p></description>"
		"<icon type=\"cached\">org.gnome.Software.png</icon>"
		"<appcategories>"
		"<appcategory>System</appcategory>"
		"</appcategories>"
		"<project_license>GPLv2+</project_license>"
		"<url type=\"homepage\">https://wiki.gnome.org/Design/Apps/Software</url>"
		"<project_group>GNOME</project_group>"
		"<compulsory_for_desktop>GNOME</compulsory_for_desktop>"
		"<screenshots>"
		"<screenshot type=\"default\">"
		"<image type=\"thumbnail\" height=\"351\" width=\"624\">http://a.png</image>"
		"</screenshot>"
		"<screenshot type=\"normal\">"
		"<image type=\"thumbnail\">http://b.png</image>"
		"</screenshot>"
		"</screenshots>"
		"<releases>"
		"<release version=\"3.11.90\" timestamp=\"1392724800\"/>"
		"</releases>"
		"<languages>"
		"<lang percentage=\"90\">en_GB</lang>"
		"<lang>pl</lang>"
		"</languages>"
		"<metadata>"
		"<value key=\"X-Kudo-GTK3\"/>"
		"</metadata>"
		"</application>";

	app = as_app_new ();

	/* to object */
	root = as_node_from_xml (src, -1, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "application");
	g_assert (n != NULL);
	ret = as_app_node_parse (app, n, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_id_full (app), ==, "org.gnome.Software.desktop");
	g_assert_cmpstr (as_app_get_id (app), ==, "org.gnome.Software");
	g_assert_cmpstr (as_app_get_name (app, "pl"), ==, "Oprogramowanie");
	g_assert_cmpstr (as_app_get_comment (app, NULL), ==, "Application manager");
	g_assert_cmpstr (as_app_get_icon (app), ==, "org.gnome.Software.png");
	g_assert_cmpint (as_app_get_icon_kind (app), ==, AS_APP_ICON_KIND_CACHED);
	g_assert_cmpstr (as_app_get_project_group (app), ==, "GNOME");
	g_assert_cmpstr (as_app_get_project_license (app), ==, "GPLv2+");
	g_assert_cmpint (as_app_get_categories(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_screenshots(app)->len, ==, 2);
	g_assert_cmpint (as_app_get_releases(app)->len, ==, 1);
	g_assert_cmpstr (as_app_get_metadata_item (app, "X-Kudo-GTK3"), ==, "");
	as_node_unref (root);

	/* back to node */
	root = as_node_new ();
	n = as_app_node_insert (app, root);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==, src);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	g_object_unref (app);
}

static void
ch_test_node_func (void)
{
	GNode *n1;
	GNode *n2;
	GNode *root;

	/* create a simple tree */
	root = as_node_new ();
	n1 = as_node_insert (root, "apps", NULL, 0,
			     "version", "2",
			     NULL);
	g_assert (n1 != NULL);
	g_assert_cmpstr (as_node_get_name (n1), ==, "apps");
	g_assert_cmpstr (as_node_get_data (n1), ==, NULL);
	g_assert_cmpstr (as_node_get_attribute (n1, "version"), ==, "2");
	g_assert_cmpstr (as_node_get_attribute (n1, "xxx"), ==, NULL);
	n2 = as_node_insert (n1, "id", "hal", 0, NULL);
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_name (n2), ==, "id");
	g_assert_cmpstr (as_node_get_data (n2), ==, "hal");
	g_assert_cmpstr (as_node_get_attribute (n2, "xxx"), ==, NULL);

	/* find the n2 node */
	n2 = as_node_find (root, "apps/id");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_name (n2), ==, "id");

	/* don't find invalid nodes */
	n2 = as_node_find (root, "apps/id/xxx");
	g_assert (n2 == NULL);
	n2 = as_node_find (root, "apps/xxx");
	g_assert (n2 == NULL);
	n2 = as_node_find (root, "apps//id");
	g_assert (n2 == NULL);

	as_node_unref (root);
}

static void
ch_test_node_xml_func (void)
{
	const gchar *valid = "<foo><bar key=\"value\">baz</bar></foo>";
	GError *error = NULL;
	GNode *n2;
	GNode *root;
	GString *xml;

	/* invalid XML */
	root = as_node_from_xml ("<moo>", -1, &error);
	g_assert (root == NULL);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_clear_error (&error);
	root = as_node_from_xml ("<foo></bar>", -1, &error);
	g_assert (root == NULL);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_clear_error (&error);

	/* valid XML */
	root = as_node_from_xml (valid, -1, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);

	n2 = as_node_find (root, "foo/bar");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_data (n2), ==, "baz");
	g_assert_cmpstr (as_node_get_attribute (n2, "key"), ==, "value");

	/* convert back */
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==, valid);
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
		"<foo>\n <bar key=\"value\">baz</bar>\n</foo>\n");
	g_string_free (xml, TRUE);

	as_node_unref (root);
}

static void
ch_test_node_hash_func (void)
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
ch_test_node_localized_func (void)
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
ch_test_node_localized_wrap_func (void)
{
	GError *error = NULL;
	GHashTable *hash;
	GNode *n1;
	GNode *root;
	const gchar *xml =
		"<description>"
		" <p>Hi</p>"
		" <p xml:lang=\"pl\">Czesc</p>"
		" <ul>"
		"  <li>First</li>"
		"  <li xml:lang=\"pl\">Pierwszy</li>"
		" </ul>"
		"</description>";

	root = as_node_from_xml (xml, -1, &error);
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
		"<p>Hi</p><ul><li>First</li></ul>");
	g_assert_cmpstr (g_hash_table_lookup (hash, "pl"), ==,
		"<p>Czesc</p><ul><li>Pierwszy</li></ul>");
	g_hash_table_unref (hash);

	as_node_unref (root);
}

static void
ch_test_app_subsume_func (void)
{
	AsApp *app;
	AsApp *donor;
	GList *list;

	app = as_app_new ();
	donor = as_app_new ();
	as_app_set_icon (donor, "gtk-find", -1);
	as_app_add_pkgname (donor, "hal", -1);
	as_app_add_language (donor, -1, "en_GB", -1);

	/* copy all useful properties */
	as_app_subsume (app, donor);

	g_assert_cmpstr (as_app_get_icon (app), ==, "gtk-find");
	g_assert_cmpint (as_app_get_pkgnames(app)->len, ==, 1);
	list = as_app_get_languages (app);
	g_assert_cmpint (g_list_length (list), ==, 1);
	g_list_free (list);

	g_object_unref (app);
	g_object_unref (donor);
}

static void
ch_test_app_search_func (void)
{
	AsApp *app;

	app = as_app_new ();
	as_app_set_name (app, NULL, "GNOME Software", -1);
	as_app_set_comment (app, NULL, "Install and remove software", -1);

	g_assert_cmpint (as_app_search_matches (app, "software"), ==, 80);
	g_assert_cmpint (as_app_search_matches (app, "soft"), ==, 80);
	g_assert_cmpint (as_app_search_matches (app, "install"), ==, 60);

	g_object_unref (app);
}

static void
ch_test_store_func (void)
{
	AsStore *store;
	GError *error = NULL;
	GFile *file;
	GTimer *timer;
	const gchar *filename = "./test.xml.gz";
	gboolean ret;
	guint i;
	guint loops = 10;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		return;

	file = g_file_new_for_path (filename);
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		store = as_store_new ();
		ret = as_store_parse_file (store, file, NULL, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);
		g_assert_cmpint (as_store_get_apps (store)->len, ==, 1415);
		g_assert (as_store_get_app_by_id (store, "org.gnome.Software") != NULL);
		g_assert (as_store_get_app_by_pkgname (store, "gnome-software") != NULL);
		g_object_unref (store);
	}
	g_print ("%.0f ms: ", g_timer_elapsed (timer, NULL) * 1000 / loops);

	g_object_unref (file);
	g_timer_destroy (timer);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/AppStream/release", ch_test_release_func);
	g_test_add_func ("/AppStream/image", ch_test_image_func);
	g_test_add_func ("/AppStream/screenshot", ch_test_screenshot_func);
	g_test_add_func ("/AppStream/app", ch_test_app_func);
	g_test_add_func ("/AppStream/app{subsume}", ch_test_app_subsume_func);
	g_test_add_func ("/AppStream/app{search}", ch_test_app_search_func);
	g_test_add_func ("/AppStream/node", ch_test_node_func);
	g_test_add_func ("/AppStream/node{xml}", ch_test_node_xml_func);
	g_test_add_func ("/AppStream/node{hash}", ch_test_node_hash_func);
	g_test_add_func ("/AppStream/node{localized}", ch_test_node_localized_func);
	g_test_add_func ("/AppStream/node{localized-wrap}", ch_test_node_localized_wrap_func);
	g_test_add_func ("/AppStream/store", ch_test_store_func);

	return g_test_run ();
}

