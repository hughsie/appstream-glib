/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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

#include <config.h>
#include <fnmatch.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "gstreamer";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/lib64/gstreamer-1.0/libgst*.so");
}

typedef struct {
	const gchar *path;
	const gchar *text;
} AsbGstreamerDescData;

static const AsbGstreamerDescData data[] = {
	{ "/usr/lib64/gstreamer-1.0/libgsta52dec.so",	"AC-3" },
	{ "/usr/lib64/gstreamer-1.0/libgstaiff.so",	"AIFF" },
	{ "/usr/lib64/gstreamer-1.0/libgstamrnb.so",	"AMR-NB" },
	{ "/usr/lib64/gstreamer-1.0/libgstamrwbdec.so",	"AMR-WB" },
	{ "/usr/lib64/gstreamer-1.0/libgstapetag.so",	"APE" },
	{ "/usr/lib64/gstreamer-1.0/libgstasf.so",	"ASF" },
	{ "/usr/lib64/gstreamer-1.0/libgstavi.so",	"AVI" },
	{ "/usr/lib64/gstreamer-1.0/libgstavidemux.so",	"AVI" },
	{ "/usr/lib64/gstreamer-1.0/libgstdecklink.so",	"SDI" },
	{ "/usr/lib64/gstreamer-1.0/libgstdtsdec.so",	"DTS" },
	{ "/usr/lib64/gstreamer-1.0/libgstdv.so",	"DV" },
	{ "/usr/lib64/gstreamer-1.0/libgstdvb.so",	"DVB" },
	{ "/usr/lib64/gstreamer-1.0/libgstdvdread.so",	"DVD" },
	{ "/usr/lib64/gstreamer-1.0/libgstdvdspu.so",	"Bluray" },
	{ "/usr/lib64/gstreamer-1.0/libgstespeak.so",	"eSpeak" },
	{ "/usr/lib64/gstreamer-1.0/libgstfaad.so",	"MPEG-4|MPEG-2 AAC" },
	{ "/usr/lib64/gstreamer-1.0/libgstflac.so",	"FLAC" },
	{ "/usr/lib64/gstreamer-1.0/libgstflv.so",	"Flash" },
	{ "/usr/lib64/gstreamer-1.0/libgstflxdec.so",	"FLX" },
	{ "/usr/lib64/gstreamer-1.0/libgstgsm.so",	"GSM" },
	{ "/usr/lib64/gstreamer-1.0/libgstid3tag.so",	"ID3" },
	{ "/usr/lib64/gstreamer-1.0/libgstisomp4.so",	"MP4" },
	{ "/usr/lib64/gstreamer-1.0/libgstmad.so",	"MP3" },
	{ "/usr/lib64/gstreamer-1.0/libgstmatroska.so",	"MKV" },
	{ "/usr/lib64/gstreamer-1.0/libgstmfc.so",	"MFC" },
	{ "/usr/lib64/gstreamer-1.0/libgstmidi.so",	"MIDI" },
	{ "/usr/lib64/gstreamer-1.0/libgstmimic.so",	"Mimic" },
	{ "/usr/lib64/gstreamer-1.0/libgstmms.so",	"MMS" },
	{ "/usr/lib64/gstreamer-1.0/libgstmpeg2dec.so",	"MPEG-2" },
	{ "/usr/lib64/gstreamer-1.0/libgstmpg123.so",	"MP3" },
	{ "/usr/lib64/gstreamer-1.0/libgstmxf.so",	"MXF" },
	{ "/usr/lib64/gstreamer-1.0/libgstogg.so",	"Ogg" },
	{ "/usr/lib64/gstreamer-1.0/libgstopus.so",	"Opus" },
	{ "/usr/lib64/gstreamer-1.0/libgstrmdemux.so",	"RealMedia" },
	{ "/usr/lib64/gstreamer-1.0/libgstschro.so",	"Dirac" },
	{ "/usr/lib64/gstreamer-1.0/libgstsiren.so",	"Siren" },
	{ "/usr/lib64/gstreamer-1.0/libgstspeex.so",	"Speex" },
	{ "/usr/lib64/gstreamer-1.0/libgsttheora.so",	"Theora" },
	{ "/usr/lib64/gstreamer-1.0/libgsttwolame.so",	"MP2" },
	{ "/usr/lib64/gstreamer-1.0/libgstvorbis.so",	"Vorbis" },
	{ "/usr/lib64/gstreamer-1.0/libgstvpx.so",	"VP8|VP9" },
	{ "/usr/lib64/gstreamer-1.0/libgstwavenc.so",	"WAV" },
	{ "/usr/lib64/gstreamer-1.0/libgstx264.so",	"H.264/MPEG-4 AVC" },
	{ NULL,		NULL }
};

/**
 * asb_utils_is_file_in_tmpdir:
 */
static gboolean
asb_utils_is_file_in_tmpdir (const gchar *tmpdir, const gchar *filename)
{
	g_autofree gchar *tmp = NULL;
	tmp = g_build_filename (tmpdir, filename, NULL);
	return g_file_test (tmp, G_FILE_TEST_EXISTS);
}

/**
 * asb_plugin_process_app:
 */
gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	guint i;
	guint j;

	for (i = 0; data[i].path != NULL; i++) {
		g_auto(GStrv) split = NULL;
		if (!asb_utils_is_file_in_tmpdir (tmpdir, data[i].path))
			continue;
		split = g_strsplit (data[i].text, "|", -1);
		for (j = 0; split[j] != NULL; j++)
			as_app_add_keyword (AS_APP (app), NULL, split[j]);
	}

	return TRUE;
}
