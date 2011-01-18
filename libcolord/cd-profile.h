/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if !defined (__COLORD_H_INSIDE__) && !defined (CD_COMPILATION)
#error "Only <colord.h> can be included directly."
#endif

#ifndef __CD_PROFILE_H
#define __CD_PROFILE_H

#include <glib-object.h>
#include <gio/gio.h>

#include <libcolord/cd-enum.h>

G_BEGIN_DECLS

#define CD_TYPE_PROFILE		(cd_profile_get_type ())
#define CD_PROFILE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CD_TYPE_PROFILE, CdProfile))
#define CD_PROFILE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CD_TYPE_PROFILE, CdProfileClass))
#define CD_IS_PROFILE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), CD_TYPE_PROFILE))
#define CD_IS_PROFILE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CD_TYPE_PROFILE))
#define CD_PROFILE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CD_TYPE_PROFILE, CdProfileClass))
#define CD_PROFILE_ERROR	(cd_profile_error_quark ())
#define CD_PROFILE_TYPE_ERROR	(cd_profile_error_get_type ())

typedef struct _CdProfilePrivate CdProfilePrivate;

typedef struct
{
	 GObject		 parent;
	 CdProfilePrivate	*priv;
} CdProfile;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*changed)		(CdProfile		*profile);
	/*< private >*/
	/* Padding for future expansion */
	void (*_cd_profile_reserved1) (void);
	void (*_cd_profile_reserved2) (void);
	void (*_cd_profile_reserved3) (void);
	void (*_cd_profile_reserved4) (void);
	void (*_cd_profile_reserved5) (void);
	void (*_cd_profile_reserved6) (void);
	void (*_cd_profile_reserved7) (void);
	void (*_cd_profile_reserved8) (void);
} CdProfileClass;

/**
 * CdProfileError:
 * @CD_PROFILE_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	CD_PROFILE_ERROR_FAILED,
	CD_PROFILE_ERROR_LAST
} CdProfileError;

GType		 cd_profile_get_type			(void);
GQuark		 cd_profile_error_quark			(void);
CdProfile	*cd_profile_new				(void);
gchar		*cd_profile_to_string			(CdProfile	*profile);
gboolean	 cd_profile_set_object_path_sync	(CdProfile	*profile,
							 const gchar	*object_path,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_profile_set_filename_sync		(CdProfile	*profile,
							 const gchar	*value,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_profile_install_system_wide_sync	(CdProfile	*profile,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 cd_profile_set_qualifier_sync		(CdProfile	*profile,
							 const gchar	*value,
							 GCancellable	*cancellable,
							 GError		**error);
const gchar	*cd_profile_get_id			(CdProfile	*profile);
const gchar	*cd_profile_get_filename		(CdProfile	*profile);
const gchar	*cd_profile_get_qualifier		(CdProfile	*profile);
const gchar	*cd_profile_get_title			(CdProfile	*profile);
const gchar	*cd_profile_get_object_path		(CdProfile	*profile);
CdProfileKind	 cd_profile_get_kind			(CdProfile	*profile);

G_END_DECLS

#endif /* __CD_PROFILE_H */
