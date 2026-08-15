/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>
#include <fnmatch.h>
#include <gdk/gdk.h>

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangofc-fontmap.h>
#include <fontconfig/fontconfig.h>
#include <hb.h>
#include <hb-cairo.h>

#include <asb-plugin.h>

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-app-private.h>

const gchar *
asb_plugin_get_name (void)
{
	return "font";
}

void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/fonts/*/*.otf");
	asb_plugin_add_glob (globs, "/usr/share/fonts/*/*.ttc");
	asb_plugin_add_glob (globs, "/usr/share/fonts/*/*.ttf");
}

static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (asb_plugin_match_glob ("/usr/share/fonts/*/*.otf", filename))
		return TRUE;
	if (asb_plugin_match_glob ("/usr/share/fonts/*/*.ttc", filename))
		return TRUE;
	if (asb_plugin_match_glob ("/usr/share/fonts/*/*.ttf", filename))
		return TRUE;
	return FALSE;
}

static void
asb_font_fix_metadata (AsbApp *app)
{
	GList *l;
	PangoLanguage *plang;
	const gchar *tmp;
	const gchar *value;
	gint percentage;
	guint j;
	g_autoptr(GList) langs = NULL;
	g_autoptr(GString) str = NULL;
	struct {
		const gchar	*lang;
		const gchar	*value;
	} text_icon[] =  {
		{ "en",		"Aa" },
		{ "am",		"ግዕዝ" },
		{ "ar",		"أب" },
		{ "as",         "অআই" },
		{ "bn",         "অআই" },
		{ "be",		"Аа" },
		{ "bg",		"Аа" },
		{ "cs",		"Aa" },
		{ "da",		"Aa" },
		{ "de",		"Aa" },
		{ "es",		"Aa" },
		{ "fr",		"Aa" },
		{ "gu",		"અબક" },
		{ "hi",         "अआइ" },
		{ "he",		"אב" },
		{ "ii",		"ꆈꌠꁱꂷ" },
		{ "it",		"Aa" },
		{ "iu",		"ᐃᓄᒃᑎᑐᑦ" },
		{ "kn",         "ಅಆಇ" },
		{ "ml",		"ആഇ" },
		{ "ne",         "अआइ" },
		{ "nqo",	"ߒߞߏ" },
		{ "nl",		"Aa" },
		{ "or",         "ଅଆଇ" },
		{ "pa",         "ਅਆਇ" },
		{ "pl",		"ĄĘ" },
		{ "pt",		"Aa" },
		{ "ru",		"Аа" },
		{ "sv",		"Åäö" },
		{ "ta",         "அஆஇ" },
		{ "te",         "అఆఇ" },
		{ "ua",		"Аа" },
		{ "und-zsye",   "😀" },
		{ "zh-tw",	"漢" },
		{ NULL, NULL } };
	struct {
		const gchar	*lang;
		const gchar	*value;
	} text_sample[] =  {
		{ "en",		"How quickly daft jumping zebras vex." },
		{ "am",		"ኢትዮጵያ አፍሪካ ውስጥ  ናት" },
		{ "ar",		"نصٌّ حكيمٌ لهُ سِرٌّ قاطِعٌ وَذُو شَأنٍ عَظيمٍ مكتوبٌ على ثوبٍ أخضرَ ومُغلفٌ بجلدٍ أزرق" },
		{ "as",		"আর আপনি সাক্ষাৎ"},
		{ "bn",		"আর আপনি সাক্ষাৎ"},
		{ "be",		"У Іўі худы жвавы чорт у зялёнай камізэльцы пабег пад’есці фаршу з юшкай." },
		{ "bg",		"Под южно дърво, цъфтящо в синьо, бягаше малко, пухкаво зайче." },
		{ "cs",		"Příliš žluťoučký kůň úpěl ďábelské ódy" },
		{ "da",		"Quizdeltagerne spiste jordbær med fløde, mens cirkusklovnen Walther spillede på xylofon." },
		{ "de",		"Falsches Üben von Xylophonmusik quält jeden größeren Zwerg." },
		{ "es",		"Aquel biógrafo se zampó un extraño sándwich de vodka y ajo" },
		{ "fr",		"Voix ambiguë d'un cœur qui, au zéphyr, préfère les jattes de kiwis." },
		{ "gu",		"ઇ.સ. ૧૯૭૮ ની ૨૫ તારીખે, ૦૬-૩૪ વાગે, ઐશ્વર્યવાન, વફાદાર, અંગ્રેજ ઘરધણીના આ ઝાડ પાસે ઊભેલા બાદશાહ; અને ઓસરીમાંના ઠળીયા તથા છાણાના ઢગલા દુર કરીને, ઔપચારીકતાથી ઉભેલા ઋષી સમાન પ્રજ્ઞાચક્ષુ ખાલસાજી ભટ મળ્યા હતા." },
		{ "he",		"יַעֲקֹב בֶּן־דָּגָן הַשָּׂמֵחַ טִפֵּס בֶּעֱזוּז לְרֹאשׁ סֻלָּם מָאֳרָךְ לִצְפּוֹת בִּמְעוֹף דּוּכִיפַת וְנֵץ" },
		{ "hi",		"आपसे मिलकर खुशी हुई "},
		{ "it",		"Senza qualche prova ho il dubbio che si finga morto." },
		{ "iu",		"ᑐᓵᑦᓯᐊᕈᓐᓇᖖᒋᑦᑐᐊᓘᔪᖓ", },
		{ "ja",		"いろはにほへと ちりぬるを わかよたれそ つねならむ うゐのおくやま けふこえて あさきゆめみし ゑひもせす" },
		{ "kn",		"ಸಂತೋಷ ನೀವು ಭೇಟಿ"},
		{ "ml",		"അജവും ആനയും ഐരാവതവും ഗരുഡനും കഠോര സ്വരം പൊഴിക്കെ ഹാരവും ഒഢ്യാണവും ഫാലത്തില്‍ മഞ്ഞളും ഈറന്‍ കേശത്തില്‍ ഔഷധ എണ്ണയുമായി ഋതുമതിയും അനഘയും ഭൂനാഥയുമായ ഉമ ദുഃഖഛവിയോടെ ഇടതു പാദം ഏന്തി ങ്യേയാദൃശം നിര്‍ഝരിയിലെ ചിറ്റലകളെ ഓമനിക്കുമ്പോള്‍ ബാ‍ലയുടെ കണ്‍കളില്‍ നീര്‍ ഊര്‍ന്നു വിങ്ങി" },
		{ "ne",		"के तपाइ नेपाली बोल्नुहुन्छ?"},
		{ "nl",		"Pa's wijze lynx bezag vroom het fikse aquaduct." },
		{ "or",		"ଆପଣ ଓଡ଼ିଆ କୁହନ୍ତି କି? "},
		{ "pa",		"ਖੁਸ਼ੀ ਤੁਹਾਨੂੰ ਮੀਟਿੰਗ ਲਈ"},
		{ "pl",		"Pójdźże, kiń tę chmurność w głąb flaszy!" },
		{ "pt",		"À noite, vovô Kowalsky vê o ímã cair no pé do pingüim queixoso e vovó põe açúcar no chá de tâmaras do jabuti feliz." },
		{ "ru",		"В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!" },
		{ "sv",		"Gud hjälpe qvickt Zorns mö få aw byxor" },
		{ "ta",		"மகிழ்ச்சி நீங்கள் சந்தித்த"},
		{ "te",		"ఆనందం మీరు సమావేశం"},
		{ "ua",		"Чуєш їх, доцю, га? Кумедна ж ти, прощайся без ґольфів!" },
		{ "und-zsye",   "😀 🤔 ☹ 💩 😺 🙈 💃 🛌 👓 🐳 🌴 🌽 🥐 🍦☕ 🌍 🏠 🚂 🌥 ☃ 🎶 🛠 💯" },
		{ "zh-tw",	"秋風滑過拔地紅樓角落，誤見釣人低聲吟詠離騷。" },
		{ NULL, NULL } };

	/* ensure FontSampleText is defined */
	if (as_app_get_metadata_item (AS_APP (app), "FontSampleText") == NULL) {
		for (j = 0; text_sample[j].lang != NULL; j++) {
			percentage = as_app_get_language (AS_APP (app), text_sample[j].lang);
			if (percentage >= 0) {
				as_app_add_metadata (AS_APP (app),
						      "FontSampleText",
						      text_sample[j].value);
				break;
			}
		}
	}

	/* ensure FontIconText is defined */
	if (as_app_get_metadata_item (AS_APP (app), "FontIconText") == NULL) {
		for (j = 0; text_icon[j].lang != NULL; j++) {
			percentage = as_app_get_language (AS_APP (app), text_icon[j].lang);
			if (percentage >= 0) {
				as_app_add_metadata (AS_APP (app),
						      "FontIconText",
						      text_icon[j].value);
				break;
			}
		}
	}

	/* can we use a pango version */
	langs = as_app_get_languages (AS_APP (app));
	if (langs == NULL) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "No langs detected");
		return;
	}
	if (as_app_get_metadata_item (AS_APP (app), "FontIconText") == NULL) {
		for (l = langs; l != NULL; l = l->next) {
			tmp = l->data;
			plang = pango_language_from_string (tmp);
			value = pango_language_get_sample_string (plang);
			if (value == NULL)
				continue;
			as_app_add_metadata (AS_APP (app),
					      "FontSampleText",
					      value);
			if (g_strcmp0 (value, "The quick brown fox jumps over the lazy dog.") == 0) {
				as_app_add_metadata (AS_APP (app),
						      "FontIconText",
						      "Aa");
			} else {
				g_autofree gchar *icon_tmp = NULL;
				icon_tmp = g_utf8_substring (value, 0, 2);
				as_app_add_metadata (AS_APP (app),
						      "FontIconText",
						      icon_tmp);
			}
		}
	}

	/* still not defined? */
	if (as_app_get_metadata_item (AS_APP (app), "FontSampleText") == NULL) {
		str = g_string_sized_new (1024);
		for (l = langs; l != NULL; l = l->next) {
			tmp = l->data;
			g_string_append_printf (str, "%s, ", tmp);
		}
		if (str->len > 2)
			g_string_truncate (str, str->len - 2);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "No FontSampleText for langs: %s",
				 str->str);
	}
}

