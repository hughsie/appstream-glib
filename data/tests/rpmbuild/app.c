/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 * SPDX-License-Identifier: LGPL-2.1+ */

#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
	GtkApplication *app;
	gtk_init (&argc, &argv);
	app = gtk_application_new ("org.freedesktop.AppStream", G_APPLICATION_FLAGS_NONE);
	gtk_application_get_menu_by_id (app, "not-going-to-exist");
	g_object_unref (app);
	return 0;
}
