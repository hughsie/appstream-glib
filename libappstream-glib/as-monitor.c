/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:as-monitor
 * @short_description: a hashed array monitor of applications
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * This object will monitor a set of directories for changes.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-monitor.h"

typedef struct
{
	GPtrArray		*array;		/* of GFileMonitor */
	GPtrArray		*files;		/* of gchar* */
	GPtrArray		*queue_add;	/* of gchar* */
	GPtrArray		*queue_changed;	/* of gchar* */
	GPtrArray		*queue_temp;	/* of gchar* */
	guint			 pending_id;
} AsMonitorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsMonitor, as_monitor, G_TYPE_OBJECT)

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

#define GET_PRIVATE(o) (as_monitor_get_instance_private (o))

/**
 * as_monitor_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.4.2
 **/
G_DEFINE_QUARK (as-monitor-error-quark, as_monitor_error)

static void
as_monitor_finalize (GObject *object)
{
	AsMonitor *monitor = AS_MONITOR (object);
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);

	if (priv->pending_id)
		g_source_remove (priv->pending_id);
	g_ptr_array_unref (priv->array);
	g_ptr_array_unref (priv->files);
	g_ptr_array_unref (priv->queue_add);
	g_ptr_array_unref (priv->queue_changed);
	g_ptr_array_unref (priv->queue_temp);

	G_OBJECT_CLASS (as_monitor_parent_class)->finalize (object);
}

static void
as_monitor_init (AsMonitor *monitor)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->files = g_ptr_array_new_with_free_func (g_free);
	priv->queue_add = g_ptr_array_new_with_free_func (g_free);
	priv->queue_changed = g_ptr_array_new_with_free_func (g_free);
	priv->queue_temp = g_ptr_array_new_with_free_func (g_free);
}

static void
as_monitor_class_init (AsMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * AsMonitor::added:
	 * @monitor: the #AsMonitor instance that emitted the signal
	 * @filename: the filename
	 *
	 * The ::changed signal is emitted when a file has been added.
	 *
	 * Since: 0.4.2
	 **/
	signals [SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsMonitorClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * AsMonitor::removed:
	 * @monitor: the #AsMonitor instance that emitted the signal
	 * @filename: the filename
	 *
	 * The ::changed signal is emitted when a file has been removed.
	 *
	 * Since: 0.4.2
	 **/
	signals [SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsMonitorClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * AsMonitor::changed:
	 * @monitor: the #AsMonitor instance that emitted the signal
	 * @filename: the filename
	 *
	 * The ::changed signal is emitted when a watched file has changed.
	 *
	 * Since: 0.4.2
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsMonitorClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	object_class->finalize = as_monitor_finalize;
}

static const gchar *
_g_file_monitor_to_string (GFileMonitorEvent ev)
{
	if (ev == G_FILE_MONITOR_EVENT_CHANGED)
		return "CHANGED";
	if (ev == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
		return "CHANGES_DONE_HINT";
	if (ev == G_FILE_MONITOR_EVENT_DELETED)
		return "DELETED";
	if (ev == G_FILE_MONITOR_EVENT_CREATED)
		return "CREATED";
	if (ev == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
		return "ATTRIBUTE_CHANGED";
	if (ev == G_FILE_MONITOR_EVENT_PRE_UNMOUNT)
		return "PRE_UNMOUNT";
	if (ev == G_FILE_MONITOR_EVENT_UNMOUNTED)
		return "UNMOUNTED";
	if (ev == G_FILE_MONITOR_EVENT_MOVED)
		return "MOVED";
	if (ev == G_FILE_MONITOR_EVENT_RENAMED)
		return "RENAMED";
	if (ev == G_FILE_MONITOR_EVENT_MOVED_IN)
		return "MOVED_IN";
	if (ev == G_FILE_MONITOR_EVENT_MOVED_OUT)
		return "MOVED_OUT";
	g_warning ("Failed to convert GFileMonitorEvent %u", ev);
	return NULL;
}

static const gchar *
_g_ptr_array_str_find (GPtrArray *array, const gchar *fn)
{
	guint i;
	const gchar *tmp;
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (tmp, fn) == 0)
			return fn;
	}
	return NULL;
}

static gboolean
_g_ptr_array_str_remove (GPtrArray *array, const gchar *fn)
{
	guint i;
	const gchar *tmp;
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (tmp, fn) == 0) {
			g_ptr_array_remove_index_fast (array, i);
			return TRUE;
		}
	}
	return FALSE;
}

static void
_g_ptr_array_str_add (GPtrArray *array, const gchar *fn)
{
	if (_g_ptr_array_str_find (array, fn) != NULL)
		return;
	g_ptr_array_add (array, g_strdup (fn));
}