static gboolean
asb_font_is_sfnt(const FcPattern *pat)
{
	gchar *fontwrapper = NULL;
	gchar *fontformat = NULL;

	if (FcPatternGetString (pat, FC_FONT_WRAPPER, 0,
				(FcChar8 **) &fontwrapper) == FcResultMatch) {
		return g_strcmp0(fontwrapper, "SFNT") == 0;
	}
	if (FcPatternGetString (pat, FC_FONTFORMAT, 0,
				(FcChar8 **) &fontformat) == FcResultMatch) {
		if (g_ascii_strcasecmp (fontformat, "TrueType") == 0 ||
		    g_ascii_strcasecmp (fontformat, "CFF") == 0)
			return TRUE;
	}
	return FALSE;
}

static void
asb_font_add_metadata (AsbApp *app, const FcPattern *pat, const gchar *filename)
{
	gchar *family = NULL, *fullname = NULL, *style = NULL;

	if (!asb_font_is_sfnt (pat))
		return;

        /* look at the metadata table */
	FcPatternGetString (pat, FC_FAMILY, 0, (FcChar8 **) &family);
	FcPatternGetString (pat, FC_STYLE, 0, (FcChar8 **) &style);
	FcPatternGetString (pat, FC_FULLNAME, 0, (FcChar8 **) &fullname);
	if (family == NULL || style == NULL || fullname == NULL) {
		asb_package_log(
				asb_app_get_package (app), ASB_PACKAGE_LOG_LEVEL_WARNING,
				"Unable to find out family or style or fullname from a font %s",
				filename);
            return;
	}
	as_app_add_metadata (AS_APP (app), "FontFamily", family);
	as_app_add_metadata (AS_APP (app), "FontSubFamily", style);
	as_app_add_metadata (AS_APP (app), "FontFullName", fullname);
}

