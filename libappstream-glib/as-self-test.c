/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2018 Richard Hughes <richard@hughsie.com>
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
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>

#include "as-agreement-private.h"
#include "as-app-private.h"
#include "as-app-builder.h"
#include "as-bundle-private.h"
#include "as-translation-private.h"
#include "as-checksum-private.h"
#include "as-content-rating-private.h"
#include "as-enums.h"
#include "as-icon-private.h"
#include "as-image-private.h"
#include "as-require-private.h"
#include "as-review-private.h"
#include "as-markup.h"
#include "as-monitor.h"
#include "as-node-private.h"
#include "as-problem.h"
#include "as-launchable-private.h"
#include "as-provide-private.h"
#include "as-ref-string.h"
#include "as-release-private.h"
#include "as-suggest-private.h"
#include "as-screenshot-private.h"
#include "as-store.h"
#include "as-tag.h"
#include "as-utils-private.h"
#include "as-yaml.h"

#define AS_TEST_WILDCARD_SHA1	"\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?\?"

static gboolean
as_test_compare_lines (const gchar *txt1, const gchar *txt2, GError **error)
{
	g_autofree gchar *output = NULL;

	/* exactly the same */
	if (g_strcmp0 (txt1, txt2) == 0)
		return TRUE;

	/* matches a pattern */
	if (fnmatch (txt2, txt1, FNM_NOESCAPE) == 0)
		return TRUE;

	/* save temp files and diff them */
	if (!g_file_set_contents ("/tmp/a", txt1, -1, error))
		return FALSE;
	if (!g_file_set_contents ("/tmp/b", txt2, -1, error))
		return FALSE;
	if (!g_spawn_command_line_sync ("diff -urNp /tmp/b /tmp/a",
					&output, NULL, NULL, error))
		return FALSE;

	/* just output the diff */
	g_set_error_literal (error, 1, 0, output);
	return FALSE;
}

static gchar *
as_test_get_filename (const gchar *filename)
{
	g_autofree gchar *path = NULL;

	/* try the source then the destdir */
	path = g_build_filename (TESTDIRSRC, filename, NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);
		path = g_build_filename (TESTDIRBUILD, filename, NULL);
	}
	return realpath (path, NULL);
}

static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

static gboolean
as_test_hang_check_cb (gpointer user_data)
{
	g_main_loop_quit (_test_loop);
	_test_loop_timeout_id = 0;
	_test_loop = NULL;
	return G_SOURCE_REMOVE;
}

static void
as_test_loop_run_with_timeout (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	g_assert (_test_loop == NULL);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, as_test_hang_check_cb, NULL);
	g_main_loop_run (_test_loop);
}

static void
as_test_loop_quit (void)
{
	if (_test_loop_timeout_id > 0) {
		g_source_remove (_test_loop_timeout_id);
		_test_loop_timeout_id = 0;
	}
	if (_test_loop != NULL) {
		g_main_loop_quit (_test_loop);
		g_main_loop_unref (_test_loop);
		_test_loop = NULL;
	}
}

static void
monitor_test_cb (AsMonitor *mon, const gchar *filename, guint *cnt)
{
	(*cnt)++;
}

static void
as_test_monitor_dir_func (void)
{
	gboolean ret;
	guint cnt_added = 0;
	guint cnt_removed = 0;
	guint cnt_changed = 0;
	g_autoptr(AsMonitor) mon = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *tmpdir = "/tmp/monitor-test/usr/share/app-info/xmls";
	g_autofree gchar *tmpfile = NULL;
	g_autofree gchar *tmpfile_new = NULL;
	g_autofree gchar *cmd_touch = NULL;

	tmpfile = g_build_filename (tmpdir, "test.txt", NULL);
	tmpfile_new = g_build_filename (tmpdir, "newtest.txt", NULL);
	g_unlink (tmpfile);
	g_unlink (tmpfile_new);

	mon = as_monitor_new ();
	g_signal_connect (mon, "added",
			  G_CALLBACK (monitor_test_cb), &cnt_added);
	g_signal_connect (mon, "removed",
			  G_CALLBACK (monitor_test_cb), &cnt_removed);
	g_signal_connect (mon, "changed",
			  G_CALLBACK (monitor_test_cb), &cnt_changed);

	/* add watch */
	ret = as_monitor_add_directory (mon, tmpdir, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* create directory */
	g_mkdir_with_parents (tmpdir, 0700);

	/* touch file */
	cmd_touch = g_strdup_printf ("touch %s", tmpfile);
	ret = g_spawn_command_line_sync (cmd_touch, NULL, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 1);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 0);

	/* just change the mtime */
	cnt_added = cnt_removed = cnt_changed = 0;
	ret = g_spawn_command_line_sync (cmd_touch, NULL, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 0);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 1);

	/* delete it */
	cnt_added = cnt_removed = cnt_changed = 0;
	g_unlink (tmpfile);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 0);
	g_assert_cmpint (cnt_removed, ==, 1);
	g_assert_cmpint (cnt_changed, ==, 0);

	/* save a new file with temp copy */
	cnt_added = cnt_removed = cnt_changed = 0;
	ret = g_file_set_contents (tmpfile, "foo", -1, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 1);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 0);

	/* modify file with temp copy */
	cnt_added = cnt_removed = cnt_changed = 0;
	ret = g_file_set_contents (tmpfile, "bar", -1, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 0);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 1);

	/* rename the file */
	cnt_added = cnt_removed = cnt_changed = 0;
	g_assert_cmpint (g_rename (tmpfile, tmpfile_new), ==, 0);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 1);
	g_assert_cmpint (cnt_removed, ==, 1);
	g_assert_cmpint (cnt_changed, ==, 0);

	g_unlink (tmpfile);
	g_unlink (tmpfile_new);
}

static void
as_test_monitor_file_func (void)
{
	gboolean ret;
	guint cnt_added = 0;
	guint cnt_removed = 0;
	guint cnt_changed = 0;
	g_autoptr(AsMonitor) mon = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *tmpfile = "/tmp/one.txt";
	const gchar *tmpfile_new = "/tmp/two.txt";
	g_autofree gchar *cmd_touch = NULL;

	g_unlink (tmpfile);
	g_unlink (tmpfile_new);

	mon = as_monitor_new ();
	g_signal_connect (mon, "added",
			  G_CALLBACK (monitor_test_cb), &cnt_added);
	g_signal_connect (mon, "removed",
			  G_CALLBACK (monitor_test_cb), &cnt_removed);
	g_signal_connect (mon, "changed",
			  G_CALLBACK (monitor_test_cb), &cnt_changed);

	/* add a single file */
	ret = as_monitor_add_file (mon, tmpfile, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* touch file */
	cnt_added = cnt_removed = cnt_changed = 0;
	cmd_touch = g_strdup_printf ("touch %s", tmpfile);
	ret = g_spawn_command_line_sync (cmd_touch, NULL, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 1);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 0);

	/* just change the mtime */
	cnt_added = cnt_removed = cnt_changed = 0;
	ret = g_spawn_command_line_sync (cmd_touch, NULL, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 0);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 1);

	/* delete it */
	cnt_added = cnt_removed = cnt_changed = 0;
	g_unlink (tmpfile);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 0);
	g_assert_cmpint (cnt_removed, ==, 1);
	g_assert_cmpint (cnt_changed, ==, 0);

	/* save a new file with temp copy */
	cnt_added = cnt_removed = cnt_changed = 0;
	ret = g_file_set_contents (tmpfile, "foo", -1, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 1);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 0);

	/* modify file with temp copy */
	cnt_added = cnt_removed = cnt_changed = 0;
	ret = g_file_set_contents (tmpfile, "bar", -1, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	as_test_loop_quit ();
	g_assert_cmpint (cnt_added, ==, 0);
	g_assert_cmpint (cnt_removed, ==, 0);
	g_assert_cmpint (cnt_changed, ==, 1);
}

static void
as_test_app_builder_gettext_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	g_autofree gchar *fn = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GList) list = NULL;
	const gchar *gettext_domains[] = { "app", "notgoingtoexist", NULL };

	app = as_app_new ();
	fn = as_test_get_filename ("usr");
	g_assert (fn != NULL);
	for (i = 0; gettext_domains[i] != NULL; i++) {
		g_autoptr(AsTranslation) translation = NULL;
		translation = as_translation_new ();
		as_translation_set_kind (translation, AS_TRANSLATION_KIND_GETTEXT);
		as_translation_set_id (translation, gettext_domains[i]);
		as_app_add_translation (app, translation);
	}
	ret = as_app_builder_search_translations (app, fn, 25,
						  AS_APP_BUILDER_FLAG_NONE,
						  NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check langs */
	g_assert_cmpint (as_app_get_language (app, "en_GB"), ==, 100);
	g_assert_cmpint (as_app_get_language (app, "ru"), ==, 33);
	g_assert_cmpint (as_app_get_language (app, "fr_FR"), ==, -1);

	/* check fallback */
	g_assert_cmpint (as_app_get_language (app, "ru_RU"), ==, 33);

	/* check size */
	list = as_app_get_languages (app);
	g_assert_cmpint (g_list_length (list), ==, 2);
}

static void
as_test_app_builder_gettext_nodomain_func (void)
{
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *fn = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GList) list = NULL;

	app = as_app_new ();
	fn = as_test_get_filename ("usr");
	ret = as_app_builder_search_translations (app, fn, 50,
						  AS_APP_BUILDER_FLAG_USE_FALLBACKS,
						  NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check langs */
	g_assert_cmpint (as_app_get_language (app, "en_GB"), ==, 100);
	g_assert_cmpint (as_app_get_language (app, "ru"), ==, -1);
	g_assert_cmpint (as_app_get_language (app, "fr_FR"), ==, -1);

	/* check size */
	list = as_app_get_languages (app);
	g_assert_cmpint (g_list_length (list), ==, 1);
}

