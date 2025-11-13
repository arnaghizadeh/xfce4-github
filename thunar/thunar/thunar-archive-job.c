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

#include "thunar/thunar-archive-job.h"
#include "thunar/thunar-archive-utils.h"
#include "thunar/thunar-private.h"

#include <glib/gi18n.h>

/* Operation types */
typedef enum
{
  THUNAR_ARCHIVE_OPERATION_COMPRESS,
  THUNAR_ARCHIVE_OPERATION_EXTRACT,
} ThunarArchiveOperation;

/* Property identifiers */
enum
{
  PROP_0,
  PROP_SOURCE_FILES,
  PROP_ARCHIVE_FILE,
  PROP_TARGET_DIR,
  PROP_FORMAT,
  PROP_OPERATION,
};

static void     thunar_archive_job_finalize     (GObject      *object);
static void     thunar_archive_job_get_property (GObject      *object,
                                                  guint         prop_id,
                                                  GValue       *value,
                                                  GParamSpec   *pspec);
static void     thunar_archive_job_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec);
static gboolean thunar_archive_job_execute      (ThunarJob    *job,
                                                  GError      **error);

struct _ThunarArchiveJobClass
{
  ThunarJobClass __parent__;
};

struct _ThunarArchiveJob
{
  ThunarJob               __parent__;

  ThunarArchiveOperation  operation;
  GList                  *source_files;
  GFile                  *archive_file;
  GFile                  *target_dir;
  ThunarArchiveFormat     format;
};

G_DEFINE_TYPE (ThunarArchiveJob, thunar_archive_job, THUNAR_TYPE_JOB)



