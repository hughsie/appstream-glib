/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_AGREEMENT_SECTION (as_agreement_section_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsAgreementSection, as_agreement_section, AS, AGREEMENT_SECTION, GObject)

struct _AsAgreementSectionClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

AsAgreementSection *as_agreement_section_new	(void);

const gchar	*as_agreement_section_get_kind	(AsAgreementSection	*agreement_section);
void		 as_agreement_section_set_kind	(AsAgreementSection	*agreement_section,
						 const gchar		*kind);
const gchar	*as_agreement_section_get_name	(AsAgreementSection	*agreement_section,
						 const gchar		*locale);
void		 as_agreement_section_set_name	(AsAgreementSection	*agreement_section,
						 const gchar		*locale,
						 const gchar		*name);
const gchar	*as_agreement_section_get_description	(AsAgreementSection	*agreement_section,
						 const gchar		*locale);
void		 as_agreement_section_set_description	(AsAgreementSection	*agreement_section,
						 const gchar		*locale,
						 const gchar		*desc);

G_END_DECLS
