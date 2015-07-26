/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Ikey Doherty <ikey@solus-project.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * SECTION:asb-package-eopkg
 * @short_description: Object representing a .EOPKG package file.
 * @stability: Unstable
 *
 * This object represents one .eopkg package file.
 */

#include "config.h"

#include "as-cleanup.h"
#include "asb-package-eopkg.h"
#include "asb-plugin.h"

#include <archive.h>
#include <archive_entry.h>
#include <libxml/xmlreader.h>
#include <errno.h>
#include <string.h>

G_DEFINE_TYPE (AsbPackageEopkg, asb_package_eopkg, ASB_TYPE_PACKAGE)

/**
 * Storage for eopkg metadata
 */
typedef struct eopkg_meta_t {
	gchar *name;		/**<Binary package name */
	gchar *source;		/**<Distro source name */
	gint release;		/**<Release number */
	gchar *version;		/**<Package version */
	gchar *url;		/**<Upstream URL, i.e. homepage */
	GSList *deps;		/**<List of string-name dependencies */
	GSList *licenses;	/**<List of licenses (usually SPDX) */
} eopkg_meta_t;

/**
 * State tracking for metadata.xml traversal
 */
typedef struct meta_state_t {
	gboolean in_name;
	gboolean in_source;
	gboolean in_packager;
	gboolean in_url;
	gboolean in_dep;
	gboolean in_rundeps;
	gboolean in_license;
	gboolean in_package;
	gboolean in_update;
	gboolean in_version;
	gboolean need_update;
} meta_state_t;

/**
 * State tracking for files.xml traversal
 */
typedef struct file_state_t {
	gboolean in_file;
	gboolean in_path;
} file_state_t;

/**
 * Complete binary .eopkg representation
 */
typedef struct eopkg_t {
	eopkg_meta_t *meta;	/**<Metadata */
	GPtrArray *files;	/**<List of files (not directories) */
} eopkg_t;


/**
 * Process metadata.xml node
 */
gboolean process_meta_node(meta_state_t *self, eopkg_meta_t *meta, xmlTextReaderPtr r);

/**
 * Examine a metadata file, returning the appropriate storage when complete
 */
eopkg_meta_t *examine_metadata(const char *filename);

/**
 * Process a files.xml node
 */
gboolean process_file_node(file_state_t *self, GPtrArray *ret, xmlTextReaderPtr r);

/**
 * Examine a files xml file, returning the appropriate storage when complete
 */
GPtrArray *examine_files(const char *filename);

/**
 * Utility to free an eopkg_meta_t
 */
void eopkg_meta_free(eopkg_meta_t *eopkg);

/**
 * Free sources for an eopkg_t
 */
void close_eopkg(eopkg_t *eopkg);

/**
 * Open, and inspect, the archive identified by filename. This must be an
 * .eopkg file
 */
eopkg_t *open_eopkg(const char *filename);

gboolean process_meta_node(meta_state_t *self, eopkg_meta_t *meta, xmlTextReaderPtr r)
{
	const xmlChar *name = NULL;
	int rel;
	const xmlChar *val = NULL;

	name = xmlTextReaderConstName(r);
	if (!name) {
		return FALSE;
	}

	if (xmlStrEqual(name, BAD_CAST "Source")) {
		self->in_source = !self->in_source;
	} else if (xmlStrEqual(name, BAD_CAST "Package")) {
		self->in_package = !self->in_package;
	} else if (xmlStrEqual(name, BAD_CAST "Update")) {
		self->in_update = !self->in_update;
		if (self->in_update) {
			xmlChar *attr = xmlTextReaderGetAttribute(r, BAD_CAST "release");
			if (!attr) {
				fprintf(stderr, "Malformed spec: No release ID\n");
				return FALSE;
			}
			rel = atoi((const char*)attr);
			if (rel > meta->release) {
				meta->release = rel;
				self->need_update = TRUE;
			}
			xmlFree(attr);
		}
	}

	if (self->in_source) {
		if (xmlStrEqual(name, BAD_CAST "Name")) {
			self->in_name = !self->in_name;
		} else if (xmlStrEqual(name, BAD_CAST "Packager")) {
			self->in_packager = !self->in_packager;
		} else if (xmlStrEqual(name, BAD_CAST "Homepage")) {
			self->in_url = !self->in_url;
		}
		if (self->in_name && !self->in_packager && !meta->source) {
			val = xmlTextReaderConstValue(r);
			if (!val) {
				return TRUE;
			}
			meta->source = g_strdup((gchar*)val);
		} else if (self->in_url && !meta->url) {
			val = xmlTextReaderConstValue(r);
			if (!val) {
				return TRUE;
			}
			meta->url = g_strdup((gchar*)val);
		}
	} else if (self->in_package) {
		if (xmlStrEqual(name, BAD_CAST "Name")) {
			self->in_name = !self->in_name;
		} else if (xmlStrEqual(name, BAD_CAST "License")) {
			self->in_license = !self->in_license;
		} else if (xmlStrEqual(name, BAD_CAST "RuntimeDependencies")) {
			self->in_rundeps = !self->in_rundeps;
		}
		if (self->in_name && !self->in_update) {
			val = xmlTextReaderConstValue(r);
			if (!val) {
				return TRUE;
			}
			meta->name = g_strdup((gchar*)val);
		} else if (self->in_license) {
			val = xmlTextReaderConstValue(r);
			if (!val) {
				return TRUE;
			}
			meta->licenses = g_slist_prepend(meta->licenses, g_strdup((gchar*)val));
		} else if (self->in_rundeps) {
			if (xmlStrEqual(name, BAD_CAST "Dependency")) {
				self->in_dep = !self->in_dep;
			}
			val = xmlTextReaderConstValue(r);
			if (self->in_dep && val) {
				meta->deps = g_slist_prepend(meta->deps, g_strdup((gchar*)val));
			}
		} else if (self->in_update && self->need_update) {
			/* invariably we'll hit here only once from sorted (default) high-to-low history in spec */
			if (xmlStrEqual(name, BAD_CAST "Version")) {
				self->in_version = !self->in_version;
			}
			if (self->in_version) {
				val = xmlTextReaderConstValue(r);
				if (!val) {
					return TRUE;
				}
				if (meta->version) {
					g_free(meta->version);
				}
				meta->version = g_strdup((gchar*)val);
				self->need_update = FALSE;
			}
		}
	}
	return TRUE;
}