static void
as_test_app_builder_qt_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	g_autofree gchar *fn = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GList) list = NULL;
	const gchar *gettext_domains[] = { "kdeapp", "notgoingtoexist", NULL };

	app = as_app_new ();
	fn = as_test_get_filename ("usr");
	g_assert (fn != NULL);
	for (i = 0; gettext_domains[i] != NULL; i++) {
		g_autoptr(AsTranslation) translation = NULL;
		translation = as_translation_new ();
		as_translation_set_kind (translation, AS_TRANSLATION_KIND_QT);
		as_translation_set_id (translation, gettext_domains[i]);
		as_app_add_translation (app, translation);
	}
	ret = as_app_builder_search_translations (app, fn, 25,
						  AS_APP_BUILDER_FLAG_NONE,
						  NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check langs */
	g_assert_cmpint (as_app_get_language (app, "fr"), ==, 100);
	g_assert_cmpint (as_app_get_language (app, "en_GB"), ==, -1);

	/* check size */
	list = as_app_get_languages (app);
	g_assert_cmpint (g_list_length (list), ==, 1);
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
	g_assert_cmpint (as_tag_from_string (NULL), ==, AS_TAG_UNKNOWN);

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
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src = "<release type=\"stable\" timestamp=\"123\" urgency=\"critical\" version=\"0.1.2\"/>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsRelease) release = NULL;

	release = as_release_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "release");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_release_node_parse (release, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint ((gint32) as_release_get_timestamp (release), ==, 123);
	g_assert_cmpint (as_release_get_urgency (release), ==, AS_URGENCY_KIND_CRITICAL);
	g_assert_cmpint (as_release_get_state (release), ==, AS_RELEASE_STATE_UNKNOWN);
	g_assert_cmpint (as_release_get_kind (release), ==, AS_RELEASE_KIND_STABLE);
	g_assert_cmpstr (as_release_get_version (release), ==, "0.1.2");

	/* state is not stored in the XML */
	as_release_set_state (release, AS_RELEASE_STATE_INSTALLED);
	g_assert_cmpint (as_release_get_state (release), ==, AS_RELEASE_STATE_INSTALLED);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_release_node_insert (release, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_release_date_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	const gchar *src = "<release date=\"2016-01-18\"/>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsRelease) release = NULL;

	release = as_release_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "release");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_release_node_parse (release, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint ((gint32) as_release_get_timestamp (release), ==, 1453075200);
}

static void
as_test_provide_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src = "<binary>/usr/bin/gnome-shell</binary>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsProvide) provide = NULL;

	provide = as_provide_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "binary");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_provide_node_parse (provide, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_provide_get_kind (provide), ==, AS_PROVIDE_KIND_BINARY);
	g_assert_cmpstr (as_provide_get_value (provide), ==, "/usr/bin/gnome-shell");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_provide_node_insert (provide, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_launchable_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src = "<launchable type=\"desktop-id\">gnome-software.desktop</launchable>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsLaunchable) launchable = NULL;

	launchable = as_launchable_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "launchable");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_launchable_node_parse (launchable, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_launchable_get_kind (launchable), ==, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_cmpstr (as_launchable_get_value (launchable), ==, "gnome-software.desktop");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_launchable_node_insert (launchable, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_release_appstream_func (void)
{
	AsChecksum *csum;
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	gboolean ret;
	guint64 sz;
	const gchar *src =
		"<release timestamp=\"123\" version=\"0.1.2\">\n"
		"<location>http://foo.com/bar.zip</location>\n"
		"<location>http://baz.com/bar.cab</location>\n"
		"<checksum type=\"sha1\" filename=\"firmware.cab\" target=\"container\">12345</checksum>\n"
		"<checksum type=\"md5\" filename=\"firmware.cab\" target=\"container\">deadbeef</checksum>\n"
		"<description><p>This is a new release</p><ul><li>Point</li></ul></description>\n"
		"<description xml:lang=\"pl\"><p>Oprogramowanie</p></description>\n"
		"<size type=\"installed\">123456</size>\n"
		"<size type=\"download\">654321</size>\n"
		"</release>\n";
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsRelease) release = NULL;

	release = as_release_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "release");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_release_node_parse (release, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint ((gint32) as_release_get_timestamp (release), ==, 123);
	g_assert_cmpstr (as_release_get_version (release), ==, "0.1.2");
	g_assert_cmpstr (as_release_get_location_default (release), ==, "http://foo.com/bar.zip");
	g_assert_cmpstr (as_release_get_description (release, "pl"), ==,
				"<p>Oprogramowanie</p>");
	g_assert_cmpstr (as_release_get_description (release, NULL), ==,
				"<p>This is a new release</p><ul><li>Point</li></ul>");

	/* checksum */
	g_assert_cmpint (as_release_get_checksums(release)->len, ==, 2);
	csum = as_release_get_checksum_by_fn (release, "firmware.inf");
	g_assert (csum == NULL);
	csum = as_release_get_checksum_by_fn (release, "firmware.cab");
	g_assert (csum != NULL);
	g_assert_cmpint (as_checksum_get_kind (csum), ==, G_CHECKSUM_SHA1);
	g_assert_cmpint (as_checksum_get_target (csum), ==, AS_CHECKSUM_TARGET_CONTAINER);
	g_assert_cmpstr (as_checksum_get_filename (csum), ==, "firmware.cab");
	g_assert_cmpstr (as_checksum_get_value (csum), ==, "12345");

	/* get by target type */
	csum = as_release_get_checksum_by_target (release, AS_CHECKSUM_TARGET_CONTENT);
	g_assert (csum == NULL);
	csum = as_release_get_checksum_by_target (release, AS_CHECKSUM_TARGET_CONTAINER);
	g_assert (csum != NULL);
	g_assert_cmpstr (as_checksum_get_value (csum), ==, "12345");

	/* test size */
	sz = as_release_get_size (release, AS_SIZE_KIND_INSTALLED);
	g_assert_cmpuint (sz, ==, 123456);
	sz = as_release_get_size (release, AS_SIZE_KIND_DOWNLOAD);
	g_assert_cmpuint (sz, ==, 654321);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 1.0);
	as_node_context_set_format_kind (ctx, AS_FORMAT_KIND_APPSTREAM);
	n = as_release_node_insert (release, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_release_appdata_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	gboolean ret;
	const gchar *src =
		"<release version=\"0.1.2\" timestamp=\"123\">\n"
		"<description>\n"
		"<p>This is a new release</p>\n"
		"<p xml:lang=\"pl\">Oprogramowanie</p>\n"
		"</description>\n"
		"</release>\n";
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsRelease) release = NULL;

	release = as_release_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "release");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	as_node_context_set_format_kind (ctx, AS_FORMAT_KIND_APPDATA);
	ret = as_release_node_parse (release, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint ((gint32) as_release_get_timestamp (release), ==, 123);
	g_assert_cmpstr (as_release_get_version (release), ==, "0.1.2");
	g_assert_cmpstr (as_release_get_description (release, NULL), ==,
				"<p>This is a new release</p>");
	g_assert_cmpstr (as_release_get_description (release, "pl"), ==,
				"<p>Oprogramowanie</p>");
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
	g_autoptr(GdkPixbuf) pb = NULL;
	g_autoptr(GdkPixbuf) pb2 = NULL;

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
	g_autoptr(GError) error = NULL;
	g_autofree gchar *fn_both = NULL;
	g_autofree gchar *fn_horiz = NULL;
	g_autofree gchar *fn_internal1 = NULL;
	g_autofree gchar *fn_internal2 = NULL;
	g_autofree gchar *fn_none = NULL;
	g_autofree gchar *fn_vert = NULL;
	g_autoptr(AsImage) im = NULL;

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
	g_autoptr(GDir) dir = NULL;
	g_autofree gchar *output_dir = NULL;

	/* only do this test if an "output" directory exists */
	output_dir = g_build_filename (TESTDIRSRC, "output", NULL);
	if (!g_file_test (output_dir, G_FILE_TEST_EXISTS))
		return;

	/* look for test screenshots */
	dir = g_dir_open (TESTDIRSRC, 0, &error);
	g_assert_no_error (error);
	g_assert (dir != NULL);
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		guint i;
		g_autofree gchar *path = NULL;

		if (!g_str_has_prefix (tmp, "ss-"))
			continue;
		path = g_build_filename (TESTDIRSRC, tmp, NULL);

		for (i = 0; i < AS_TEST_RESIZE_LAST; i++) {
			g_autofree gchar *new_path = NULL;
			g_autoptr(GString) basename = NULL;

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
as_test_icon_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src = "<icon type=\"cached\">app.png</icon>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autofree gchar *prefix = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	icon = as_icon_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "icon");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_icon_node_parse (icon, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_icon_get_kind (icon), ==, AS_ICON_KIND_CACHED);
	g_assert_cmpstr (as_icon_get_name (icon), ==, "app.png");
	g_assert_cmpstr (as_icon_get_filename (icon), ==, NULL);
	g_assert_cmpstr (as_icon_get_url (icon), ==, NULL);
	g_assert_cmpint (as_icon_get_height (icon), ==, 64);
	g_assert_cmpint (as_icon_get_width (icon), ==, 64);
	g_assert_cmpint (as_icon_get_scale (icon), ==, 1);
	g_assert (as_icon_get_pixbuf (icon) == NULL);
	g_assert (as_icon_get_data (icon) == NULL);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_icon_node_insert (icon, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, "<icon type=\"cached\" height=\"64\" width=\"64\">app.png</icon>", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* convert to embeddded icon */
	prefix = as_test_get_filename ("rpmbuild");
	g_assert (prefix != NULL);
	as_icon_set_prefix (icon, prefix);
	ret = as_icon_convert_to_kind (icon, AS_ICON_KIND_EMBEDDED, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_icon_get_kind (icon), ==, AS_ICON_KIND_EMBEDDED);
	g_assert_cmpstr (as_icon_get_filename (icon), ==, NULL);
	g_assert_cmpstr (as_icon_get_url (icon), ==, NULL);
	g_assert (as_icon_get_pixbuf (icon) != NULL);
	g_assert (as_icon_get_data (icon) != NULL);
}

static void
as_test_icon_scale_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src = "<icon type=\"cached\" height=\"128\" scale=\"2\" width=\"128\">app.png</icon>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	icon = as_icon_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "icon");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_icon_node_parse (icon, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_icon_get_kind (icon), ==, AS_ICON_KIND_CACHED);
	g_assert_cmpstr (as_icon_get_name (icon), ==, "app.png");
	g_assert_cmpstr (as_icon_get_filename (icon), ==, NULL);
	g_assert_cmpstr (as_icon_get_url (icon), ==, NULL);
	g_assert_cmpint (as_icon_get_height (icon), ==, 128);
	g_assert_cmpint (as_icon_get_width (icon), ==, 128);
	g_assert_cmpint (as_icon_get_scale (icon), ==, 2);
	g_assert (as_icon_get_pixbuf (icon) == NULL);
	g_assert (as_icon_get_data (icon) == NULL);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.9);
	n = as_icon_node_insert (icon, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_checksum_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src = "<checksum type=\"sha1\" filename=\"f&amp;n.cab\" target=\"container\">12&amp;45</checksum>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsChecksum) csum = NULL;

	/* helpers */
	g_assert_cmpint (as_checksum_target_from_string ("container"), ==, AS_CHECKSUM_TARGET_CONTAINER);
	g_assert_cmpint (as_checksum_target_from_string ("content"), ==, AS_CHECKSUM_TARGET_CONTENT);
	g_assert_cmpint (as_checksum_target_from_string (NULL), ==, AS_CHECKSUM_TARGET_UNKNOWN);
	g_assert_cmpstr (as_checksum_target_to_string (AS_CHECKSUM_TARGET_CONTAINER), ==, "container");
	g_assert_cmpstr (as_checksum_target_to_string (AS_CHECKSUM_TARGET_CONTENT), ==, "content");
	g_assert_cmpstr (as_checksum_target_to_string (AS_CHECKSUM_TARGET_UNKNOWN), ==, NULL);

	csum = as_checksum_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "checksum");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_checksum_node_parse (csum, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_checksum_get_kind (csum), ==, G_CHECKSUM_SHA1);
	g_assert_cmpint (as_checksum_get_target (csum), ==, AS_CHECKSUM_TARGET_CONTAINER);
	g_assert_cmpstr (as_checksum_get_filename (csum), ==, "f&n.cab");
	g_assert_cmpstr (as_checksum_get_value (csum), ==, "12&45");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_checksum_node_insert (csum, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_icon_embedded_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
"<icon type=\"embedded\" height=\"32\" width=\"32\"><name>app.png</name>"
"<filecontent>\n"
"iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAABmJLR0QA/wD/AP+gvaeTAAAAB3RJ\n"
"TUUH1gsaCxQZBldDDAAACLxJREFUWIW9lmtsHNUVx/8zd3Zm9uFd73ptZ/3Gid+OoUlwyAscSJw4\n"
"tIEKCGCQUPuBIlUIhbbEwIfuh0oRUYtKUEEIVQIJSpomPJKACYKQENNg7BiDE8dJnDi7drzrxz5m\n"
"d3a9O3Nnbj8YaOo6qSFSj3Q0V3Nnzv93z9x7znD4kbZzZ4dbM8QWSbBsAoc2XdeLJFH8OJ2m9/j9\n"
"/vRC4wgLfdDv9zsIobcKgqWVF8idAhHKljU20aol1daCggJOFCUcP3709u7uE88CePa6AZ5/frs1\n"
"lbKvAi+0ihbxpzyPqsaGFXp1dY2tsHARJ8syKKWiruvQdQpKDSxf3iz29Pa0/xAA7rvBK688apmY\n"
"KGwmhGwURHErGGtoaGjUa2vqrIsW+Xir1QpKDVCqg1INuk6vCMNgmgxOZy5eevnFbEJJVfr9/vEF\n"
"ZcDv91fabMIrcQVrG5fWmA31jXJxcQlvs9lAqSF+JxaPxwAwMPbvl1NpFUpCQSw+CSWRwrIbb8aN\n"
"TU3m5593tQJ4bUEAVru4b9u2B28qKy3nDGN2hbquIR6PgX2vNiucyWagJOKIK9NQEgnwAoMgJsCL\n"
"Scg5NoTCY7ihcom192TPPQsGoLpWU1ZaziUScRiG8R+Tmp5FXFEQT0SgKHGAmaCaBqaZ4OUoBi8M\n"
"YvCby5gIq8ikDciyFdVV1Uil1Na2trb8zs7Oqf8JIFgs/el0ajXH8aA0i0QyjpgShZKIgeoUpm4A\n"
"1AAhFAwzWFzajO7+Xrz9eidWr1qN9m13o7ysHA6HA6qqIhAM4Msve8Tg6Fjg9g0tuySLdWdnZ2f2\n"
"agCk5bY1zqKikjvcbjfp+uIYdKrA4UzDV8QhEkhh1eoNWPqT5XC5FqFz7xF83H0MqVQKT+/oAC/I\n"
"6Ds1gk9OHkXXmc/x1UAYmmZBbVUl2u+/zzIdibSMBC7dUVpbeiA4HJy3NvCUJx/2f91HRVGCy5UD\n"
"XzGPgkJAsKhIJROwOexIj53AzGfbMTxyBDdUlGPbvfdi7579EJ1leLj9fjze/hhEyxREWwRTioLR\n"
"uIAXXjsY3/qzreamjRtXCTo52NbWJs0L4H/GPzQ6GkwzMHhyvVBiJpRoCn2fKpgcTaJ7910IvfdL\n"
"HB4ahc23FCubm3Hi3V3YuNyHG4sBqps4/OFHICQMrzeNbGoKlaUFiMUVe8dfPn1h2bLlRm1t7cqM\n"
"ln5mXgAAMBn7YGpyAnmeAsTjJoa+pLh1wzY8+rtfw5Xph5Ar4mCPiDs3b0H/P/+OW9dvxqI8J47v\n"
"2op3//oq0lNhWKRJ2B0RuOwmBGRQfUOpoWtJ/uV9PW9sWH8HCBF+09LS4p0XQNP1d86eOzuT68oF\n"
"pYAgisj15IIXZNjK1uPQZyZqapsQHDmHClmD21WAvjd+j4r6tXhsx5PY8vO74c2sh6bH4HAlEY+M\n"
"4aal1VKhzWj6OiR0XQiMRevr6uwgeGheAEnIHhkY6CeECHDluEDsFO/v24vXX3wJB4cbMcSWoqKi\n"
"AuGRYdg8DbjwzVe47NgIx+0dIISDr6QIMnFDFGTkejkEg2dRXVnGWZBesf2B5iWnR+K9xSUl4MC2\n"
"zgvQ0fGcks1qQ6mUijxPPiwOAkflIARbBr/a8QTGYxnIshXBVCGK1z4MX8ujcC6ux7Gut3DondfR\n"
"dfwAbMUJmGoRIpclTE7E4HLYUFNVITYt9qw8P8EGRNECgGuYC/B9MzKovj84GqgvKS4Vhi+JYFYD\n"
"jFogyTISiQQMg0KwyNB1Cosgoq6pCYK9DjwkqIkM5GQ+il0SnPUueHK9IIRH6/p1lsnpqDuWESZ1\n"
"XQeAvKsCUGq8f/rUwFPVVdWCRbBAz4gQigPYv+dVSKIF09PT4J1ZdPd0Y3FZPjwuO0TeDlm2wuuW\n"
"QAgBADDGYDIGMAabLYe/1H/O5+QzBZFIEgAiVwUwTfLV2NioaRizxzEUzYNsNwBJg8frxsTEBDgp\n"
"D26PF+Vl5ZBEAoHnwX3bTxkAZppgAEzTRFY3IYgyhi+OuvPk+NKp6RkA7PS8ewAA/H6/yTgcmZqa\n"
"gMedD6b54OSbUeq9BWtWrcN4KAQzHQMnyNB0A1nNgGEyGObsig2DAeDAgQM4gtSMjoHB8ywYjk/Y\n"
"eWXF9NQ0GLgDV80AAGiZ7L7h4XOtzc23WFfcdDO4b5fnXO/EewcOwJaK4mRfH3JzVsHrsoMaJqyS\n"
"BaJFAGMmpqNRBIJjdGBomI5enuSn4vR8NJY4I1vT9yaTyRQMvHlNANMkHw2eOU1Wr177vTgA5OTk\n"
"YEtbGw59cAhp9SN48grRVL8YIm9CUeJmODSqhcNholMEZij6VM1+9pLquxweu1BeaZt8SlVVAOxP\n"
"R48em54LwM298dxzfzj/yCO/WMLzs1+HEAGEEFBKsePpDoRC47BaraBSsZmb5ws5nTmnrTbHUBau\n"
"s4l0VguEEkYoqhKXNtxSZJ14MDMzwxsmxnjGLZmvK/7XP6Fh0L/1njy5Y+2adZKqJhEKBdiFi8Pq\n"
"xYsXpSJf/sj4+LhDTaWLHRjnI8GQ7RJ1mHGWl8kwryhz0+W5XKRpsaCulKzMrabSAPixhrqyktLx\n"
"VzOb20mXoRt3PfkPRK+agd27H5cymYI9OjU3CYQfN0z2vka1w+mkdnzXrl3JtrY2KavPPA1wv5Uk\n"
"yS5KIgQigOMAxgBqUGhZDdlsNgWwP0oW685Wz5FYfX2NdSZjaoGLZ6IGjNYn38TAvAALtU2bNnk0\n"
"qj2A2fLaiNkiEwFwCuAOiIK45/Dhw1EAeKFdOLvIa6uorLtZVNQ0G/ymV2VU3/LEW+j60QA/xHbf\n"
"h3wmksFKn8NbWN6IGUPA170nUpRqbf8XAAD48wNYyRHyyZIim91b0gCNy0HvF0dAriMmd4XzVziZ\n"
"4wIA8uEphNdV8X1qRr9LZnHRoFlElMTla2VgrgB3Fb/W3Nw42L6ZrClzs7d5ngtrmvHQQgEWInYt\n"
"xxVXYLZ16ADU690D3JzxXLG581caBWBep/71278AZpn8hFce4VcAAAAASUVORK5CYII=\n"
"</filecontent>"
"</icon>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	icon = as_icon_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "icon");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_icon_node_parse (icon, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_icon_get_kind (icon), ==, AS_ICON_KIND_EMBEDDED);
	g_assert_cmpstr (as_icon_get_name (icon), ==, "app.png");
	g_assert_cmpstr (as_icon_get_filename (icon), ==, NULL);
	g_assert_cmpstr (as_icon_get_url (icon), ==, NULL);
	g_assert_cmpint (as_icon_get_height (icon), ==, 32);
	g_assert_cmpint (as_icon_get_width (icon), ==, 32);
	g_assert (as_icon_get_data (icon) != NULL);
	g_assert (as_icon_get_pixbuf (icon) != NULL);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_icon_node_insert (icon, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* convert to cached icon */
	as_icon_set_prefix (icon, "/tmp");
	ret = as_icon_convert_to_kind (icon, AS_ICON_KIND_CACHED, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_icon_get_kind (icon), ==, AS_ICON_KIND_CACHED);
	g_assert_cmpstr (as_icon_get_filename (icon), ==, NULL);
	g_assert_cmpstr (as_icon_get_url (icon), ==, NULL);
	g_assert (as_icon_get_pixbuf (icon) != NULL);
	g_assert (as_icon_get_data (icon) != NULL);
	g_assert (g_file_test ("/tmp/32x32/app.png", G_FILE_TEST_EXISTS));
}

static void
as_test_image_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<image type=\"thumbnail\" height=\"12\" width=\"34\" xml:lang=\"en_GB\">"
		"http://www.hughsie.com/a.jpg</image>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autofree gchar *filename = NULL;
	g_autoptr(AsImage) image = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	image = as_image_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "image");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_image_node_parse (image, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_image_get_kind (image), ==, AS_IMAGE_KIND_THUMBNAIL);
	g_assert_cmpint (as_image_get_height (image), ==, 12);
	g_assert_cmpint (as_image_get_width (image), ==, 34);
	g_assert_cmpstr (as_image_get_locale (image), ==, "en_GB");
	g_assert_cmpstr (as_image_get_url (image), ==, "http://www.hughsie.com/a.jpg");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_image_node_insert (image, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
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
as_test_agreement_func (void)
{
	GError *error = NULL;
	AsAgreementSection *sect;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<agreement type=\"eula\" version_id=\"1.2.3a\">\n"
		"<agreement_section type=\"intro\">\n"
		"<description><p>Mighty Fine</p></description>\n"
		"</agreement_section>\n"
		"</agreement>\n";
	gboolean ret;
	g_autoptr(AsAgreement) agreement = NULL;
	g_autoptr(AsNodeContext) ctx = NULL;

	agreement = as_agreement_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "agreement");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_agreement_node_parse (agreement, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_agreement_get_kind (agreement), ==, AS_AGREEMENT_KIND_EULA);
	g_assert_cmpstr (as_agreement_get_version_id (agreement), ==, "1.2.3a");
	sect = as_agreement_get_section_default (agreement);
	g_assert_nonnull (sect);
	g_assert_cmpstr (as_agreement_section_get_kind (sect), ==, "intro");
	g_assert_cmpstr (as_agreement_section_get_description (sect, NULL), ==, "<p>Mighty Fine</p>");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_agreement_node_insert (agreement, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_review_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<review date=\"2016-09-15\" id=\"17\" rating=\"80\">\n"
		"<priority>5</priority>\n"
		"<summary>Hello world</summary>\n"
		"<description><p>Mighty Fine</p></description>\n"
		"<version>1.2.3</version>\n"
		"<reviewer_id>deadbeef</reviewer_id>\n"
		"<reviewer_name>Richard Hughes</reviewer_name>\n"
		"<lang>en_GB</lang>\n"
		"<metadata>\n"
		"<value key=\"foo\">bar</value>\n"
		"</metadata>\n"
		"</review>\n";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsReview) review = NULL;

	review = as_review_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "review");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_review_node_parse (review, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_review_get_priority (review), ==, 5);
	g_assert (as_review_get_date (review) != NULL);
	g_assert_cmpstr (as_review_get_id (review), ==, "17");
	g_assert_cmpstr (as_review_get_version (review), ==, "1.2.3");
	g_assert_cmpstr (as_review_get_reviewer_id (review), ==, "deadbeef");
	g_assert_cmpstr (as_review_get_reviewer_name (review), ==, "Richard Hughes");
	g_assert_cmpstr (as_review_get_summary (review), ==, "Hello world");
	g_assert_cmpstr (as_review_get_locale (review), ==, "en_GB");
	g_assert_cmpstr (as_review_get_description (review), ==, "<p>Mighty Fine</p>");
	g_assert_cmpstr (as_review_get_metadata_item (review, "foo"), ==, "bar");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_review_node_insert (review, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_require_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	const gchar *src =
		"<component type=\"desktop\">\n"
		"<requires>\n"
		"<id>gimp.desktop</id>\n"
		"<firmware compare=\"ge\" version=\"0.1.2\">bootloader</firmware>\n"
		"<firmware compare=\"eq\" version=\"1.0.0\">runtime</firmware>\n"
		"<hardware>4be0643f-1d98-573b-97cd-ca98a65347dd</hardware>\n"
		"</requires>\n"
		"</component>\n";
	gboolean ret;
	GPtrArray *requires;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsRequire) require = NULL;
	g_autoptr(GString) xml = NULL;

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "component");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	app = as_app_new ();
	ret = as_app_node_parse (app, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	requires = as_app_get_requires (app);
	g_assert_cmpint (requires->len, ==, 4);
	require = g_ptr_array_index (requires, 0);
	g_assert_cmpint (as_require_get_kind (require), ==, AS_REQUIRE_KIND_ID);
	g_assert_cmpint (as_require_get_compare (require), ==, AS_REQUIRE_COMPARE_UNKNOWN);
	g_assert_cmpstr (as_require_get_version (require), ==, NULL);
	g_assert_cmpstr (as_require_get_value (require), ==, "gimp.desktop");
	require = as_app_get_require_by_value (app, AS_REQUIRE_KIND_FIRMWARE, "bootloader");
	g_assert_cmpint (as_require_get_kind (require), ==, AS_REQUIRE_KIND_FIRMWARE);
	g_assert_cmpint (as_require_get_compare (require), ==, AS_REQUIRE_COMPARE_GE);
	g_assert_cmpstr (as_require_get_version (require), ==, "0.1.2");
	g_assert_cmpstr (as_require_get_value (require), ==, "bootloader");
	require = g_ptr_array_index (requires, 3);
	g_assert_cmpint (as_require_get_kind (require), ==, AS_REQUIRE_KIND_HARDWARE);
	g_assert_cmpint (as_require_get_compare (require), ==, AS_REQUIRE_COMPARE_UNKNOWN);
	g_assert_cmpstr (as_require_get_version (require), ==, NULL);
	g_assert_cmpstr (as_require_get_value (require), ==, "4be0643f-1d98-573b-97cd-ca98a65347dd");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_app_node_insert (app, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* test we can go back and forth */
	for (guint i = 0; i < AS_REQUIRE_COMPARE_LAST; i++) {
		const gchar *tmp = as_require_compare_to_string (i);
		g_assert_cmpint (as_require_compare_from_string (tmp), ==, i);
	}

	/* check predicates */
	require = as_require_new ();
	as_require_set_version (require, "0.1.2");
	as_require_set_compare (require, AS_REQUIRE_COMPARE_EQ);
	ret = as_require_version_compare (require, "0.1.2", &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_require_set_compare (require, AS_REQUIRE_COMPARE_LT);
	ret = as_require_version_compare (require, "0.1.1", &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_require_set_compare (require, AS_REQUIRE_COMPARE_LE);
	ret = as_require_version_compare (require, "0.1.2", &error);
	g_assert_no_error (error);
	g_assert (ret);

	as_require_set_version (require, "0.1.?");
	as_require_set_compare (require, AS_REQUIRE_COMPARE_GLOB);
	ret = as_require_version_compare (require, "0.1.9", &error);
	g_assert_no_error (error);
	g_assert (ret);

	as_require_set_version (require, "0.1.[0-9]");
	as_require_set_compare (require, AS_REQUIRE_COMPARE_REGEX);
	ret = as_require_version_compare (require, "0.1.9", &error);
	g_assert_no_error (error);
	g_assert (ret);
}

static void
as_test_suggest_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<suggests type=\"upstream\">\n"
		"<id>gimp.desktop</id>\n"
		"<id>mypaint.desktop</id>\n"
		"</suggests>\n";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsSuggest) suggest = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	suggest = as_suggest_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "suggests");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_suggest_node_parse (suggest, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_suggest_get_kind (suggest), ==, AS_SUGGEST_KIND_UPSTREAM);
	g_assert_cmpint (as_suggest_get_ids(suggest)->len, ==, 2);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_suggest_node_insert (suggest, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_bundle_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<bundle type=\"limba\" runtime=\"1\" sdk=\"2\">gnome-3-16</bundle>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsBundle) bundle = NULL;

	bundle = as_bundle_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "bundle");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_bundle_node_parse (bundle, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_bundle_get_kind (bundle), ==, AS_BUNDLE_KIND_LIMBA);
	g_assert_cmpstr (as_bundle_get_id (bundle), ==, "gnome-3-16");
	g_assert_cmpstr (as_bundle_get_runtime (bundle), ==, "1");
	g_assert_cmpstr (as_bundle_get_sdk (bundle), ==, "2");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_bundle_node_insert (bundle, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_translation_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<translation type=\"gettext\">gnome-software</translation>";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsTranslation) translation = NULL;

	translation = as_translation_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "translation");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_translation_node_parse (translation, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_node_unref (root);

	/* verify */
	g_assert_cmpint (as_translation_get_kind (translation), ==, AS_TRANSLATION_KIND_GETTEXT);
	g_assert_cmpstr (as_translation_get_id (translation), ==, "gnome-software");

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_translation_node_insert (translation, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_screenshot_func (void)
{
	GPtrArray *images;
	AsImage *im;
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<screenshot priority=\"-64\">\n"
		"<caption>Hello</caption>\n"
		"<image type=\"source\" height=\"800\" width=\"600\">http://1.png</image>\n"
		"<image type=\"thumbnail\" height=\"100\" width=\"100\">http://2.png</image>\n"
		"</screenshot>\n";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsScreenshot) screenshot = NULL;

	screenshot = as_screenshot_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "screenshot");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_screenshot_node_parse (screenshot, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpint (as_screenshot_get_kind (screenshot), ==, AS_SCREENSHOT_KIND_NORMAL);
	g_assert_cmpint (as_screenshot_get_priority (screenshot), ==, -64);
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
	as_node_context_set_version (ctx, 0.8);
	n = as_screenshot_node_insert (screenshot, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_content_rating_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	const gchar *src =
		"<content_rating type=\"oars-1.0\">\n"
		"<content_attribute id=\"drugs-alcohol\">moderate</content_attribute>\n"
		"<content_attribute id=\"violence-cartoon\">mild</content_attribute>\n"
		"</content_rating>\n";
	gboolean ret;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsContentRating) content_rating = NULL;

	content_rating = as_content_rating_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "content_rating");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_content_rating_node_parse (content_rating, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_content_rating_get_kind (content_rating), ==, "oars-1.0");
	g_assert_cmpint (as_content_rating_get_value (content_rating, "drugs-alcohol"), ==,
			 AS_CONTENT_RATING_VALUE_MODERATE);
	g_assert_cmpint (as_content_rating_get_value (content_rating, "violence-cartoon"), ==,
			 AS_CONTENT_RATING_VALUE_MILD);
	g_assert_cmpint (as_content_rating_get_value (content_rating, "violence-bloodshed"), ==,
			 AS_CONTENT_RATING_VALUE_UNKNOWN);
	as_node_unref (root);

	/* check CSM */
	g_assert_cmpint (as_content_rating_get_minimum_age (content_rating), ==, 13);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.8);
	n = as_content_rating_node_insert (content_rating, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_app_func (void)
{
	AsIcon *ic;
	AsBundle *bu;
	AsRelease *rel;
	AsLaunchable *lau;
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GPtrArray *icons;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<component type=\"desktop\" merge=\"replace\" priority=\"-4\">\n"
		"<id>org.gnome.Software.desktop</id>\n"
		"<pkgname>gnome-software</pkgname>\n"
		"<source_pkgname>gnome-software-src</source_pkgname>\n"
		"<bundle type=\"flatpak\">app/org.gnome.Software/x86_64/master</bundle>\n"
		"<translation type=\"gettext\">gnome-software</translation>\n"
		"<suggests type=\"upstream\">\n"
		"<id>gimp.desktop</id>\n"
		"<id>mypaint.desktop</id>\n"
		"</suggests>\n"
		"<name>Software</name>\n"
		"<name xml:lang=\"pl\">Oprogramowanie</name>\n"
		"<summary>Application manager</summary>\n"
		"<developer_name>GNOME Foundation</developer_name>\n"
		"<description><p>Software allows you to find stuff</p></description>\n"
		"<description xml:lang=\"pt_BR\"><p>O aplicativo Software.</p></description>\n"
		"<icon type=\"cached\" height=\"64\" width=\"64\">org.gnome.Software1.png</icon>\n"
		"<icon type=\"cached\" height=\"64\" width=\"64\">org.gnome.Software2.png</icon>\n"
		"<categories>\n"
		"<category>System</category>\n"
		"</categories>\n"
		"<architectures>\n"
		"<arch>i386</arch>\n"
		"</architectures>\n"
		"<keywords>\n"
		"<keyword>Installing</keyword>\n"
		"</keywords>\n"
		"<kudos>\n"
		"<kudo>SearchProvider</kudo>\n"
		"</kudos>\n"
		"<permissions>\n"
		"<permission>Network</permission>\n"
		"</permissions>\n"
		"<vetos>\n"
		"<veto>Required AppData: ConsoleOnly</veto>\n"
		"</vetos>\n"
		"<mimetypes>\n"
		"<mimetype>application/vnd.oasis.opendocument.spreadsheet</mimetype>\n"
		"</mimetypes>\n"
		"<project_license>GPLv2+</project_license>\n"
		"<url type=\"homepage\">https://wiki.gnome.org/Design/Apps/Software</url>\n"
		"<project_group>GNOME</project_group>\n"
		"<compulsory_for_desktop>GNOME</compulsory_for_desktop>\n"
		"<screenshots>\n"
		"<screenshot type=\"default\">\n"
		"<image type=\"thumbnail\" height=\"351\" width=\"624\">http://a.png</image>\n"
		"</screenshot>\n"
		"<screenshot>\n"
		"<image type=\"thumbnail\">http://b.png</image>\n"
		"</screenshot>\n"
		"</screenshots>\n"
		"<reviews>\n"
		"<review date=\"2016-09-15\">\n"
		"<summary>Hello world</summary>\n"
		"</review>\n"
		"</reviews>\n"
		"<content_rating type=\"oars-1.0\">\n"
		"<content_attribute id=\"drugs-alcohol\">moderate</content_attribute>\n"
		"</content_rating>\n"
		"<releases>\n"
		"<release timestamp=\"1392724801\" version=\"3.11.91\"/>\n"
		"<release timestamp=\"1392724800\" version=\"3.11.90\"/>\n"
		"</releases>\n"
		"<provides>\n"
		"<binary>/usr/bin/gnome-shell</binary>\n"
		"<dbus type=\"session\">org.gnome.Software</dbus>\n"
		"<dbus type=\"system\">org.gnome.Software2</dbus>\n"
		"</provides>\n"
		"<launchable type=\"desktop-id\">gnome-software.desktop</launchable>\n"
		"<languages>\n"
		"<lang percentage=\"90\">en_GB</lang>\n"
		"<lang>pl</lang>\n"
		"</languages>\n"
		"<custom>\n"
		"<value key=\"SomethingRandom\"/>\n"
		"</custom>\n"
		"</component>\n";
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsApp) app = NULL;

	app = as_app_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "component");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_app_node_parse (app, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_id (app), ==, "org.gnome.Software.desktop");
	g_assert_cmpstr (as_app_get_id_filename (app), ==, "org.gnome.Software");
	g_assert_cmpstr (as_app_get_unique_id (app), ==, "*/flatpak/*/desktop/org.gnome.Software.desktop/master");
	g_assert_cmpstr (as_app_get_name (app, "pl"), ==, "Oprogramowanie");
	g_assert_cmpstr (as_app_get_comment (app, NULL), ==, "Application manager");
	g_assert_cmpstr (as_app_get_description (app, NULL), ==, "<p>Software allows you to find stuff</p>");
	g_assert_cmpstr (as_app_get_description (app, "pt_BR"), ==, "<p>O aplicativo Software.</p>");
	g_assert_cmpstr (as_app_get_developer_name (app, NULL), ==, "GNOME Foundation");
	g_assert_cmpstr (as_app_get_source_pkgname (app), ==, "gnome-software-src");
	g_assert_cmpstr (as_app_get_project_group (app), ==, "GNOME");
	g_assert_cmpstr (as_app_get_project_license (app), ==, "GPLv2+");
	g_assert_cmpstr (as_app_get_branch (app), ==, "master");
	g_assert_cmpint (as_app_get_categories(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_priority (app), ==, -4);
	g_assert_cmpint (as_app_get_screenshots(app)->len, ==, 2);
	g_assert_cmpint (as_app_get_releases(app)->len, ==, 2);
	g_assert_cmpint (as_app_get_launchables(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_provides(app)->len, ==, 3);
	g_assert_cmpint (as_app_get_kudos(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_permissions(app)->len, ==, 1);
	g_assert_cmpstr (as_app_get_metadata_item (app, "SomethingRandom"), ==, "");
	g_assert_cmpint (as_app_get_language (app, "en_GB"), ==, 90);
	g_assert_cmpint (as_app_get_language (app, "pl"), ==, 0);
	g_assert_cmpint (as_app_get_language (app, "xx_XX"), ==, -1);
	g_assert (as_app_has_kudo (app, "SearchProvider"));
	g_assert (as_app_has_kudo_kind (app, AS_KUDO_KIND_SEARCH_PROVIDER));
	g_assert (as_app_has_permission (app, "Network"));
	g_assert (!as_app_has_kudo (app, "MagicValue"));
	g_assert (!as_app_has_kudo_kind (app, AS_KUDO_KIND_USER_DOCS));
	g_assert (as_app_has_compulsory_for_desktop (app, "GNOME"));
	g_assert (!as_app_has_compulsory_for_desktop (app, "KDE"));
	as_node_unref (root);

	/* check equality */
	g_assert (as_app_equal (app, app));

	/* check newest release */
	rel = as_app_get_release_default (app);
	g_assert (rel != NULL);
	g_assert_cmpstr (as_release_get_version (rel), ==, "3.11.91");

	/* check specific release */
	rel = as_app_get_release_by_version (app, "3.11.91");
	g_assert (rel != NULL);
	g_assert_cmpstr (as_release_get_version (rel), ==, "3.11.91");

	/* check icons */
	icons = as_app_get_icons (app);
	g_assert (icons != NULL);
	g_assert_cmpint (icons->len, ==, 2);

	/* check bundle */
	bu = as_app_get_bundle_default (app);
	g_assert (bu != NULL);
	g_assert_cmpint (as_bundle_get_kind (bu), ==, AS_BUNDLE_KIND_FLATPAK);
	g_assert_cmpstr (as_bundle_get_id (bu), ==, "app/org.gnome.Software/x86_64/master");

	/* check launchable */
	lau = as_app_get_launchable_by_kind (app, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert (lau != NULL);
	g_assert_cmpint (as_launchable_get_kind (lau), ==, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_cmpstr (as_launchable_get_value (lau), ==, "gnome-software.desktop");

	/* check we can get a specific icon */
	ic = as_app_get_icon_for_size (app, 999, 999);
	g_assert (ic == NULL);
	ic = as_app_get_icon_for_size (app, 64, 64);
	g_assert (ic != NULL);
	g_assert_cmpstr (as_icon_get_name (ic), ==, "org.gnome.Software1.png");
	g_assert_cmpint (as_icon_get_kind (ic), ==, AS_ICON_KIND_CACHED);

	/* we can't extend ourself */
	as_app_add_extends (app, "org.gnome.Software.desktop");
	g_assert_cmpint (as_app_get_extends(app)->len, ==, 0);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 1.0);
	n = as_app_node_insert (app, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* test contact demunging */
	as_app_set_update_contact (app, "richard_at_hughsie_dot_co_dot_uk");
	g_assert_cmpstr (as_app_get_update_contact (app), ==, "richard@hughsie.co.uk");
}

static void
as_test_app_launchable_fallback_func (void)
{
	AsLaunchable *lau;
	AsNode *n;
	gboolean ret;
	const gchar *src =
		"<component type=\"desktop\">\n"
		"<id>org.gnome.Software</id>\n"
		"</component>\n";
	g_autoptr(AsApp) app = NULL;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(GError) error = NULL;

	app = as_app_new ();

	/* to object */
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "component");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_app_node_parse (app, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_id (app), ==, "org.gnome.Software");
	g_assert_cmpint (as_app_get_launchables(app)->len, ==, 1);
	lau = as_app_get_launchable_by_kind (app, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert (lau != NULL);
	g_assert_cmpint (as_launchable_get_kind (lau), ==, AS_LAUNCHABLE_KIND_DESKTOP_ID);
	g_assert_cmpstr (as_launchable_get_value (lau), ==, "org.gnome.Software.desktop");
}

static void
as_test_app_validate_check (GPtrArray *array,
			    AsProblemKind kind,
			    const gchar *message)
{
	AsProblem *problem;
	gchar *tmp;
	guint i;

	for (i = 0; i < array->len; i++) {
		g_autofree gchar *message_no_data = NULL;
		problem = g_ptr_array_index (array, i);
		if (as_problem_get_kind (problem) != kind)
			continue;
		message_no_data = g_strdup (as_problem_get_message (problem));
		tmp = g_strrstr (message_no_data, " [");
		if (tmp != NULL)
			*tmp = '\0';
		tmp = g_strrstr (message_no_data, ", ");
		if (tmp != NULL)
			*tmp = '\0';
		if (g_strcmp0 (message_no_data, message) == 0)
			return;
	}
	g_print ("\n");
	for (i = 0; i < array->len; i++) {
		problem = g_ptr_array_index (array, i);
		g_print ("%u\t%s\n",
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
	g_autofree gchar *filename = NULL;
	g_autoptr(AsApp) app = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("success.appdata.xml");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check success */
	g_assert_cmpint (as_app_get_kind (app), ==, AS_APP_KIND_DESKTOP);
	g_assert_cmpstr (as_app_get_id (app), ==, "gnome-power-statistics.desktop");
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "0 A.D.");
	g_assert_cmpstr (as_app_get_comment (app, "C"), ==, "Observe power management");
	g_assert_cmpstr (as_app_get_metadata_license (app), ==, "CC0-1.0 AND CC-BY-3.0");
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
		g_print ("%s\n", as_problem_get_message (problem));
	}
	g_assert_cmpint (probs->len, ==, 0);
	g_ptr_array_unref (probs);

	/* check screenshots were loaded */
	screenshots = as_app_get_screenshots (app);
	g_assert_cmpint (screenshots->len, ==, 1);
	ss = as_app_get_screenshot_default (app);
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
	g_autofree gchar *filename = NULL;
	g_autoptr(AsApp) app = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("example.metainfo.xml");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check success */
	g_assert_cmpint (as_app_get_kind (app), ==, AS_APP_KIND_ADDON);
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
	g_autofree gchar *filename = NULL;
	g_autoptr(AsApp) app = NULL;

	/* open file */
	app = as_app_new ();
	filename = as_test_get_filename ("intltool.appdata.xml.in");
	ret = as_app_parse_file (app, filename, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check success */
	g_assert_cmpint (as_app_get_kind (app), ==, AS_APP_KIND_DESKTOP);
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
	g_autofree gchar *filename = NULL;
	g_autoptr(AsApp) app = NULL;

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
	g_autofree gchar *filename = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GPtrArray) probs = NULL;
	g_autoptr(GPtrArray) probs2 = NULL;

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

	as_test_app_validate_check (probs, AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				    "<component> has invalid type attribute");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<metadata_license> is not valid");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<project_license> is not valid");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<update_contact> is not present");
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
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<translation> not specified");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<release> versions are not in order");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<release> version was duplicated");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
				    "<release> timestamp is in the future");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<content_rating> required for game");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_MARKUP_INVALID,
				    "<id> has invalid character");
	g_assert_cmpint (probs->len, ==, 34);

	/* again, harder */
	probs2 = as_app_validate (app, AS_APP_VALIDATE_FLAG_STRICT, &error);
	g_assert_no_error (error);
	g_assert (probs2 != NULL);
	as_test_app_validate_check (probs2, AS_PROBLEM_KIND_TAG_INVALID,
				    "XML data contains unknown tag");
	g_assert_cmpint (probs2->len, ==, 40);
}

