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

#ifndef __THUNAR_ARCHIVE_DIALOG_H__
#define __THUNAR_ARCHIVE_DIALOG_H__

#include "thunar/thunar-archive-utils.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarArchiveDialogClass ThunarArchiveDialogClass;
typedef struct _ThunarArchiveDialog      ThunarArchiveDialog;

#define THUNAR_TYPE_ARCHIVE_DIALOG            (thunar_archive_dialog_get_type ())
#define THUNAR_ARCHIVE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ARCHIVE_DIALOG, ThunarArchiveDialog))
#define THUNAR_ARCHIVE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ARCHIVE_DIALOG, ThunarArchiveDialogClass))
#define THUNAR_IS_ARCHIVE_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ARCHIVE_DIALOG))
#define THUNAR_IS_ARCHIVE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_ARCHIVE_DIALOG))
#define THUNAR_ARCHIVE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ARCHIVE_DIALOG, ThunarArchiveDialogClass))

GType                thunar_archive_dialog_get_type           (void) G_GNUC_CONST;

GtkWidget           *thunar_archive_dialog_new                (GtkWindow           *parent,
                                                               GFile               *directory,
                                                               GList               *files);

ThunarArchiveFormat  thunar_archive_dialog_get_format         (ThunarArchiveDialog *dialog);
gchar               *thunar_archive_dialog_get_filename       (ThunarArchiveDialog *dialog);
GFile               *thunar_archive_dialog_get_archive_file   (ThunarArchiveDialog *dialog);

G_END_DECLS;

#endif /* !__THUNAR_ARCHIVE_DIALOG_H__ */