eopkg_meta_t *examine_metadata(const char *filename)
{
	xmlTextReaderPtr r = xmlReaderForFile(filename, NULL, 0);
	int ret;
	meta_state_t self = {0};
	eopkg_meta_t *meta = NULL;

	meta = calloc(1, sizeof(eopkg_meta_t));
	if (!meta) {
		fprintf(stderr, "OOM\n");
		return NULL;
	}

	while ((ret = xmlTextReaderRead(r)) > 0) {
		if (!process_meta_node(&self, meta, r)) {
			fprintf(stderr, "process_meta_node exited abnormally\n");
			break;
		}
	}

	xmlFreeTextReader(r);
	return meta;
}

gboolean process_file_node(file_state_t *self, GPtrArray *ret, xmlTextReaderPtr r)
{
	const xmlChar *name = NULL;

	name = xmlTextReaderConstName(r);
	if (!name) {
		return FALSE;
	}

	if (xmlStrEqual(name, BAD_CAST "File")) {
		self->in_file = !self->in_file;
	} else if (self->in_file && xmlStrEqual(name, BAD_CAST "Path")) {
		self->in_path = !self->in_path;
	} else if (self->in_path) {
		const xmlChar *val = xmlTextReaderConstValue(r);
		gchar *tmp = NULL;
		if (!val) {
			return TRUE;
		}
		if (val[0] != '/') {
			tmp = g_strdup_printf("/%s", (gchar*)val);
		} else {
			tmp = g_strdup((gchar*)val);
		}
		g_ptr_array_add(ret, tmp);
	}
	return TRUE;
}

GPtrArray *examine_files(const char *filename)
{
	xmlTextReaderPtr r = xmlReaderForFile(filename, NULL, 0);
	int ret;
	file_state_t self = {0};
	GPtrArray *arr = NULL;

	arr = g_ptr_array_new_with_free_func(g_free);
	if (!arr) {
		fprintf(stderr, "OOM\n");
		return NULL;
	}

	while ((ret = xmlTextReaderRead(r)) > 0) {
		if (!process_file_node(&self, arr, r)) {
			fprintf(stderr, "process_file_node exited abnormally\n");
			break;
		}
	}

	g_ptr_array_add(arr, NULL);

	xmlFreeTextReader(r);
	return arr;
}

void eopkg_meta_free(eopkg_meta_t *eopkg)
{
	if (!eopkg) {
		return;
	}
	if (eopkg->name) {
		g_free(eopkg->name);
	}
	if (eopkg->source) {
		g_free(eopkg->source);
	}
	if (eopkg->version) {
		g_free(eopkg->version);
	}
	if (eopkg->url) {
		g_free(eopkg->url);
	}
	if (eopkg->deps) {
		g_slist_free_full(eopkg->deps, g_free);
	}
	if (eopkg->licenses) {
		g_slist_free_full(eopkg->licenses, g_free);
	}
	free(eopkg);
}

void close_eopkg(eopkg_t *eopkg)
{
	if (eopkg->meta) {
		eopkg_meta_free(eopkg->meta);
	}
	if (eopkg->files) {
		g_ptr_array_unref(eopkg->files);
	}
	free(eopkg);
}