static void
as_test_app_validate_meta_bad_func (void)
{
	AsProblem *problem;
	GError *error = NULL;
	gboolean ret;
	guint i;
	g_autofree gchar *filename = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GPtrArray) probs = NULL;

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
				    "<update_contact> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<extends> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<metadata_license> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "<pkgname> not allowed in metainfo");
}

static void
as_test_store_local_appdata_func (void)
{
	AsApp *app;
	AsFormat *format;
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autofree gchar *filename_full = NULL;
	g_autoptr(AsStore) store = NULL;

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

	/* check format */
	format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPDATA);
	g_assert (format != NULL);
	filename_full = g_build_filename (filename,
					  "usr/share/appdata/broken.appdata.xml",
					  NULL);
	g_assert_cmpstr (as_format_get_filename (format), ==, filename_full);
}

static void
as_test_store_validate_func (void)
{
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GPtrArray) probs = NULL;

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
				    "metadata version is v0.1 and "
				    "<screenshots> only introduced in v0.4");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "metadata version is v0.1 and "
				    "<compulsory_for_desktop> only introduced in v0.4");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "metadata version is v0.1 and "
				    "<project_group> only introduced in v0.4");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_INVALID,
				    "metadata version is v0.1 and "
				    "<description> markup was introduced in v0.6");
}

static void
_as_app_add_format_kind (AsApp *app, AsFormatKind kind)
{
	g_autoptr(AsFormat) format = as_format_new ();
	as_format_set_kind (format, kind);
	as_app_add_format (app, format);
}

static void
as_test_app_validate_style_func (void)
{
	AsProblem *problem;
	GError *error = NULL;
	guint i;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GPtrArray) probs = NULL;

	app = as_app_new ();
	as_app_add_url (app, AS_URL_KIND_UNKNOWN, "dave.com");
	as_app_set_id (app, "dave.exe");
	as_app_set_kind (app, AS_APP_KIND_DESKTOP);
	_as_app_add_format_kind (app, AS_FORMAT_KIND_APPDATA);
	as_app_set_metadata_license (app, "BSD");
	as_app_set_project_license (app, "GPL-2.0+");
	as_app_set_name (app, "C", "Test app name that is very log indeed.");
	as_app_set_comment (app, "C", "Awesome");
	as_app_set_update_contact (app, "someone_who_cares@upstream_project.org");

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
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "Not enough <screenshot> tags");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_STYLE_INCORRECT,
				    "<summary> is shorter than <name>");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<url> is not present");
	as_test_app_validate_check (probs, AS_PROBLEM_KIND_TAG_MISSING,
				    "<translation> not specified");
	g_assert_cmpint (probs->len, ==, 11);
}

static void
as_test_app_parse_file_desktop_func (void)
{
	AsFormat *format;
	AsIcon *ic;
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autoptr(AsApp) app = NULL;

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
	g_assert_cmpint (as_app_get_vetos(app)->len, ==, 1);
	g_assert_cmpstr (as_app_get_project_group (app), ==, NULL);
	g_assert_cmpint (as_app_get_categories(app)->len, ==, 1);
	g_assert_cmpint (as_app_get_keywords(app, NULL)->len, ==, 2);
	g_assert_cmpint (as_app_get_keywords(app, "pl")->len, ==, 1);
	g_assert (as_app_has_category (app, "System"));
	g_assert (!as_app_has_category (app, "NotGoingToExist"));

	/* check format */
	g_assert_cmpint (as_app_get_formats(app)->len, ==, 1);
	format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_DESKTOP);
	g_assert (format != NULL);
	g_assert_cmpstr (as_format_get_filename (format), ==, filename);

	/* check icons */
	g_assert_cmpint (as_app_get_icons(app)->len, ==, 1);
	ic = as_app_get_icon_default (app);
	g_assert (ic != NULL);
	g_assert_cmpstr (as_icon_get_name (ic), ==, "audio-input-microphone");
	g_assert_cmpint (as_icon_get_kind (ic), ==, AS_ICON_KIND_STOCK);
	g_assert_cmpint (as_icon_get_width (ic), ==, 0);
	g_assert_cmpint (as_icon_get_height (ic), ==, 0);

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
	AsNode *n;
	AsNode *root;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<component type=\"desktop\">\n"
		"<id>org.gnome.Software.desktop</id>\n"
		"<description>Software is awesome:\n\n * Bada\n * Boom!</description>\n"
		"<launchable type=\"desktop-id\">org.gnome.Software.desktop</launchable>\n"
		"</component>\n";
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsApp) app = NULL;

	app = as_app_new ();

	/* to object */
	root = as_node_from_xml (src,
				 AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				 &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "component");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_app_node_parse (app, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_id (app), ==, "org.gnome.Software.desktop");
	g_assert_cmpstr (as_app_get_description (app, "C"), ==,
		"Software is awesome:\n\n * Bada\n * Boom!");
	as_node_unref (root);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_app_node_insert (app, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_node_reflow_text_func (void)
{
	AsRefString *tmp;

	/* plain text */
	tmp = as_node_reflow_text ("Dave", -1);
	g_assert_cmpstr (tmp, ==, "Dave");
	as_ref_string_unref (tmp);

	/* stripping */
	tmp = as_node_reflow_text ("    Dave    ", -1);
	g_assert_cmpstr (tmp, ==, "Dave");
	as_ref_string_unref (tmp);

	/* paragraph */
	tmp = as_node_reflow_text ("Dave\n\nSoftware", -1);
	g_assert_cmpstr (tmp, ==, "Dave\n\nSoftware");
	as_ref_string_unref (tmp);

	/* pathological */
	tmp = as_node_reflow_text (
		"\n"
		"  Dave: \n"
		"  Software is \n"
		"  awesome.\n\n\n"
		"  Okay!\n", -1);
	g_assert_cmpstr (tmp, ==, "Dave: Software is awesome.\n\nOkay!");
	as_ref_string_unref (tmp);
}

static void
as_test_node_sort_func (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(GString) str = NULL;

	root = as_node_from_xml ("<d>ddd</d><c>ccc</c><b>bbb</b><a>aaa</a>", 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);

	/* verify that the tags are sorted */
	str = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_SORT_CHILDREN);
	g_assert_cmpstr (str->str, ==, "<a>aaa</a><b>bbb</b><c>ccc</c><d>ddd</d>");
}

