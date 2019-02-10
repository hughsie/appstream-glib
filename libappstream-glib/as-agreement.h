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

#include "as-agreement-section.h"

G_BEGIN_DECLS

#define AS_TYPE_AGREEMENT (as_agreement_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsAgreement, as_agreement, AS, AGREEMENT, GObject)

struct _AsAgreementClass
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

/**
 * AsAgreementKind:
 * @AS_AGREEMENT_KIND_UNKNOWN:		Unknown value
 * @AS_AGREEMENT_KIND_GENERIC:		A generic agreement without a specific type
 * @AS_AGREEMENT_KIND_EULA:		An End User License Agreement
 * @AS_AGREEMENT_KIND_PRIVACY:		A privacy agreement, typically a GDPR statement
 *
 * The kind of the agreement.
 **/
typedef enum {
	AS_AGREEMENT_KIND_UNKNOWN,
	AS_AGREEMENT_KIND_GENERIC,
	AS_AGREEMENT_KIND_EULA,
	AS_AGREEMENT_KIND_PRIVACY,
	/*< private >*/
	AS_AGREEMENT_KIND_LAST
} AsAgreementKind;

AsAgreement	*as_agreement_new			(void);

const gchar	*as_agreement_kind_to_string		(AsAgreementKind	 value);
AsAgreementKind	 as_agreement_kind_from_string		(const gchar		*value);

AsAgreementKind	 as_agreement_get_kind			(AsAgreement		*agreement);
void		 as_agreement_set_kind			(AsAgreement		*agreement,
							 AsAgreementKind	 kind);
const gchar	*as_agreement_get_version_id		(AsAgreement		*agreement);
void		 as_agreement_set_version_id		(AsAgreement		*agreement,
							 const gchar		*version_id);
AsAgreementSection *as_agreement_get_section_default	(AsAgreement		*agreement);
GPtrArray	*as_agreement_get_sections		(AsAgreement		*agreement);
void		 as_agreement_add_section		(AsAgreement		*agreement,
							 AsAgreementSection	*agreement_section);

G_END_DECLS
