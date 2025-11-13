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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "thunar/thunar-archive-utils.h"
#include "thunar/thunar-file.h"
#include <gio/gio.h>

/* Archive format information table */
static const ThunarArchiveFormatInfo archive_formats[] = {
  {
    THUNAR_ARCHIVE_FORMAT_ZIP,
    "ZIP",
    ".zip",
    "application/zip",
    "ZIP Archive"
  },
  {
    THUNAR_ARCHIVE_FORMAT_TAR_GZ,
    "TAR.GZ",
    ".tar.gz",
    "application/x-compressed-tar",
    "Compressed TAR Archive (gzip)"
  },
  {
    THUNAR_ARCHIVE_FORMAT_TAR_BZ2,
    "TAR.BZ2",
    ".tar.bz2",
    "application/x-bzip-compressed-tar",
    "Compressed TAR Archive (bzip2)"
  },
  {
    THUNAR_ARCHIVE_FORMAT_TAR_XZ,
    "TAR.XZ",
    ".tar.xz",
    "application/x-xz-compressed-tar",
    "Compressed TAR Archive (xz)"
  },
  {
    THUNAR_ARCHIVE_FORMAT_7Z,
    "7Z",
    ".7z",
    "application/x-7z-compressed",
    "7-Zip Archive"
  },
  {
    THUNAR_ARCHIVE_FORMAT_TAR,
    "TAR",
    ".tar",
    "application/x-tar",
    "TAR Archive"
  }
};

static const gint n_archive_formats = G_N_ELEMENTS (archive_formats);



/**
 * thunar_archive_utils_is_archive:
 * @file : a #ThunarFile.
 *
 * Checks if the given @file is a supported archive format.
 *
 * Return value: %TRUE if @file is an archive, %FALSE otherwise.
 **/
gboolean
thunar_archive_utils_is_archive (ThunarFile *file)
{
  const gchar *content_type;
  gint         i;

  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* Get the content type */
  content_type = thunar_file_get_content_type (file);
  if (content_type == NULL)
    return FALSE;

  /* Check if it matches any supported archive format */
  for (i = 0; i < n_archive_formats; i++)
    {
      if (g_content_type_is_a (content_type, archive_formats[i].mime_type))
        return TRUE;
    }

  /* Also check for additional MIME types */
  if (g_content_type_is_a (content_type, "application/gzip") ||
      g_content_type_is_a (content_type, "application/x-bzip") ||
      g_content_type_is_a (content_type, "application/x-xz") ||
      g_content_type_is_a (content_type, "application/x-archive") ||
      g_content_type_is_a (content_type, "application/x-rar"))
    return TRUE;

  return FALSE;
}



/**
 * thunar_archive_utils_get_format:
 * @file : a #ThunarFile.
 *
 * Determines the archive format of the given @file.
 *
 * Return value: The #ThunarArchiveFormat of @file, or
 *               THUNAR_ARCHIVE_FORMAT_UNKNOWN if not recognized.
 **/
ThunarArchiveFormat
thunar_archive_utils_get_format (ThunarFile *file)
{
  const gchar *content_type;
  const gchar *basename;
  gint         i;

  g_return_val_if_fail (THUNAR_IS_FILE (file), THUNAR_ARCHIVE_FORMAT_UNKNOWN);

  /* Get the content type */
  content_type = thunar_file_get_content_type (file);
  if (content_type == NULL)
    return THUNAR_ARCHIVE_FORMAT_UNKNOWN;

  /* Check MIME type against known formats */
  for (i = 0; i < n_archive_formats; i++)
    {
      if (g_content_type_is_a (content_type, archive_formats[i].mime_type))
        return archive_formats[i].format;
    }

  /* Fallback: check by file extension */
  basename = thunar_file_get_basename (file);
  if (basename != NULL)
    {
      for (i = 0; i < n_archive_formats; i++)
        {
          if (g_str_has_suffix (basename, archive_formats[i].extension))
            return archive_formats[i].format;
        }
    }

  return THUNAR_ARCHIVE_FORMAT_UNKNOWN;
}



/**
 * thunar_archive_utils_get_format_name:
 * @format : a #ThunarArchiveFormat.
 *
 * Gets the name of the given archive @format.
 *
 * Return value: The format name, or "Unknown" if not recognized.
 **/
const gchar *
thunar_archive_utils_get_format_name (ThunarArchiveFormat format)
{
  gint i;

  for (i = 0; i < n_archive_formats; i++)
    {
      if (archive_formats[i].format == format)
        return archive_formats[i].name;
    }

  return "Unknown";
}



/**
 * thunar_archive_utils_get_format_ext:
 * @format : a #ThunarArchiveFormat.
 *
 * Gets the file extension for the given archive @format.
 *
 * Return value: The format extension, or "" if not recognized.
 **/
const gchar *
thunar_archive_utils_get_format_ext (ThunarArchiveFormat format)
{
  gint i;

  for (i = 0; i < n_archive_formats; i++)
    {
      if (archive_formats[i].format == format)
        return archive_formats[i].extension;
    }

  return "";
}



/**
 * thunar_archive_utils_get_format_desc:
 * @format : a #ThunarArchiveFormat.
 *
 * Gets the description for the given archive @format.
 *
 * Return value: The format description, or "Unknown" if not recognized.
 **/