static void
as_test_node_func (void)
{
	AsNode *n1;
	AsNode *n2;
	g_autoptr(AsNode) root = NULL;

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
	as_node_set_data (n2, "udev", 0);
	g_assert_cmpstr (as_node_get_data (n2), ==, "udev");
	as_node_add_attribute (n2, "enabled", "true");
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
	const gchar *valid = "<!--\n"
			     "  this documents foo\n"
			     "-->"
			     "<foo>"
			     "<!-- this documents bar -->"
			     "<bar key=\"value\">baz</bar>"
			     "</foo>";
	GError *error = NULL;
	AsNode *n2;
	AsNode *root;
	GString *xml;

	/* invalid XML */
	root = as_node_from_xml ("<moo>", 0, &error);
	g_assert (root == NULL);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_clear_error (&error);
	root = as_node_from_xml ("<foo></bar>", 0, &error);
	g_assert (root == NULL);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_clear_error (&error);

	/* valid XML */
	root = as_node_from_xml (valid, 0, &error);
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
	root = as_node_from_xml ("<p>One</p><p>Two</p>", 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_ALL), ==, 3);
	xml = as_node_to_xml (root->children, AS_NODE_TO_XML_FLAG_INCLUDE_SIBLINGS);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==, "<p>One</p><p>Two</p>");
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* keep comments */
	root = as_node_from_xml (valid,
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
	as_node_unref (root);

	/* keep comment formatting */
	root = as_node_from_xml (valid,
				 AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS |
				 AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				 &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n2 = as_node_find (root, "foo/bar");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_comment (n2), ==, " this documents bar ");
	n2 = as_node_find (root, "foo");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_comment (n2), ==, "\n  this documents foo\n");

	/* check comments were preserved */
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_NONE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==, valid);
	g_string_free (xml, TRUE);
	as_node_unref (root);

	/* check comments are appended together */
	root = as_node_from_xml ("<!-- 1st -->\n<!-- 2nd -->\n<foo/>\n",
				 AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS |
				 AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				 &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n2 = as_node_find (root, "foo");
	g_assert (n2 != NULL);
	g_assert_cmpstr (as_node_get_comment (n2), ==, " 1st <&> 2nd ");

	/* check comments were output as two blocks */
	xml = as_node_to_xml (root, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	g_assert (xml != NULL);
	g_assert_cmpstr (xml->str, ==, "<!-- 1st -->\n<!-- 2nd -->\n<foo/>\n");
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_node_hash_func (void)
{
	GHashTable *hash;
	AsNode *n1;
	AsNode *root;
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
	AsNode *n1;
	AsNode *root;
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
	AsNode *n1;
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
	g_autoptr(GHashTable) hash = NULL;
	g_autoptr(AsNode) root = NULL;

	root = as_node_from_xml (xml, 0, &error);
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
	AsNode *n;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(GString) str = NULL;

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
	AsNode *n1;
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
	g_autoptr(GHashTable) hash = NULL;
	g_autoptr(AsNode) root = NULL;

	root = as_node_from_xml (xml, 0, &error);
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
	AsIcon *ic;
	GList *list;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(AsApp) donor = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(AsIcon) icon2 = NULL;
	g_autoptr(AsScreenshot) ss = NULL;

	donor = as_app_new ();
	icon = as_icon_new ();
	as_icon_set_name (icon, "some-custom-icon");
	as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
	as_app_add_icon (donor, icon);
	icon2 = as_icon_new ();
	as_icon_set_name (icon2, "gtk-find");
	as_icon_set_kind (icon2, AS_ICON_KIND_STOCK);
	as_app_add_icon (donor, icon2);
	as_app_set_state (donor, AS_APP_STATE_INSTALLED);
	as_app_add_pkgname (donor, "hal");
	as_app_add_language (donor, -1, "en_GB");
	as_app_add_metadata (donor, "donor", "true");
	as_app_add_metadata (donor, "overwrite", "1111");
	as_app_add_keyword (donor, "C", "klass");
	as_app_add_keyword (donor, "pl", "klaski");
	ss = as_screenshot_new ();
	as_app_add_screenshot (donor, ss);

	/* copy all useful properties */
	app = as_app_new ();
	as_app_add_metadata (app, "overwrite", "2222");
	as_app_add_metadata (app, "recipient", "true");
	as_app_subsume_full (app, donor,
			     AS_APP_SUBSUME_FLAG_NO_OVERWRITE |
			     AS_APP_SUBSUME_FLAG_DEDUPE);
	as_app_add_screenshot (app, ss);

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

	/* check icon */
	g_assert_cmpint (as_app_get_icons(app)->len, ==, 2);
	ic = as_app_get_icon_default (app);
	g_assert (ic != NULL);
	g_assert_cmpstr (as_icon_get_name (ic), ==, "gtk-find");
	g_assert_cmpint (as_icon_get_kind (ic), ==, AS_ICON_KIND_STOCK);
	g_assert_cmpint (as_icon_get_width (ic), ==, 0);
	g_assert_cmpint (as_icon_get_height (ic), ==, 0);

	/* test both ways */
	as_app_subsume_full (app, donor,
			     AS_APP_SUBSUME_FLAG_BOTH_WAYS |
			     AS_APP_SUBSUME_FLAG_METADATA);
	g_assert_cmpstr (as_app_get_metadata_item (app, "donor"), ==, "true");
	g_assert_cmpstr (as_app_get_metadata_item (app, "recipient"), ==, "true");
	g_assert_cmpstr (as_app_get_metadata_item (donor, "donor"), ==, "true");
	g_assert_cmpstr (as_app_get_metadata_item (donor, "recipient"), ==, "true");
	g_assert_cmpint (as_app_get_screenshots(app)->len, ==, 1);
}

static void
as_test_app_screenshot_func (void)
{
	AsScreenshot *ss;
	GPtrArray *screenshots;
	g_autoptr(AsApp) app = as_app_new ();
	g_autoptr(AsScreenshot) ss1 = as_screenshot_new ();
	g_autoptr(AsScreenshot) ss2 = as_screenshot_new ();

	as_screenshot_set_kind (ss1, AS_SCREENSHOT_KIND_DEFAULT);
	as_screenshot_set_caption (ss1, NULL, "bbb");
	as_app_add_screenshot (app, ss1);

	as_screenshot_set_kind (ss2, AS_SCREENSHOT_KIND_NORMAL);
	as_screenshot_set_caption (ss2, NULL, "aaa");
	as_app_add_screenshot (app, ss2);

	screenshots = as_app_get_screenshots (app);
	ss = g_ptr_array_index (screenshots, 0);
	g_assert (ss == ss1);
	g_assert_cmpint (as_screenshot_get_kind (ss), ==, AS_SCREENSHOT_KIND_DEFAULT);
	ss = g_ptr_array_index (screenshots, 1);
	g_assert (ss == ss2);
	g_assert_cmpint (as_screenshot_get_kind (ss), ==, AS_SCREENSHOT_KIND_NORMAL);
}

static void
as_test_app_search_func (void)
{
	const gchar *all[] = { "gnome", "install", "software", NULL };
	const gchar *none[] = { "gnome", "xxx", "software", NULL };
	const gchar *mime[] = { "application/vnd.oasis.opendocument.text", NULL };
	g_auto(GStrv) tokens = NULL;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GHashTable) search_blacklist = NULL;
	g_autoptr(AsStemmer) stemmer = as_stemmer_new ();

	app = as_app_new ();
	as_app_set_stemmer (app, stemmer);
	as_app_set_id (app, "org.gnome.Software.desktop");
	as_app_add_pkgname (app, "gnome-software");
	as_app_set_name (app, NULL, "GNOME Software X-Plane");
	as_app_set_comment (app, NULL, "Install and remove software");
	as_app_add_mimetype (app, "application/vnd.oasis.opendocument.text");
	as_app_add_keyword (app, NULL, "awesome");
	as_app_add_keyword (app, NULL, "c++");
	as_app_add_keyword (app, NULL, "d-feet");

	search_blacklist = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (search_blacklist,
			     (gpointer) "and",
			     GUINT_TO_POINTER (1));
	as_app_set_search_blacklist (app, search_blacklist);

	g_assert_cmpint (as_app_search_matches (app, "software"), ==, 96);
	g_assert_cmpint (as_app_search_matches (app, "soft"), ==, 24);
	g_assert_cmpint (as_app_search_matches (app, "install"), ==, 32);
	g_assert_cmpint (as_app_search_matches (app, "awesome"), ==, 128);
	g_assert_cmpint (as_app_search_matches (app, "c++"), ==, 128);
	g_assert_cmpint (as_app_search_matches (app, "d-feet"), ==, 128);
	g_assert_cmpint (as_app_search_matches_all (app, (gchar**) all), ==, 96);
	g_assert_cmpint (as_app_search_matches_all (app, (gchar**) none), ==, 0);
	g_assert_cmpint (as_app_search_matches_all (app, (gchar**) mime), ==, 4);

	/* test searching for all tokenized tokens */
	tokens = as_utils_search_tokenize ("org.gnome.Software");
	g_assert_cmpstr (tokens[0], ==, "org.gnome.software");
	g_assert_cmpstr (tokens[1], ==, NULL);
	g_assert_cmpint (as_app_search_matches_all (app, tokens), ==, 256);

	/* test tokenization of hyphenated name */
	g_assert_cmpint (as_app_search_matches (app, "x-plane"), ==, 64);
	g_assert_cmpint (as_app_search_matches (app, "plane"), ==, 64);

	/* do not add short or common keywords */
	g_assert_cmpint (as_app_search_matches (app, "and"), ==, 0);
}

/* load and save embedded icons */
static void
as_test_store_embedded_func (void)
{
	AsApp *app;
	AsIcon *icon;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GString) xml = NULL;
	const gchar *xml_src =
"<components origin=\"origin\" version=\"0.6\">"
"<component type=\"desktop\">"
"<id>eog.desktop</id>"
"<pkgname>eog</pkgname>"
"<name>Image Viewer</name>"
"<icon type=\"embedded\" height=\"32\" width=\"32\">"
"<name>eog.png</name>"
"<filecontent>\n"
"iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAABmJLR0QA/wD/AP+gvaeTAAAAB3RJ\n"
"TUUH1gsaCxQZBldDDAAACLxJREFUWIW9lmtsHNUVx/8zd3Zm9uFd73ptZ/3Gid+OoUlwyAscSJw4\n"
"tIEKCGCQUPuBIlUIhbbEwIfuh0oRUYtKUEEIVQIJSpomPJKACYKQENNg7BiDE8dJnDi7drzrxz5m\n"
"d3a9O3Nnbj8YaOo6qSFSj3Q0V3Nnzv93z9x7znD4kbZzZ4dbM8QWSbBsAoc2XdeLJFH8OJ2m9/j9\n"
"/vRC4wgLfdDv9zsIobcKgqWVF8idAhHKljU20aol1daCggJOFCUcP3709u7uE88CePa6AZ5/frs1\n"
"lbKvAi+0ihbxpzyPqsaGFXp1dY2tsHARJ8syKKWiruvQdQpKDSxf3iz29Pa0/xAA7rvBK688apmY\n"
"KGwmhGwURHErGGtoaGjUa2vqrIsW+Xir1QpKDVCqg1INuk6vCMNgmgxOZy5eevnFbEJJVfr9/vEF\n"
"ZcDv91fabMIrcQVrG5fWmA31jXJxcQlvs9lAqSF+JxaPxwAwMPbvl1NpFUpCQSw+CSWRwrIbb8aN\n"
"TU3m5593tQJ4bUEAVru4b9u2B28qKy3nDGN2hbquIR6PgX2vNiucyWagJOKIK9NQEgnwAoMgJsCL\n"
"Scg5NoTCY7ihcom192TPPQsGoLpWU1ZaziUScRiG8R+Tmp5FXFEQT0SgKHGAmaCaBqaZ4OUoBi8M\n"
"YvCby5gIq8ikDciyFdVV1Uil1Na2trb8zs7Oqf8JIFgs/el0ajXH8aA0i0QyjpgShZKIgeoUpm4A\n"
"1AAhFAwzWFzajO7+Xrz9eidWr1qN9m13o7ysHA6HA6qqIhAM4Msve8Tg6Fjg9g0tuySLdWdnZ2f2\n"
"agCk5bY1zqKikjvcbjfp+uIYdKrA4UzDV8QhEkhh1eoNWPqT5XC5FqFz7xF83H0MqVQKT+/oAC/I\n"
"6Ds1gk9OHkXXmc/x1UAYmmZBbVUl2u+/zzIdibSMBC7dUVpbeiA4HJy3NvCUJx/2f91HRVGCy5UD\n"
"XzGPgkJAsKhIJROwOexIj53AzGfbMTxyBDdUlGPbvfdi7579EJ1leLj9fjze/hhEyxREWwRTioLR\n"
"uIAXXjsY3/qzreamjRtXCTo52NbWJs0L4H/GPzQ6GkwzMHhyvVBiJpRoCn2fKpgcTaJ7910IvfdL\n"
"HB4ahc23FCubm3Hi3V3YuNyHG4sBqps4/OFHICQMrzeNbGoKlaUFiMUVe8dfPn1h2bLlRm1t7cqM\n"
"ln5mXgAAMBn7YGpyAnmeAsTjJoa+pLh1wzY8+rtfw5Xph5Ar4mCPiDs3b0H/P/+OW9dvxqI8J47v\n"
"2op3//oq0lNhWKRJ2B0RuOwmBGRQfUOpoWtJ/uV9PW9sWH8HCBF+09LS4p0XQNP1d86eOzuT68oF\n"
"pYAgisj15IIXZNjK1uPQZyZqapsQHDmHClmD21WAvjd+j4r6tXhsx5PY8vO74c2sh6bH4HAlEY+M\n"
"4aal1VKhzWj6OiR0XQiMRevr6uwgeGheAEnIHhkY6CeECHDluEDsFO/v24vXX3wJB4cbMcSWoqKi\n"
"AuGRYdg8DbjwzVe47NgIx+0dIISDr6QIMnFDFGTkejkEg2dRXVnGWZBesf2B5iWnR+K9xSUl4MC2\n"
"zgvQ0fGcks1qQ6mUijxPPiwOAkflIARbBr/a8QTGYxnIshXBVCGK1z4MX8ujcC6ux7Gut3DondfR\n"
"dfwAbMUJmGoRIpclTE7E4HLYUFNVITYt9qw8P8EGRNECgGuYC/B9MzKovj84GqgvKS4Vhi+JYFYD\n"
"jFogyTISiQQMg0KwyNB1Cosgoq6pCYK9DjwkqIkM5GQ+il0SnPUueHK9IIRH6/p1lsnpqDuWESZ1\n"
"XQeAvKsCUGq8f/rUwFPVVdWCRbBAz4gQigPYv+dVSKIF09PT4J1ZdPd0Y3FZPjwuO0TeDlm2wuuW\n"
"QAgBADDGYDIGMAabLYe/1H/O5+QzBZFIEgAiVwUwTfLV2NioaRizxzEUzYNsNwBJg8frxsTEBDgp\n"
"D26PF+Vl5ZBEAoHnwX3bTxkAZppgAEzTRFY3IYgyhi+OuvPk+NKp6RkA7PS8ewAA/H6/yTgcmZqa\n"
"gMedD6b54OSbUeq9BWtWrcN4KAQzHQMnyNB0A1nNgGEyGObsig2DAeDAgQM4gtSMjoHB8ywYjk/Y\n"
"eWXF9NQ0GLgDV80AAGiZ7L7h4XOtzc23WFfcdDO4b5fnXO/EewcOwJaK4mRfH3JzVsHrsoMaJqyS\n"
"BaJFAGMmpqNRBIJjdGBomI5enuSn4vR8NJY4I1vT9yaTyRQMvHlNANMkHw2eOU1Wr177vTgA5OTk\n"
"YEtbGw59cAhp9SN48grRVL8YIm9CUeJmODSqhcNholMEZij6VM1+9pLquxweu1BeaZt8SlVVAOxP\n"
"R48em54LwM298dxzfzj/yCO/WMLzs1+HEAGEEFBKsePpDoRC47BaraBSsZmb5ws5nTmnrTbHUBau\n"
"s4l0VguEEkYoqhKXNtxSZJ14MDMzwxsmxnjGLZmvK/7XP6Fh0L/1njy5Y+2adZKqJhEKBdiFi8Pq\n"
"xYsXpSJf/sj4+LhDTaWLHRjnI8GQ7RJ1mHGWl8kwryhz0+W5XKRpsaCulKzMrabSAPixhrqyktLx\n"
"VzOb20mXoRt3PfkPRK+agd27H5cymYI9OjU3CYQfN0z2vka1w+mkdnzXrl3JtrY2KavPPA1wv5Uk\n"
"yS5KIgQigOMAxgBqUGhZDdlsNgWwP0oW685Wz5FYfX2NdSZjaoGLZ6IGjNYn38TAvAALtU2bNnk0\n"
"qj2A2fLaiNkiEwFwCuAOiIK45/Dhw1EAeKFdOLvIa6uorLtZVNQ0G/ymV2VU3/LEW+j60QA/xHbf\n"
"h3wmksFKn8NbWN6IGUPA170nUpRqbf8XAAD48wNYyRHyyZIim91b0gCNy0HvF0dAriMmd4XzVziZ\n"
"4wIA8uEphNdV8X1qRr9LZnHRoFlElMTla2VgrgB3Fb/W3Nw42L6ZrClzs7d5ngtrmvHQQgEWInYt\n"
"xxVXYLZ16ADU690D3JzxXLG581caBWBep/71278AZpn8hFce4VcAAAAASUVORK5CYII=\n"
"</filecontent>"
"</icon>"
"<launchable type=\"desktop-id\">eog.desktop</launchable>"
"</component>"
"</components>";

	/* load AppStream file with embedded icon */
	store = as_store_new ();
	as_store_set_origin (store, "origin");
	ret = as_store_from_xml (store, xml_src, "/tmp/origin", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check the icon was parsed */
	g_assert_cmpint (as_store_get_size (store), ==, 1);
	app = as_store_get_app_by_id (store, "eog.desktop");
	g_assert (app != NULL);
	g_assert_cmpint (as_app_get_kind (app), ==, AS_APP_KIND_DESKTOP);
	icon = as_app_get_icon_default (app);
	g_assert (icon != NULL);
	g_assert_cmpint (as_icon_get_kind (icon), ==, AS_ICON_KIND_EMBEDDED);
	g_assert_cmpstr (as_icon_get_name (icon), ==, "eog.png");
	g_assert_cmpstr (as_icon_get_prefix (icon), ==, "/tmp/origin/icons");

	/* convert back to a file */
	xml = as_store_to_xml (store, AS_NODE_TO_XML_FLAG_NONE);
	ret = as_test_compare_lines (xml->str, xml_src, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* strip out the embedded icons */
	ret = as_store_convert_icons (store, AS_ICON_KIND_CACHED, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check exists */
	g_assert (g_file_test ("/tmp/origin/icons/32x32/eog.png", G_FILE_TEST_EXISTS));
}

static void
store_changed_cb (AsStore *store, guint *cnt)
{
	as_test_loop_quit ();
	(*cnt)++;
	g_debug ("changed callback, now #%u", *cnt);
}

static void
store_app_changed_cb (AsStore *store, AsApp *app, guint *cnt)
{
	(*cnt)++;
}

/* automatically reload changed directories */
static void
as_test_store_auto_reload_dir_func (void)
{
	AsApp *app;
	gboolean ret;
	guint cnt = 0;
	guint cnt_added = 0;
	guint cnt_removed = 0;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsStore) store = NULL;

	/* add this file to a store */
	store = as_store_new ();
	g_signal_connect (store, "changed",
			  G_CALLBACK (store_changed_cb), &cnt);
	g_signal_connect (store, "app-added",
			  G_CALLBACK (store_app_changed_cb), &cnt_added);
	g_signal_connect (store, "app-removed",
			  G_CALLBACK (store_app_changed_cb), &cnt_removed);
	as_store_set_watch_flags (store, AS_STORE_WATCH_FLAG_ADDED |
					   AS_STORE_WATCH_FLAG_REMOVED);

	as_store_set_destdir (store, "/tmp/repo-tmp");
	g_mkdir_with_parents ("/tmp/repo-tmp/usr/share/app-info/xmls", 0700);
	g_unlink ("/tmp/repo-tmp/usr/share/app-info/xmls/foo.xml");

	/* load store */
	ret = as_store_load (store, AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (cnt, ==, 1);
	g_assert_cmpint (cnt_added, ==, 0);
	g_assert_cmpint (cnt_removed, ==, 0);

	/* create file */
	ret = g_file_set_contents ("/tmp/repo-tmp/usr/share/app-info/xmls/foo.xml",
				   "<components version=\"0.6\">"
				   "<component type=\"desktop\">"
				   "<id>test.desktop</id>"
				   "</component>"
				   "</components>",
				   -1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	as_test_loop_run_with_timeout (2000);
	g_assert_cmpint (cnt, ==, 2);
	g_assert_cmpint (cnt_added, ==, 1);
	g_assert_cmpint (cnt_removed, ==, 0);

	/* verify */
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (app != NULL);

	/* remove file */
	g_unlink ("/tmp/repo-tmp/usr/share/app-info/xmls/foo.xml");
	as_test_loop_run_with_timeout (2000);
	g_assert_cmpint (cnt, ==, 3);
	g_assert_cmpint (cnt_added, ==, 1);
	g_assert_cmpint (cnt_removed, ==, 1);
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (app == NULL);
}

/* automatically reload changed files */
static void
as_test_store_auto_reload_file_func (void)
{
	AsApp *app;
	AsFormat *format;
	AsRelease *rel;
	gboolean ret;
	guint cnt = 0;
	guint cnt_added = 0;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;

	/* set initial file */
	ret = g_file_set_contents ("/tmp/foo.xml",
				   "<components version=\"0.6\">"
				   "<component type=\"desktop\">"
				   "<id>test.desktop</id>"
				   "<releases>"
				   "<release version=\"0.1.2\" timestamp=\"123\">"
				   "</release>"
				   "</releases>"
				   "</component>"
				   "</components>",
				   -1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add this file to a store */
	store = as_store_new ();
	g_signal_connect (store, "changed",
			  G_CALLBACK (store_changed_cb), &cnt);
	g_signal_connect (store, "app-added",
			  G_CALLBACK (store_app_changed_cb), &cnt_added);
	g_signal_connect (store, "app-removed",
			  G_CALLBACK (store_app_changed_cb), &cnt_added);
	as_store_set_watch_flags (store, AS_STORE_WATCH_FLAG_ADDED |
					   AS_STORE_WATCH_FLAG_REMOVED);
	file = g_file_new_for_path ("/tmp/foo.xml");
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (cnt, ==, 1);

	/* verify */
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (app != NULL);
	rel = as_app_get_release_default (app);
	g_assert_cmpstr (as_release_get_version (rel), ==, "0.1.2");

	/* check format */
	format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPSTREAM);
	g_assert (format != NULL);
	g_assert_cmpstr (as_format_get_filename (format), ==, "/tmp/foo.xml");

	/* change the file, and ensure we get the callback */
	g_debug ("changing file");
	ret = g_file_set_contents ("/tmp/foo.xml",
				   "<components version=\"0.6\">"
				   "<component type=\"desktop\">"
				   "<id>test.desktop</id>"
				   "<releases>"
				   "<release version=\"0.1.0\" timestamp=\"100\">"
				   "</release>"
				   "</releases>"
				   "</component>"
				   "<component type=\"desktop\">"
				   "<id>baz.desktop</id>"
				   "</component>"
				   "</components>",
				   -1, &error);
	g_assert_no_error (error);
	g_assert (ret);
	as_test_loop_run_with_timeout (2000);
	g_assert_cmpint (cnt, ==, 2);

	/* verify */
	app = as_store_get_app_by_id (store, "baz.desktop");
	g_assert (app != NULL);
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (app != NULL);
	rel = as_app_get_release_default (app);
	g_assert_cmpstr (as_release_get_version (rel), ==, "0.1.0");

	/* remove file */
	g_unlink ("/tmp/foo.xml");
	as_test_loop_run_with_timeout (2000);
	g_assert_cmpint (cnt, ==, 3);
	app = as_store_get_app_by_id (store, "baz.desktop");
	g_assert (app == NULL);
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (app == NULL);
}

/* get an application from the store ignoring the prefix */
static void
as_test_store_prefix_func (void)
{
	g_autoptr (AsStore) store = as_store_new ();
	g_autoptr (AsApp) app = as_app_new ();
	g_autoptr (GPtrArray) apps = NULL;
	AsApp *app_tmp;

	/* add app */
	as_app_set_id (app, "flatpak-user:org.gnome.Software.desktop");
	as_store_add_app (store, app);

	app_tmp = as_store_get_app_by_id (store, "org.gnome.Software.desktop");
	g_assert (app_tmp == NULL);
	app_tmp = as_store_get_app_by_id_ignore_prefix (store, "org.gnome.Software.desktop");
	g_assert (app_tmp != NULL);
	g_assert_cmpstr (as_app_get_id (app_tmp), ==,
			 "flatpak-user:org.gnome.Software.desktop");

	/* there might be multiple apps we want to get */
	apps = as_store_get_apps_by_id (store, "flatpak-user:org.gnome.Software.desktop");
	g_assert (apps != NULL);
	g_assert_cmpint (apps->len, ==, 1);
	app_tmp = g_ptr_array_index (apps, 0);
	g_assert_cmpstr (as_app_get_id (app_tmp), ==,
			 "flatpak-user:org.gnome.Software.desktop");

	/* exact unique match */
	app_tmp = as_store_get_app_by_unique_id (store, "*/*/*/*/test/*",
						 AS_STORE_SEARCH_FLAG_NONE);
	g_assert (app_tmp == NULL);
	app_tmp = as_store_get_app_by_unique_id (store, "*/*/*/*/test/*",
						 AS_STORE_SEARCH_FLAG_USE_WILDCARDS);
	g_assert (app_tmp == NULL);
	app_tmp = as_store_get_app_by_unique_id (store, "*/*/*/*/org.gnome.Software.desktop/*",
						 AS_STORE_SEARCH_FLAG_NONE);
	g_assert (app_tmp != NULL);
	app_tmp = as_store_get_app_by_unique_id (store, "*/*/*/*/org.gnome.Software.desktop/*",
						 AS_STORE_SEARCH_FLAG_USE_WILDCARDS);
	g_assert (app_tmp != NULL);
	app_tmp = as_store_get_app_by_unique_id (store, "*/*/*/*/*/*",
						 AS_STORE_SEARCH_FLAG_USE_WILDCARDS);
	g_assert (app_tmp != NULL);
}

static void
as_test_store_wildcard_func (void)
{
	AsApp *app_tmp;
	g_autoptr (AsApp) app1 = as_app_new ();
	g_autoptr (AsApp) app2 = as_app_new ();
	g_autoptr (AsStore) store = as_store_new ();

	/* package from fedora */
	as_app_set_id (app1, "gimp.desktop");
	as_app_set_origin (app1, "fedora");
	as_app_add_pkgname (app1, "polari");
	_as_app_add_format_kind (app1, AS_FORMAT_KIND_DESKTOP);
	as_store_add_app (store, app1);

	/* package from updates */
	as_app_set_id (app2, "gimp.desktop");
	as_app_set_origin (app2, "updates");
	as_app_add_pkgname (app2, "polari");
	_as_app_add_format_kind (app2, AS_FORMAT_KIND_DESKTOP);
	as_store_add_app (store, app2);

	/* check negative match */
	app_tmp = as_store_get_app_by_unique_id (store, "*/*/xxx/*/gimp.desktop/*",
						 AS_STORE_SEARCH_FLAG_USE_WILDCARDS);
	g_assert (app_tmp == NULL);
	app_tmp = as_store_get_app_by_unique_id (store, "*/snap/*/*/gimp.desktop/*",
						 AS_STORE_SEARCH_FLAG_USE_WILDCARDS);
	g_assert (app_tmp == NULL);
}

/* load a store with a origin and scope encoded in the symlink name */
static void
as_test_store_flatpak_func (void)
{
	AsApp *app;
	AsFormat *format;
	GError *error = NULL;
	GPtrArray *apps;
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autofree gchar *filename_root = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;

	/* make throws us under a bus, yet again */
	g_setenv ("AS_SELF_TEST_PREFIX_DELIM", "_", TRUE);

	/* load a symlinked file to the store */
	store = as_store_new ();
	filename_root = as_test_get_filename (".");
	filename = g_build_filename (filename_root, "flatpak_remote-name.xml", NULL);
	if (!g_file_test (filename, G_FILE_TEST_IS_SYMLINK)) {
		g_debug ("not doing symlink test in distcheck as regular file");
		return;
	}
	file = g_file_new_for_path (filename);
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test extraction of symlink data */
	g_assert_cmpstr (as_store_get_origin (store), ==, "flatpak");
	g_assert_cmpint (as_store_get_size (store), ==, 1);
	apps = as_store_get_apps (store);
	g_assert_cmpint (apps->len, ==, 1);
	app = g_ptr_array_index (apps, 0);
	g_assert_cmpstr (as_app_get_id (app), ==, "flatpak:test.desktop");
	g_assert_cmpstr (as_app_get_unique_id (app), ==, "system/flatpak/remote-name/desktop/test.desktop/master");
	g_assert_cmpstr (as_app_get_id_filename (app), ==, "test");
	g_assert_cmpstr (as_app_get_origin (app), ==, "remote-name");

	/* check format */
	format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPSTREAM);
	g_assert (format != NULL);
	g_assert_cmpstr (as_format_get_filename (format), ==, filename);

	/* back to normality */
	g_unsetenv ("AS_SELF_TEST_PREFIX_DELIM");
}

/* demote the .desktop "application" to an addon */
static void
as_test_store_demote_func (void)
{
	AsApp *app;
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *filename1 = NULL;
	g_autofree gchar *filename2 = NULL;
	g_autoptr(AsApp) app_appdata = NULL;
	g_autoptr(AsApp) app_desktop = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GString) xml = NULL;

	/* load example desktop file */
	app_desktop = as_app_new ();
	filename1 = as_test_get_filename ("example.desktop");
	ret = as_app_parse_file (app_desktop, filename1,
				 AS_APP_PARSE_FLAG_ALLOW_VETO, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_app_get_kind (app_desktop), ==, AS_APP_KIND_DESKTOP);

	/* load example appdata file */
	app_appdata = as_app_new ();
	filename2 = as_test_get_filename ("example.appdata.xml");
	ret = as_app_parse_file (app_appdata, filename2,
				 AS_APP_PARSE_FLAG_ALLOW_VETO, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_app_get_kind (app_appdata), ==, AS_APP_KIND_ADDON);

	/* add apps */
	store = as_store_new ();
	as_store_set_api_version (store, 0.8);
	as_store_add_app (store, app_desktop);
	as_store_add_app (store, app_appdata);

	/* check we demoted */
	g_assert_cmpint (as_store_get_size (store), ==, 1);
	app = as_store_get_app_by_id (store, "example.desktop");
	g_assert (app != NULL);
	g_assert_cmpint (as_app_get_kind (app), ==, AS_APP_KIND_ADDON);
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
	g_autoptr(AsApp) app_appdata = NULL;
	g_autoptr(AsApp) app_appinfo = NULL;
	g_autoptr(AsApp) app_desktop = NULL;
	g_autoptr(AsStore) store_all = NULL;
	g_autoptr(AsStore) store_desktop_appdata = NULL;

	/* test desktop + appdata */
	store_desktop_appdata = as_store_new ();

	app_desktop = as_app_new ();
	as_app_set_id (app_desktop, "gimp.desktop");
	_as_app_add_format_kind (app_desktop, AS_FORMAT_KIND_DESKTOP);
	as_app_set_name (app_desktop, NULL, "GIMP");
	as_app_set_comment (app_desktop, NULL, "GNU Bla Bla");
	as_app_set_priority (app_desktop, -1);
	as_app_set_state (app_desktop, AS_APP_STATE_INSTALLED);
	as_app_set_scope (app_desktop, AS_APP_SCOPE_SYSTEM);

	app_appdata = as_app_new ();
	as_app_set_id (app_appdata, "gimp.desktop");
	_as_app_add_format_kind (app_appdata, AS_FORMAT_KIND_APPDATA);
	as_app_set_description (app_appdata, NULL, "<p>Gimp is awesome</p>");
	as_app_add_pkgname (app_appdata, "gimp");
	as_app_set_priority (app_appdata, -1);
	as_app_set_state (app_appdata, AS_APP_STATE_INSTALLED);
	as_app_set_scope (app_desktop, AS_APP_SCOPE_SYSTEM);

	as_store_add_app (store_desktop_appdata, app_desktop);
	as_store_add_app (store_desktop_appdata, app_appdata);

	app_tmp = as_store_get_app_by_id (store_desktop_appdata, "gimp.desktop");
	g_assert (app_tmp != NULL);
	g_assert_cmpstr (as_app_get_name (app_tmp, NULL), ==, "GIMP");
	g_assert_cmpstr (as_app_get_comment (app_tmp, NULL), ==, "GNU Bla Bla");
	g_assert_cmpstr (as_app_get_description (app_tmp, NULL), ==, "<p>Gimp is awesome</p>");
	g_assert_cmpstr (as_app_get_pkgname_default (app_tmp), ==, "gimp");
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_DESKTOP) != NULL);
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_APPDATA) != NULL);
	g_assert_cmpint (as_app_get_state (app_tmp), ==, AS_APP_STATE_INSTALLED);

	/* test desktop + appdata + appstream */
	store_all = as_store_new ();

	app_appinfo = as_app_new ();
	as_app_set_id (app_appinfo, "gimp.desktop");
	_as_app_add_format_kind (app_appinfo, AS_FORMAT_KIND_APPSTREAM);
	as_app_set_name (app_appinfo, NULL, "GIMP");
	as_app_set_comment (app_appinfo, NULL, "GNU Bla Bla");
	as_app_set_description (app_appinfo, NULL, "<p>Gimp is Distro</p>");
	as_app_add_pkgname (app_appinfo, "gimp");
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
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_DESKTOP) != NULL);
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_APPDATA) != NULL);
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_APPSTREAM) != NULL);
	g_assert_cmpint (as_app_get_formats(app_tmp)->len, ==, 3);
	g_assert_cmpint (as_app_get_state (app_tmp), ==, AS_APP_STATE_INSTALLED);
}

