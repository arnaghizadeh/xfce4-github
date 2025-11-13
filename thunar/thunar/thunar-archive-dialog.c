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

#include "thunar/thunar-archive-dialog.h"
#include "thunar/thunar-archive-utils.h"
#include "thunar/thunar-file.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-dialogs.h"

#include <glib/gi18n.h>

/* Property identifiers */
enum
{
  PROP_0,
  PROP_DIRECTORY,
  PROP_FILES,
};

static void thunar_archive_dialog_finalize     (GObject                 *object);
static void thunar_archive_dialog_get_property (GObject                 *object,
                                                 guint                    prop_id,
                                                 GValue                  *value,
                                                 GParamSpec              *pspec);
static void thunar_archive_dialog_set_property (GObject                 *object,
                                                 guint                    prop_id,
                                                 const GValue            *value,
                                                 GParamSpec              *pspec);
static void thunar_archive_dialog_format_changed (GtkComboBox           *combo,
                                                   ThunarArchiveDialog   *dialog);
static void thunar_archive_dialog_entry_changed  (GtkEntry              *entry,
                                                   ThunarArchiveDialog   *dialog);

struct _ThunarArchiveDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarArchiveDialog
{
  GtkDialog           __parent__;

  GFile              *directory;
  GList              *files;

  GtkWidget          *entry;
  GtkWidget          *format_combo;
  GtkWidget          *ok_button;

  ThunarArchiveFormat selected_format;
};

G_DEFINE_TYPE (ThunarArchiveDialog, thunar_archive_dialog, GTK_TYPE_DIALOG)



static void
thunar_archive_dialog_class_init (ThunarArchiveDialogClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_archive_dialog_finalize;
  gobject_class->get_property = thunar_archive_dialog_get_property;
  gobject_class->set_property = thunar_archive_dialog_set_property;

  /**
   * ThunarArchiveDialog:directory:
   *
   * The directory where the archive will be created.
   **/
  g_object_class_install_property (gobject_class,
                                    PROP_DIRECTORY,
                                    g_param_spec_object ("directory",
                                                         "directory",
                                                         "directory",
                                                         G_TYPE_FILE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  /**
   * ThunarArchiveDialog:files:
   *
   * The list of files to be compressed.
   **/
  g_object_class_install_property (gobject_class,
                                    PROP_FILES,
                                    g_param_spec_pointer ("files",
                                                          "files",
                                                          "files",
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_CONSTRUCT_ONLY));
}



static void
thunar_archive_dialog_init (ThunarArchiveDialog *dialog)
{
  GtkWidget *content_area;
  GtkWidget *grid;
  GtkWidget *label;
  GList     *formats;
  GList     *lp;
  const ThunarArchiveFormatInfo *info;
  gchar     *text;
  gint       row = 0;

  /* Set dialog properties */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Create Archive"));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 450, -1);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  /* Add buttons */
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("C_reate"), GTK_RESPONSE_OK,
                          NULL);

  dialog->ok_button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
                                                           GTK_RESPONSE_OK);
  gtk_widget_set_sensitive (dialog->ok_button, FALSE);

  /* Get content area */
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);

  /* Create grid layout */
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (content_area), grid);
  gtk_widget_show (grid);

  /* Archive name label */
  label = gtk_label_new_with_mnemonic (_("Archive _name:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  /* Archive name entry */
  dialog->entry = gtk_entry_new ();
  gtk_widget_set_hexpand (dialog->entry, TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->entry);
  gtk_grid_attach (GTK_GRID (grid), dialog->entry, 1, row, 1, 1);
  gtk_widget_show (dialog->entry);
  g_signal_connect (G_OBJECT (dialog->entry), "changed",
                    G_CALLBACK (thunar_archive_dialog_entry_changed), dialog);

  row++;

  /* Format label */
  label = gtk_label_new_with_mnemonic (_("Archive _format:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
  gtk_widget_show (label);

  /* Format combo box */
  dialog->format_combo = gtk_combo_box_text_new ();
  gtk_widget_set_hexpand (dialog->format_combo, TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->format_combo);
  gtk_grid_attach (GTK_GRID (grid), dialog->format_combo, 1, row, 1, 1);
  gtk_widget_show (dialog->format_combo);

  /* Populate format combo box */
  formats = thunar_archive_utils_get_all_formats ();
  for (lp = formats; lp != NULL; lp = lp->next)
    {
      info = (const ThunarArchiveFormatInfo *) lp->data;
      text = g_strdup_printf ("%s (%s)", info->description, info->extension);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (dialog->format_combo),
                                  info->name, text);
      g_free (text);
    }
  g_list_free (formats);

  /* Set default format to ZIP */
  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->format_combo), 0);
  dialog->selected_format = THUNAR_ARCHIVE_FORMAT_ZIP;

  g_signal_connect (G_OBJECT (dialog->format_combo), "changed",
                    G_CALLBACK (thunar_archive_dialog_format_changed), dialog);
}



static void
thunar_archive_dialog_finalize (GObject *object)
{
  ThunarArchiveDialog *dialog = THUNAR_ARCHIVE_DIALOG (object);

  /* Free resources */
  if (dialog->directory != NULL)
    g_object_unref (dialog->directory);

  if (dialog->files != NULL)
    g_list_free_full (dialog->files, g_object_unref);

  (*G_OBJECT_CLASS (thunar_archive_dialog_parent_class)->finalize) (object);
}