static gboolean
asb_font_is_pixbuf_empty (const GdkPixbuf *pixbuf)
{
	gint i, j;
	gint rowstride;
	gint width;
	guchar *pixels;
	guchar *tmp;
	guint length;
	guint cnt = 0;

	pixels = gdk_pixbuf_get_pixels_with_length (pixbuf, &length);
	width = gdk_pixbuf_get_width (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	for (j = 0; j < gdk_pixbuf_get_height (pixbuf); j++) {
		for (i = 0; i < width; i++) {
			/* any opacity */
			tmp = pixels + j * rowstride + i * 4;
			if (tmp[3] > 0)
				cnt++;
		}
	}
	if (cnt > 5)
		return FALSE;
	return TRUE;
}

static GdkPixbuf *
asb_font_get_pixbuf (const FcPattern *pat,
		     guint width,
		     guint height,
		     const gchar *text,
		     GError **error)
{
	hb_face_t *hb_face;
	cairo_font_face_t *font_face;
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_text_extents_t te;
	GdkPixbuf *pixbuf;
	guint text_size = 64;
	guint border_width = 8;
	const gchar *filename = NULL;
	gint idx = -1;

	/* set up font */
	FcPatternGetString (pat, FC_FILE, 0, (FcChar8 **) &filename);
	FcPatternGetInteger (pat, FC_INDEX, 0, &idx);
	if (filename == NULL || idx == -1) {
		g_set_error_literal (error, ASB_PLUGIN_ERROR, ASB_PLUGIN_ERROR_FAILED,
				     "Unable to get filename or index from pattern");
		return NULL;
	}

	hb_face = hb_face_create_from_file_or_fail (filename, idx);
	font_face = hb_cairo_font_face_create_for_face (hb_face);
	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					      (gint) width, (gint) height);
	cr = cairo_create (surface);
	cairo_set_font_face (cr, font_face);

	/* calculate best font size */
	while (text_size-- > 0) {
		cairo_set_font_size (cr, text_size);
		cairo_text_extents (cr, text, &te);
		if (te.width <= 0.01f || te.height <= 0.01f)
			continue;
		if (te.width < width - (border_width * 2) &&
		    te.height < height - (border_width * 2))
			break;
	}

	/* center text and blit to a pixbuf */
	cairo_move_to (cr, (width - te.width) / 2., (height - te.height) / 2. - te.y_bearing);
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_show_text (cr, text);
	pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, (gint) width, (gint) height);
	if (pixbuf == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Could not get font pixbuf");
	}

	cairo_destroy (cr);
	cairo_font_face_destroy (font_face);
	cairo_surface_destroy (surface);
	hb_face_destroy (hb_face);
	return pixbuf;
}