static void
as_test_store_merges_local_func (void)
{
	AsApp *app_tmp;
	g_autoptr(AsApp) app_appdata = NULL;
	g_autoptr(AsApp) app_appinfo = NULL;
	g_autoptr(AsApp) app_desktop = NULL;
	g_autoptr(AsStore) store = NULL;

	/* test desktop + appdata + appstream */
	store = as_store_new ();
	as_store_set_add_flags (store, AS_STORE_ADD_FLAG_PREFER_LOCAL);

	app_desktop = as_app_new ();
	as_app_set_id (app_desktop, "gimp.desktop");
	_as_app_add_format_kind (app_desktop, AS_FORMAT_KIND_DESKTOP);
	as_app_set_name (app_desktop, NULL, "GIMP");
	as_app_set_comment (app_desktop, NULL, "GNU Bla Bla");
	as_app_set_priority (app_desktop, -1);
	as_app_set_state (app_desktop, AS_APP_STATE_INSTALLED);

	app_appdata = as_app_new ();
	as_app_set_id (app_appdata, "gimp.desktop");
	_as_app_add_format_kind (app_appdata, AS_FORMAT_KIND_APPDATA);
	as_app_set_description (app_appdata, NULL, "<p>Gimp is awesome</p>");
	as_app_add_pkgname (app_appdata, "gimp");
	as_app_set_priority (app_appdata, -1);
	as_app_set_state (app_appdata, AS_APP_STATE_INSTALLED);

	app_appinfo = as_app_new ();
	as_app_set_id (app_appinfo, "gimp.desktop");
	_as_app_add_format_kind (app_appinfo, AS_FORMAT_KIND_APPSTREAM);
	as_app_set_name (app_appinfo, NULL, "GIMP");
	as_app_set_comment (app_appinfo, NULL, "Fedora GNU Bla Bla");
	as_app_set_description (app_appinfo, NULL, "<p>Gimp is Distro</p>");
	as_app_add_pkgname (app_appinfo, "gimp");
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
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_DESKTOP) != NULL);
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_APPDATA) != NULL);
	g_assert (as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_APPSTREAM) != NULL);
	g_assert_cmpint (as_app_get_formats(app_tmp)->len, ==, 3);
	g_assert_cmpint (as_app_get_state (app_tmp), ==, AS_APP_STATE_INSTALLED);
}

static void
as_test_store_cab_func (void)
{
#ifdef HAVE_GCAB
	gboolean ret;
	const gchar *src;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;
	g_autofree gchar *fn = NULL;
	g_autoptr(GString) xml = NULL;

	/* parse a .cab file as a store */
	store = as_store_new ();
	as_store_set_api_version (store, 0.9);
	fn = as_test_get_filename ("colorhug-als-2.0.2.cab");
	g_assert (fn != NULL);
	file = g_file_new_for_path (fn);
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check output */
	src =
	"<components origin=\"colorhug-als-2.0.2.cab\" version=\"0.9\">\n"
	"<component type=\"firmware\">\n"
	"<id>com.hughski.ColorHug2.firmware</id>\n"
	"<name>ColorHug Firmware</name>\n"
	"<summary>Firmware for the ColorHug Colorimeter</summary>\n"
	"<developer_name>Hughski Limited</developer_name>\n"
	"<description><p>Updating the firmware on your ColorHug device "
	"improves performance and adds new features.</p></description>\n"
	"<project_license>GPL-2.0+</project_license>\n"
	"<url type=\"homepage\">http://www.hughski.com/</url>\n"
	"<releases>\n"
	"<release timestamp=\"1424116753\" version=\"2.0.2\">\n"
	"<location>http://www.hughski.com/downloads/colorhug2/firmware/colorhug-2.0.2.cab</location>\n"
	"<checksum type=\"sha1\" filename=\"colorhug-als-2.0.2.cab\" target=\"container\">" AS_TEST_WILDCARD_SHA1 "</checksum>\n"
	"<checksum type=\"sha1\" filename=\"firmware.bin\" target=\"content\">" AS_TEST_WILDCARD_SHA1 "</checksum>\n"
	"<description><p>This unstable release adds the following features:</p>"
	"<ul><li>Add TakeReadingArray to enable panel latency measurements</li>"
	"<li>Speed up the auto-scaled measurements considerably, using 256ms as"
	" the smallest sample duration</li></ul></description>\n"
	"<size type=\"installed\">14</size>\n"
	"<size type=\"download\">2015</size>\n"
	"</release>\n"
	"</releases>\n"
	"<provides>\n"
	"<firmware type=\"flashed\">84f40464-9272-4ef7-9399-cd95f12da696</firmware>\n"
	"</provides>\n"
	"</component>\n"
	"</components>\n";
	xml = as_store_to_xml (store, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str, src, &error);
	g_assert_no_error (error);
	g_assert (ret);
#endif
}

static void
as_test_store_empty_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsStore) store = NULL;

	store = as_store_new ();
	ret = as_store_from_xml (store, "", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
}

static void
as_test_store_func (void)
{
	AsApp *app;
	GString *xml;
	gboolean ret;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GError) error = NULL;

	/* create a store and add a single app */
	store = as_store_new ();
	g_assert_cmpfloat (as_store_get_api_version (store), <, 1.f);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.f);
	app = as_app_new ();
	as_app_set_id (app, "gnome-software.desktop");
	as_app_set_kind (app, AS_APP_KIND_DESKTOP);
	as_store_add_app (store, app);
	g_object_unref (app);
	g_assert_cmpstr (as_store_get_origin (store), ==, NULL);

	/* check string output */
	as_store_set_api_version (store, 0.6);
	xml = as_store_to_xml (store, 0);
	ret = as_test_compare_lines (xml->str,
				     "<components version=\"0.6\">"
				     "<component type=\"desktop\">"
				     "<id>gnome-software.desktop</id>"
				     "</component>"
				     "</components>",
				     &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);

	/* add and then remove another app */
	app = as_app_new ();
	as_app_set_id (app, "junk.desktop");
	as_app_set_kind (app, AS_APP_KIND_FONT);
	as_store_add_app (store, app);
	g_object_unref (app);
	as_store_remove_app (store, app);

	/* check string output */
	as_store_set_api_version (store, 0.6);
	xml = as_store_to_xml (store, 0);
	ret = as_test_compare_lines (xml->str,
				     "<components version=\"0.6\">"
				     "<component type=\"desktop\">"
				     "<id>gnome-software.desktop</id>"
				     "</component>"
				     "</components>",
				     &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);

	/* add another app and ensure it's sorted */
	app = as_app_new ();
	as_app_set_id (app, "aaa.desktop");
	as_app_set_kind (app, AS_APP_KIND_FONT);
	as_store_add_app (store, app);
	g_object_unref (app);
	xml = as_store_to_xml (store, 0);
	g_assert_cmpstr (xml->str, ==,
		"<components version=\"0.6\">"
		"<component type=\"font\">"
		"<id>aaa.desktop</id>"
		"</component>"
		"<component type=\"desktop\">"
		"<id>gnome-software.desktop</id>"
		"</component>"
		"</components>");
	g_string_free (xml, TRUE);

	/* empty the store */
	as_store_remove_all (store);
	g_assert_cmpint (as_store_get_size (store), ==, 0);
	g_assert (as_store_get_app_by_id (store, "aaa.desktop") == NULL);
	g_assert (as_store_get_app_by_id (store, "gnome-software.desktop") == NULL);
	xml = as_store_to_xml (store, 0);
	g_assert_cmpstr (xml->str, ==,
		"<components version=\"0.6\"/>");
	g_string_free (xml, TRUE);
}

static void
as_test_store_unique_func (void)
{
	AsApp *app;
	g_autoptr(AsApp) app1 = NULL;
	g_autoptr(AsApp) app2 = NULL;
	g_autoptr(AsApp) app3 = NULL;
	g_autoptr(AsBundle) bundle2 = NULL;
	g_autoptr(AsBundle) bundle3 = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GPtrArray) apps = NULL;

	/* create a store and add a single app */
	store = as_store_new ();
	as_store_set_add_flags (store, AS_STORE_ADD_FLAG_USE_UNIQUE_ID);
	app1 = as_app_new ();
	as_app_set_id (app1, "org.gnome.Software.desktop");
	as_app_set_kind (app1, AS_APP_KIND_DESKTOP);
	as_app_add_pkgname (app1, "gnome-software");
	as_store_add_app (store, app1);

	/* add a stable bundle */
	app2 = as_app_new ();
	bundle2 = as_bundle_new ();
	as_bundle_set_kind (bundle2, AS_BUNDLE_KIND_FLATPAK);
	as_bundle_set_id (bundle2, "app/org.gnome.Software/i386/3-18");
	as_app_set_id (app2, "org.gnome.Software.desktop");
	as_app_set_kind (app2, AS_APP_KIND_DESKTOP);
	as_app_add_bundle (app2, bundle2);
	as_store_add_app (store, app2);

	/* add a master bundle */
	app3 = as_app_new ();
	bundle3 = as_bundle_new ();
	as_bundle_set_kind (bundle3, AS_BUNDLE_KIND_FLATPAK);
	as_bundle_set_id (bundle3, "app/org.gnome.Software/i386/master");
	as_app_set_id (app3, "org.gnome.Software.desktop");
	as_app_set_kind (app3, AS_APP_KIND_DESKTOP);
	as_app_add_bundle (app3, bundle3);
	as_store_add_app (store, app3);

	g_assert_cmpint (as_store_get_size (store), ==, 3);
	apps = as_store_get_apps_by_id (store, "org.gnome.Software.desktop");
	g_assert_cmpint (apps->len, ==, 3);
	app = g_ptr_array_index (apps, 0);
	g_assert_cmpstr (as_app_get_unique_id (app), ==,
			 "*/package/*/desktop/org.gnome.Software.desktop/*");
	app = g_ptr_array_index (apps, 1);
	g_assert_cmpstr (as_app_get_unique_id (app), ==,
			 "*/flatpak/*/desktop/org.gnome.Software.desktop/3-18");
	app = g_ptr_array_index (apps, 2);
	g_assert_cmpstr (as_app_get_unique_id (app), ==,
			 "*/flatpak/*/desktop/org.gnome.Software.desktop/master");
	app = as_store_get_app_by_unique_id (store,
					     "*/flatpak/*/desktop/"
					     "org.gnome.Software.desktop/master",
					     AS_STORE_SEARCH_FLAG_NONE);
	g_assert (app != NULL);
}

