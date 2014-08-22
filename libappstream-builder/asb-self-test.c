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

#include "as-cleanup.h"

#include "asb-context-private.h"
#include "asb-plugin.h"
#include "asb-plugin-loader.h"
#include "asb-task.h"
#include "asb-utils.h"

#ifdef HAVE_RPM
#include "asb-package-rpm.h"
#endif

/**
 * asb_test_get_filename:
 **/
static gchar *
asb_test_get_filename (const gchar *filename)
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

#ifdef HAVE_RPM
static void
asb_test_package_rpm_func (void)
{
	AsRelease *rel;
	GError *error = NULL;
	GPtrArray *releases;
	gboolean ret;
	gchar *tmp;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsbPackage *pkg = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *glob = NULL;

	/* open file */
	filename = asb_test_get_filename ("test-0.1-1.fc21.noarch.rpm");
	g_assert (filename != NULL);
	pkg = asb_package_rpm_new ();
	ret = asb_package_open (pkg, filename, &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = asb_package_ensure (pkg,
				  ASB_PACKAGE_ENSURE_DEPS |
				  ASB_PACKAGE_ENSURE_FILES |
				  ASB_PACKAGE_ENSURE_LICENSE |
				  ASB_PACKAGE_ENSURE_RELEASES |
				  ASB_PACKAGE_ENSURE_SOURCE |
				  ASB_PACKAGE_ENSURE_URL,
				  &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check attributes */
	g_assert (asb_package_get_enabled (pkg));
	g_assert_cmpstr (asb_package_get_filename (pkg), ==, filename);
	g_assert_cmpstr (asb_package_get_basename (pkg), ==, "test-0.1-1.fc21.noarch.rpm");
	g_assert_cmpstr (asb_package_get_name (pkg), ==, "test");
	g_assert_cmpstr (asb_package_get_nevr (pkg), ==, "test-0.1-1.fc21");
	g_assert_cmpstr (asb_package_get_evr (pkg), ==, "0.1-1.fc21");
	g_assert_cmpstr (asb_package_get_url (pkg), ==, "http://people.freedesktop.org/~hughsient/");
	g_assert_cmpstr (asb_package_get_license (pkg), ==, "GPL-2.0+");
	g_assert_cmpstr (asb_package_get_source	(pkg), ==, "test-0.1-1.fc21");
	g_assert_cmpstr (asb_package_get_source_pkgname (pkg), ==, "test");

	/* filelists */
	tmp = g_strjoinv (";", asb_package_get_filelist (pkg));
	g_assert_cmpstr (tmp, ==, "/usr/share/test-0.1/README");
	g_free (tmp);

	/* deps */
	tmp = g_strjoinv (";", asb_package_get_deps (pkg));
	g_assert_cmpstr (tmp, ==, "bar;baz;foo");
	g_free (tmp);

	/* releases */
	releases = asb_package_get_releases (pkg);
	g_assert_cmpint (releases->len, ==, 1);
	rel = g_ptr_array_index (releases, 0);
	g_assert (rel != NULL);
	g_assert_cmpstr (as_release_get_version (rel), ==, "0.1");
	g_assert_cmpint (as_release_get_timestamp (rel), ==, 1274097600);
	g_assert_cmpstr (as_release_get_description (rel, NULL), ==, NULL);
	rel = asb_package_get_release (pkg, "0.1");
	g_assert (rel != NULL);
	g_assert_cmpint (as_release_get_timestamp (rel), ==, 1274097600);

	/* check config */
	g_assert_cmpstr (asb_package_get_config (pkg, "test"), ==, NULL);
	asb_package_set_config (pkg, "test", "dave1");
	g_assert_cmpstr (asb_package_get_config (pkg, "test"), ==, "dave1");
	asb_package_set_config (pkg, "test", "dave2");
	g_assert_cmpstr (asb_package_get_config (pkg, "test"), ==, "dave2");

	/* compare */
	g_assert_cmpint (asb_package_compare (pkg, pkg), ==, 0);

	/* explode all */
	ret = asb_utils_ensure_exists_and_empty ("/tmp/asb-test", &error);
	g_assert_no_error (error);
	g_assert (ret);
	ret = asb_package_explode (pkg, "/tmp/asb-test", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (g_file_test ("/tmp/asb-test/usr/share/test-0.1/README", G_FILE_TEST_EXISTS));

	/* explode with a glob */
	ret = asb_utils_ensure_exists_and_empty ("/tmp/asb-test", &error);
	g_assert_no_error (error);
	g_assert (ret);
	glob = asb_glob_value_array_new ();
	asb_plugin_add_glob (glob, "/usr/share/*");
	ret = asb_package_explode (pkg, "/tmp/asb-test", glob, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert (g_file_test ("/tmp/asb-test/usr/share/test-0.1/README", G_FILE_TEST_EXISTS));
}
#endif

static void
asb_test_utils_glob_func (void)
{
	_cleanup_ptrarray_unref_ GPtrArray *array = NULL;

	array = asb_glob_value_array_new ();
	g_ptr_array_add (array, asb_glob_value_new ("*.desktop", "DESKTOP"));
	g_ptr_array_add (array, asb_glob_value_new ("*.appdata.xml", "APPDATA"));
	g_assert_cmpint (array->len, ==, 2);
	g_assert_cmpstr (asb_glob_value_search (array, "moo"), ==, NULL);
	g_assert_cmpstr (asb_glob_value_search (array, "gimp.desktop"), ==, "DESKTOP");
	g_assert_cmpstr (asb_glob_value_search (array, "gimp.appdata.xml"), ==, "APPDATA");
}

static void
asb_test_utils_replace_func (void)
{
	guint n;
	_cleanup_string_free_ GString *str = NULL;

	str = g_string_new ("I want to have searched for search");
	n = asb_string_replace (str, "search", "replace");
	g_assert_cmpstr (str->str, ==, "I want to have replaceed for replace");
	g_assert_cmpint (n, ==, 2);
}

static void
asb_test_plugin_loader_func (void)
{
	AsbPluginLoader *loader = NULL;
	AsbPlugin *plugin;
	GError *error = NULL;
	GPtrArray *plugins;
	gboolean ret;
	_cleanup_object_unref_ AsbContext *ctx = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *globs = NULL;

	/* set up loader */
	ctx = asb_context_new ();
	loader = asb_context_get_plugin_loader (ctx);
	ret = asb_plugin_loader_setup (loader, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get the list of globs */
	globs = asb_plugin_loader_get_globs (loader);
	g_assert_cmpint (globs->len, ==, 22);
	g_assert_cmpstr (asb_glob_value_search (globs, "/usr/share/applications/gimp.desktop"), ==, "");
	g_assert_cmpstr (asb_glob_value_search (globs, "/srv/dave.txt"), ==, NULL);

	/* get the list of plugins */
	plugins = asb_plugin_loader_get_plugins (loader);
	g_assert_cmpint (plugins->len, ==, 17);
	plugin = g_ptr_array_index (plugins, 0);
	g_assert (plugin != NULL);
	g_assert (plugin->module != NULL);
	g_assert (plugin->enabled);
	g_assert_cmpstr (plugin->name, ==, "nm");
	g_assert (plugin->ctx == ctx);

	/* match the correct one */
	plugin = asb_plugin_loader_match_fn (loader, "/usr/share/applications/gimp.desktop");
	g_assert (plugin != NULL);
	g_assert_cmpstr (plugin->name, ==, "desktop");
}

#ifdef HAVE_RPM

typedef enum {
	ASB_TEST_CONTEXT_MODE_NO_CACHE,
	ASB_TEST_CONTEXT_MODE_WITH_CACHE,
	ASB_TEST_CONTEXT_MODE_WITH_OLD_CACHE,
	ASB_TEST_CONTEXT_MODE_LAST
} AsbTestContextMode;

static void
asb_test_context_test_func (AsbTestContextMode mode)
{
	AsApp *app;
	GError *error = NULL;
	const gchar *expected_xml;
	gboolean ret;
	_cleanup_free_ gchar *filename1 = NULL;
	_cleanup_free_ gchar *filename2 = NULL;
	_cleanup_free_ gchar *filename3 = NULL;
	_cleanup_free_ gchar *filename4 = NULL;
	_cleanup_free_ gchar *filename5 = NULL;
	_cleanup_object_unref_ AsbContext *ctx = NULL;
	_cleanup_object_unref_ AsStore *store_failed = NULL;
	_cleanup_object_unref_ AsStore *store_ignore = NULL;
	_cleanup_object_unref_ AsStore *store = NULL;
	_cleanup_object_unref_ GFile *file_failed = NULL;
	_cleanup_object_unref_ GFile *file_ignore = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_string_free_ GString *xml = NULL;
	_cleanup_string_free_ GString *xml_failed = NULL;
	_cleanup_string_free_ GString *xml_ignore = NULL;

	/* set up the context */
	ctx = asb_context_new ();
	g_assert (!asb_context_get_add_cache_id (ctx));
	asb_context_set_max_threads (ctx, 1);
	asb_context_set_api_version (ctx, 0.8);
	asb_context_set_add_cache_id (ctx, TRUE);
	asb_context_set_no_net (ctx, TRUE);
	asb_context_set_basename (ctx, "asb-self-test");
	asb_context_set_cache_dir (ctx, "/tmp/asbuilder/cache");
	asb_context_set_output_dir (ctx, "/tmp/asbuilder/output");
	asb_context_set_temp_dir (ctx, "/tmp/asbuilder/temp");
	switch (mode) {
	case ASB_TEST_CONTEXT_MODE_WITH_CACHE:
		asb_context_set_old_metadata (ctx, "/tmp/asbuilder/output");
		break;
	case ASB_TEST_CONTEXT_MODE_WITH_OLD_CACHE:
		{
			_cleanup_free_ gchar *old_cache_dir = NULL;
			old_cache_dir = asb_test_get_filename (".");
			asb_context_set_old_metadata (ctx, old_cache_dir);
		}
		break;
	default:
		break;
	}
	g_assert (asb_context_get_add_cache_id (ctx));
	g_assert_cmpstr (asb_context_get_temp_dir (ctx), ==, "/tmp/asbuilder/temp");
	ret = asb_context_setup (ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add a console application */
	filename1 = asb_test_get_filename ("test-0.1-1.fc21.noarch.rpm");
	g_assert (filename1 != NULL);
	ret = asb_context_add_filename (ctx, filename1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add another GUI application */
	filename2 = asb_test_get_filename ("app-1-1.fc21.x86_64.rpm");
	g_assert (filename2 != NULL);
	ret = asb_context_add_filename (ctx, filename2, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add addons for the GUI application */
	filename3 = asb_test_get_filename ("app-extra-1-1.fc21.noarch.rpm");
	g_assert (filename3 != NULL);
	ret = asb_context_add_filename (ctx, filename3, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add application with no icon */
	filename4 = asb_test_get_filename ("app-console-1-1.fc21.noarch.rpm");
	g_assert (filename4 != NULL);
	ret = asb_context_add_filename (ctx, filename4, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* the same GUI application in the different architecture */
	filename5 = asb_test_get_filename ("app-1-1.fc21.i686.rpm");
	g_assert (filename5 != NULL);
	ret = asb_context_add_filename (ctx, filename5, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* verify queue size */
	switch (mode) {
	case ASB_TEST_CONTEXT_MODE_NO_CACHE:
	case ASB_TEST_CONTEXT_MODE_WITH_OLD_CACHE:
		g_assert_cmpint (asb_context_get_packages(ctx)->len, ==, 5);
		break;
	default:
		/* no packages should need extracting */
		g_assert_cmpint (asb_context_get_packages(ctx)->len, ==, 0);
		break;
	}

	/* run the plugins */
	ret = asb_context_process (ctx, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check files created */
	g_assert (g_file_test ("/tmp/asbuilder/output/asb-self-test.xml.gz", G_FILE_TEST_EXISTS));
	g_assert (g_file_test ("/tmp/asbuilder/output/asb-self-test-failed.xml.gz", G_FILE_TEST_EXISTS));
	g_assert (g_file_test ("/tmp/asbuilder/output/asb-self-test-ignore.xml.gz", G_FILE_TEST_EXISTS));
	g_assert (g_file_test ("/tmp/asbuilder/output/asb-self-test-icons.tar.gz", G_FILE_TEST_EXISTS));

	/* load AppStream metadata */
	file = g_file_new_for_path ("/tmp/asbuilder/output/asb-self-test.xml.gz");
	store = as_store_new ();
	ret = as_store_from_file (store, file, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_store_get_size (store), ==, 2);
	app = as_store_get_app_by_pkgname (store, "app");
	g_assert (app != NULL);
	app = as_store_get_app_by_id (store, "app.desktop");
	g_assert (app != NULL);

	/* check it matches what we expect */
	xml = as_store_to_xml (store, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	expected_xml =
		"<components version=\"0.8\" builder_id=\"appstream-glib:4\" origin=\"asb-self-test\">\n"
		"<component type=\"addon\">\n"
		"<id>app-extra</id>\n"
		"<pkgname>app-extra</pkgname>\n"
		"<source_pkgname>app</source_pkgname>\n"
		"<name>Extra</name>\n"
		"<summary>Addons for extra functionality</summary>\n"
		"<project_license>GPL-2.0+</project_license>\n"
		"<url type=\"homepage\">http://people.freedesktop.org/</url>\n"
		"<extends>app.desktop</extends>\n"
		"<releases>\n"
		"<release version=\"1\" timestamp=\"1407844800\"/>\n"
		"</releases>\n"
		"<languages>\n"
		"<lang percentage=\"100\">en_GB</lang>\n"
		"<lang percentage=\"33\">ru</lang>\n"
		"</languages>\n"
		"<metadata>\n"
		"<value key=\"X-CacheID\">app-extra-1-1.fc21.noarch.rpm</value>\n"
		"</metadata>\n"
		"</component>\n"
		"<component type=\"desktop\">\n"
		"<id>app.desktop</id>\n"
		"<pkgname>app</pkgname>\n"
		"<name>App</name>\n"
		"<summary>A test application</summary>\n"
		"<description><p>Long description goes here.</p></description>\n"
		"<icon type=\"cached\">app.png</icon>\n"
		"<categories>\n"
		"<category>Profiling</category>\n"
		"<category>System</category>\n"
		"</categories>\n"
		"<keywords>\n"
		"<keyword>Administration</keyword>\n"
		"<keyword>Remote</keyword>\n"
		"</keywords>\n"
		"<kudos>\n"
		"<kudo>AppMenu</kudo>\n"
		"<kudo>ModernToolkit</kudo>\n"
		"<kudo>Notifications</kudo>\n"
		"<kudo>SearchProvider</kudo>\n"
		"<kudo>UserDocs</kudo>\n"
		"</kudos>\n"
		"<project_license>GPL-2.0+</project_license>\n"
		"<url type=\"homepage\">http://people.freedesktop.org/~hughsient/appdata/</url>\n"
		"<releases>\n"
		"<release version=\"1\" timestamp=\"1407844800\"/>\n"
		"</releases>\n"
		"<provides>\n"
		"<dbus type=\"session\">org.freedesktop.AppStream</dbus>\n"
		"</provides>\n"
		"<languages>\n"
		"<lang percentage=\"100\">en_GB</lang>\n"
		"<lang percentage=\"33\">ru</lang>\n"
		"</languages>\n"
		"<metadata>\n"
		"<value key=\"X-CacheID\">app-1-1.fc21.x86_64.rpm</value>\n"
		"</metadata>\n"
		"</component>\n"
		"</components>\n";
	if (g_strcmp0 (xml->str, expected_xml) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", expected_xml, xml->str);
	g_assert_cmpstr (xml->str, ==, expected_xml);

	/* load failed metadata */
	file_failed = g_file_new_for_path ("/tmp/asbuilder/output/asb-self-test-failed.xml.gz");
	store_failed = as_store_new ();
	ret = as_store_from_file (store_failed, file_failed, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (as_store_get_size (store_failed), ==, 2);
	app = as_store_get_app_by_id (store_failed, "console1.desktop");
	g_assert (app != NULL);
	app = as_store_get_app_by_id (store_failed, "console2.desktop");
	g_assert (app != NULL);

	/* check output */
	xml_failed = as_store_to_xml (store_failed, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	expected_xml =
		"<components version=\"0.8\" builder_id=\"appstream-glib:4\" origin=\"asb-self-test-failed\">\n"
		"<component type=\"desktop\">\n"
		"<id>console1.desktop</id>\n"
		"<pkgname>app-console</pkgname>\n"
		"<source_pkgname>app</source_pkgname>\n"
		"<name>Console1</name>\n"
		"<summary>A console1 test application</summary>\n"
		"<icon type=\"cached\">console1.png</icon>\n"
		"<categories>\n"
		"<category>ConsoleOnly</category>\n"
		"</categories>\n"
		"<vetos>\n"
		"<veto>Required AppData: ConsoleOnly</veto>\n"
		"</vetos>\n"
		"<project_license>GPL-2.0+</project_license>\n"
		"<url type=\"homepage\">http://people.freedesktop.org/</url>\n"
		"<releases>\n"
		"<release version=\"1\" timestamp=\"1407844800\"/>\n"
		"</releases>\n"
		"<languages>\n"
		"<lang percentage=\"100\">en_GB</lang>\n"
		"<lang percentage=\"33\">ru</lang>\n"
		"</languages>\n"
		"<metadata>\n"
		"<value key=\"X-CacheID\">app-console-1-1.fc21.noarch.rpm</value>\n"
		"</metadata>\n"
		"</component>\n"
		"<component type=\"desktop\">\n"
		"<id>console2.desktop</id>\n"
		"<pkgname>app-console</pkgname>\n"
		"<source_pkgname>app</source_pkgname>\n"
		"<name>Console2</name>\n"
		"<summary>A console2 test application</summary>\n"
		"<icon type=\"cached\">console2.png</icon>\n"
		"<categories>\n"
		"<category>ConsoleOnly</category>\n"
		"</categories>\n"
		"<vetos>\n"
		"<veto>Required AppData: ConsoleOnly</veto>\n"
		"</vetos>\n"
		"<project_license>GPL-2.0+</project_license>\n"
		"<url type=\"homepage\">http://people.freedesktop.org/</url>\n"
		"<releases>\n"
		"<release version=\"1\" timestamp=\"1407844800\"/>\n"
		"</releases>\n"
		"<languages>\n"
		"<lang percentage=\"100\">en_GB</lang>\n"
		"<lang percentage=\"33\">ru</lang>\n"
		"</languages>\n"
		"<metadata>\n"
		"<value key=\"X-CacheID\">app-console-1-1.fc21.noarch.rpm</value>\n"
		"</metadata>\n"
		"</component>\n"
		"</components>\n";
	if (g_strcmp0 (xml_failed->str, expected_xml) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", expected_xml, xml_failed->str);
	g_assert_cmpstr (xml_failed->str, ==, expected_xml);

	/* load ignored metadata */
	file_ignore = g_file_new_for_path ("/tmp/asbuilder/output/asb-self-test-ignore.xml.gz");
	store_ignore = as_store_new ();
	ret = as_store_from_file (store_ignore, file_ignore, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* check output */
	xml_ignore = as_store_to_xml (store_ignore, AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	expected_xml =
		"<components version=\"0.8\" builder_id=\"appstream-glib:4\" origin=\"asb-self-test-ignore\">\n"
		"<component>\n"
		"<id>app-console.noarch</id>\n"
		"<metadata>\n"
		"<value key=\"X-CacheID\">app-console-1-1.fc21.noarch.rpm</value>\n"
		"</metadata>\n"
		"</component>\n"
		"<component>\n"
		"<id>app.i686</id>\n"
		"<metadata>\n"
		"<value key=\"X-CacheID\">app-1-1.fc21.i686.rpm</value>\n"
		"</metadata>\n"
		"</component>\n"
		"<component>\n"
		"<id>test.noarch</id>\n"
		"<metadata>\n"
		"<value key=\"X-CacheID\">test-0.1-1.fc21.noarch.rpm</value>\n"
		"</metadata>\n"
		"</component>\n"
		"</components>\n";
	if (g_strcmp0 (xml_ignore->str, expected_xml) != 0)
		g_warning ("Expected:\n%s\nGot:\n%s", expected_xml, xml_ignore->str);
	g_assert_cmpstr (xml_ignore->str, ==, expected_xml);

}
#endif

static void
asb_test_context_nocache_func (void)
{
#ifdef HAVE_RPM
	GError *error = NULL;
	gboolean ret;
	ret = asb_utils_rmtree ("/tmp/asbuilder/output", &error);
	g_assert_no_error (error);
	g_assert (ret);
	asb_test_context_test_func (ASB_TEST_CONTEXT_MODE_NO_CACHE);
#endif
}

static void
asb_test_context_cache_func (void)
{
#ifdef HAVE_RPM
	GError *error = NULL;
	gboolean ret;

	/* remove icons */
	ret = asb_utils_rmtree ("/tmp/asbuilder/temp/icons", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* run again, this time using the old metadata as a cache */
	asb_test_context_test_func (ASB_TEST_CONTEXT_MODE_WITH_CACHE);

	/* remove temp space */
	ret = asb_utils_rmtree ("/tmp/asbuilder", &error);
	g_assert_no_error (error);
	g_assert (ret);
#endif
}

static void
asb_test_context_oldcache_func (void)
{
#ifdef HAVE_RPM
	GError *error = NULL;
	gboolean ret;

	/* run again, this time using the old metadata as a cache */
	asb_test_context_test_func (ASB_TEST_CONTEXT_MODE_WITH_OLD_CACHE);

	/* remove temp space */
	ret = asb_utils_rmtree ("/tmp/asbuilder", &error);
	g_assert_no_error (error);
	g_assert (ret);
#endif
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
	g_setenv ("ASB_IS_SELF_TEST", "", TRUE);

	/* tests go here */
	g_test_add_func ("/AppStreamBuilder/utils{replace}", asb_test_utils_replace_func);
	g_test_add_func ("/AppStreamBuilder/utils{glob}", asb_test_utils_glob_func);
	g_test_add_func ("/AppStreamBuilder/plugin-loader", asb_test_plugin_loader_func);
	g_test_add_func ("/AppStreamBuilder/context{no-cache}", asb_test_context_nocache_func);
	g_test_add_func ("/AppStreamBuilder/context{cache}", asb_test_context_cache_func);
	g_test_add_func ("/AppStreamBuilder/context{old-cache}", asb_test_context_oldcache_func);
#ifdef HAVE_RPM
	g_test_add_func ("/AppStreamBuilder/package{rpm}", asb_test_package_rpm_func);
#endif
	return g_test_run ();
}

