/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_ARCHIVE_UTILS_H__
#define __THUNAR_ARCHIVE_UTILS_H__

#include "thunar/thunar-file.h"
#include <glib.h>

G_BEGIN_DECLS;

/* Supported archive formats */
typedef enum
{
  THUNAR_ARCHIVE_FORMAT_ZIP,
  THUNAR_ARCHIVE_FORMAT_TAR_GZ,
  THUNAR_ARCHIVE_FORMAT_TAR_BZ2,
  THUNAR_ARCHIVE_FORMAT_TAR_XZ,
  THUNAR_ARCHIVE_FORMAT_7Z,
  THUNAR_ARCHIVE_FORMAT_TAR,
  THUNAR_ARCHIVE_FORMAT_UNKNOWN
} ThunarArchiveFormat;

/* Archive format information */
typedef struct
{
  ThunarArchiveFormat format;
  const gchar        *name;
  const gchar        *extension;
  const gchar        *mime_type;
  const gchar        *description;
} ThunarArchiveFormatInfo;

gboolean             thunar_archive_utils_is_archive         (ThunarFile          *file);
ThunarArchiveFormat  thunar_archive_utils_get_format         (ThunarFile          *file);
const gchar         *thunar_archive_utils_get_format_name    (ThunarArchiveFormat  format);
const gchar         *thunar_archive_utils_get_format_ext     (ThunarArchiveFormat  format);
const gchar         *thunar_archive_utils_get_format_desc    (ThunarArchiveFormat  format);
GList               *thunar_archive_utils_get_all_formats    (void);
ThunarArchiveFormat  thunar_archive_utils_format_from_name   (const gchar         *name);
gchar               *thunar_archive_utils_get_unique_filename(GFile               *directory,
                                                               const gchar         *basename,
                                                               const gchar         *extension);
gboolean             thunar_archive_utils_can_compress       (void);
gboolean             thunar_archive_utils_can_extract        (ThunarArchiveFormat  format);

G_END_DECLS;

#endif /* !__THUNAR_ARCHIVE_UTILS_H__ */
