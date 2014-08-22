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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * SECTION:asb-panel
 * @short_description: A panel for showing parallel tasks.
 * @stability: Unstable
 *
 * This object provides a console panel showing tasks and thier statuses.
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "as-cleanup.h"
#include "asb-panel.h"

typedef struct _AsbPanelPrivate	AsbPanelPrivate;
struct _AsbPanelPrivate
{
	GPtrArray		*items;
	GMutex			 mutex;
	GTimer			*timer;
	gint			 tty_fd;
	guint			 job_number_max;
	guint			 job_total;
	guint			 line_width_max;
	guint			 number_cleared;
	guint			 title_width;
	guint			 title_width_max;
	guint			 time_secs_min;
	gboolean		 enabled;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsbPanel, asb_panel, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_panel_get_instance_private (o))

typedef struct {
	guint		 job_number;
	gchar		*title;
	gchar		*status;
	GThread		*thread;
} AsbPanelItem;

/**
 * asb_panel_item_free:
 **/
static void
asb_panel_item_free (AsbPanelItem *item)
{
	g_free (item->title);
	g_free (item->status);
	g_slice_free (AsbPanelItem, item);
}

/**
 * asb_panel_print_raw:
 **/
static gssize
asb_panel_print_raw (AsbPanel *panel, const gchar *tmp)
{
	AsbPanelPrivate *priv = GET_PRIVATE (panel);
	gssize count;
	gssize wrote;

	count = strlen (tmp) + 1;
	wrote = write (priv->tty_fd, tmp, count);
	if (wrote != count) {
		g_warning ("Only wrote %" G_GSSIZE_FORMAT
			   " of %" G_GSSIZE_FORMAT " bytes",
			   wrote, count);
	}
	return count;
}

/**
 * asb_panel_print:
 **/
G_GNUC_PRINTF(2,3) static void
asb_panel_print (AsbPanel *panel, const gchar *fmt, ...)
{
	AsbPanelPrivate *priv = GET_PRIVATE (panel);
	va_list args;
	gsize len;
	guint i;
	_cleanup_free_ gchar *startline = NULL;
	_cleanup_free_ gchar *tmp = NULL;
	_cleanup_string_free_ GString *str = NULL;

	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);

	/* pad out to the largest line we've seen */
	str = g_string_new (tmp);
	for (i = str->len; i < priv->line_width_max; i++)
		g_string_append (str, " ");

	/* don't do console cleverness in make check */
	if (!priv->enabled) {
		g_debug ("%s", str->str);
		priv->line_width_max = str->len - 1;
		return;
	}

	/* is this bigger than anything else we've seen? */
	len = asb_panel_print_raw (panel, str->str);
	if (len - 1 > priv->line_width_max)
		priv->line_width_max = len - 1;

	/* go back to the start of the line and drop down one line */
	startline = g_strdup_printf ("\033[%" G_GSIZE_FORMAT "D\033[1B", len);
	asb_panel_print_raw (panel, startline);
}

/**
 * asb_panel_get_time_string:
 **/
static gchar *
asb_panel_get_time_string (AsbPanel *panel)
{
	AsbPanelPrivate *priv = GET_PRIVATE (panel);
	guint seconds;

	/* not enough jobs to get an accurate time */
	if (priv->job_number_max < 20)
		return g_strdup ("??");

	/* calculate time */
	seconds = (g_timer_elapsed (priv->timer, NULL) / (gdouble) priv->job_number_max) *
		   (gdouble) (priv->job_total - priv->job_number_max);
	if (seconds < priv->time_secs_min)
		priv->time_secs_min = seconds;
	if (priv->time_secs_min > 60)
		return g_strdup_printf ("~%im", priv->time_secs_min / 60);
	return g_strdup_printf ("~%is", priv->time_secs_min);
}

/**
 * asb_panel_refresh:
 **/
static void
asb_panel_refresh (AsbPanel *panel)
{
	AsbPanelItem *item = NULL;
	AsbPanelPrivate *priv = GET_PRIVATE (panel);
	guint i;
	guint j;

	g_mutex_lock (&priv->mutex);

	/* clear four lines of blank */
	if (priv->enabled && priv->number_cleared < priv->items->len) {
		for (i = 0; i < priv->items->len + 1; i++)
			asb_panel_print_raw (panel, "\n");
		for (i = 0; i < priv->items->len + 1; i++)
			asb_panel_print_raw (panel, "\033[1A");
		priv->number_cleared = priv->items->len;
	}

	/* find existing and print status lines */
	for (i = 0; i < priv->items->len; i++) {
		_cleanup_string_free_ GString *str = NULL;
		item = g_ptr_array_index (priv->items, i);
		str = g_string_new (item->title);
		if (str->len > priv->title_width_max)
			g_string_truncate (str, priv->title_width_max);
		for (j = str->len; j < priv->title_width; j++)
			g_string_append (str, " ");
		if (priv->title_width < str->len)
			priv->title_width = str->len;
		if (item->status != NULL) {
			g_string_append (str, " ");
			g_string_append (str, item->status);
		}
		asb_panel_print (panel, "%s", str->str);
	}

	/* any slots now unused */
	for (i = i; i < priv->number_cleared; i++)
		asb_panel_print (panel, "Thread unused");

	/* print percentage completion */
	if (item != NULL) {
		_cleanup_free_ gchar *time_str = NULL;
		time_str = asb_panel_get_time_string (panel);
		asb_panel_print (panel, "Done: %.1f%% [%s]",
				 (gdouble) priv->job_number_max * 100.f /
				 (gdouble) priv->job_total,
				 time_str);
	} else {
		asb_panel_print (panel, "Done: 100.0%%");
	}

	/* go back up to the start */
	if (priv->enabled) {
		for (i = 0; i < priv->number_cleared + 1; i++)
			asb_panel_print_raw (panel, "\033[1A");
	}

	g_mutex_unlock (&priv->mutex);
}

