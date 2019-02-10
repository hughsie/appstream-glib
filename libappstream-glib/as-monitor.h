/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AS_TYPE_MONITOR (as_monitor_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsMonitor, as_monitor, AS, MONITOR, GObject)

struct _AsMonitorClass
{
	GObjectClass		parent_class;
	void			(*added)	(AsMonitor	*monitor,
						 const gchar	*filename);
	void			(*removed)	(AsMonitor	*monitor,
						 const gchar	*filename);
	void			(*changed)	(AsMonitor	*monitor,
						 const gchar	*filename);
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
};

/**
 * AsMonitorError:
 * @AS_MONITOR_ERROR_FAILED:			Generic failure
 *
 * The error type.
 **/
typedef enum {
	AS_MONITOR_ERROR_FAILED,
	/*< private >*/
	AS_MONITOR_ERROR_LAST
} AsMonitorError;

#define	AS_MONITOR_ERROR			as_monitor_error_quark ()

AsMonitor	*as_monitor_new			(void);
GQuark		 as_monitor_error_quark		(void);

gboolean	 as_monitor_add_directory	(AsMonitor	*monitor,
						 const gchar	*filename,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 as_monitor_add_file		(AsMonitor	*monitor,
						 const gchar	*filename,
						 GCancellable	*cancellable,
						 GError		**error);

G_END_DECLS
