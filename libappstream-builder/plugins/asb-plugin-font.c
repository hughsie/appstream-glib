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
#include <cairo/cairo-ft.h>
#include <ft2build.h>
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_MODULE_H
#include <pango/pango.h>
#include <pango/pangofc-fontmap.h>
#include <fontconfig/fontconfig.h>

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
	asb_plugin_add_glob (globs, "/usr/share/fonts/*/*.ttf");
}

static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (asb_plugin_match_glob ("/usr/share/fonts/*/*.otf", filename))
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
		{ "it",		"Aa" },
		{ "kn",         "ಅಆಇ" },
		{ "ml",		"ആഇ" },
		{ "ne",         "अआइ" },
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
asb_font_string_is_valid (const gchar *text)
{
	guint i;

	if (text == NULL || text[0] == '\0')
		return FALSE;
	for (i = 0; text[i] != '\0'; i++) {
		if (g_ascii_iscntrl (text[i]))
			return FALSE;
	}
	return TRUE;
}

static void
asb_font_add_metadata (AsbApp *app, FT_Face ft_face)
{
	FT_SfntName sfname;
	guint i;
	guint j;
	guint len;
	struct {
		FT_UShort	 idx;
		const gchar	*key;
	} tt_idx_to_md_name[] =  {
		{ TT_NAME_ID_FONT_FAMILY,		"FontFamily" },
		{ TT_NAME_ID_FONT_SUBFAMILY,		"FontSubFamily" },
		{ TT_NAME_ID_FULL_NAME,			"FontFullName" },
		{ TT_NAME_ID_PREFERRED_FAMILY,		"FontParent" },
		{ 0, NULL } };

	if (!FT_IS_SFNT (ft_face))
		return;

	/* look at the metadata table */
	len = FT_Get_Sfnt_Name_Count (ft_face);
	for (i = 0; i < len; i++) {
		FT_Get_Sfnt_Name (ft_face, i, &sfname);
		for (j = 0; tt_idx_to_md_name[j].key != NULL; j++) {
			g_autofree gchar *val = NULL;
			if (sfname.name_id != tt_idx_to_md_name[j].idx)
				continue;
			val = g_locale_to_utf8 ((gchar *) sfname.string,
						sfname.string_len,
						NULL, NULL, NULL);
			if (val == NULL)
				continue;
			if (asb_font_string_is_valid (val)) {
				as_app_add_metadata (AS_APP (app),
						     tt_idx_to_md_name[j].key,
						     val);
			} else {
				asb_package_log (asb_app_get_package (app),
						 ASB_PACKAGE_LOG_LEVEL_WARNING,
						 "Ignoring %s value: '%s'",
						 tt_idx_to_md_name[j].key, val);
			}
		}
	}
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
asb_font_get_pixbuf (FT_Face ft_face,
		     guint width,
		     guint height,
		     const gchar *text,
		     GError **error)
{
	cairo_font_face_t *font_face;
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_text_extents_t te;
	GdkPixbuf *pixbuf;
	guint text_size = 64;
	guint border_width = 8;

	/* set up font */
	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					      (gint) width, (gint) height);
	cr = cairo_create (surface);
	font_face = cairo_ft_font_face_create_for_ft_face (ft_face, FT_LOAD_DEFAULT);
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
	cairo_move_to (cr,
		       (width / 2) - te.width / 2 - te.x_bearing,
		       (height / 2) - te.height / 2 - te.y_bearing);
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
asb_font_add_screenshot (AsbPlugin *plugin, AsbApp *app, FT_Face ft_face,
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
		pixbuf = asb_font_get_pixbuf (ft_face, 640, 48, tmp, error);
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
	if (tmp != NULL) {
		gint priority = 0;

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
		if (g_strstr_len (tmp, -1, "Fallback") != NULL)
			priority -= 128;
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
	FT_Error rc;
	FT_Face ft_face = NULL;
	FT_Library library = NULL;
	const gchar *tmp;
	gboolean ret = TRUE;
	guint i;
	const FcPattern *pattern;
	g_autofree gchar *cache_id = NULL;
	g_autofree gchar *comment = NULL;
	g_autofree gchar *icon_filename = NULL;
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
	pattern = fonts->fonts[0];
	FT_Init_FreeType (&library);
	rc = FT_New_Face (library, filename, 0, &ft_face);
	if (rc != 0) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "FT_Open_Face failed to open %s: %i",
			     filename, rc);
		goto out;
	}

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
	if (as_app_get_name (AS_APP (app), NULL) == NULL)
		asb_plugin_font_set_name (app, ft_face->family_name);
	if (as_app_get_comment (AS_APP (app), NULL) == NULL) {
		comment = g_strdup_printf ("A %s font from %s",
					   ft_face->style_name,
					   ft_face->family_name);
		as_app_set_comment (AS_APP (app), "C", comment);
	}
	asb_font_add_languages (app, pattern);
	asb_font_add_metadata (app, ft_face);
	asb_font_fix_metadata (app);
	ret = asb_font_add_screenshot (plugin, app, ft_face, cache_id, error);
	if (!ret)
		goto out;

	/* generate icon */
	tmp = as_app_get_metadata_item (AS_APP (app), "FontIconText");
	if (tmp != NULL) {
		g_autoptr(AsIcon) icon = NULL;
		pixbuf = asb_font_get_pixbuf (ft_face, 64, 64, tmp, error);
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
	FcConfigAppFontClear (config);
	FcConfigDestroy (config);
	if (ft_face != NULL)
		FT_Done_Face (ft_face);
	if (library != NULL)
		FT_Done_Library (library);
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