static void
as_test_store_provides_func (void)
{
	AsApp *app;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GPtrArray) apps1 = NULL;
	g_autoptr(GPtrArray) apps2 = NULL;

	/* create a store and add a single app */
	store = as_store_new ();
	ret = as_store_from_xml (store,
		"<components version=\"0.6\">"
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"<provides>"
		"<firmware type=\"flashed\">deadbeef</firmware>"
		"</provides>"
		"</component>"
		"</components>", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get an appication by the provide value */
	app = as_store_get_app_by_provide (store,
					   AS_PROVIDE_KIND_FIRMWARE_FLASHED,
					   "deadbeef");
	g_assert_cmpstr (as_app_get_id (app), ==, "test.desktop");
	app = as_store_get_app_by_provide (store,
					   AS_PROVIDE_KIND_FIRMWARE_RUNTIME,
					   "deadbeef");
	g_assert (app == NULL);
	app = as_store_get_app_by_provide (store,
					   AS_PROVIDE_KIND_FIRMWARE_FLASHED,
					   "beefdead");
	g_assert (app == NULL);

	/* arrays of apps */
	apps1 = as_store_get_apps_by_provide (store,
					      AS_PROVIDE_KIND_FIRMWARE_FLASHED,
					      "deadbeef");
	g_assert_cmpint (apps1->len, ==, 1);
	app = g_ptr_array_index (apps1, 0);
	g_assert_cmpstr (as_app_get_id (app), ==, "test.desktop");
	apps2 = as_store_get_apps_by_provide (store,
					      AS_PROVIDE_KIND_FIRMWARE_FLASHED,
					      "beefdead");
	g_assert_cmpint (apps2->len, ==, 0);
}

static void
as_test_store_versions_func (void)
{
	AsApp *app;
	AsStore *store;
	GError *error = NULL;
	gboolean ret;
	GString *xml;

	/* load a file to the store */
	store = as_store_new ();
	ret = as_store_from_xml (store,
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
		"</components>", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpfloat (as_store_get_api_version (store), <, 0.6 + 0.01);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.6 - 0.01);

	/* verify source kind */
	app = as_store_get_app_by_id (store, "test.desktop");
	g_assert (as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPSTREAM) != NULL);

	/* test with latest features */
	as_store_set_api_version (store, 0.6);
	g_assert_cmpfloat (as_store_get_api_version (store), <, 0.6 + 0.01);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.6 - 0.01);
	xml = as_store_to_xml (store, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (xml->str,
		"<components version=\"0.6\">\n"
		"<component type=\"desktop\">\n"
		"<id>test.desktop</id>\n"
		"<description><p>Hello world</p></description>\n"
		"<architectures>\n"
		"<arch>i386</arch>\n"
		"</architectures>\n"
		"<releases>\n"
		"<release timestamp=\"123\" version=\"0.1.2\">\n"
		"<description><p>Hello</p></description>\n"
		"</release>\n"
		"</releases>\n"
		"<launchable type=\"desktop-id\">test.desktop</launchable>\n"
		"</component>\n"
		"</components>\n", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (xml, TRUE);
	g_object_unref (store);

	/* load a version 0.6 file to the store */
	store = as_store_new ();
	ret = as_store_from_xml (store,
		"<components version=\"0.6\">"
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"</component>"
		"</components>", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test latest spec version */
	xml = as_store_to_xml (store, 0);
	g_assert_cmpstr (xml->str, ==,
		"<components version=\"0.6\">"
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"<launchable type=\"desktop-id\">test.desktop</launchable>"
		"</component>"
		"</components>");
	g_string_free (xml, TRUE);

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
		"<launchable type=\"desktop-id\">eclipse.desktop</launchable>"
		"</component>"
		"</components>";
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GString) str = NULL;

	/* load a file to the store */
	store = as_store_new ();
	ret = as_store_from_xml (store, xml, NULL, &error);
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
	g_assert_cmpint (as_app_search_matches (app, "eclipse-php"), >, 0);

	/* check it marshals back to the same XML */
	str = as_store_to_xml (store, 0);
	ret = as_test_compare_lines (str->str, xml, &error);
	g_assert_no_error (error);
	g_assert (ret);
}

/*
 * test that we don't save the same translated data as C back to the file
 */
static void
as_test_node_no_dup_c_func (void)
{
	GError *error = NULL;
	AsNode *n;
	AsNode *root;
	GString *xml;
	gboolean ret;
	const gchar *src =
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"<name>Krita</name>"
		"<name xml:lang=\"pl\">Krita</name>"
		"</component>";
	g_autoptr(AsApp) app = NULL;
	g_autoptr(AsNodeContext) ctx = NULL;

	/* to object */
	app = as_app_new ();
	root = as_node_from_xml (src, 0, &error);
	g_assert_no_error (error);
	g_assert (root != NULL);
	n = as_node_find (root, "component");
	g_assert (n != NULL);
	ctx = as_node_context_new ();
	ret = as_app_node_parse (app, n, ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify */
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Krita");
	g_assert_cmpstr (as_app_get_name (app, "pl"), ==, "Krita");
	as_node_unref (root);

	/* back to node */
	root = as_node_new ();
	as_node_context_set_version (ctx, 0.4);
	n = as_app_node_insert (app, root, ctx);
	xml = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
	g_assert_cmpstr (xml->str, ==,
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"<name>Krita</name>"
		"<launchable type=\"desktop-id\">test.desktop</launchable>"
		"</component>");
	g_string_free (xml, TRUE);
	as_node_unref (root);
}

static void
as_test_store_origin_func (void)
{
	AsApp *app;
	AsFormat *format;
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;

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
	g_assert_cmpstr (as_app_get_icon_path (app), !=, NULL);
	g_assert (g_str_has_suffix (as_app_get_icon_path (app), "icons"));
	g_assert_cmpstr (as_app_get_origin (app), ==, "fedora-21");

	/* check format */
	format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPSTREAM);
	g_assert (format != NULL);
	g_assert_cmpstr (as_format_get_filename (format), ==, filename);
}

static void
as_test_store_speed_appstream_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint loops = 10;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GTimer) timer = NULL;

	filename = as_test_get_filename ("example-v04.xml.gz");
	file = g_file_new_for_path (filename);
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		g_autoptr(AsStore) store = NULL;
		store = as_store_new ();
		as_store_set_add_flags (store, AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS);
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
as_test_store_speed_search_func (void)
{
	AsApp *app;
	GPtrArray *apps;
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint j;
	guint loops = 1000;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GTimer) timer = NULL;
	g_autoptr(AsStore) store = NULL;

	/* run creating a store and tokenizing */
	filename = as_test_get_filename ("example-v04.xml.gz");
	file = g_file_new_for_path (filename);
	store = as_store_new ();
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* tokenize and search */
	timer = g_timer_new ();
	apps = as_store_get_apps (store);
	for (j = 0; j < apps->len; j++) {
		app = g_ptr_array_index (apps, j);
		as_app_search_matches (app, "xxx");
	}
	g_print ("cold=%.0fms: ", g_timer_elapsed (timer, NULL) * 1000);

	/* search again */
	g_timer_reset (timer);
	for (i = 0; i < loops; i++) {
		for (j = 0; j < apps->len; j++) {
			app = g_ptr_array_index (apps, j);
			as_app_search_matches (app, "xxx");
		}
	}
	g_print ("hot=%.2f ms: ", (g_timer_elapsed (timer, NULL) * 1000) / (gdouble) loops);
}

static void
as_test_store_speed_appdata_func (void)
{
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint loops = 10;
	g_autofree gchar *filename = NULL;
	g_autoptr(GTimer) timer = NULL;

	filename = as_test_get_filename (".");
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		g_autoptr(AsStore) store = NULL;
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
	g_autofree gchar *filename = NULL;
	g_autoptr(GTimer) timer = NULL;

	filename = as_test_get_filename (".");
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		g_autoptr(AsStore) store = NULL;
		store = as_store_new ();
		as_store_set_destdir (store, filename);
		ret = as_store_load (store, AS_STORE_LOAD_FLAG_DESKTOP, NULL, &error);
		g_assert_no_error (error);
		g_assert (ret);
		g_assert_cmpint (as_store_get_apps (store)->len, >, 0);
	}
	g_print ("%.0f ms: ", g_timer_elapsed (timer, NULL) * 1000 / loops);
}

static void
as_test_utils_appstream_id_func (void)
{
	g_autofree gchar *id = NULL;
	g_assert (as_utils_appstream_id_valid ("org.gnome.Software"));
	g_assert (!as_utils_appstream_id_valid ("xml:gravatar@jr.rlabs.io"));
	id = as_utils_appstream_id_build ("gravatar@jr.rlabs.io");
	g_assert_cmpstr (id, ==, "gravatar_jr.rlabs.io");
}

static void
as_test_utils_guid_func (void)
{
	g_autofree gchar *guid1 = NULL;
	g_autofree gchar *guid2 = NULL;

	/* invalid */
	g_assert (!as_utils_guid_is_valid (NULL));
	g_assert (!as_utils_guid_is_valid (""));
	g_assert (!as_utils_guid_is_valid ("1ff60ab2-3905-06a1-b476"));
	g_assert (!as_utils_guid_is_valid ("1ff60ab2-XXXX-XXXX-XXXX-0371f00c9e9b"));
	g_assert (!as_utils_guid_is_valid (" 1ff60ab2-3905-06a1-b476-0371f00c9e9b"));

	/* valid */
	g_assert (as_utils_guid_is_valid ("1ff60ab2-3905-06a1-b476-0371f00c9e9b"));

	/* make valid */
	guid1 = as_utils_guid_from_string ("python.org");
	g_assert_cmpstr (guid1, ==, "886313e1-3b8a-5372-9b90-0c9aee199e5d");
	guid2 = as_utils_guid_from_string ("8086:0406");
	g_assert_cmpstr (guid2, ==, "1fbd1f2c-80f4-5d7c-a6ad-35c7b9bd5486");
}

static void
as_test_utils_icons_func (void)
{
	gchar *tmp;
	GError *error = NULL;
	g_autofree gchar *destdir = NULL;

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

	/* theme name, HiDPI */
	tmp = as_utils_find_icon_filename_full (destdir, "test3",
						AS_UTILS_FIND_ICON_HI_DPI,
						&error);
	g_assert_cmpstr (tmp, !=, NULL);
	g_assert_no_error (error);
	g_free (tmp);

	/* full pixmaps invalid */
	tmp = as_utils_find_icon_filename (destdir, "/usr/share/pixmaps/not-going-to-exist.png", &error);
	g_assert_cmpstr (tmp, ==, NULL);
	g_assert_error (error, AS_UTILS_ERROR, AS_UTILS_ERROR_FAILED);
	g_free (tmp);
	g_clear_error (&error);

	/* all invalid */
	tmp = as_utils_find_icon_filename (destdir, "not-going-to-exist.png", &error);
	g_assert_cmpstr (tmp, ==, NULL);
	g_assert_error (error, AS_UTILS_ERROR, AS_UTILS_ERROR_FAILED);
	g_free (tmp);
	g_clear_error (&error);
}

static void
as_test_utils_spdx_token_func (void)
{
	gchar **tok;
	gchar *tmp;

	/* simple */
	tok = as_utils_spdx_license_tokenize ("LGPL-2.0+");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+");
	g_strfreev (tok);
	g_free (tmp);

	/* empty */
	tok = as_utils_spdx_license_tokenize ("");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "");
	g_strfreev (tok);
	g_free (tmp);

	/* invalid */
	tok = as_utils_spdx_license_tokenize (NULL);
	g_assert (tok == NULL);

	/* random */
	tok = as_utils_spdx_license_tokenize ("Public Domain");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "Public Domain");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences */
	tok = as_utils_spdx_license_tokenize ("LGPL-2.0+ AND GPL-2.0 AND LGPL-3.0");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  @GPL-2.0  &  @LGPL-3.0");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences, using the new style */
	tok = as_utils_spdx_license_tokenize ("LGPL-2.0-or-later AND GPL-2.0-only");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  @GPL-2.0");
	g_strfreev (tok);
	g_free (tmp);

	/* multiple licences, deprectated 'and' & 'or' */
	tok = as_utils_spdx_license_tokenize ("LGPL-2.0+ and GPL-2.0 or LGPL-3.0");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  @GPL-2.0  |  @LGPL-3.0");
	g_strfreev (tok);
	g_free (tmp);

	/* brackets */
	tok = as_utils_spdx_license_tokenize ("LGPL-2.0+ and (GPL-2.0 or GPL-2.0+) and MIT");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@LGPL-2.0+  &  (  @GPL-2.0  |  @GPL-2.0+  )  &  @MIT");
	g_strfreev (tok);
	g_free (tmp);

	/* detokenisation */
	tok = as_utils_spdx_license_tokenize ("LGPLv2+ and (QPL or GPLv2) and MIT");
	tmp = as_utils_spdx_license_detokenize (tok);
	g_assert_cmpstr (tmp, ==, "LGPLv2+ AND (QPL OR GPLv2) AND MIT");
	g_strfreev (tok);
	g_free (tmp);

	/* "+" operator */
	tok = as_utils_spdx_license_tokenize ("CC-BY-SA-3.0+ AND Zlib");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@CC-BY-SA-3.0  +  &  @Zlib");
	g_free (tmp);
	tmp = as_utils_spdx_license_detokenize (tok);
	g_assert_cmpstr (tmp, ==, "CC-BY-SA-3.0+ AND Zlib");
	g_strfreev (tok);
	g_free (tmp);

	/* detokenisation literals */
	tok = as_utils_spdx_license_tokenize ("Public Domain");
	tmp = as_utils_spdx_license_detokenize (tok);
	g_assert_cmpstr (tmp, ==, "Public Domain");
	g_strfreev (tok);
	g_free (tmp);

	/* invalid tokens */
	tmp = as_utils_spdx_license_detokenize (NULL);
	g_assert (tmp == NULL);

	/* leading brackets */
	tok = as_utils_spdx_license_tokenize ("(MPLv1.1 or LGPLv3+) and LGPLv3");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "(  MPLv1.1  |  LGPLv3+  )  &  LGPLv3");
	g_strfreev (tok);
	g_free (tmp);

	/*  trailing brackets */
	tok = as_utils_spdx_license_tokenize ("MPLv1.1 and (LGPLv3 or GPLv3)");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "MPLv1.1  &  (  LGPLv3  |  GPLv3  )");
	g_strfreev (tok);
	g_free (tmp);

	/*  deprecated names */
	tok = as_utils_spdx_license_tokenize ("CC0 and (CC0 or CC0)");
	tmp = g_strjoinv ("  ", tok);
	g_assert_cmpstr (tmp, ==, "@CC0-1.0  &  (  @CC0-1.0  |  @CC0-1.0  )");
	g_strfreev (tok);
	g_free (tmp);

	/* SPDX strings */
	g_assert (as_utils_is_spdx_license ("CC0"));
	g_assert (as_utils_is_spdx_license ("LicenseRef-proprietary"));
	g_assert (as_utils_is_spdx_license ("CC0 and GFDL-1.3"));
	g_assert (as_utils_is_spdx_license ("CC0 AND GFDL-1.3"));
	g_assert (as_utils_is_spdx_license ("CC-BY-SA-3.0+"));
	g_assert (as_utils_is_spdx_license ("CC-BY-SA-3.0+ AND Zlib"));
	g_assert (as_utils_is_spdx_license ("NOASSERTION"));
	g_assert (!as_utils_is_spdx_license ("CC0 dave"));
	g_assert (!as_utils_is_spdx_license (""));
	g_assert (!as_utils_is_spdx_license (NULL));

	/* importing non-SPDX formats */
	tmp = as_utils_license_to_spdx ("CC0 and (Public Domain and GPLv3+ with exceptions)");
	g_assert_cmpstr (tmp, ==, "CC0-1.0 AND (LicenseRef-public-domain AND GPL-3.0+)");
	g_free (tmp);
}

static void
as_test_utils_markup_import_func (void)
{
	guint i;
	struct {
		const gchar *old;
		const gchar *new;
	} table[] = {
		{ "",			NULL },
		{ "dave",		"<p>dave</p>" },
		{ "dave!\ndave?",	"<p>dave! dave?</p>" },
		{ "dave!\n\ndave?",	"<p>dave!</p><p>dave?</p>" },
		{ NULL,			NULL }
	};
	for (i = 0; table[i].old != NULL; i++) {
		g_autofree gchar *new = NULL;
		g_autoptr(GError) error = NULL;
		new = as_markup_import (table[i].old,
					AS_MARKUP_CONVERT_FORMAT_SIMPLE,
					&error);
		g_assert_no_error (error);
		g_assert_cmpstr (new, ==, table[i].new);
	}
}