static gchar *
asb_font_get_caption (AsbApp *app)
{
	const gchar *family;
	const gchar *subfamily;

	family = as_app_get_metadata_item (AS_APP (app), "FontFamily");
	subfamily = as_app_get_metadata_item (AS_APP (app), "FontSubFamily");
	if (family == NULL && subfamily == NULL)
		return NULL;
	if (family == NULL)
		return g_strdup (subfamily);
	if (subfamily == NULL)
		return g_strdup (family);
	return g_strdup_printf ("%s – %s", family, subfamily);
}

static gboolean
asb_font_add_screenshot (AsbPlugin *plugin, AsbApp *app, const FcPattern *pat,
			 const gchar *cache_id, GError **error)
{
	AsImage *im_tmp;
	AsScreenshot *ss_tmp;
	GPtrArray *screenshots;
	const gchar *cache_dir;
	const gchar *temp_dir;
	const gchar *tmp;
	guint i;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *cache_fn = NULL;
	g_autofree gchar *output_fn = NULL;
	g_autofree gchar *caption = NULL;
	g_autofree gchar *url_tmp = NULL;
	g_autoptr(AsImage) im = NULL;
	g_autoptr(AsScreenshot) ss = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	tmp = as_app_get_metadata_item (AS_APP (app), "FontSampleText");
	if (tmp == NULL)
		return TRUE;

	/* is in the cache */
	cache_dir = asb_context_get_cache_dir (plugin->ctx);
	cache_fn = g_strdup_printf ("%s/screenshots/%s.png",
				    cache_dir, cache_id);
	if (g_file_test (cache_fn, G_FILE_TEST_EXISTS)) {
		pixbuf = gdk_pixbuf_new_from_file (cache_fn, error);
		if (pixbuf == NULL)
			return FALSE;
	} else {
		pixbuf = asb_font_get_pixbuf (pat, 640, 48, tmp, error);
		if (pixbuf == NULL)
			return FALSE;
	}

	/* check pixbuf is not just blank */
	if (asb_font_is_pixbuf_empty (pixbuf)) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Could not generate font preview");
		return FALSE;
	}

	/* save to the cache for next time */
	if (!g_file_test (cache_fn, G_FILE_TEST_EXISTS)) {
		if (!gdk_pixbuf_save (pixbuf, cache_fn, "png", error, NULL))
			return FALSE;
	}

	/* copy it to the screenshot directory */
	im = as_image_new ();
	as_image_set_pixbuf (im, pixbuf);
	as_image_set_kind (im, AS_IMAGE_KIND_SOURCE);
	basename = g_strdup_printf ("%s-%s.png",
				    as_app_get_id_filename (AS_APP (app)),
				    as_image_get_md5 (im));
	as_image_set_basename (im, basename);
	url_tmp = g_build_filename ("file://",
				    basename,
				    NULL);
	as_image_set_url (im, url_tmp);

	/* put this in a special place so it gets packaged up */
	temp_dir = asb_context_get_temp_dir (plugin->ctx);
	output_fn = g_build_filename (temp_dir, "screenshots", basename, NULL);
	if (!gdk_pixbuf_save (pixbuf, output_fn, "png", error, NULL))
		return FALSE;

	/* check the screenshot does not already exist */
	screenshots = as_app_get_screenshots (AS_APP (app));
	for (i = 0; i < screenshots->len; i++) {
		ss_tmp = g_ptr_array_index (screenshots, i);
		im_tmp = as_screenshot_get_source (ss_tmp);
		if (im_tmp == NULL)
			continue;
		if (g_strcmp0 (as_image_get_md5 (im_tmp),
			       as_image_get_md5 (im)) == 0) {
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Font screenshot already exists with hash %s",
				     as_image_get_md5 (im));
			return FALSE;
		}
	}

	/* add screenshot */
	ss = as_screenshot_new ();
	as_screenshot_set_kind (ss, AS_SCREENSHOT_KIND_DEFAULT);
	as_screenshot_add_image (ss, im);
	caption = asb_font_get_caption (app);
	if (caption != NULL)
		as_screenshot_set_caption (ss, NULL, caption);
	as_app_add_screenshot (AS_APP (app), ss);

	/* find screenshot priority */
	tmp = as_app_get_metadata_item (AS_APP (app), "FontSubFamily");
	if (tmp != NULL && g_ascii_strcasecmp (tmp, "Regular") != 0) {
		gint priority = -1;

		/* negative */
		if (g_strstr_len (tmp, -1, "Italic") != NULL)
			priority -= 2;
		if (g_strstr_len (tmp, -1, "Light") != NULL)
			priority -= 4;
		if (g_strstr_len (tmp, -1, "ExtraLight") != NULL)
			priority -= 8;
		if (g_strstr_len (tmp, -1, "Semibold") != NULL)
			priority -= 16;
		if (g_strstr_len (tmp, -1, "Bold") != NULL)
			priority -= 32;
		if (g_strstr_len (tmp, -1, "Medium") != NULL)
			priority -= 64;
		if (g_strstr_len (tmp, -1, "Black") != NULL)
			priority -= 128;
		if (g_strstr_len (tmp, -1, "Fallback") != NULL)
			priority -= 256;
		if (priority != 0)
			as_screenshot_set_priority (ss, priority);
	}


	/* save to cache */
	if (!g_file_test (cache_fn, G_FILE_TEST_EXISTS)) {
		if (!gdk_pixbuf_save (pixbuf, cache_fn, "png", error, NULL))
			return FALSE;
	}
	return TRUE;
}