eopkg_t *open_eopkg(const char *filename)
{
	eopkg_t *ret = NULL;
	eopkg_meta_t *meta = NULL;
	GPtrArray *files = NULL;
	struct archive *a = NULL;
	struct archive_entry *entry = NULL;
	int r;
	char fname[PATH_MAX];
	char template[] = "/tmp/solus-eopkg-XXXXXX";
	int fd;

	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	/* open 'er up */
	r = archive_read_open_filename(a, filename, 10480);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "Unable to open archive\n");
		goto clean;
	}

	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		const char *name = archive_entry_pathname(entry);
		gboolean filesxml = FALSE;

		if ((filesxml = g_str_equal(name, "files.xml")) || g_str_equal(name, "metadata.xml")) {
			strncpy(fname, template, sizeof(template));
			fd = mkstemp(fname);
			if (fd <= 0) {
				fprintf(stderr, "Failed to open temporary file: %s\n", strerror(errno));
				goto clean;
			}

			r = archive_read_data_into_fd(a, fd);
			if (r != ARCHIVE_OK) {
				fprintf(stderr, "Failed to extra file: %s\n", name);
				close(fd);
				(void)unlink(fname);
				goto clean;
			}

			if (filesxml) {
				files = examine_files(fname);
			} else {
				meta = examine_metadata(fname);
			}
			close(fd);
			(void)unlink(fname);

		} else {
			archive_read_data_skip(a);
		}
	}

	if (!meta) {
		fprintf(stderr, "Failed to inspect metadata\n");
		goto bail;
	}
	if (!files) {
		fprintf(stderr, "Failed to inspect files\n");
		goto bail;
	}

	ret = calloc(1, sizeof(eopkg_t));
	if (!ret) {
		fprintf(stderr, "OOM\n");
		goto bail;
	}

	ret->meta = meta;
	ret->files = files;

clean:
	archive_read_free(a);

	return ret;

bail:
	if (meta) {
		eopkg_meta_free(meta);
	}
	if (files) {
		g_ptr_array_unref(files);
	}
	return NULL;
}

/**
 * asb_package_eopkg_init:
 **/
static void
asb_package_eopkg_init (AsbPackageEopkg *pkg)
{
}


/**
 * asb_package_eopkg_open:
 **/
static gboolean
asb_package_eopkg_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	eopkg_t *eopkg = NULL;
	gchar *rel = NULL;
	GSList *elem = NULL;

	eopkg = open_eopkg(filename);
	if (!eopkg)
		return FALSE;

	asb_package_set_name (pkg, eopkg->meta->name);
	asb_package_set_source (pkg, eopkg->meta->source);

	rel = g_strdup_printf ("%d", eopkg->meta->release);
	asb_package_set_release (pkg, rel);
	asb_package_set_version (pkg, eopkg->meta->version);
	asb_package_set_epoch (pkg, 1);
	g_free(rel);

	for (elem = eopkg->meta->deps; elem; elem = elem->next) {
		asb_package_add_dep (pkg, elem->data);
	}
	asb_package_set_filelist (pkg, (gchar**)eopkg->files->pdata);

	asb_package_set_license (pkg, eopkg->meta->licenses->data);

	close_eopkg(eopkg);

	return TRUE;
}

/**
 * asb_package_eopkg_explode:
 **/
static gboolean
asb_package_eopkg_explode (AsbPackage *pkg,
			 const gchar *dir,
			 GPtrArray *glob,
			 GError **error)
{
	const char *name = "install.tar.xz";
	_cleanup_free_ gchar *tpath = NULL;

	if (!asb_utils_explode (asb_package_get_filename (pkg),
		dir, NULL, error)) {
		return FALSE;
	}

	tpath = g_build_filename(dir, name, NULL);
	if (!g_file_test (tpath, G_FILE_TEST_EXISTS)) {
		return FALSE;
	}

	if (!asb_utils_explode (tpath, dir, glob, error)) {
		return FALSE;
	}

	return TRUE;
}

/**
 * asb_package_eopkg_class_init:
 **/
static void
asb_package_eopkg_class_init (AsbPackageEopkgClass *klass)
{
	AsbPackageClass *package_class = ASB_PACKAGE_CLASS (klass);
	package_class->open = asb_package_eopkg_open;
	package_class->explode = asb_package_eopkg_explode;
}

/**
 * asb_package_eopkg_new:
 *
 * Creates a new EOPKG package.
 *
 * Returns: a package
 *
 * Since: 0.1.0
 **/
AsbPackage *
asb_package_eopkg_new (void)
{
	AsbPackage *pkg;
	pkg = g_object_new (ASB_TYPE_PACKAGE_EOPKG, NULL);
	return ASB_PACKAGE (pkg);
}
