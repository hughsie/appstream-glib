/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Colin Walters <walters@verbum.org>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef __AS_CLEANUP_H__
#define __AS_CLEANUP_H__

#include <gio/gio.h>

#include "as-node.h"
#include "as-yaml.h"

#include <stdlib.h>

G_BEGIN_DECLS

#define GS_DEFINE_CLEANUP_FUNCTION(Type, name, func) \
  static inline void name (void *v) \
  { \
    func (*(Type*)v); \
  }

#define GS_DEFINE_CLEANUP_FUNCTION0(Type, name, func) \
  static inline void name (void *v) \
  { \
    if (*(Type*)v) \
      func (*(Type*)v); \
  }

GS_DEFINE_CLEANUP_FUNCTION0(GNode*, gs_local_node_unref, as_node_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GNode*, gs_local_yaml_unref, as_yaml_unref)

GS_DEFINE_CLEANUP_FUNCTION(void*, gs_local_free_libc, free)

#define _cleanup_free_libc_ __attribute__ ((cleanup(gs_local_free_libc)))
#define _cleanup_node_unref_ __attribute__ ((cleanup(gs_local_node_unref)))
#define _cleanup_yaml_unref_ __attribute__ ((cleanup(gs_local_yaml_unref)))

G_END_DECLS

#endif