static void
asb_font_add_languages (AsbApp *app, const FcPattern *pattern)
{
	const gchar *tmp;
	FcResult fc_rc = FcResultMatch;
	FcStrList *list;
	FcStrSet *langs;
	FcValue fc_value;
	gint i;
	gboolean skip_langs;

	skip_langs = g_getenv ("ASB_IS_SELF_TEST") != NULL;
	for (i = 0; fc_rc == FcResultMatch && !skip_langs; i++) {
		fc_rc = FcPatternGet (pattern, FC_LANG, i, &fc_value);
		if (fc_rc == FcResultMatch) {
			langs = FcLangSetGetLangs (fc_value.u.l);
			list = FcStrListCreate (langs);
			FcStrListFirst (list);
			while ((tmp = (const gchar*) FcStrListNext (list)) != NULL) {
				as_app_add_language (AS_APP (app), 0, tmp);
			}
			FcStrListDone (list);
			FcStrSetDestroy (langs);
		}
	}

	/* assume 'en' is available */
	if (g_list_length (as_app_get_languages (AS_APP (app))) == 0)
		as_app_add_language (AS_APP (app), 0, "en");
}

static void
asb_plugin_font_set_name (AsbApp *app, const gchar *name)
{
	const gchar *ptr;
	guint i;
	const gchar *prefixes[] = { "GFS ", NULL };
	const gchar *suffixes[] = { " SIL",
				    " ADF",
				    " CLM",
				    " GPL&GNU",
				    " SC",
				    NULL };
	g_autofree gchar *tmp = NULL;

	/* remove font foundry suffix */
	tmp = g_strdup (name);
	for (i = 0; suffixes[i] != NULL; i++) {
		if (g_str_has_suffix (tmp, suffixes[i])) {
			gsize len = strlen (tmp);
			tmp[len - strlen (suffixes[i])] = '\0';
		}
	}

	/* remove font foundry prefix */
	ptr = tmp;
	for (i = 0; prefixes[i] != NULL; i++) {
		if (g_str_has_prefix (tmp, prefixes[i]))
			ptr += strlen (prefixes[i]);
	}
	as_app_set_name (AS_APP (app), "C", ptr);
}