static void
as_monitor_emit_added (AsMonitor *monitor, const gchar *filename)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	g_debug ("Emit ::added(%s)", filename);
	g_signal_emit (monitor, signals[SIGNAL_ADDED], 0, filename);
	_g_ptr_array_str_add (priv->files, filename);
}

static void
as_monitor_emit_removed (AsMonitor *monitor, const gchar *filename)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	g_debug ("Emit ::removed(%s)", filename);
	g_signal_emit (monitor, signals[SIGNAL_REMOVED], 0, filename);
	_g_ptr_array_str_remove (priv->files, filename);
}

static void
as_monitor_emit_changed (AsMonitor *monitor, const gchar *filename)
{
	g_debug ("Emit ::changed(%s)", filename);
	g_signal_emit (monitor, signals[SIGNAL_CHANGED], 0, filename);
}

static void
as_monitor_process_pending (AsMonitor *monitor)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	guint i;
	const gchar *tmp;

	/* stop the timer */
	if (priv->pending_id) {
		g_source_remove (priv->pending_id);
		priv->pending_id = 0;
	}

	/* emit all the pending changed signals */
	for (i = 0; i < priv->queue_changed->len; i++) {
		tmp = g_ptr_array_index (priv->queue_changed, i);
		as_monitor_emit_changed (monitor, tmp);
	}
	g_ptr_array_set_size (priv->queue_changed, 0);

	/* emit all the pending add signals */
	for (i = 0; i < priv->queue_add->len; i++) {
		tmp = g_ptr_array_index (priv->queue_add, i);
		/* did we atomically replace an existing file */
		if (_g_ptr_array_str_find (priv->files, tmp) != NULL) {
			g_debug ("detecting atomic replace of existing file");
			as_monitor_emit_changed (monitor, tmp);
		} else {
			as_monitor_emit_added (monitor, tmp);
		}
	}
	g_ptr_array_set_size (priv->queue_add, 0);
}

static gboolean
as_monitor_process_pending_trigger_cb (gpointer user_data)
{
	AsMonitor *monitor = AS_MONITOR (user_data);
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);

	g_debug ("No CHANGES_DONE_HINT, catching in timeout");
	as_monitor_process_pending (monitor);
	priv->pending_id = 0;
	return FALSE;
}

static void
as_monitor_process_pending_trigger (AsMonitor *monitor, guint timeout_ms)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	if (priv->pending_id)
		g_source_remove (priv->pending_id);
	priv->pending_id = g_timeout_add (timeout_ms,
					  as_monitor_process_pending_trigger_cb,
					  monitor);
}

/**
 * as_monitor_file_changed_cb:
 *
 * touch newfile      -> CREATED+CHANGED+ATTRIBUTE_CHANGED+CHANGES_DONE_HINT
 *                       or, just CREATED
 * touch newfile      -> ATTRIBUTE_CHANGED+CHANGES_DONE_HINT
 * echo "1" > newfile -> CHANGED+CHANGES_DONE_HINT
 * rm newfile         -> DELETED
 **/