const gchar *
thunar_archive_utils_get_format_desc (ThunarArchiveFormat format)
{
  gint i;

  for (i = 0; i < n_archive_formats; i++)
    {
      if (archive_formats[i].format == format)
        return archive_formats[i].description;
    }

  return "Unknown";
}



/**
 * thunar_archive_utils_get_all_formats:
 *
 * Gets a list of all supported archive formats.
 *
 * Return value: A newly allocated #GList of #ThunarArchiveFormatInfo pointers.
 *               Free with g_list_free() when done.
 **/
GList *
thunar_archive_utils_get_all_formats (void)
{
  GList *list = NULL;
  gint   i;

  for (i = 0; i < n_archive_formats; i++)
    list = g_list_prepend (list, (gpointer) &archive_formats[i]);

  return g_list_reverse (list);
}



/**
 * thunar_archive_utils_format_from_name:
 * @name : the format name (e.g., "ZIP", "TAR.GZ").
 *
 * Gets the #ThunarArchiveFormat from a format name.
 *
 * Return value: The corresponding #ThunarArchiveFormat, or
 *               THUNAR_ARCHIVE_FORMAT_UNKNOWN if not found.
 **/
ThunarArchiveFormat
thunar_archive_utils_format_from_name (const gchar *name)
{
  gint i;

  g_return_val_if_fail (name != NULL, THUNAR_ARCHIVE_FORMAT_UNKNOWN);

  for (i = 0; i < n_archive_formats; i++)
    {
      if (g_ascii_strcasecmp (archive_formats[i].name, name) == 0)
        return archive_formats[i].format;
    }

  return THUNAR_ARCHIVE_FORMAT_UNKNOWN;
}



/**
 * thunar_archive_utils_get_unique_filename:
 * @directory : the target #GFile directory.
 * @basename  : the base filename (without extension).
 * @extension : the file extension (including dot).
 *
 * Generates a unique filename in @directory by appending (1), (2), etc.
 * if necessary to avoid conflicts.
 *
 * Return value: A newly allocated filename that doesn't exist in @directory.
 *               Free with g_free() when done.
 **/
gchar *
thunar_archive_utils_get_unique_filename (GFile       *directory,
                                          const gchar *basename,
                                          const gchar *extension)
{
  GFile   *file;
  gchar   *filename;
  gchar   *unique_filename;
  gint     counter = 1;

  g_return_val_if_fail (G_IS_FILE (directory), NULL);
  g_return_val_if_fail (basename != NULL, NULL);
  g_return_val_if_fail (extension != NULL, NULL);

  /* Try the original filename first */
  filename = g_strdup_printf ("%s%s", basename, extension);
  file = g_file_get_child (directory, filename);

  /* If it doesn't exist, we're done */
  if (!g_file_query_exists (file, NULL))
    {
      g_object_unref (file);
      return filename;
    }

  g_free (filename);
  g_object_unref (file);

  /* Keep trying with (1), (2), etc. until we find a unique name */
  do
    {
      unique_filename = g_strdup_printf ("%s(%d)%s", basename, counter, extension);
      file = g_file_get_child (directory, unique_filename);
      counter++;

      if (!g_file_query_exists (file, NULL))
        {
          g_object_unref (file);
          return unique_filename;
        }

      g_free (unique_filename);
      g_object_unref (file);
    }
  while (counter < 9999); /* Safety limit */

  /* Fallback: use timestamp */
  unique_filename = g_strdup_printf ("%s_%ld%s", basename,
                                     (long) g_get_real_time (), extension);
  return unique_filename;
}



/**
 * thunar_archive_utils_can_compress:
 *
 * Checks if compression utilities are available on the system.
 *
 * Return value: %TRUE if compression is supported, %FALSE otherwise.
 **/
gboolean
thunar_archive_utils_can_compress (void)
{
  /* Check for required utilities */
  gchar *zip_path = g_find_program_in_path ("zip");
  gchar *tar_path = g_find_program_in_path ("tar");

  gboolean can_compress = (zip_path != NULL || tar_path != NULL);

  g_free (zip_path);
  g_free (tar_path);

  return can_compress;
}



/**
 * thunar_archive_utils_can_extract:
 * @format : a #ThunarArchiveFormat.
 *
 * Checks if the required utilities are available to extract
 * archives of the given @format.
 *
 * Return value: %TRUE if extraction is supported, %FALSE otherwise.
 **/
gboolean
thunar_archive_utils_can_extract (ThunarArchiveFormat format)
{
  gchar    *program = NULL;
  gboolean  result = FALSE;

  switch (format)
    {
    case THUNAR_ARCHIVE_FORMAT_ZIP:
      program = g_find_program_in_path ("unzip");
      break;

    case THUNAR_ARCHIVE_FORMAT_TAR:
    case THUNAR_ARCHIVE_FORMAT_TAR_GZ:
    case THUNAR_ARCHIVE_FORMAT_TAR_BZ2:
    case THUNAR_ARCHIVE_FORMAT_TAR_XZ:
      program = g_find_program_in_path ("tar");
      break;

    case THUNAR_ARCHIVE_FORMAT_7Z:
      program = g_find_program_in_path ("7z");
      if (program == NULL)
        program = g_find_program_in_path ("7za");
      break;

    default:
      return FALSE;
    }

  result = (program != NULL);
  g_free (program);

  return result;
}