static gboolean
asb_plugin_font_app (AsbPlugin *plugin, AsbApp *app,
		     const gchar *filename, GError **error)
{
	FcConfig *config;
	FcFontSet *fonts;
	const gchar *tmp;
	gboolean ret = TRUE;
	guint i;
	FcPattern *pattern = NULL;
	g_autofree gchar *cache_id = NULL;
	g_autofree gchar *comment = NULL;
	g_autofree gchar *icon_filename = NULL;
	const gchar *family, *style;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	/* create a new fontconfig configuration */
	config = FcConfigCreate ();

	/* ensure that default configuration and fonts are not loaded */
	FcConfigSetCurrent (config);

	/* add just this one font */
	ret = FcConfigAppFontAddFile (config, (FcChar8 *) filename);
	if (FALSE && !ret) { /* FIXME: fails since f22 even for success */
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Failed to AddFile %s", filename);
		goto out;
	}
	fonts = FcConfigGetFonts (config, FcSetApplication);
	if (fonts == NULL || fonts->fonts == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "FcConfigGetFonts failed");
		goto out;
	}
	pattern = FcPatternDuplicate (fonts->fonts[0]);

	/* use the filename as the cache-id */
	cache_id = g_path_get_basename (filename);
	for (i = 0; cache_id[i] != '\0'; i++) {
		if (g_ascii_isalnum (cache_id[i]))
			continue;
		cache_id[i] = '_';
	}

	/* create app that might get merged later */
	if (asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_ADD_DEFAULT_ICONS)) {
		as_app_add_category (AS_APP (app), "Addons");
		as_app_add_category (AS_APP (app), "Fonts");
	}
	if (FcPatternGetString (pattern, FC_FAMILY, 0,
				(FcChar8 **) &family) != FcResultMatch) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Could not get a family name from %s",
			     filename);
		goto out;
	}
	if (as_app_get_name (AS_APP (app), NULL) == NULL)
		asb_plugin_font_set_name (app, family);
	tmp = as_app_get_comment (AS_APP (app), NULL);
	if (tmp == NULL ||
	    g_ascii_strncasecmp (tmp, "A Regular font", 14) != 0) {
		if (FcPatternGetString (pattern, FC_STYLE, 0,
					(FcChar8 **)&style) != FcResultMatch) {
			ret = FALSE;
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Could not get a style name from %s",
				     filename);
			goto out;
		}
		comment = g_strdup_printf ("A %s font from %s",
					   style, family);
		as_app_set_comment (AS_APP (app), "C", comment);
	}
	asb_font_add_languages (app, pattern);
	asb_font_add_metadata (app, pattern, filename);
	asb_font_fix_metadata (app);
	ret = asb_font_add_screenshot (plugin, app, pattern, cache_id, error);
	if (!ret)
		goto out;

	/* generate icon */
	tmp = as_app_get_metadata_item (AS_APP (app), "FontIconText");
	if (tmp != NULL) {
		g_autoptr(AsIcon) icon = NULL;
		pixbuf = asb_font_get_pixbuf (pattern, 64, 64, tmp, error);
		if (pixbuf == NULL) {
			ret = FALSE;
			goto out;
		}

		/* check pixbuf is not just blank */
		if (asb_font_is_pixbuf_empty (pixbuf)) {
			ret = FALSE;
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Could not generate %ix%i font icon with '%s'",
				     64, 64, tmp);
			goto out;
		}

		/* add icon */
		icon_filename = g_strdup_printf ("%s.png",
						 as_app_get_id_filename (AS_APP (app)));
		icon = as_icon_new ();
		as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
		as_icon_set_name (icon, icon_filename);
		as_icon_set_pixbuf (icon, pixbuf);
		as_icon_set_width (icon, 64);
		as_icon_set_height (icon, 64);
		as_app_add_icon (AS_APP (app), icon);
	}