static void
as_monitor_file_changed_cb (GFileMonitor *mon,
				 GFile *file, GFile *other_file,
				 GFileMonitorEvent event_type,
				 AsMonitor *monitor)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	const gchar *tmp;
	gboolean is_temp;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *filename = NULL;
	g_autofree gchar *filename_other = NULL;

	/* get both filenames */
	filename = g_file_get_path (file);
	is_temp = !g_file_test (filename, G_FILE_TEST_EXISTS);
	if (other_file != NULL)
		filename_other = g_file_get_path (other_file);
	g_debug ("modified: %s %s [%i]", filename,
		_g_file_monitor_to_string (event_type), is_temp);

	/* ignore hidden files */
	basename = g_path_get_basename (filename);
	if (g_str_has_prefix (basename, ".")) {
		g_debug ("ignoring hidden file");
		return;
	}
	if (g_str_has_suffix (basename, ".swx") ||
	    g_str_has_suffix (basename, ".swp")) {
		g_debug ("ignoring temp file");
		return;
	}

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		as_monitor_process_pending_trigger (monitor, 50);
		break;
	case G_FILE_MONITOR_EVENT_CREATED:
	case G_FILE_MONITOR_EVENT_MOVED_IN:
		if (!is_temp) {
			_g_ptr_array_str_add (priv->queue_add, filename);
		} else {
			_g_ptr_array_str_add (priv->queue_temp, filename);
		}
		/* file monitors do not send CHANGES_DONE_HINT */
		as_monitor_process_pending_trigger (monitor, 800);
		break;
	case G_FILE_MONITOR_EVENT_DELETED:
	case G_FILE_MONITOR_EVENT_MOVED_OUT:
		/* only emit notifications for files we know about */
		if (_g_ptr_array_str_find (priv->files, filename)) {
			as_monitor_emit_removed (monitor, filename);
		} else {
			g_debug ("ignoring deleted file %s", filename);
		}
		break;
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
		/* if the file is not pending and not a temp file, add */
		if (_g_ptr_array_str_find (priv->queue_add, filename) == NULL &&
		    _g_ptr_array_str_find (priv->queue_temp, filename) == NULL) {
			_g_ptr_array_str_add (priv->queue_changed, filename);
		}
		as_monitor_process_pending_trigger (monitor, 800);
		break;
	case G_FILE_MONITOR_EVENT_RENAMED:
		/* a temp file that was just created and atomically
		 * renamed to its final destination */
		tmp = _g_ptr_array_str_find (priv->queue_temp, filename);
		if (tmp != NULL) {
			g_debug ("detected atomic save, adding %s", filename_other);
			g_ptr_array_remove_fast (priv->queue_temp, (gpointer) tmp);
			if (_g_ptr_array_str_find (priv->files, filename_other) != NULL)
				as_monitor_emit_changed (monitor, filename_other);
			else
				as_monitor_emit_added (monitor, filename_other);
		} else {
			g_debug ("detected rename, treating it as remove->add");
			as_monitor_emit_removed (monitor, filename);
			as_monitor_emit_added (monitor, filename_other);
		}
		break;
	default:
		break;
	}
}

/**
 * as_monitor_add_directory:
 * @monitor: an #AsMonitor
 * @filename: directory name
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError or %NULL
 *
 * Adds a directory of files to the watch list.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.0
 **/
gboolean
as_monitor_add_directory (AsMonitor *monitor,
			  const gchar *filename,
			  GCancellable *cancellable,
			  GError **error)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	const gchar *tmp;
	g_autoptr(GFileMonitor) mon = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GDir) dir = NULL;

	g_return_val_if_fail (AS_IS_MONITOR (monitor), FALSE);

	/* find the files already in the directory */
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		dir = g_dir_open (filename, 0, error);
		if (dir == NULL)
			return FALSE;
		while ((tmp = g_dir_read_name (dir)) != NULL) {
			g_autofree gchar *fn = NULL;
			fn = g_build_filename (filename, tmp, NULL);
			g_debug ("adding existing file: %s", fn);
			_g_ptr_array_str_add (priv->files, fn);
		}
	}

	/* create new file monitor */
	file = g_file_new_for_path (filename);
	mon = g_file_monitor_directory (file, G_FILE_MONITOR_WATCH_MOVES,
					cancellable, error);
	if (mon == NULL)
		return FALSE;
	g_signal_connect (mon, "changed",
			  G_CALLBACK (as_monitor_file_changed_cb), monitor);
	g_ptr_array_add (priv->array, g_object_ref (mon));

	return TRUE;
}

/**
 * as_monitor_add_file:
 * @monitor: an #AsMonitor
 * @filename: a filename
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError or %NULL
 *
 * Adds a file to the watch list.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.0
 **/
gboolean
as_monitor_add_file (AsMonitor *monitor,
		     const gchar *filename,
		     GCancellable *cancellable,
		     GError **error)
{
	AsMonitorPrivate *priv = GET_PRIVATE (monitor);
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFileMonitor) mon = NULL;

	g_return_val_if_fail (AS_IS_MONITOR (monitor), FALSE);

	/* already watched */
	if (_g_ptr_array_str_find (priv->files, filename) != NULL)
		return TRUE;

	/* create new file monitor */
	file = g_file_new_for_path (filename);
	mon = g_file_monitor_file (file, G_FILE_MONITOR_NONE,
				   cancellable, error);
	if (mon == NULL)
		return FALSE;
	g_signal_connect (mon, "changed",
			  G_CALLBACK (as_monitor_file_changed_cb), monitor);
	g_ptr_array_add (priv->array, g_object_ref (mon));

	/* only add if actually exists */
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		_g_ptr_array_str_add (priv->files, filename);

	return TRUE;
}

/**
 * as_monitor_new:
 *
 * Creates a new #AsMonitor.
 *
 * Returns: (transfer full): a #AsMonitor
 *
 * Since: 0.4.2
 **/
AsMonitor *
as_monitor_new (void)
{
	AsMonitor *monitor;
	monitor = g_object_new (AS_TYPE_MONITOR, NULL);
	return AS_MONITOR (monitor);
}
