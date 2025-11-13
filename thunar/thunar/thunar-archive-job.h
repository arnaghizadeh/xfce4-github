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

#ifndef __THUNAR_ARCHIVE_JOB_H__
#define __THUNAR_ARCHIVE_JOB_H__

#include "thunar/thunar-job.h"
#include "thunar/thunar-archive-utils.h"

G_BEGIN_DECLS;

typedef struct _ThunarArchiveJobClass ThunarArchiveJobClass;
typedef struct _ThunarArchiveJob      ThunarArchiveJob;

#define THUNAR_TYPE_ARCHIVE_JOB            (thunar_archive_job_get_type ())
#define THUNAR_ARCHIVE_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ARCHIVE_JOB, ThunarArchiveJob))
#define THUNAR_ARCHIVE_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ARCHIVE_JOB, ThunarArchiveJobClass))
#define THUNAR_IS_ARCHIVE_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ARCHIVE_JOB))
#define THUNAR_IS_ARCHIVE_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_ARCHIVE_JOB))
#define THUNAR_ARCHIVE_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ARCHIVE_JOB, ThunarArchiveJobClass))

GType      thunar_archive_job_get_type (void) G_GNUC_CONST;

ThunarJob *thunar_archive_job_compress (GList               *source_files,
                                        GFile               *archive_file,
                                        ThunarArchiveFormat  format);

ThunarJob *thunar_archive_job_extract  (GFile               *archive_file,
                                        GFile               *target_dir);

G_END_DECLS;

#endif /* !__THUNAR_ARCHIVE_JOB_H__ */