/**
 * asb_panel_ensure_item:
 **/
static AsbPanelItem *
asb_panel_ensure_item (AsbPanel *panel)
{
	AsbPanelPrivate *priv = GET_PRIVATE (panel);
	AsbPanelItem *item;
	guint i;

	/* find existing */
	g_mutex_lock (&priv->mutex);
	for (i = 0; i < priv->items->len; i++) {
		item = g_ptr_array_index (priv->items, i);
		if (item->thread == g_thread_self ())
			goto out;
	}

	/* create */
	item = g_slice_new0 (AsbPanelItem);
	item->thread = g_thread_self ();
	g_ptr_array_add (priv->items, item);
out:
	g_mutex_unlock (&priv->mutex);
	return item;
}

/**
 * asb_panel_set_job_number:
 * @panel: A #AsbPanel
 * @job_number: numeric identifier
 *
 * Sets the job number for the task.
 *
 * Since: 0.2.3
 **/
void
asb_panel_set_job_number (AsbPanel *panel, guint job_number)
{
	AsbPanelItem *item;
	AsbPanelPrivate *priv = GET_PRIVATE (panel);

	/* record the highest seen job number for % calculations */
	if (job_number > priv->job_number_max)
		priv->job_number_max = job_number;

	item = asb_panel_ensure_item (panel);
	item->job_number = job_number;
	asb_panel_refresh (panel);
}

/**
 * asb_panel_remove:
 * @panel: A #AsbPanel
 *
 * Remove the currently running task.
 *
 * Since: 0.2.3
 **/
void
asb_panel_remove (AsbPanel *panel)
{
	AsbPanelItem *item;
	AsbPanelPrivate *priv = GET_PRIVATE (panel);

	item = asb_panel_ensure_item (panel);
	g_mutex_lock (&priv->mutex);
	g_ptr_array_remove (priv->items, item);
	g_mutex_unlock (&priv->mutex);
	asb_panel_refresh (panel);
}

/**
 * asb_panel_set_title:
 * @panel: A #AsbPanel
 * @title: text to display
 *
 * Sets the title for the currently running task.
 *
 * Since: 0.2.3
 **/
void
asb_panel_set_title (AsbPanel *panel, const gchar *title)
{
	AsbPanelItem *item;
	item = asb_panel_ensure_item (panel);
	g_free (item->title);
	item->title = g_strdup (title);
	asb_panel_refresh (panel);
}

/**
 * asb_panel_set_status:
 * @panel: A #AsbPanel
 * @fmt: format string
 * @...: varargs
 *
 * Sets the status for the currently running task.
 *
 * Since: 0.2.3
 **/
void
asb_panel_set_status (AsbPanel *panel, const gchar *fmt, ...)
{
	AsbPanelItem *item;
	va_list args;

	item = asb_panel_ensure_item (panel);
	g_free (item->status);

	va_start (args, fmt);
	item->status = g_strdup_vprintf (fmt, args);
	va_end (args);

	asb_panel_refresh (panel);
}

/**
 * asb_panel_set_job_total:
 * @panel: A #AsbPanel
 * @job_number: numeric identifier
 *
 * Sets the largest job number for all of the tasks.
 *
 * Since: 0.2.3
 **/
void
asb_panel_set_job_total (AsbPanel *panel, guint job_total)
{
	AsbPanelPrivate *priv = GET_PRIVATE (panel);
	priv->job_total = job_total;
}

/**
 * asb_panel_finalize:
 **/
static void
asb_panel_finalize (GObject *object)
{
	AsbPanel *panel = ASB_PANEL (object);
	AsbPanelPrivate *priv = GET_PRIVATE (panel);

	g_ptr_array_unref (priv->items);
	g_mutex_clear (&priv->mutex);
	g_timer_destroy (priv->timer);

	G_OBJECT_CLASS (asb_panel_parent_class)->finalize (object);
}

/**
 * asb_panel_init:
 **/
static void
asb_panel_init (AsbPanel *panel)
{
	AsbPanelPrivate *priv = GET_PRIVATE (panel);
	priv->items = g_ptr_array_new_with_free_func ((GDestroyNotify) asb_panel_item_free);
	priv->title_width = 20;
	priv->title_width_max = 60;
	priv->timer = g_timer_new ();
	priv->time_secs_min = G_MAXUINT;
	g_mutex_init (&priv->mutex);

	/* self test fix */
	priv->enabled = g_getenv ("ASB_IS_SELF_TEST") == NULL;

	/* find an actual TTY */
	priv->tty_fd = open ("/dev/tty", O_RDWR, 0);
	if (priv->tty_fd < 0)
		priv->tty_fd = open ("/dev/console", O_RDWR, 0);
	if (priv->tty_fd < 0)
		priv->tty_fd = open ("/dev/stdout", O_RDWR, 0);
	g_assert (priv->tty_fd > 0);
}

/**
 * asb_panel_class_init:
 **/
static void
asb_panel_class_init (AsbPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asb_panel_finalize;
}

/**
 * asb_panel_new:
 *
 * Creates a new panel.
 *
 * Returns: A #AsbPanel
 *
 * Since: 0.2.3
 **/
AsbPanel *
asb_panel_new (void)
{
	AsbPanel *panel;
	panel = g_object_new (ASB_TYPE_PANEL, NULL);
	return ASB_PANEL (panel);
}
