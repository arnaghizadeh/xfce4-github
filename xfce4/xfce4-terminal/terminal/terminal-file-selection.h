/*-
 * Copyright (c) 2024
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TERMINAL_FILE_SELECTION_H
#define TERMINAL_FILE_SELECTION_H

#include <gtk/gtk.h>
#include <vte/vte.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_FILE_SELECTION (terminal_file_selection_get_type ())
#define TERMINAL_FILE_SELECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TERMINAL_TYPE_FILE_SELECTION, TerminalFileSelection))
#define TERMINAL_FILE_SELECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_FILE_SELECTION, TerminalFileSelectionClass))
#define TERMINAL_IS_FILE_SELECTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TERMINAL_TYPE_FILE_SELECTION))
#define TERMINAL_IS_FILE_SELECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_FILE_SELECTION))
#define TERMINAL_FILE_SELECTION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_FILE_SELECTION, TerminalFileSelectionClass))

typedef struct _TerminalFileSelection TerminalFileSelection;
typedef struct _TerminalFileSelectionClass TerminalFileSelectionClass;
typedef struct _TerminalFileEntry TerminalFileEntry;

/**
 * TerminalFileEntry:
 * Represents a detected file/directory entry in terminal output.
 */
struct _TerminalFileEntry
{
  gchar *name;            /* The displayed filename */
  gchar *full_path;       /* Full resolved path if available */
  glong start_col;        /* Start column position */
  glong end_col;          /* End column position */
  glong row;              /* Row number in terminal */
  gboolean is_directory;  /* TRUE if this is a directory */
  gboolean is_selected;   /* Current selection state */
  gboolean is_hovered;    /* Current hover state */
};

GType
terminal_file_selection_get_type (void) G_GNUC_CONST;

TerminalFileSelection *
terminal_file_selection_new (VteTerminal *terminal);

void
terminal_file_selection_set_enabled (TerminalFileSelection *selection,
                                     gboolean enabled);

gboolean
terminal_file_selection_get_enabled (TerminalFileSelection *selection);

void
terminal_file_selection_set_working_directory (TerminalFileSelection *selection,
                                               const gchar *directory);

void
terminal_file_selection_clear (TerminalFileSelection *selection);

void
terminal_file_selection_clear_all (TerminalFileSelection *selection);

GList *
terminal_file_selection_get_selected (TerminalFileSelection *selection);

void
terminal_file_selection_draw (TerminalFileSelection *selection,
                              cairo_t *cr,
                              gint width,
                              gint height);

gboolean
terminal_file_selection_handle_button_press (TerminalFileSelection *selection,
                                             GdkEventButton *event);

gboolean
terminal_file_selection_handle_button_release (TerminalFileSelection *selection,
                                               GdkEventButton *event);

gboolean
terminal_file_selection_handle_motion (TerminalFileSelection *selection,
                                       GdkEventMotion *event);

void
terminal_file_selection_update_from_contents (TerminalFileSelection *selection);

void
terminal_file_selection_set_selection_colors (TerminalFileSelection *selection,
                                              const GdkRGBA *fg_color,
                                              const GdkRGBA *bg_color);

void
terminal_file_selection_set_hover_colors (TerminalFileSelection *selection,
                                          const GdkRGBA *fg_color,
                                          const GdkRGBA *bg_color);

void
terminal_file_selection_open_selected_in_thunar (TerminalFileSelection *selection);

gboolean
terminal_file_selection_is_app_using_mouse (TerminalFileSelection *selection);

void
terminal_file_selection_set_mouse_reporting (TerminalFileSelection *selection,
                                             gboolean active);

void
terminal_file_selection_set_double_click_opens (TerminalFileSelection *selection,
                                                gboolean enabled);

G_END_DECLS

#endif /* !TERMINAL_FILE_SELECTION_H */