out:
	if (pattern)
		FcPatternDestroy (pattern);
	FcConfigAppFontClear (config);
	FcConfigDestroy (config);
	return ret;
}

gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	gchar **filelist;
	guint i;

	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		GError *error_local = NULL;
		g_autofree gchar *filename = NULL;

		if (!_asb_plugin_check_filename (filelist[i]))
			continue;
		filename = g_build_filename (tmpdir, filelist[i], NULL);
		if (!asb_plugin_font_app (plugin, app, filename, &error_local)) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to get font from %s: %s",
					 filename,
					 error_local->message);
			g_clear_error (&error_local);
		}
	}
	return TRUE;
}

void
asb_plugin_merge (AsbPlugin *plugin, GList *list)
{
	AsApp *app;
	AsApp *found;
	GList *l;
	GPtrArray *extends_tmp;
	const gchar *tmp;
	g_autoptr(GHashTable) hash = NULL;

	/* add all the fonts to a hash */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) g_object_unref);
	for (l = list; l != NULL; l = l->next) {
		if (!ASB_IS_APP (l->data))
			continue;
		app = AS_APP (l->data);
		if (as_app_get_kind (app) != AS_APP_KIND_FONT)
			continue;
		g_hash_table_insert (hash,
				     g_strdup (as_app_get_id (app)),
				     g_object_ref (app));
	}

	/* merge all the extended fonts */
	for (l = list; l != NULL; l = l->next) {
		if (!ASB_IS_APP (l->data))
			continue;
		app = AS_APP (l->data);
		if (as_app_get_kind (app) != AS_APP_KIND_FONT)
			continue;
		extends_tmp = as_app_get_extends (app);
		if (extends_tmp->len == 0)
			continue;
		tmp = g_ptr_array_index (extends_tmp, 0);
		found = g_hash_table_lookup (hash, tmp);
		if (found == NULL) {
			g_warning ("not found: %s", tmp);
			continue;
		}
		as_app_subsume_full (found, app,
				     AS_APP_SUBSUME_FLAG_NO_OVERWRITE |
				     AS_APP_SUBSUME_FLAG_DEDUPE);
		as_app_add_veto (app,
				 "%s was merged into %s",
				 as_app_get_id (app),
				 as_app_get_id (found));
	}
}