static void
thunar_archive_job_class_init (ThunarArchiveJobClass *klass)
{
  GObjectClass   *gobject_class;
  ThunarJobClass *job_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_archive_job_finalize;
  gobject_class->get_property = thunar_archive_job_get_property;
  gobject_class->set_property = thunar_archive_job_set_property;

  job_class = THUNAR_JOB_CLASS (klass);
  job_class->execute = thunar_archive_job_execute;

  g_object_class_install_property (gobject_class,
                                    PROP_SOURCE_FILES,
                                    g_param_spec_pointer ("source-files",
                                                          "source-files",
                                                          "source-files",
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                    PROP_ARCHIVE_FILE,
                                    g_param_spec_object ("archive-file",
                                                         "archive-file",
                                                         "archive-file",
                                                         G_TYPE_FILE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                    PROP_TARGET_DIR,
                                    g_param_spec_object ("target-dir",
                                                         "target-dir",
                                                         "target-dir",
                                                         G_TYPE_FILE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                    PROP_FORMAT,
                                    g_param_spec_int ("format",
                                                      "format",
                                                      "format",
                                                      THUNAR_ARCHIVE_FORMAT_ZIP,
                                                      THUNAR_ARCHIVE_FORMAT_UNKNOWN,
                                                      THUNAR_ARCHIVE_FORMAT_ZIP,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                    PROP_OPERATION,
                                    g_param_spec_int ("operation",
                                                      "operation",
                                                      "operation",
                                                      THUNAR_ARCHIVE_OPERATION_COMPRESS,
                                                      THUNAR_ARCHIVE_OPERATION_EXTRACT,
                                                      THUNAR_ARCHIVE_OPERATION_COMPRESS,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY));
}



static void
thunar_archive_job_init (ThunarArchiveJob *job)
{
  job->source_files = NULL;
  job->archive_file = NULL;
  job->target_dir = NULL;
  job->format = THUNAR_ARCHIVE_FORMAT_ZIP;
  job->operation = THUNAR_ARCHIVE_OPERATION_COMPRESS;
}



static void
thunar_archive_job_finalize (GObject *object)
{
  ThunarArchiveJob *job = THUNAR_ARCHIVE_JOB (object);

  g_list_free_full (job->source_files, g_object_unref);

  if (job->archive_file != NULL)
    g_object_unref (job->archive_file);

  if (job->target_dir != NULL)
    g_object_unref (job->target_dir);

  (*G_OBJECT_CLASS (thunar_archive_job_parent_class)->finalize) (object);
}



static void
thunar_archive_job_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ThunarArchiveJob *job = THUNAR_ARCHIVE_JOB (object);

  switch (prop_id)
    {
    case PROP_SOURCE_FILES:
      g_value_set_pointer (value, job->source_files);
      break;
    case PROP_ARCHIVE_FILE:
      g_value_set_object (value, job->archive_file);
      break;
    case PROP_TARGET_DIR:
      g_value_set_object (value, job->target_dir);
      break;
    case PROP_FORMAT:
      g_value_set_int (value, job->format);
      break;
    case PROP_OPERATION:
      g_value_set_int (value, job->operation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_archive_job_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ThunarArchiveJob *job = THUNAR_ARCHIVE_JOB (object);
  GList            *lp;

  switch (prop_id)
    {
    case PROP_SOURCE_FILES:
      job->source_files = NULL;
      for (lp = g_value_get_pointer (value); lp != NULL; lp = lp->next)
        job->source_files = g_list_prepend (job->source_files,
                                            g_object_ref (lp->data));
      job->source_files = g_list_reverse (job->source_files);
      break;
    case PROP_ARCHIVE_FILE:
      job->archive_file = g_value_dup_object (value);
      break;
    case PROP_TARGET_DIR:
      job->target_dir = g_value_dup_object (value);
      break;
    case PROP_FORMAT:
      job->format = g_value_get_int (value);
      break;
    case PROP_OPERATION:
      job->operation = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_archive_job_compress_execute (ThunarArchiveJob *job,
                                      GError          **error)
{
  GSubprocess *process = NULL;
  gchar       *archive_path = NULL;
  gchar      **argv = NULL;
  GList       *lp;
  GPtrArray   *args;
  gboolean     success = FALSE;
  GFile       *working_dir = NULL;
  gchar       *cwd = NULL;
  gint         i;

  g_return_val_if_fail (job->archive_file != NULL, FALSE);
  g_return_val_if_fail (job->source_files != NULL, FALSE);

  archive_path = g_file_get_path (job->archive_file);
  if (archive_path == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
                           _("Invalid archive filename"));
      return FALSE;
    }

  /* Get working directory (parent of first source file) */
  working_dir = g_file_get_parent (G_FILE (job->source_files->data));
  cwd = g_file_get_path (working_dir);

  /* Build command based on format */
  args = g_ptr_array_new ();

  switch (job->format)
    {
    case THUNAR_ARCHIVE_FORMAT_ZIP:
      g_ptr_array_add (args, g_strdup ("zip"));
      g_ptr_array_add (args, g_strdup ("-r"));
      g_ptr_array_add (args, g_strdup ("-q"));
      g_ptr_array_add (args, g_strdup (archive_path));
      break;

    case THUNAR_ARCHIVE_FORMAT_TAR_GZ:
      g_ptr_array_add (args, g_strdup ("tar"));
      g_ptr_array_add (args, g_strdup ("-czf"));
      g_ptr_array_add (args, g_strdup (archive_path));
      break;

    case THUNAR_ARCHIVE_FORMAT_TAR_BZ2:
      g_ptr_array_add (args, g_strdup ("tar"));
      g_ptr_array_add (args, g_strdup ("-cjf"));
      g_ptr_array_add (args, g_strdup (archive_path));
      break;

    case THUNAR_ARCHIVE_FORMAT_TAR_XZ:
      g_ptr_array_add (args, g_strdup ("tar"));
      g_ptr_array_add (args, g_strdup ("-cJf"));
      g_ptr_array_add (args, g_strdup (archive_path));
      break;

    case THUNAR_ARCHIVE_FORMAT_7Z:
      g_ptr_array_add (args, g_strdup ("7z"));
      g_ptr_array_add (args, g_strdup ("a"));
      g_ptr_array_add (args, g_strdup ("-bd"));
      g_ptr_array_add (args, g_strdup (archive_path));
      break;

    case THUNAR_ARCHIVE_FORMAT_TAR:
      g_ptr_array_add (args, g_strdup ("tar"));
      g_ptr_array_add (args, g_strdup ("-cf"));
      g_ptr_array_add (args, g_strdup (archive_path));
      break;

    default:
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Unsupported archive format"));
      goto cleanup;
    }

  /* Add source files (use basenames for relative paths) */
  for (lp = job->source_files; lp != NULL; lp = lp->next)
    {
      gchar *basename = g_file_get_basename (G_FILE (lp->data));
      g_ptr_array_add (args, basename);
    }

  g_ptr_array_add (args, NULL);
  argv = (gchar **) g_ptr_array_free (args, FALSE);

  /* Update progress */
  thunar_job_info_message (THUNAR_JOB (job), _("Creating archive..."));
  thunar_job_percent (THUNAR_JOB (job), 0.0);

  /* Execute the command */
  process = g_subprocess_newv ((const gchar * const *) argv,
                               G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                               G_SUBPROCESS_FLAGS_STDERR_PIPE,
                               error);

  if (process == NULL)
    goto cleanup;

  /* Set working directory by using launcher instead */
  g_object_unref (process);

  GSubprocessLauncher *launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                                              G_SUBPROCESS_FLAGS_STDERR_PIPE);
  g_subprocess_launcher_set_cwd (launcher, cwd);

  process = g_subprocess_launcher_spawnv (launcher,
                                          (const gchar * const *) argv,
                                          error);
  g_object_unref (launcher);

  if (process == NULL)
    goto cleanup;

  /* Wait for completion */
  success = g_subprocess_wait_check (process,
                                     thunar_job_get_cancellable (THUNAR_JOB (job)),
                                     error);

  if (!success && error != NULL && *error == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Failed to create archive"));
    }

  /* Update progress */
  if (success)
    thunar_job_percent (THUNAR_JOB (job), 1.0);

cleanup:
  if (process != NULL)
    g_object_unref (process);

  g_free (archive_path);
  g_free (cwd);

  if (working_dir != NULL)
    g_object_unref (working_dir);

  if (argv != NULL)
    {
      for (i = 0; argv[i] != NULL; i++)
        g_free (argv[i]);
      g_free (argv);
    }

  return success;
}



static gboolean
thunar_archive_job_extract_execute (ThunarArchiveJob *job,
                                     GError          **error)
{
  GSubprocess         *process = NULL;
  gchar               *archive_path = NULL;
  gchar               *target_path = NULL;
  gchar              **argv = NULL;
  GPtrArray           *args;
  gboolean             success = FALSE;
  ThunarArchiveFormat  format;
  ThunarFile          *archive_thunar_file;
  gint                 i;

  g_return_val_if_fail (job->archive_file != NULL, FALSE);
  g_return_val_if_fail (job->target_dir != NULL, FALSE);

  archive_path = g_file_get_path (job->archive_file);
  target_path = g_file_get_path (job->target_dir);

  if (archive_path == NULL || target_path == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
                           _("Invalid file path"));
      goto cleanup;
    }

  /* Detect archive format */
  archive_thunar_file = thunar_file_get (job->archive_file, error);
  if (archive_thunar_file == NULL)
    goto cleanup;

  format = thunar_archive_utils_get_format (archive_thunar_file);
  g_object_unref (archive_thunar_file);

  if (!thunar_archive_utils_can_extract (format))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Cannot extract this archive format"));
      goto cleanup;
    }

  /* Build command based on format */
  args = g_ptr_array_new ();

  switch (format)
    {
    case THUNAR_ARCHIVE_FORMAT_ZIP:
      g_ptr_array_add (args, g_strdup ("unzip"));
      g_ptr_array_add (args, g_strdup ("-q"));
      g_ptr_array_add (args, g_strdup ("-o"));
      g_ptr_array_add (args, g_strdup (archive_path));
      g_ptr_array_add (args, g_strdup ("-d"));
      g_ptr_array_add (args, g_strdup (target_path));
      break;

    case THUNAR_ARCHIVE_FORMAT_TAR:
    case THUNAR_ARCHIVE_FORMAT_TAR_GZ:
    case THUNAR_ARCHIVE_FORMAT_TAR_BZ2:
    case THUNAR_ARCHIVE_FORMAT_TAR_XZ:
      g_ptr_array_add (args, g_strdup ("tar"));
      g_ptr_array_add (args, g_strdup ("-xf"));
      g_ptr_array_add (args, g_strdup (archive_path));
      g_ptr_array_add (args, g_strdup ("-C"));
      g_ptr_array_add (args, g_strdup (target_path));
      break;

    case THUNAR_ARCHIVE_FORMAT_7Z:
      g_ptr_array_add (args, g_strdup ("7z"));
      g_ptr_array_add (args, g_strdup ("x"));
      g_ptr_array_add (args, g_strdup ("-bd"));
      g_ptr_array_add (args, g_strdup ("-y"));
      g_ptr_array_add (args, g_strdup_printf ("-o%s", target_path));
      g_ptr_array_add (args, g_strdup (archive_path));
      break;

    default:
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Unsupported archive format"));
      goto cleanup;
    }

  g_ptr_array_add (args, NULL);
  argv = (gchar **) g_ptr_array_free (args, FALSE);

  /* Update progress */
  thunar_job_info_message (THUNAR_JOB (job), _("Extracting archive..."));
  thunar_job_percent (THUNAR_JOB (job), 0.0);

  /* Execute the command */
  process = g_subprocess_newv ((const gchar * const *) argv,
                               G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                               G_SUBPROCESS_FLAGS_STDERR_PIPE,
                               error);

  if (process == NULL)
    goto cleanup;

  /* Wait for completion */
  success = g_subprocess_wait_check (process,
                                     thunar_job_get_cancellable (THUNAR_JOB (job)),
                                     error);

  if (!success && error != NULL && *error == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Failed to extract archive"));
    }

  /* Update progress */
  if (success)
    thunar_job_percent (THUNAR_JOB (job), 1.0);

cleanup:
  if (process != NULL)
    g_object_unref (process);

  g_free (archive_path);
  g_free (target_path);

  if (argv != NULL)
    {
      for (i = 0; argv[i] != NULL; i++)
        g_free (argv[i]);
      g_free (argv);
    }

  return success;
}



static gboolean
thunar_archive_job_execute (ThunarJob *job,
                             GError   **error)
{
  ThunarArchiveJob *archive_job = THUNAR_ARCHIVE_JOB (job);

  if (thunar_job_set_error_if_cancelled (job, error))
    return FALSE;

  if (archive_job->operation == THUNAR_ARCHIVE_OPERATION_COMPRESS)
    return thunar_archive_job_compress_execute (archive_job, error);
  else
    return thunar_archive_job_extract_execute (archive_job, error);
}



/**
 * thunar_archive_job_compress:
 * @source_files : a #GList of #GFile items to compress.
 * @archive_file : the target archive #GFile.
 * @format       : the #ThunarArchiveFormat to use.
 *
 * Creates a new #ThunarJob for compressing files.
 *
 * Return value: the newly created #ThunarJob.
 **/
ThunarJob *
thunar_archive_job_compress (GList               *source_files,
                             GFile               *archive_file,
                             ThunarArchiveFormat  format)
{
  g_return_val_if_fail (source_files != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (archive_file), NULL);

  return g_object_new (THUNAR_TYPE_ARCHIVE_JOB,
                       "operation", THUNAR_ARCHIVE_OPERATION_COMPRESS,
                       "source-files", source_files,
                       "archive-file", archive_file,
                       "format", format,
                       NULL);
}



/**
 * thunar_archive_job_extract:
 * @archive_file : the archive #GFile to extract.
 * @target_dir   : the target directory #GFile.
 *
 * Creates a new #ThunarJob for extracting an archive.
 *
 * Return value: the newly created #ThunarJob.
 **/
ThunarJob *
thunar_archive_job_extract (GFile *archive_file,
                            GFile *target_dir)
{
  g_return_val_if_fail (G_IS_FILE (archive_file), NULL);
  g_return_val_if_fail (G_IS_FILE (target_dir), NULL);

  return g_object_new (THUNAR_TYPE_ARCHIVE_JOB,
                       "operation", THUNAR_ARCHIVE_OPERATION_EXTRACT,
                       "archive-file", archive_file,
                       "target-dir", target_dir,
                       NULL);
}