static void
as_test_utils_func (void)
{
	gboolean ret;
	gchar *tmp;
	gchar **tokens;
	GError *error = NULL;

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

	/* valid description markup */
	tmp = as_markup_convert_simple ("<p>Hello world!</p>", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "Hello world!");
	g_free (tmp);
	tmp = as_markup_convert_simple ("<p>Hello world</p><p></p>"
					"<ul><li>Item</li></ul>",
					&error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "Hello world\n • Item");
	g_free (tmp);

	/* valid description markup */
	tmp = as_markup_convert ("<p>Hello world with a very long line that"
				 " probably needs splitting at least once in"
				 " the right place.</p>"
				 "<ul><li>"
				 "This is an overly long item that needs to be"
				 " broken into multiple lines that only has one"
				 " initial bullet point."
				 "</li></ul>",
				 AS_MARKUP_CONVERT_FORMAT_MARKDOWN, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==,
			 "Hello world with a very long line that probably"
			 " needs splitting at least once\n"
			 "in the right place.\n"
			 " * This is an overly long item that needs to be"
			 " broken into multiple lines that\n"
			 "   only has one initial bullet point.");
	g_free (tmp);

	/* valid description markup */
	tmp = as_markup_convert_simple ("bare text", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "bare text");
	g_free (tmp);

	/* invalid XML */
	tmp = as_markup_convert_simple ("<p>Hello world</dave>", &error);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_assert_cmpstr (tmp, ==, NULL);
	g_clear_error (&error);

	/* validate */
	ret = as_markup_validate ("<p>hello</p>", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = as_markup_validate ("<ol><li>hello</ol>", &error);
	g_assert_error (error, AS_NODE_ERROR, AS_NODE_ERROR_FAILED);
	g_assert (!ret);
	g_clear_error (&error);

	/* passthrough */
	tmp = as_markup_convert ("<p>pa&amp;ra</p><ul><li>one</li><li>two</li></ul>",
				 AS_MARKUP_CONVERT_FORMAT_APPSTREAM,
				 &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "<p>pa&amp;ra</p><ul><li>one</li><li>two</li></ul>");
	g_free (tmp);

	/* ignore errors */
	tmp = as_markup_convert_full ("<p>para</p><ol><li>one</li></ol><li>two</li>",
				      AS_MARKUP_CONVERT_FORMAT_APPSTREAM,
				      AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS,
				      &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "<p>para</p><ul><li>one</li></ul>");
	g_free (tmp);
	tmp = as_markup_convert_full ("<p>para</p><ul><li>one</li><li>two</ul>",
				      AS_MARKUP_CONVERT_FORMAT_APPSTREAM,
				      AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS,
				      &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "<p>para</p>");
	g_free (tmp);

	/* valid tokens */
	g_assert (as_utils_search_token_valid ("battery"));
	g_assert (!as_utils_search_token_valid ("<b>"));

	/* check tokenisation */
	tokens = as_utils_search_tokenize ("a c b");
	g_assert (tokens == NULL);
	tokens = as_utils_search_tokenize ("batteries are (really) stupid");
	g_assert_cmpstr (tokens[0], ==, "batteries");
	g_assert_cmpstr (tokens[1], ==, "are");
	g_assert_cmpstr (tokens[2], ==, "stupid");
	g_assert_cmpstr (tokens[3], ==, NULL);
	g_strfreev (tokens);
}

static void
as_test_utils_version_func (void)
{
	guint i;
	struct {
		guint32 val;
		const gchar *ver;
		AsVersionParseFlag flags;
	} version_from_uint32[] = {
		{ 0x0,		"0.0.0.0",	AS_VERSION_PARSE_FLAG_NONE },
		{ 0xff,		"0.0.0.255",	AS_VERSION_PARSE_FLAG_NONE },
		{ 0xff01,	"0.0.255.1",	AS_VERSION_PARSE_FLAG_NONE },
		{ 0xff0001,	"0.255.0.1",	AS_VERSION_PARSE_FLAG_NONE },
		{ 0xff000100,	"255.0.1.0",	AS_VERSION_PARSE_FLAG_NONE },
		{ 0x0,		"0.0.0",	AS_VERSION_PARSE_FLAG_USE_TRIPLET },
		{ 0xff,		"0.0.255",	AS_VERSION_PARSE_FLAG_USE_TRIPLET },
		{ 0xff01,	"0.0.65281",	AS_VERSION_PARSE_FLAG_USE_TRIPLET },
		{ 0xff0001,	"0.255.1",	AS_VERSION_PARSE_FLAG_USE_TRIPLET },
		{ 0xff000100,	"255.0.256",	AS_VERSION_PARSE_FLAG_USE_TRIPLET },
		{ 0,		NULL }
	};
	struct {
		guint16 val;
		const gchar *ver;
		AsVersionParseFlag flags;
	} version_from_uint16[] = {
		{ 0x0,		"0.0",		AS_VERSION_PARSE_FLAG_NONE },
		{ 0xff,		"0.255",	AS_VERSION_PARSE_FLAG_NONE },
		{ 0xff01,	"255.1",	AS_VERSION_PARSE_FLAG_NONE },
		{ 0x0,		"0.0",		AS_VERSION_PARSE_FLAG_USE_BCD },
		{ 0x0110,	"1.10",		AS_VERSION_PARSE_FLAG_USE_BCD },
		{ 0x9999,	"99.99",	AS_VERSION_PARSE_FLAG_USE_BCD },
		{ 0,		NULL }
	};
	struct {
		const gchar *old;
		const gchar *new;
	} version_parse[] = {
		{ "0",		"0" },
		{ "0x1a",	"0.0.26" },
		{ "257",	"0.0.257" },
		{ "1.2.3",	"1.2.3" },
		{ "0xff0001",	"0.255.1" },
		{ "16711681",	"0.255.1" },
		{ "20150915",	"20150915" },
		{ "dave",	"dave" },
		{ "0x1x",	"0x1x" },
		{ NULL,		NULL }
	};

	/* check version conversion */
	for (i = 0; version_from_uint32[i].ver != NULL; i++) {
		g_autofree gchar *ver = NULL;
		ver = as_utils_version_from_uint32 (version_from_uint32[i].val,
						    version_from_uint32[i].flags);
		g_assert_cmpstr (ver, ==, version_from_uint32[i].ver);
	}
	for (i = 0; version_from_uint16[i].ver != NULL; i++) {
		g_autofree gchar *ver = NULL;
		ver = as_utils_version_from_uint16 (version_from_uint16[i].val,
						    version_from_uint16[i].flags);
		g_assert_cmpstr (ver, ==, version_from_uint16[i].ver);
	}

	/* check version parsing */
	for (i = 0; version_parse[i].old != NULL; i++) {
		g_autofree gchar *ver = NULL;
		ver = as_utils_version_parse (version_parse[i].old);
		g_assert_cmpstr (ver, ==, version_parse[i].new);
	}
}

static void
as_test_store_metadata_func (void)
{
	GError *error = NULL;
	GPtrArray *apps;
	gboolean ret;
	const gchar *xml =
		"<components version=\"0.6\">"
		"<component type=\"desktop\">"
		"<id>test.desktop</id>"
		"<metadata>"
		"<value key=\"foo\">bar</value>"
		"</metadata>"
		"</component>"
		"<component type=\"desktop\">"
		"<id>tested.desktop</id>"
		"<metadata>"
		"<value key=\"foo\">bar</value>"
		"</metadata>"
		"</component>"
		"</components>";
	g_autoptr(AsStore) store = NULL;

	store = as_store_new ();
	ret = as_store_from_xml (store, xml, NULL, &error);
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
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GTimer) timer = NULL;

	/* create lots of applications in the store */
	store = as_store_new ();
	as_store_add_metadata_index (store, "X-CacheID");
	for (i = 0; i < repeats; i++) {
		g_autofree gchar *id = g_strdup_printf ("app-%05u", i);
		g_autoptr(AsApp) app = as_app_new ();
		as_app_set_id (app, id);
		as_app_add_metadata (app, "X-CacheID", "dave.i386");
		as_app_add_metadata (app, "baz", "dave");
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
as_test_yaml_broken_func (void)
{
#ifdef AS_BUILD_DEP11
	g_autoptr(AsYaml) node = NULL;
	g_autoptr(GError) error1 = NULL;
	g_autoptr(GError) error2 = NULL;
	node = as_yaml_from_data ("s---\n"
				  "File: DEP-11\n",
				  -1,
				  AS_YAML_FROM_FLAG_NONE,
				  &error1);
	g_assert_error (error1, AS_NODE_ERROR, AS_NODE_ERROR_INVALID_MARKUP);
	g_assert (node == NULL);
	node = as_yaml_from_data ("---\n"
				  "%File: DEP-11\n",
				  -1,
				  AS_YAML_FROM_FLAG_NONE,
				  &error2);
	g_assert_error (error2, AS_NODE_ERROR, AS_NODE_ERROR_INVALID_MARKUP);
	g_assert_cmpstr (error2->message, ==,
			 "scanner error: while scanning a directive at ln:2 col:1, "
			 "found unexpected non-alphabetical character at ln:2 col:6");
	g_assert (node == NULL);
#else
	g_test_skip ("Compiled without YAML (DEP-11) support");
#endif
}

static void
as_test_yaml_func (void)
{
#ifdef AS_BUILD_DEP11
	AsYaml *node;
	GError *error = NULL;
	GString *str;
	const gchar *expected;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;
	gboolean ret;

	/* simple header */
	node = as_yaml_from_data (
		"File: DEP-11\n"
		"Origin: aequorea\n"
		"Version: '0.6'\n",
		-1,
		AS_YAML_FROM_FLAG_NONE,
		&error);
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
		"---\n"
		"Mimetypes:\n"
		"  - text/html\n"
		"  - text/xml\n"
		"  - application/xhtml+xml\n"
		"Kudos:\n"
		"  - AppMenu\n"
		"  - SearchProvider\n"
		"  - Notifications\n",
		-1,
		AS_YAML_FROM_FLAG_NONE,
		&error);
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
	filename = as_test_get_filename ("usr/share/app-info/yaml/aequorea.yml");
	g_assert (filename != NULL);
	file = g_file_new_for_path (filename);
	node = as_yaml_from_file (file, AS_YAML_FROM_FLAG_NONE, NULL, &error);
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
		" [KVL]Package=iceweasel\n"
		" [MAP]Icon\n"
		"  [SEQ]cached\n"
		"   [MAP]{\n"
		"    [KVL]name=iceweasel.png\n"
		"    [KVL]width=64\n"
		"    [KVL]height=64\n"
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
	ret = as_test_compare_lines (str->str, expected, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_string_free (str, TRUE);
	as_yaml_unref (node);
#else
	g_test_skip ("Compiled without YAML (DEP-11) support");
#endif
}

static void
as_test_store_yaml_func (void)
{
#ifdef AS_BUILD_DEP11
	AsApp *app;
	GError *error = NULL;
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GString) str = NULL;
	const gchar *xml =
		"<components origin=\"aequorea\" version=\"0.6\">\n"
		"<component type=\"desktop\">\n"
		"<id>dave.desktop</id>\n"
		"<name>dave</name>\n"
		"</component>\n"
		"<component type=\"desktop\">\n"
		"<id>iceweasel.desktop</id>\n"
		"<pkgname>iceweasel</pkgname>\n"
		"<name>Iceweasel</name>\n"
		"<icon type=\"cached\" height=\"64\" width=\"64\">iceweasel.png</icon>\n"
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
	filename = as_test_get_filename ("usr/share/app-info/yaml/aequorea.yml");
	g_assert (filename != NULL);
	file = g_file_new_for_path (filename);
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test it matches expected XML */
	str = as_store_to_xml (store, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	ret = as_test_compare_lines (str->str, xml, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* test store properties */
	g_assert_cmpstr (as_store_get_origin (store), ==, "aequorea");
	g_assert_cmpfloat (as_store_get_api_version (store), <, 0.6 + 0.01);
	g_assert_cmpfloat (as_store_get_api_version (store), >, 0.6 - 0.01);
	g_assert_cmpint (as_store_get_size (store), ==, 2);
	g_assert (as_store_get_app_by_id (store, "iceweasel.desktop") != NULL);
	g_assert (as_store_get_app_by_id (store, "dave.desktop") != NULL);

	/* test application properties */
	app = as_store_get_app_by_id (store, "iceweasel.desktop");
	g_assert_cmpint (as_app_get_kind (app), ==, AS_APP_KIND_DESKTOP);
	g_assert_cmpstr (as_app_get_pkgname_default (app), ==, "iceweasel");
	g_assert_cmpstr (as_app_get_name (app, "C"), ==, "Iceweasel");
	g_assert_cmpstr (as_app_get_origin (app), ==, "aequorea");
#else
	g_test_skip ("Compiled without YAML (DEP-11) support");
#endif
}

static void
as_test_store_speed_yaml_func (void)
{
#ifdef AS_BUILD_DEP11
	GError *error = NULL;
	gboolean ret;
	guint i;
	guint loops = 10;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GTimer) timer = NULL;

	filename = as_test_get_filename ("example-v06.yml.gz");
	g_assert (filename != NULL);
	file = g_file_new_for_path (filename);
	timer = g_timer_new ();
	for (i = 0; i < loops; i++) {
		g_autoptr(AsStore) store = NULL;
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
#else
	g_test_skip ("Compiled without YAML (DEP-11) support");
#endif
}

static void
as_test_utils_vercmp_func (void)
{
	/* same */
	g_assert_cmpint (as_utils_vercmp ("1.2.3", "1.2.3"), ==, 0);
	g_assert_cmpint (as_utils_vercmp ("001.002.003", "001.002.003"), ==, 0);

	/* same, not dotted decimal */
	g_assert_cmpint (as_utils_vercmp ("1.2.3", "0x1020003"), ==, 0);
	g_assert_cmpint (as_utils_vercmp ("0x10203", "0x10203"), ==, 0);

	/* upgrade and downgrade */
	g_assert_cmpint (as_utils_vercmp ("1.2.3", "1.2.4"), <, 0);
	g_assert_cmpint (as_utils_vercmp ("001.002.000", "001.002.009"), <, 0);
	g_assert_cmpint (as_utils_vercmp ("1.2.3", "1.2.2"), >, 0);
	g_assert_cmpint (as_utils_vercmp ("001.002.009", "001.002.000"), >, 0);

	/* unequal depth */
	g_assert_cmpint (as_utils_vercmp ("1.2.3", "1.2.3.1"), <, 0);
	g_assert_cmpint (as_utils_vercmp ("1.2.3.1", "1.2.4"), <, 0);

	/* mixed-alpha-numeric */
	g_assert_cmpint (as_utils_vercmp ("1.2.3a", "1.2.3a"), ==, 0);
	g_assert_cmpint (as_utils_vercmp ("1.2.3a", "1.2.3b"), <, 0);
	g_assert_cmpint (as_utils_vercmp ("1.2.3b", "1.2.3a"), >, 0);

	/* alpha version append */
	g_assert_cmpint (as_utils_vercmp ("1.2.3", "1.2.3a"), <, 0);
	g_assert_cmpint (as_utils_vercmp ("1.2.3a", "1.2.3"), >, 0);

	/* alpha only */
	g_assert_cmpint (as_utils_vercmp ("alpha", "alpha"), ==, 0);
	g_assert_cmpint (as_utils_vercmp ("alpha", "beta"), <, 0);
	g_assert_cmpint (as_utils_vercmp ("beta", "alpha"), >, 0);

	/* alpha-compare */
	g_assert_cmpint (as_utils_vercmp ("1.2a.3", "1.2a.3"), ==, 0);
	g_assert_cmpint (as_utils_vercmp ("1.2a.3", "1.2b.3"), <, 0);
	g_assert_cmpint (as_utils_vercmp ("1.2b.3", "1.2a.3"), >, 0);

	/* invalid */
	g_assert_cmpint (as_utils_vercmp ("1", NULL), ==, G_MAXINT);
	g_assert_cmpint (as_utils_vercmp (NULL, "1"), ==, G_MAXINT);
	g_assert_cmpint (as_utils_vercmp (NULL, NULL), ==, G_MAXINT);
}

static void
as_test_utils_install_filename_func (void)
{
	gboolean ret;
	GError *error = NULL;
	g_autofree gchar *filename1 = NULL;
	g_autofree gchar *filename2 = NULL;
	g_autofree gchar *filename3 = NULL;
	g_autofree gchar *filename4 = NULL;

	/* appdata to shared */
	filename1 = as_test_get_filename ("broken.appdata.xml");
	ret = as_utils_install_filename	(AS_UTILS_LOCATION_SHARED,
					 filename1, NULL,
					 "/tmp/destdir/",
					 &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (g_file_test ("/tmp/destdir/usr/share/appdata/broken.appdata.xml", G_FILE_TEST_EXISTS));

	/* metainfo to cache */
	filename2 = as_test_get_filename ("example.metainfo.xml");
	ret = as_utils_install_filename	(AS_UTILS_LOCATION_CACHE,
					 filename2, NULL,
					 "/tmp/destdir/",
					 &error);
	g_assert_error (error, AS_UTILS_ERROR, AS_UTILS_ERROR_INVALID_TYPE);
	g_assert (!ret);
	g_assert (!g_file_test ("/tmp/destdir/var/cache/appdata/example.metainfo.xml", G_FILE_TEST_EXISTS));
	g_clear_error (&error);

	/* appstream to cache */
	filename3 = as_test_get_filename ("origin.xml");
	ret = as_utils_install_filename	(AS_UTILS_LOCATION_CACHE,
					 filename3, NULL,
					 "/tmp/destdir/",
					 &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (g_file_test ("/tmp/destdir/var/cache/app-info/xmls/origin.xml", G_FILE_TEST_EXISTS));

	/* icons to cache, override origin */
	filename4 = as_test_get_filename ("origin-icons.tar.gz");
	ret = as_utils_install_filename	(AS_UTILS_LOCATION_CACHE,
					 filename4, "neworigin",
					 "/tmp/destdir/",
					 &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (g_file_test ("/tmp/destdir/var/cache/app-info/icons/neworigin/64x64/org.gnome.Software.png", G_FILE_TEST_EXISTS));

	/* icons to shared */
	ret = as_utils_install_filename	(AS_UTILS_LOCATION_SHARED,
					 filename4, NULL,
					 "/tmp/destdir/",
					 &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (g_file_test ("/tmp/destdir/usr/share/app-info/icons/origin/64x64/org.gnome.Software.png", G_FILE_TEST_EXISTS));
}

static void
as_test_utils_string_replace_func (void)
{
	guint i;
	struct {
		const gchar *str;
		const gchar *search;
		const gchar *replace;
		const gchar *result;
	} table[] = {
		{ "",		"",		"",		"" },
		{ "one",	"one",		"two",		"two" },
		{ "one",	"one",		"1",		"1" },
		{ "one",	"one",		"onlyme",	"onlyme" },
		{ "we few ppl",	" few ",	"",		"weppl" },
		{ "bee&",	"&",		"&amp;",	"bee&amp;" },
		{ NULL,		NULL,		NULL,		NULL }
	};
	for (i = 0; table[i].str != NULL; i++) {
		g_autoptr(GString) str = g_string_new (table[i].str);
		as_utils_string_replace (str,
					 table[i].search,
					 table[i].replace);
		g_assert_cmpstr (str->str, ==, table[i].result);
	}
}

static void
as_test_utils_locale_compat_func (void)
{
	/* empty */
	g_assert (as_utils_locale_is_compatible (NULL, NULL));

	/* same */
	g_assert (as_utils_locale_is_compatible ("en_GB", "en_GB"));

	/* forward and reverse compatible */
	g_assert (as_utils_locale_is_compatible ("en_GB", "en"));
	g_assert (as_utils_locale_is_compatible ("en", "en_GB"));

	/* different language and locale */
	g_assert (!as_utils_locale_is_compatible ("en_GB", "fr_FR"));

	/* politics */
	g_assert (!as_utils_locale_is_compatible ("zh_CN", "zh_TW"));

	/* never going to match system locale or language */
	g_assert (!as_utils_locale_is_compatible ("xx_XX", NULL));
	g_assert (!as_utils_locale_is_compatible (NULL, "xx_XX"));
}

static void
as_test_markup_import_html (void)
{
	const gchar *input;
	g_autofree gchar *out_complex = NULL;
	g_autofree gchar *out_list = NULL;
	g_autofree gchar *out_simple = NULL;
	g_autoptr(GError) error = NULL;
	guint i;
	struct {
		const gchar *html;
		const gchar *markup;
	} table[] = {
		{ "",					"" },
		{ "dave",				"<p>dave</p>" },
		{ "&trade;",				"<p>™</p>" },
		{ "<p>paul</p>",			"<p>paul</p>" },
		{ "<p>tim</p><p>baz</p>",		"<p>tim</p>\n<p>baz</p>" },
		{ "<ul><li>1</li></ul>",		"<ul><li>1</li></ul>" },
		{ "<ul><li>1</li><li>2</li></ul>",	"<ul><li>1</li><li>2</li></ul>" },
		{ "<p>foo<i>awesome</i></p>",		"<p>fooawesome</p>" },
		{ "a<img src=\"moo.png\">b",		"<p>ab</p>" },
		{ "<h2>title</h2>content",		"<p>content</p>" },
		{ "para1<br><br>para2",			"<p>para1</p>\n<p>para2</p>" },
		{ "para1<h1>ignore</h1>para2",		"<p>para1</p>\n<p>para2</p>" },
		{ NULL,				NULL}
	};
	for (i = 0; table[i].html != NULL; i++) {
		g_autofree gchar *tmp = NULL;
		g_autoptr(GError) error_local = NULL;
		tmp = as_markup_import (table[i].html,
					AS_MARKUP_CONVERT_FORMAT_HTML,
					&error_local);
		g_assert_no_error (error_local);
		g_assert_cmpstr (tmp, ==, table[i].markup);
	}

	/* simple, from meta */
	input = "This game is simply awesome&trade; in every way!";
	out_simple = as_markup_import (input, AS_MARKUP_CONVERT_FORMAT_HTML, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (out_simple, ==, "<p>This game is simply awesome™ in every way!</p>");

	/* complex non-compliant HTML, from div */
	input = "  <h1>header</h1>"
		"  <p>First line of the <i>description</i> is okay...</p>"
		"  <img src=\"moo.png\">"
		"  <img src=\"png\">"
		"  <p>Second <strong>line</strong> is <a href=\"#moo\">even</a> better!</p>";
	out_complex = as_markup_import (input, AS_MARKUP_CONVERT_FORMAT_HTML, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (out_complex, ==, "<p>First line of the description is okay...</p>\n"
					  "<p>Second line is even better!</p>");

	/* complex list */
	input = "  <ul>"
		"  <li>First line of the list</li>"
		"  <li>Second line of the list</li>"
		"  </ul>";
	out_list = as_markup_import (input, AS_MARKUP_CONVERT_FORMAT_HTML, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (out_list, ==, "<ul><li>First line of the list</li><li>Second line of the list</li></ul>");
}

static void
as_test_utils_unique_id_func (void)
{
	const guint loops = 100000;
	guint i;
	gdouble duration_ns;
	g_autoptr(GTimer) timer = g_timer_new ();

	/* pathological cases */
	g_assert (!as_utils_unique_id_equal ("foo", "bar"));
	g_assert (!as_utils_unique_id_equal ("foo/bar/baz", "foo/bar"));

	for (i = 0; i < loops; i++) {
		g_assert (as_utils_unique_id_equal ("aa/bb/cc/dd/ee/ff",
						    "aa/bb/cc/dd/ee/ff"));
		g_assert (as_utils_unique_id_equal ("aa/bb/cc/dd/ee/ff",
						    "aa/*/cc/dd/ee/ff"));
		g_assert (as_utils_unique_id_equal ("user/flatpak/utopia/desktop/gimp.desktop/master",
						    "*/*/*/*/*/*"));
		g_assert (!as_utils_unique_id_equal ("zz/zz/zz/zz/zz/zz",
						     "aa/bb/cc/dd/ee/ff"));
		g_assert (!as_utils_unique_id_equal ("user/*/*/shell-extension/gmail_notify@jablona123.pl.shell-extension/*",
						     "*/*/*/desktop/org.gnome.accerciser.desktop/*"));
	}
	duration_ns = g_timer_elapsed (timer, NULL) * 1000000000.f;
	g_print ("%.0f ns: ", duration_ns / (loops * 4));

	/* allow ignoring using bitfields */
	g_assert (as_utils_unique_id_match ("aa/bb/cc/dd/ee/ff",
					    "aa/bb/cc/dd/ee/XXXXXXXXXXXXX",
					    AS_UNIQUE_ID_MATCH_FLAG_SCOPE |
					    AS_UNIQUE_ID_MATCH_FLAG_BUNDLE_KIND |
					    AS_UNIQUE_ID_MATCH_FLAG_ORIGIN |
					    AS_UNIQUE_ID_MATCH_FLAG_KIND |
					    AS_UNIQUE_ID_MATCH_FLAG_ID));
	g_assert (as_utils_unique_id_match ("XXXXXXXXXXXXX/bb/cc/dd/ee/ff",
					    "aa/bb/cc/dd/ee/ff",
					    AS_UNIQUE_ID_MATCH_FLAG_BUNDLE_KIND |
					    AS_UNIQUE_ID_MATCH_FLAG_ORIGIN |
					    AS_UNIQUE_ID_MATCH_FLAG_KIND |
					    AS_UNIQUE_ID_MATCH_FLAG_ID |
					    AS_UNIQUE_ID_MATCH_FLAG_BRANCH));
}

static void
as_test_store_merge_func (void)
{
	g_autoptr (AsApp) app1 = NULL;
	g_autoptr (AsApp) app2 = NULL;
	g_autoptr (AsApp) app_merge = NULL;
	g_autoptr (AsStore) store = NULL;
	g_autoptr (AsFormat) format = NULL;

	store = as_store_new ();
	as_store_set_add_flags (store,
				AS_STORE_ADD_FLAG_USE_UNIQUE_ID |
				AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC);

	/* add app */
	app1 = as_app_new ();
	as_app_set_id (app1, "org.gnome.Software.desktop");
	as_app_set_branch (app1, "master");
	_as_app_add_format_kind (app1, AS_FORMAT_KIND_APPDATA);
	as_app_add_pkgname (app1, "gnome-software");
	g_assert_cmpstr (as_app_get_unique_id (app1), ==,
			 "*/package/*/*/org.gnome.Software.desktop/master");
	as_store_add_app (store, app1);

	/* add merge component */
	app_merge = as_app_new ();
	as_app_set_kind (app_merge, AS_APP_KIND_DESKTOP);
	as_app_set_id (app_merge, "org.gnome.Software.desktop");
	_as_app_add_format_kind (app_merge, AS_FORMAT_KIND_APPSTREAM);
	as_app_set_origin (app_merge, "utopia");
	as_app_set_scope (app_merge, AS_APP_SCOPE_USER);
	as_app_add_category (app_merge, "special");
	format = as_format_new ();
	as_format_set_filename (format, "DO-NOT-SUBSUME.xml");
	as_app_add_format (app_merge, format);
	as_store_add_app (store, app_merge);
	g_assert_cmpstr (as_app_get_unique_id (app_merge), ==,
			 "*/*/*/desktop/org.gnome.Software.desktop/*");

	/* add app */
	app2 = as_app_new ();
	as_app_set_id (app2, "org.gnome.Software.desktop");
	as_app_set_branch (app2, "stable");
	_as_app_add_format_kind (app2, AS_FORMAT_KIND_APPSTREAM);
	as_app_add_pkgname (app2, "gnome-software");
	g_assert_cmpstr (as_app_get_unique_id (app2), ==,
			 "*/package/*/*/org.gnome.Software.desktop/stable");
	as_store_add_app (store, app2);

	/* verify that both apps have the category */
	g_assert (as_app_has_category (app1, "special"));
	g_assert (as_app_has_category (app2, "special"));

	/* verify we didn't inherit the private bits */
	g_assert (as_app_get_format_by_kind (app1, AS_FORMAT_KIND_UNKNOWN) == NULL);
	g_assert (as_app_get_format_by_kind (app2, AS_FORMAT_KIND_UNKNOWN) == NULL);
}

static void
as_test_store_merge_replace_func (void)
{
	g_autoptr (AsApp) app1 = NULL;
	g_autoptr (AsApp) app2 = NULL;
	g_autoptr (AsApp) app_merge = NULL;
	g_autoptr (AsStore) store = NULL;

	store = as_store_new ();
	as_store_set_add_flags (store, AS_STORE_ADD_FLAG_USE_UNIQUE_ID);

	/* add app */
	app1 = as_app_new ();
	as_app_set_id (app1, "org.gnome.Software.desktop");
	as_app_set_branch (app1, "master");
	_as_app_add_format_kind (app1, AS_FORMAT_KIND_APPDATA);
	as_app_add_pkgname (app1, "gnome-software");
	as_app_add_category (app1, "Family");
	as_store_add_app (store, app1);

	/* add merge component */
	app_merge = as_app_new ();
	as_app_set_kind (app_merge, AS_APP_KIND_DESKTOP);
	as_app_set_id (app_merge, "org.gnome.Software.desktop");
	_as_app_add_format_kind (app_merge, AS_FORMAT_KIND_APPSTREAM);
	as_app_set_origin (app_merge, "utopia");
	as_app_set_scope (app_merge, AS_APP_SCOPE_USER);
	as_app_set_merge_kind (app_merge, AS_APP_MERGE_KIND_REPLACE);
	as_app_add_category (app_merge, "Horror");
	as_store_add_app (store, app_merge);
	g_assert_cmpstr (as_app_get_unique_id (app_merge), ==,
			 "*/*/*/desktop/org.gnome.Software.desktop/*");

	/* add app */
	app2 = as_app_new ();
	as_app_set_id (app2, "org.gnome.Software.desktop");
	as_app_set_branch (app2, "stable");
	_as_app_add_format_kind (app2, AS_FORMAT_KIND_APPSTREAM);
	as_app_add_pkgname (app2, "gnome-software");
	as_app_add_category (app2, "Family");
	as_store_add_app (store, app2);

	/* verify that both apps have the category */
	g_assert (as_app_has_category (app1, "Horror"));
	g_assert (as_app_has_category (app2, "Horror"));

	/* verify we replaced rather than appended */
	g_assert (!as_app_has_category (app1, "Family"));
	g_assert (!as_app_has_category (app2, "Family"));
}

static void
as_test_store_merge_then_replace_func (void)
{
	g_autoptr (AsApp) app1 = NULL;
	g_autoptr (AsApp) app2 = NULL;
	g_autoptr (AsApp) app3 = NULL;
	g_autoptr (AsStore) store = NULL;

	store = as_store_new ();

	/* this test case checks that a memory error using app names as keys is fixed */

	/* add app */
	app1 = as_app_new ();
	as_app_set_id (app1, "org.gnome.Software.desktop");
	_as_app_add_format_kind (app1, AS_FORMAT_KIND_DESKTOP);
	as_app_set_priority (app1, 0);
	as_store_add_app (store, app1);
	g_clear_object (&app1);

	/* add app that merges with the first */
	app2 = as_app_new ();
	as_app_set_id (app2, "org.gnome.Software.desktop");
	_as_app_add_format_kind (app2, AS_FORMAT_KIND_DESKTOP);
	as_app_set_priority (app2, 0);
	as_store_add_app (store, app2);
	g_clear_object (&app2);

	/* add app that replaces the second */
	app3 = as_app_new ();
	as_app_set_id (app3, "org.gnome.Software.desktop");
	_as_app_add_format_kind (app3, AS_FORMAT_KIND_DESKTOP);
	as_app_set_priority (app3, 1);
	as_store_add_app (store, app3);
	g_clear_object (&app3);
}

/* shows the unique-id globbing functions at work */
static void
as_test_utils_unique_id_hash_func (void)
{
	AsApp *found;
	g_autoptr(AsApp) app1 = NULL;
	g_autoptr(AsApp) app2 = NULL;
	g_autoptr(GHashTable) hash = NULL;

	/* create new couple of apps */
	app1 = as_app_new ();
	as_app_set_id (app1, "org.gnome.Software.desktop");
	as_app_set_branch (app1, "master");
	g_assert_cmpstr (as_app_get_unique_id (app1), ==,
			 "*/*/*/*/org.gnome.Software.desktop/master");
	app2 = as_app_new ();
	as_app_set_id (app2, "org.gnome.Software.desktop");
	as_app_set_branch (app2, "stable");
	g_assert_cmpstr (as_app_get_unique_id (app2), ==,
			 "*/*/*/*/org.gnome.Software.desktop/stable");

	/* add to hash table using the unique ID as a key */
	hash = g_hash_table_new ((GHashFunc) as_utils_unique_id_hash,
				 (GEqualFunc) as_utils_unique_id_equal);
	g_hash_table_insert (hash, (gpointer) as_app_get_unique_id (app1), app1);
	g_hash_table_insert (hash, (gpointer) as_app_get_unique_id (app2), app2);

	/* get with exact key */
	found = g_hash_table_lookup (hash, "*/*/*/*/org.gnome.Software.desktop/master");
	g_assert (found != NULL);
	found = g_hash_table_lookup (hash, "*/*/*/*/org.gnome.Software.desktop/stable");
	g_assert (found != NULL);

	/* get with more details specified */
	found = g_hash_table_lookup (hash, "system/*/*/*/org.gnome.Software.desktop/master");
	g_assert (found != NULL);
	found = g_hash_table_lookup (hash, "system/*/*/*/org.gnome.Software.desktop/stable");
	g_assert (found != NULL);

	/* get with less details specified */
	found = g_hash_table_lookup (hash, "*/*/*/*/org.gnome.Software.desktop/*");
	g_assert (found != NULL);

	/* different key */
	found = g_hash_table_lookup (hash, "*/*/*/*/gimp.desktop/*");
	g_assert (found == NULL);

	/* different branch */
	found = g_hash_table_lookup (hash, "*/*/*/*/org.gnome.Software.desktop/obsolete");
	g_assert (found == NULL);

	/* check hash function */
	g_assert_cmpint (as_utils_unique_id_hash ("*/*/*/*/gimp.desktop/master"), ==,
			 as_utils_unique_id_hash ("system/*/*/*/gimp.desktop/stable"));
}

/* shows the as_utils_unique_id_*_safe functions are safe with bare text */
static void
as_test_utils_unique_id_hash_safe_func (void)
{
	AsApp *found;
	g_autoptr(AsApp) app = NULL;
	g_autoptr(GHashTable) hash = NULL;

	/* create new app */
	app = as_app_new ();
	as_app_set_id (app, "org.gnome.Software.desktop");

	/* add to hash table using the unique ID as a key */
	hash = g_hash_table_new ((GHashFunc) as_utils_unique_id_hash,
				 (GEqualFunc) as_utils_unique_id_equal);
	g_hash_table_insert (hash, (gpointer) "dave", app);

	/* get with exact key */
	found = g_hash_table_lookup (hash, "dave");
	g_assert (found != NULL);

	/* different key */
	found = g_hash_table_lookup (hash, "frank");
	g_assert (found == NULL);
}

static void
as_test_app_parse_data_func (void)
{
	const gchar *data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			    "<component>\n</component>\n     ";
	gboolean ret;
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(AsApp) app = as_app_new ();
	g_autoptr(GError) error = NULL;

	blob = g_bytes_new (data, strlen (data));
	ret = as_app_parse_data (app, blob, AS_APP_PARSE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert (ret);
}

static void
as_test_ref_string_func (void)
{
	const gchar *tmp;
	AsRefString *rstr;
	AsRefString *rstr2;

	/* basic refcounting */
	rstr = as_ref_string_new ("test");
	g_assert (rstr != NULL);
	g_assert_cmpstr (rstr, ==, "test");
	g_assert (as_ref_string_ref (rstr) != NULL);
	g_assert (as_ref_string_unref (rstr) != NULL);
	g_assert (as_ref_string_unref (rstr) == NULL);

	/* adopting const string */
	tmp = "test";
	rstr = as_ref_string_new (tmp);
	g_assert_cmpstr (rstr, ==, tmp);
	rstr2 = as_ref_string_new (rstr);
	g_assert_cmpstr (rstr2, ==, tmp);
	g_assert (rstr == rstr2);
	g_assert (as_ref_string_unref (rstr) != NULL);
	g_assert (as_ref_string_unref (rstr2) == NULL);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/AppStream/ref-string", as_test_ref_string_func);
	g_test_add_func ("/AppStream/utils{unique_id-hash}", as_test_utils_unique_id_hash_func);
	g_test_add_func ("/AppStream/utils{unique_id-hash2}", as_test_utils_unique_id_hash_safe_func);
	g_test_add_func ("/AppStream/utils{unique_id}", as_test_utils_unique_id_func);
	g_test_add_func ("/AppStream/utils{locale-compat}", as_test_utils_locale_compat_func);
	g_test_add_func ("/AppStream/utils{string-replace}", as_test_utils_string_replace_func);
	g_test_add_func ("/AppStream/tag", as_test_tag_func);
	g_test_add_func ("/AppStream/launchable", as_test_launchable_func);
	g_test_add_func ("/AppStream/provide", as_test_provide_func);
	g_test_add_func ("/AppStream/require", as_test_require_func);
	g_test_add_func ("/AppStream/checksum", as_test_checksum_func);
	g_test_add_func ("/AppStream/content_rating", as_test_content_rating_func);
	g_test_add_func ("/AppStream/release", as_test_release_func);
	g_test_add_func ("/AppStream/release{date}", as_test_release_date_func);
	g_test_add_func ("/AppStream/release{appdata}", as_test_release_appdata_func);
	g_test_add_func ("/AppStream/release{appstream}", as_test_release_appstream_func);
	g_test_add_func ("/AppStream/icon", as_test_icon_func);
	g_test_add_func ("/AppStream/icon{scale}", as_test_icon_scale_func);
	g_test_add_func ("/AppStream/icon{embedded}", as_test_icon_embedded_func);
	g_test_add_func ("/AppStream/bundle", as_test_bundle_func);
	g_test_add_func ("/AppStream/review", as_test_review_func);
	g_test_add_func ("/AppStream/agreement", as_test_agreement_func);
	g_test_add_func ("/AppStream/translation", as_test_translation_func);
	g_test_add_func ("/AppStream/suggest", as_test_suggest_func);
	g_test_add_func ("/AppStream/image", as_test_image_func);
	g_test_add_func ("/AppStream/image{resize}", as_test_image_resize_func);
	g_test_add_func ("/AppStream/image{alpha}", as_test_image_alpha_func);
	g_test_add_func ("/AppStream/screenshot", as_test_screenshot_func);
	g_test_add_func ("/AppStream/app", as_test_app_func);
	g_test_add_func ("/AppStream/app{launchable:fallback}", as_test_app_launchable_fallback_func);
	g_test_add_func ("/AppStream/app{builder:gettext}", as_test_app_builder_gettext_func);
	g_test_add_func ("/AppStream/app{builder:gettext-nodomain}", as_test_app_builder_gettext_nodomain_func);
	g_test_add_func ("/AppStream/app{builder:qt}", as_test_app_builder_qt_func);
	g_test_add_func ("/AppStream/app{translated}", as_test_app_translated_func);
	g_test_add_func ("/AppStream/app{validate-style}", as_test_app_validate_style_func);
	g_test_add_func ("/AppStream/app{validate-appdata-good}", as_test_app_validate_appdata_good_func);
	g_test_add_func ("/AppStream/app{validate-metainfo-good}", as_test_app_validate_metainfo_good_func);
	g_test_add_func ("/AppStream/app{validate-file-bad}", as_test_app_validate_file_bad_func);
	g_test_add_func ("/AppStream/app{validate-meta-bad}", as_test_app_validate_meta_bad_func);
	g_test_add_func ("/AppStream/app{validate-intltool}", as_test_app_validate_intltool_func);
	g_test_add_func ("/AppStream/app{parse-data}", as_test_app_parse_data_func);
	g_test_add_func ("/AppStream/app{parse-file:desktop}", as_test_app_parse_file_desktop_func);
	g_test_add_func ("/AppStream/app{no-markup}", as_test_app_no_markup_func);
	g_test_add_func ("/AppStream/app{subsume}", as_test_app_subsume_func);
	g_test_add_func ("/AppStream/app{search}", as_test_app_search_func);
	g_test_add_func ("/AppStream/app{screenshot}", as_test_app_screenshot_func);
	g_test_add_func ("/AppStream/markup{import-html}", as_test_markup_import_html);
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
	g_test_add_func ("/AppStream/utils{markup-import}", as_test_utils_markup_import_func);
	g_test_add_func ("/AppStream/utils{version}", as_test_utils_version_func);
	g_test_add_func ("/AppStream/utils{guid}", as_test_utils_guid_func);
	g_test_add_func ("/AppStream/utils{appstream-id}", as_test_utils_appstream_id_func);
	g_test_add_func ("/AppStream/utils{icons}", as_test_utils_icons_func);
	g_test_add_func ("/AppStream/utils{spdx-token}", as_test_utils_spdx_token_func);
	g_test_add_func ("/AppStream/utils{install-filename}", as_test_utils_install_filename_func);
	g_test_add_func ("/AppStream/utils{vercmp}", as_test_utils_vercmp_func);
	if (g_test_slow ()) {
		g_test_add_func ("/AppStream/monitor{dir}", as_test_monitor_dir_func);
		g_test_add_func ("/AppStream/monitor{file}", as_test_monitor_file_func);
	}
	g_test_add_func ("/AppStream/yaml", as_test_yaml_func);
	g_test_add_func ("/AppStream/yaml{broken}", as_test_yaml_broken_func);
	g_test_add_func ("/AppStream/store", as_test_store_func);
	g_test_add_func ("/AppStream/store{unique}", as_test_store_unique_func);
	g_test_add_func ("/AppStream/store{merge}", as_test_store_merge_func);
	g_test_add_func ("/AppStream/store{merge-replace}", as_test_store_merge_replace_func);
	g_test_add_func ("/AppStream/store{merge-then-replace}", as_test_store_merge_then_replace_func);
	g_test_add_func ("/AppStream/store{empty}", as_test_store_empty_func);
	if (g_test_slow ()) {
		g_test_add_func ("/AppStream/store{auto-reload-dir}", as_test_store_auto_reload_dir_func);
		g_test_add_func ("/AppStream/store{auto-reload-file}", as_test_store_auto_reload_file_func);
	}
	g_test_add_func ("/AppStream/store{flatpak}", as_test_store_flatpak_func);
	g_test_add_func ("/AppStream/store{prefix}", as_test_store_prefix_func);
	g_test_add_func ("/AppStream/store{wildcard}", as_test_store_wildcard_func);
	g_test_add_func ("/AppStream/store{demote}", as_test_store_demote_func);
	g_test_add_func ("/AppStream/store{cab}", as_test_store_cab_func);
	g_test_add_func ("/AppStream/store{merges}", as_test_store_merges_func);
	g_test_add_func ("/AppStream/store{merges-local}", as_test_store_merges_local_func);
	g_test_add_func ("/AppStream/store{addons}", as_test_store_addons_func);
	g_test_add_func ("/AppStream/store{versions}", as_test_store_versions_func);
	g_test_add_func ("/AppStream/store{origin}", as_test_store_origin_func);
	g_test_add_func ("/AppStream/store{yaml}", as_test_store_yaml_func);
	g_test_add_func ("/AppStream/store{metadata}", as_test_store_metadata_func);
	g_test_add_func ("/AppStream/store{metadata-index}", as_test_store_metadata_index_func);
	g_test_add_func ("/AppStream/store{validate}", as_test_store_validate_func);
	g_test_add_func ("/AppStream/store{embedded}", as_test_store_embedded_func);
	g_test_add_func ("/AppStream/store{provides}", as_test_store_provides_func);
	g_test_add_func ("/AppStream/store{local-appdata}", as_test_store_local_appdata_func);
	if (g_test_slow ()) {
		g_test_add_func ("/AppStream/store{speed-appstream}", as_test_store_speed_appstream_func);
		g_test_add_func ("/AppStream/store{speed-search}", as_test_store_speed_search_func);
		g_test_add_func ("/AppStream/store{speed-appdata}", as_test_store_speed_appdata_func);
		g_test_add_func ("/AppStream/store{speed-desktop}", as_test_store_speed_desktop_func);
		g_test_add_func ("/AppStream/store{speed-yaml}", as_test_store_speed_yaml_func);
	}

	return g_test_run ();
}