static void
thunar_archive_dialog_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarArchiveDialog *dialog = THUNAR_ARCHIVE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, dialog->directory);
      break;

    case PROP_FILES:
      g_value_set_pointer (value, dialog->files);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_archive_dialog_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarArchiveDialog *dialog = THUNAR_ARCHIVE_DIALOG (object);
  GList               *lp;
  gchar               *default_name = NULL;

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      dialog->directory = g_value_dup_object (value);
      break;

    case PROP_FILES:
      /* Copy the file list */
      dialog->files = NULL;
      for (lp = g_value_get_pointer (value); lp != NULL; lp = lp->next)
        dialog->files = g_list_prepend (dialog->files, g_object_ref (lp->data));
      dialog->files = g_list_reverse (dialog->files);

      /* Set default archive name based on the files */
      if (dialog->files != NULL && g_list_length (dialog->files) == 1)
        {
          /* Single file: use its name as default */
          GFile *file = G_FILE (dialog->files->data);
          gchar *basename = g_file_get_basename (file);
          gchar *name_without_ext;
          gchar *dot;

          /* Remove extension */
          name_without_ext = g_strdup (basename);
          dot = g_strrstr (name_without_ext, ".");
          if (dot != NULL)
            *dot = '\0';

          default_name = name_without_ext;
          g_free (basename);
        }
      else if (dialog->files != NULL)
        {
          /* Multiple files: use "Archive" as default */
          default_name = g_strdup (_("Archive"));
        }

      if (default_name != NULL && dialog->entry != NULL)
        {
          gtk_entry_set_text (GTK_ENTRY (dialog->entry), default_name);
          gtk_editable_select_region (GTK_EDITABLE (dialog->entry), 0, -1);
          g_free (default_name);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_archive_dialog_format_changed (GtkComboBox         *combo,
                                       ThunarArchiveDialog *dialog)
{
  const gchar *format_name;

  g_return_if_fail (THUNAR_IS_ARCHIVE_DIALOG (dialog));

  format_name = gtk_combo_box_get_active_id (combo);
  if (format_name != NULL)
    dialog->selected_format = thunar_archive_utils_format_from_name (format_name);
}



static void
thunar_archive_dialog_entry_changed (GtkEntry            *entry,
                                      ThunarArchiveDialog *dialog)
{
  const gchar *text;
  gboolean     valid;

  g_return_if_fail (THUNAR_IS_ARCHIVE_DIALOG (dialog));

  /* Check if the entry is not empty */
  text = gtk_entry_get_text (entry);
  valid = (text != NULL && g_utf8_strlen (text, -1) > 0);

  /* Enable/disable OK button */
  gtk_widget_set_sensitive (dialog->ok_button, valid);
}



/**
 * thunar_archive_dialog_new:
 * @parent    : the #GtkWindow parent or %NULL.
 * @directory : the target directory as #GFile.
 * @files     : the list of #GFile items to compress.
 *
 * Creates a new #ThunarArchiveDialog for creating archives.
 *
 * Return value: the newly created #ThunarArchiveDialog.
 **/
GtkWidget *
thunar_archive_dialog_new (GtkWindow *parent,
                           GFile     *directory,
                           GList     *files)
{
  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (G_IS_FILE (directory), NULL);
  g_return_val_if_fail (files != NULL, NULL);

  return g_object_new (THUNAR_TYPE_ARCHIVE_DIALOG,
                       "transient-for", parent,
                       "directory", directory,
                       "files", files,
                       NULL);
}



/**
 * thunar_archive_dialog_get_format:
 * @dialog : a #ThunarArchiveDialog.
 *
 * Gets the selected archive format.
 *
 * Return value: the selected #ThunarArchiveFormat.
 **/
ThunarArchiveFormat
thunar_archive_dialog_get_format (ThunarArchiveDialog *dialog)
{
  g_return_val_if_fail (THUNAR_IS_ARCHIVE_DIALOG (dialog), THUNAR_ARCHIVE_FORMAT_UNKNOWN);
  return dialog->selected_format;
}



/**
 * thunar_archive_dialog_get_filename:
 * @dialog : a #ThunarArchiveDialog.
 *
 * Gets the entered filename (without extension).
 *
 * Return value: a newly allocated filename string. Free with g_free().
 **/
gchar *
thunar_archive_dialog_get_filename (ThunarArchiveDialog *dialog)
{
  g_return_val_if_fail (THUNAR_IS_ARCHIVE_DIALOG (dialog), NULL);
  return g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->entry)));
}



/**
 * thunar_archive_dialog_get_archive_file:
 * @dialog : a #ThunarArchiveDialog.
 *
 * Gets the full #GFile for the archive to be created, including
 * the appropriate extension based on the selected format.
 *
 * Return value: a new #GFile. Unref with g_object_unref() when done.
 **/
GFile *
thunar_archive_dialog_get_archive_file (ThunarArchiveDialog *dialog)
{
  gchar       *basename;
  const gchar *extension;
  gchar       *unique_filename;
  GFile       *file;

  g_return_val_if_fail (THUNAR_IS_ARCHIVE_DIALOG (dialog), NULL);
  g_return_val_if_fail (dialog->directory != NULL, NULL);

  basename = thunar_archive_dialog_get_filename (dialog);
  extension = thunar_archive_utils_get_format_ext (dialog->selected_format);

  /* Get unique filename */
  unique_filename = thunar_archive_utils_get_unique_filename (dialog->directory,
                                                               basename,
                                                               extension);
  g_free (basename);

  /* Create the full file path */
  file = g_file_get_child (dialog->directory, unique_filename);
  g_free (unique_filename);

  return file;
}
