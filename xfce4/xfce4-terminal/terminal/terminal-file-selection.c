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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <math.h>

#include "terminal-file-selection.h"
#include "terminal-private.h"

/* Macro for using glong as hash key */
#define GLONG_TO_POINTER(l) ((gpointer) (glong) (l))
#define GPOINTER_TO_GLONG(p) ((glong) (p))

/* Patterns for detecting common command outputs */
#define LS_LONG_PATTERN "^[-dlrwxsStT]{10}\\s+"
#define TREE_PREFIX_PATTERN "^[│├└─\\s]+"

/* Maximum entries to track for performance */
#define MAX_FILE_ENTRIES 10000

/* Selection corner radius for rounded rectangles */
#define SELECTION_CORNER_RADIUS 3.0

struct _TerminalFileSelectionClass
{
  GObjectClass __parent__;
};

struct _TerminalFileSelection
{
  GObject __parent__;

  VteTerminal *terminal;

  /* Feature state */
  gboolean enabled;
  gboolean double_click_opens;

  /* Working directory context */
  gchar *working_directory;

  /* Detected file entries */
  GList *entries;        /* List of TerminalFileEntry */
  GHashTable *entry_map; /* Row -> GList of entries for fast lookup */

  /* Selection colors (Thunar-like) */
  GdkRGBA selection_fg;
  GdkRGBA selection_bg;
  GdkRGBA hover_fg;
  GdkRGBA hover_bg;

  /* Current hover state */
  TerminalFileEntry *hovered_entry;

  /* Mouse tracking state */
  gboolean mouse_reporting_active;
  glong scroll_offset;

  /* Regex for filename detection */
  GRegex *ls_long_regex;
  GRegex *filename_regex;

  /* Content hash for detecting changes */
  guint content_hash;

  /* Idle update source */
  guint update_idle_id;
};

enum
{
  PROP_0,
  PROP_TERMINAL,
  PROP_ENABLED,
  PROP_DOUBLE_CLICK_OPENS,
  PROP_WORKING_DIRECTORY,
  N_PROPERTIES
};

enum
{
  SIGNAL_SELECTION_CHANGED,
  SIGNAL_FILE_ACTIVATED,
  N_SIGNALS
};

static GParamSpec *file_selection_props[N_PROPERTIES] = { NULL, };
static guint file_selection_signals[N_SIGNALS];

G_DEFINE_TYPE (TerminalFileSelection, terminal_file_selection, G_TYPE_OBJECT)



static void
terminal_file_entry_free (TerminalFileEntry *entry)
{
  if (G_UNLIKELY (entry == NULL))
    return;

  g_free (entry->name);
  g_free (entry->full_path);
  g_slice_free (TerminalFileEntry, entry);
}



static TerminalFileEntry *
terminal_file_entry_new (const gchar *name,
                         glong start_col,
                         glong end_col,
                         glong row)
{
  TerminalFileEntry *entry;

  entry = g_slice_new0 (TerminalFileEntry);
  entry->name = g_strdup (name);
  entry->start_col = start_col;
  entry->end_col = end_col;
  entry->row = row;
  entry->is_directory = FALSE;
  entry->is_selected = FALSE;
  entry->is_hovered = FALSE;
  entry->full_path = NULL;

  return entry;
}



static void
terminal_file_selection_finalize (GObject *object)
{
  TerminalFileSelection *selection = TERMINAL_FILE_SELECTION (object);

  if (selection->update_idle_id != 0)
    g_source_remove (selection->update_idle_id);

  g_free (selection->working_directory);

  if (selection->entry_map != NULL)
    g_hash_table_destroy (selection->entry_map);

  g_list_free_full (selection->entries, (GDestroyNotify) terminal_file_entry_free);

  if (selection->ls_long_regex != NULL)
    g_regex_unref (selection->ls_long_regex);
  if (selection->filename_regex != NULL)
    g_regex_unref (selection->filename_regex);

  G_OBJECT_CLASS (terminal_file_selection_parent_class)->finalize (object);
}



static void
terminal_file_selection_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
  TerminalFileSelection *selection = TERMINAL_FILE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_TERMINAL:
      g_value_set_object (value, selection->terminal);
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, selection->enabled);
      break;

    case PROP_DOUBLE_CLICK_OPENS:
      g_value_set_boolean (value, selection->double_click_opens);
      break;

    case PROP_WORKING_DIRECTORY:
      g_value_set_string (value, selection->working_directory);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
terminal_file_selection_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
  TerminalFileSelection *selection = TERMINAL_FILE_SELECTION (object);

  switch (prop_id)
    {
    case PROP_TERMINAL:
      selection->terminal = g_value_get_object (value);
      break;

    case PROP_ENABLED:
      terminal_file_selection_set_enabled (selection, g_value_get_boolean (value));
      break;

    case PROP_DOUBLE_CLICK_OPENS:
      selection->double_click_opens = g_value_get_boolean (value);
      break;

    case PROP_WORKING_DIRECTORY:
      terminal_file_selection_set_working_directory (selection, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
terminal_file_selection_class_init (TerminalFileSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = terminal_file_selection_finalize;
  gobject_class->get_property = terminal_file_selection_get_property;
  gobject_class->set_property = terminal_file_selection_set_property;

  file_selection_props[PROP_TERMINAL] =
    g_param_spec_object ("terminal",
                         NULL, NULL,
                         VTE_TYPE_TERMINAL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  file_selection_props[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  file_selection_props[PROP_DOUBLE_CLICK_OPENS] =
    g_param_spec_boolean ("double-click-opens",
                          NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  file_selection_props[PROP_WORKING_DIRECTORY] =
    g_param_spec_string ("working-directory",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, file_selection_props);

  /**
   * TerminalFileSelection::selection-changed:
   * Emitted when the selection changes.
   */
  file_selection_signals[SIGNAL_SELECTION_CHANGED] =
    g_signal_new ("selection-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * TerminalFileSelection::file-activated:
   * Emitted when a file is double-clicked.
   */
  file_selection_signals[SIGNAL_FILE_ACTIVATED] =
    g_signal_new ("file-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}



static void
terminal_file_selection_init (TerminalFileSelection *selection)
{
  GError *error = NULL;

  selection->enabled = TRUE;
  selection->double_click_opens = TRUE;
  selection->entries = NULL;
  selection->hovered_entry = NULL;
  selection->working_directory = NULL;
  selection->mouse_reporting_active = FALSE;
  selection->content_hash = 0;
  selection->update_idle_id = 0;

  /* Create hash table for row -> entries lookup */
  selection->entry_map = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Default Thunar-like colors - semi-transparent so text remains visible */
  gdk_rgba_parse (&selection->selection_fg, "#ffffff");
  gdk_rgba_parse (&selection->selection_bg, "rgba(53, 132, 228, 0.4)"); /* GNOME blue 40% */
  gdk_rgba_parse (&selection->hover_fg, "#000000");
  gdk_rgba_parse (&selection->hover_bg, "rgba(53, 132, 228, 0.2)"); /* Lighter blue 20% */

  /* Compile regex patterns */
  /* Pattern to detect ls -l style output */
  selection->ls_long_regex = g_regex_new (
    "^([-dlrwxsStT@+]{10,})\\s+\\d+\\s+\\S+\\s+\\S+\\s+[\\d,]+\\s+"
    "(?:[A-Z][a-z]{2}\\s+\\d+|\\d{4}-\\d{2}-\\d{2})\\s+[\\d:]+\\s+(.+)$",
    G_REGEX_OPTIMIZE,
    0,
    &error);

  if (error != NULL)
    {
      g_warning ("Failed to compile ls -l regex: %s", error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Pattern for general filename detection (handles various output formats) */
  selection->filename_regex = g_regex_new (
    /* Match filenames but avoid false positives */
    "(?:^|\\s)([\\w.][\\w.\\-+~]*(?:/[\\w.\\-+~]+)*/?)",
    G_REGEX_OPTIMIZE,
    0,
    &error);

  if (error != NULL)
    {
      g_warning ("Failed to compile filename regex: %s", error->message);
      g_error_free (error);
    }
}



/**
 * terminal_file_selection_new:
 * @terminal: The VTE terminal to attach to.
 *
 * Creates a new file selection overlay for a terminal.
 *
 * Returns: A new #TerminalFileSelection instance.
 */
TerminalFileSelection *
terminal_file_selection_new (VteTerminal *terminal)
{
  g_return_val_if_fail (VTE_IS_TERMINAL (terminal), NULL);

  return g_object_new (TERMINAL_TYPE_FILE_SELECTION,
                       "terminal", terminal,
                       NULL);
}



/**
 * terminal_file_selection_set_enabled:
 * @selection: A #TerminalFileSelection.
 * @enabled: Whether to enable file selection.
 */
void
terminal_file_selection_set_enabled (TerminalFileSelection *selection,
                                     gboolean enabled)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  if (selection->enabled != enabled)
    {
      selection->enabled = enabled;

      if (!enabled)
        terminal_file_selection_clear_all (selection);

      g_object_notify_by_pspec (G_OBJECT (selection), file_selection_props[PROP_ENABLED]);
    }
}



/**
 * terminal_file_selection_get_enabled:
 * @selection: A #TerminalFileSelection.
 *
 * Returns: Whether file selection is enabled.
 */
gboolean
terminal_file_selection_get_enabled (TerminalFileSelection *selection)
{
  g_return_val_if_fail (TERMINAL_IS_FILE_SELECTION (selection), FALSE);
  return selection->enabled;
}



/**
 * terminal_file_selection_set_working_directory:
 * @selection: A #TerminalFileSelection.
 * @directory: The current working directory.
 */
void
terminal_file_selection_set_working_directory (TerminalFileSelection *selection,
                                               const gchar *directory)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  g_free (selection->working_directory);
  selection->working_directory = g_strdup (directory);

  /* Update full paths for existing entries */
  for (GList *l = selection->entries; l != NULL; l = l->next)
    {
      TerminalFileEntry *entry = l->data;
      g_free (entry->full_path);

      if (directory != NULL && entry->name != NULL)
        {
          if (g_path_is_absolute (entry->name))
            entry->full_path = g_strdup (entry->name);
          else
            entry->full_path = g_build_filename (directory, entry->name, NULL);

          /* Check if it's a directory */
          GFile *file = g_file_new_for_path (entry->full_path);
          GFileType type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);
          entry->is_directory = (type == G_FILE_TYPE_DIRECTORY);
          g_object_unref (file);
        }
      else
        {
          entry->full_path = NULL;
        }
    }
}



/**
 * terminal_file_selection_clear:
 * @selection: A #TerminalFileSelection.
 *
 * Clears the current selection but keeps detected entries.
 */
void
terminal_file_selection_clear (TerminalFileSelection *selection)
{
  gboolean changed = FALSE;

  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  for (GList *l = selection->entries; l != NULL; l = l->next)
    {
      TerminalFileEntry *entry = l->data;
      if (entry->is_selected)
        {
          entry->is_selected = FALSE;
          changed = TRUE;
        }
    }

  if (changed)
    g_signal_emit (selection, file_selection_signals[SIGNAL_SELECTION_CHANGED], 0);
}



/**
 * terminal_file_selection_clear_all:
 * @selection: A #TerminalFileSelection.
 *
 * Clears all entries and selection state.
 */
void
terminal_file_selection_clear_all (TerminalFileSelection *selection)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  g_hash_table_remove_all (selection->entry_map);
  g_list_free_full (selection->entries, (GDestroyNotify) terminal_file_entry_free);
  selection->entries = NULL;
  selection->hovered_entry = NULL;
  selection->content_hash = 0;

  g_signal_emit (selection, file_selection_signals[SIGNAL_SELECTION_CHANGED], 0);
}



/**
 * terminal_file_selection_get_selected:
 * @selection: A #TerminalFileSelection.
 *
 * Returns: A newly allocated list of selected entries. Free with g_list_free().
 */
GList *
terminal_file_selection_get_selected (TerminalFileSelection *selection)
{
  GList *selected = NULL;

  g_return_val_if_fail (TERMINAL_IS_FILE_SELECTION (selection), NULL);

  for (GList *l = selection->entries; l != NULL; l = l->next)
    {
      TerminalFileEntry *entry = l->data;
      if (entry->is_selected)
        selected = g_list_prepend (selected, entry);
    }

  return g_list_reverse (selected);
}



/**
 * Draw a rounded rectangle for selection/hover highlighting.
 */
static void
draw_rounded_rect (cairo_t *cr,
                   gdouble x,
                   gdouble y,
                   gdouble width,
                   gdouble height,
                   gdouble radius)
{
  gdouble degrees = G_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
}



/**
 * terminal_file_selection_draw:
 * @selection: A #TerminalFileSelection.
 * @cr: Cairo context to draw into.
 * @width: Widget width.
 * @height: Widget height.
 *
 * Draws the selection and hover highlights.
 */
void
terminal_file_selection_draw (TerminalFileSelection *selection,
                              cairo_t *cr,
                              gint width,
                              gint height)
{
  glong char_width, char_height;
  GtkBorder padding;

  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));
  g_return_if_fail (cr != NULL);

  if (!selection->enabled || selection->terminal == NULL)
    return;

  /* Get terminal metrics */
  char_width = vte_terminal_get_char_width (selection->terminal);
  char_height = vte_terminal_get_char_height (selection->terminal);

  if (char_width <= 0 || char_height <= 0)
    return;

  /* Get terminal padding */
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (selection->terminal));
  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);

  /* Draw each entry - entry->row is already a visual row (0 = top of screen) */
  for (GList *l = selection->entries; l != NULL; l = l->next)
    {
      TerminalFileEntry *entry = l->data;
      gdouble x, y, w, h;

      if (!entry->is_selected && !entry->is_hovered)
        continue;

      /* Calculate screen position - row is visual row, no scroll adjustment needed */
      y = entry->row * char_height + padding.top;
      x = entry->start_col * char_width + padding.left;
      w = (entry->end_col - entry->start_col) * char_width;
      h = char_height;

      /* Skip if off-screen */
      if (y + h < 0 || y > height)
        continue;

      /* Choose color based on state */
      if (entry->is_selected)
        {
          cairo_set_source_rgba (cr,
                                 selection->selection_bg.red,
                                 selection->selection_bg.green,
                                 selection->selection_bg.blue,
                                 selection->selection_bg.alpha);
        }
      else if (entry->is_hovered)
        {
          cairo_set_source_rgba (cr,
                                 selection->hover_bg.red,
                                 selection->hover_bg.green,
                                 selection->hover_bg.blue,
                                 selection->hover_bg.alpha);
        }

      /* Draw rounded rectangle */
      draw_rounded_rect (cr, x, y, w, h, SELECTION_CORNER_RADIUS);
      cairo_fill (cr);
    }
}



/**
 * Find an entry at the given terminal coordinates.
 */
static TerminalFileEntry *
find_entry_at_position (TerminalFileSelection *selection,
                        glong col,
                        glong row)
{
  GList *row_entries;

  row_entries = g_hash_table_lookup (selection->entry_map, GLONG_TO_POINTER (row));
  if (row_entries == NULL)
    return NULL;

  for (GList *l = row_entries; l != NULL; l = l->next)
    {
      TerminalFileEntry *entry = l->data;
      if (col >= entry->start_col && col < entry->end_col)
        return entry;
    }

  return NULL;
}



/**
 * Convert screen coordinates to terminal cell coordinates.
 * Returns the visual row on screen (0 = top row visible),
 * NOT the absolute row in scrollback buffer.
 */
static void
screen_to_cell (TerminalFileSelection *selection,
                gdouble x,
                gdouble y,
                glong *col,
                glong *row)
{
  glong char_width, char_height;
  GtkBorder padding;

  char_width = vte_terminal_get_char_width (selection->terminal);
  char_height = vte_terminal_get_char_height (selection->terminal);

  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (selection->terminal));
  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);

  *col = (glong) ((x - padding.left) / char_width);
  /* Visual row on screen, not absolute row in buffer */
  *row = (glong) ((y - padding.top) / char_height);
}



/**
 * terminal_file_selection_handle_button_press:
 * @selection: A #TerminalFileSelection.
 * @event: The button press event.
 *
 * Returns: TRUE if the event was handled.
 */
gboolean
terminal_file_selection_handle_button_press (TerminalFileSelection *selection,
                                             GdkEventButton *event)
{
  glong col, row;
  TerminalFileEntry *entry;
  gboolean ctrl_pressed;

  fprintf (stderr, "file-selection: HANDLER CALLED! enabled=%d\n",
           selection ? selection->enabled : -1);
  fflush (stderr);

  g_return_val_if_fail (TERMINAL_IS_FILE_SELECTION (selection), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (!selection->enabled)
    {
      g_warning ("file-selection: Feature is DISABLED, returning early");
      return FALSE;
    }

  /* Don't interfere with mouse reporting */
  if (terminal_file_selection_is_app_using_mouse (selection))
    return FALSE;

  /* Only handle left button */
  if (event->button != 1)
    return FALSE;

  screen_to_cell (selection, event->x, event->y, &col, &row);
  entry = find_entry_at_position (selection, col, row);
  ctrl_pressed = (event->state & GDK_CONTROL_MASK) != 0;

  g_warning ("file-selection: Click at screen (%.1f, %.1f) -> cell (%ld, %ld), entry=%p, total_entries=%u",
             event->x, event->y, col, row, (void*)entry, g_list_length (selection->entries));

  /* Handle double-click */
  if (event->type == GDK_2BUTTON_PRESS && entry != NULL)
    {
      if (selection->double_click_opens && entry->full_path != NULL)
        {
          g_signal_emit (selection, file_selection_signals[SIGNAL_FILE_ACTIVATED], 0, entry->full_path);
          terminal_file_selection_open_selected_in_thunar (selection);
          return TRUE;
        }
    }

  /* Handle single click */
  if (event->type == GDK_BUTTON_PRESS)
    {
      if (entry != NULL)
        {
          if (ctrl_pressed)
            {
              /* Toggle selection on Ctrl+Click */
              entry->is_selected = !entry->is_selected;
            }
          else
            {
              /* Clear other selections, select this one */
              for (GList *l = selection->entries; l != NULL; l = l->next)
                {
                  TerminalFileEntry *e = l->data;
                  e->is_selected = (e == entry);
                }
            }

          g_signal_emit (selection, file_selection_signals[SIGNAL_SELECTION_CHANGED], 0);
          gtk_widget_queue_draw (GTK_WIDGET (selection->terminal));
          return TRUE;
        }
      else if (!ctrl_pressed)
        {
          /* Clicked on empty space - clear selection */
          terminal_file_selection_clear (selection);
          gtk_widget_queue_draw (GTK_WIDGET (selection->terminal));
        }
    }

  return FALSE;
}



/**
 * terminal_file_selection_handle_button_release:
 * @selection: A #TerminalFileSelection.
 * @event: The button release event.
 *
 * Returns: TRUE if the event was handled.
 */
gboolean
terminal_file_selection_handle_button_release (TerminalFileSelection *selection,
                                               GdkEventButton *event)
{
  g_return_val_if_fail (TERMINAL_IS_FILE_SELECTION (selection), FALSE);
  /* Currently, we don't need special handling for button release */
  return FALSE;
}



/**
 * terminal_file_selection_handle_motion:
 * @selection: A #TerminalFileSelection.
 * @event: The motion event.
 *
 * Returns: TRUE if the event was handled.
 */
gboolean
terminal_file_selection_handle_motion (TerminalFileSelection *selection,
                                       GdkEventMotion *event)
{
  glong col, row;
  TerminalFileEntry *entry;

  g_return_val_if_fail (TERMINAL_IS_FILE_SELECTION (selection), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (!selection->enabled)
    return FALSE;

  /* Don't interfere with mouse reporting */
  if (terminal_file_selection_is_app_using_mouse (selection))
    {
      if (selection->hovered_entry != NULL)
        {
          selection->hovered_entry->is_hovered = FALSE;
          selection->hovered_entry = NULL;
          gtk_widget_queue_draw (GTK_WIDGET (selection->terminal));
        }
      return FALSE;
    }

  screen_to_cell (selection, event->x, event->y, &col, &row);
  entry = find_entry_at_position (selection, col, row);

  if (entry != selection->hovered_entry)
    {
      /* Update hover state */
      if (selection->hovered_entry != NULL)
        selection->hovered_entry->is_hovered = FALSE;

      selection->hovered_entry = entry;

      if (entry != NULL)
        entry->is_hovered = TRUE;

      gtk_widget_queue_draw (GTK_WIDGET (selection->terminal));

      /* Update cursor */
      GdkWindow *window = gtk_widget_get_window (GTK_WIDGET (selection->terminal));
      if (window != NULL)
        {
          if (entry != NULL)
            {
              GdkCursor *cursor = gdk_cursor_new_from_name (
                gdk_window_get_display (window), "pointer");
              gdk_window_set_cursor (window, cursor);
              g_object_unref (cursor);
            }
          else
            {
              gdk_window_set_cursor (window, NULL);
            }
        }
    }

  return FALSE;
}



/**
 * Parse a line of terminal output and detect file entries.
 * Uses VteCharAttributes to get accurate column positions.
 *
 * This version does NOT verify files against the working directory because
 * commands like "ls /tmp" show files in /tmp but working directory may be different.
 * Instead, we use heuristics to identify likely filenames and the last word on
 * each line (which is typically the filename in ls -la output).
 */
static void
parse_line_for_files_with_attrs (TerminalFileSelection *selection,
                                 const gchar *line,
                                 glong row,
                                 GArray *attrs)
{
  GList *row_entries = NULL;
  glong text_len = g_utf8_strlen (line, -1);

  if (line == NULL || *line == '\0' || text_len == 0)
    return;

  /* Collect all potential filename words with their positions */
  GPtrArray *candidates = g_ptr_array_new_with_free_func (g_free);
  GArray *start_cols = g_array_new (FALSE, FALSE, sizeof (glong));
  GArray *end_cols = g_array_new (FALSE, FALSE, sizeof (glong));

  /* Scan the text for words using attributes for column positions */
  const gchar *p = line;
  glong char_idx = 0;

  while (*p != '\0' && char_idx < (glong) attrs->len)
    {
      /* Skip whitespace */
      while (*p != '\0' && (*p == ' ' || *p == '\t') && char_idx < (glong) attrs->len)
        {
          p = g_utf8_next_char (p);
          char_idx++;
        }

      if (*p == '\0' || char_idx >= (glong) attrs->len)
        break;

      /* Found start of a word - get its column from attributes */
      VteCharAttributes start_attr = g_array_index (attrs, VteCharAttributes, char_idx);
      glong start_col = start_attr.column;
      const gchar *word_start = p;

      /* Find end of word */
      while (*p != '\0' && *p != ' ' && *p != '\t' && char_idx < (glong) attrs->len)
        {
          p = g_utf8_next_char (p);
          char_idx++;
        }

      /* Get end column from last character's attribute */
      glong end_col;
      if (char_idx > 0 && char_idx <= (glong) attrs->len)
        {
          VteCharAttributes end_attr = g_array_index (attrs, VteCharAttributes, char_idx - 1);
          end_col = end_attr.column + 1;  /* +1 because we want past the last char */
        }
      else
        {
          end_col = start_col + (glong)(p - word_start);
        }

      /* Extract the word */
      gchar *word = g_strndup (word_start, p - word_start);
      glong word_len = g_utf8_strlen (word, -1);

      /* Skip obvious non-files: permission strings like -rw-r--r--, numbers, "total" */
      gboolean skip = FALSE;
      if (word_len == 0)
        skip = TRUE;
      else if (word[0] == '-' && word_len >= 10)
        {
          /* Looks like permission string: -rwxr-xr-x, drwxr-xr-x, etc */
          skip = TRUE;
        }
      else if (g_ascii_isdigit (word[0]))
        {
          /* Numbers (sizes, dates, link counts) */
          skip = TRUE;
        }
      else if (strcmp (word, "total") == 0)
        skip = TRUE;
      else if (word_len == 1 && word[0] == '-')
        skip = TRUE;

      /* Check for month names (common in ls -l output) */
      if (!skip)
        {
          static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
          for (int i = 0; months[i] != NULL; i++)
            {
              if (strcmp (word, months[i]) == 0)
                {
                  skip = TRUE;
                  break;
                }
            }
        }

      /* Check for username/group patterns (common words in ls -l) */
      if (!skip)
        {
          if (strcmp (word, "root") == 0 ||
              strcmp (word, "nobody") == 0 ||
              strcmp (word, "wheel") == 0 ||
              strcmp (word, "staff") == 0)
            {
              /* These are common but could also be filenames, so we keep them */
            }
        }

      if (!skip)
        {
          /* This looks like a potential filename candidate */
          g_ptr_array_add (candidates, word);
          g_array_append_val (start_cols, start_col);
          g_array_append_val (end_cols, end_col);
        }
      else
        {
          g_free (word);
        }
    }

  /* For ls -la style output, the filename is typically the LAST word on the line.
   * For symlinks it's "name -> target" so we may have two filenames.
   * For regular ls output (multi-column), all words could be filenames.
   *
   * Strategy: Accept all candidate words as potential files if they pass heuristics.
   * This is more permissive but allows clicking on files listed from other directories.
   */
  for (guint i = 0; i < candidates->len; i++)
    {
      gchar *word = g_ptr_array_index (candidates, i);
      glong start_col = g_array_index (start_cols, glong, i);
      glong end_col = g_array_index (end_cols, glong, i);

      /* Final filtering: must have at least one letter OR be . or .. OR contain . or / */
      gboolean accept = FALSE;
      gboolean has_letter = FALSE;
      for (const gchar *c = word; *c != '\0'; c = g_utf8_next_char (c))
        {
          gunichar uc = g_utf8_get_char (c);
          if (g_unichar_isalpha (uc))
            {
              has_letter = TRUE;
              break;
            }
        }

      if (has_letter)
        accept = TRUE;
      else if (strcmp (word, ".") == 0 || strcmp (word, "..") == 0)
        accept = TRUE;
      else if (strchr (word, '.') != NULL || strchr (word, '/') != NULL)
        accept = TRUE;

      /* Also filter out common time patterns like "HH:MM" or "HH:MM:SS" */
      if (accept && strchr (word, ':') != NULL)
        {
          /* Check if it looks like a time */
          gboolean all_time_chars = TRUE;
          for (const gchar *c = word; *c != '\0'; c++)
            {
              if (!g_ascii_isdigit (*c) && *c != ':')
                {
                  all_time_chars = FALSE;
                  break;
                }
            }
          if (all_time_chars)
            accept = FALSE;
        }

      if (accept)
        {
          TerminalFileEntry *entry = terminal_file_entry_new (word, start_col, end_col, row);

          /* Try to build full path for file operations */
          if (selection->working_directory != NULL)
            {
              if (g_path_is_absolute (word))
                entry->full_path = g_strdup (word);
              else
                entry->full_path = g_build_filename (selection->working_directory, word, NULL);

              /* Check if it's actually a directory for icon purposes */
              if (g_file_test (entry->full_path, G_FILE_TEST_IS_DIR))
                entry->is_directory = TRUE;
            }

          row_entries = g_list_prepend (row_entries, entry);
          selection->entries = g_list_prepend (selection->entries, entry);
          g_warning ("file-selection: Added entry '%s' at row=%ld col=%ld-%ld",
                     word, row, start_col, end_col);
        }
    }

  /* Store row entries in map for fast lookup */
  if (row_entries != NULL)
    {
      row_entries = g_list_reverse (row_entries);
      g_hash_table_insert (selection->entry_map, GLONG_TO_POINTER (row), row_entries);
      g_warning ("file-selection: Stored %u entries for row %ld in entry_map",
                 g_list_length (row_entries), row);
    }

  g_ptr_array_free (candidates, TRUE);
  g_array_free (start_cols, TRUE);
  g_array_free (end_cols, TRUE);
}



/**
 * Parse a line of terminal output and detect file entries (legacy version without attrs).
 */
static void
parse_line_for_files (TerminalFileSelection *selection,
                      const gchar *line,
                      glong row)
{
  GMatchInfo *match_info = NULL;
  GList *row_entries = NULL;

  if (line == NULL || *line == '\0')
    return;

  /* Skip very long lines (likely binary data) */
  glong line_len = g_utf8_strlen (line, -1);
  if (line_len > 4096)
    return;

  /* Try ls -l pattern first */
  if (selection->ls_long_regex != NULL &&
      g_regex_match (selection->ls_long_regex, line, 0, &match_info))
    {
      gchar *permissions = g_match_info_fetch (match_info, 1);
      gchar *filename = g_match_info_fetch (match_info, 2);

      if (filename != NULL && *filename != '\0')
        {
          /* Handle symlinks: "name -> target" */
          gchar *arrow = strstr (filename, " -> ");
          if (arrow != NULL)
            *arrow = '\0';

          /* Find position of filename in the line */
          gchar *pos = strstr (line, filename);
          if (pos != NULL)
            {
              glong start_col = g_utf8_pointer_to_offset (line, pos);
              glong end_col = start_col + g_utf8_strlen (filename, -1);

              TerminalFileEntry *entry = terminal_file_entry_new (filename, start_col, end_col, row);

              /* Detect directories */
              if (permissions != NULL && permissions[0] == 'd')
                entry->is_directory = TRUE;

              /* Set full path */
              if (selection->working_directory != NULL)
                {
                  if (g_path_is_absolute (filename))
                    entry->full_path = g_strdup (filename);
                  else
                    entry->full_path = g_build_filename (selection->working_directory, filename, NULL);
                }

              row_entries = g_list_prepend (row_entries, entry);
              selection->entries = g_list_prepend (selection->entries, entry);
            }
        }

      g_free (permissions);
      g_free (filename);
      g_match_info_free (match_info);
    }
  else
    {
      if (match_info != NULL)
        g_match_info_free (match_info);
      match_info = NULL;

      /* Extract words and verify they exist as files, then find their
       * actual position in the line for accurate column calculation */
      gchar **words = g_strsplit_set (line, " \t", -1);

      for (gint i = 0; words[i] != NULL; i++)
        {
          gchar *word = words[i];
          glong word_len = g_utf8_strlen (word, -1);

          /* Skip empty strings and obvious non-files */
          if (word_len == 0 ||
              (word[0] == '-' && word_len > 1) ||
              g_ascii_isdigit (word[0]) ||
              strcmp (word, "total") == 0)
            {
              continue;
            }

          /* Basic filename heuristics */
          gboolean has_letter = FALSE;
          for (const gchar *c = word; *c != '\0'; c = g_utf8_next_char (c))
            {
              gunichar uc = g_utf8_get_char (c);
              if (g_unichar_isalpha (uc))
                has_letter = TRUE;
            }

          if (has_letter || strcmp (word, ".") == 0 || strcmp (word, "..") == 0 ||
              strchr (word, '.') != NULL || strchr (word, '/') != NULL)
            {
              /* Verify this looks like a real path by checking existence */
              gboolean verified = FALSE;
              gchar *test_path = NULL;

              if (selection->working_directory != NULL)
                {
                  if (g_path_is_absolute (word))
                    test_path = g_strdup (word);
                  else
                    test_path = g_build_filename (selection->working_directory, word, NULL);

                  if (g_file_test (test_path, G_FILE_TEST_EXISTS))
                    verified = TRUE;
                }

              if (verified)
                {
                  /* Find the actual position of this word in the line by searching
                   * for the last whole-word occurrence */
                  const gchar *found = NULL;
                  const gchar *p = line;

                  while ((p = strstr (p, word)) != NULL)
                    {
                      /* Check for whole word match */
                      gboolean word_start_ok = (p == line || *(p - 1) == ' ' || *(p - 1) == '\t');
                      gchar next_char = *(p + strlen (word));
                      gboolean word_end_ok = (next_char == '\0' || next_char == ' ' || next_char == '\t' || next_char == '\n');

                      if (word_start_ok && word_end_ok)
                        found = p;  /* Remember this match, keep looking for later ones */

                      p++;
                    }

                  if (found != NULL)
                    {
                      glong start_col = g_utf8_pointer_to_offset (line, found);
                      glong end_col = start_col + word_len;

                      g_debug ("file-selection: row %ld word '%s' at col %ld-%ld (line='%.40s...')",
                               row, word, start_col, end_col, line);

                      TerminalFileEntry *entry = terminal_file_entry_new (word, start_col, end_col, row);
                      entry->full_path = test_path;
                      test_path = NULL;

                      entry->is_directory = g_file_test (entry->full_path, G_FILE_TEST_IS_DIR);

                      row_entries = g_list_prepend (row_entries, entry);
                      selection->entries = g_list_prepend (selection->entries, entry);
                    }
                }

              g_free (test_path);
            }
        }

      g_strfreev (words);
    }

  /* Store row entries in map for fast lookup */
  if (row_entries != NULL)
    {
      row_entries = g_list_reverse (row_entries);
      g_hash_table_insert (selection->entry_map, GLONG_TO_POINTER (row), row_entries);
    }
}



static gboolean
update_from_contents_idle (gpointer user_data)
{
  TerminalFileSelection *selection = user_data;
  glong row;
  guint new_hash = 0;
  glong visible_rows, columns;
  glong scroll_delta;

  selection->update_idle_id = 0;

  if (!selection->enabled || selection->terminal == NULL)
    return G_SOURCE_REMOVE;

  /* Get terminal dimensions */
  visible_rows = vte_terminal_get_row_count (selection->terminal);
  columns = vte_terminal_get_column_count (selection->terminal);

  /* Get scroll position - this is the first visible row in scrollback */
  GtkAdjustment *vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (selection->terminal));
  scroll_delta = (vadj != NULL) ? (glong) gtk_adjustment_get_value (vadj) : 0;

  /* Quick content change detection using simple hash */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gchar *sample = vte_terminal_get_text (selection->terminal, NULL, NULL, NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
  if (sample != NULL)
    {
      /* Simple hash of first 1000 chars */
      for (gint j = 0; j < 1000 && sample[j] != '\0'; j++)
        new_hash = new_hash * 31 + sample[j];

      g_free (sample);
    }

  if (new_hash == selection->content_hash)
    return G_SOURCE_REMOVE;

  selection->content_hash = new_hash;

  /* Clear existing entries */
  g_hash_table_remove_all (selection->entry_map);
  g_list_free_full (selection->entries, (GDestroyNotify) terminal_file_entry_free);
  selection->entries = NULL;
  selection->hovered_entry = NULL;

  /* Scan each visible row using vte_terminal_get_text_range
   * This gives us the text content for each row */
  for (row = 0; row < visible_rows && g_list_length (selection->entries) < MAX_FILE_ENTRIES; row++)
    {
      gchar *line;
      glong absolute_row = scroll_delta + row;

      /* Get text for this specific row - don't use attrs array since VTE's deprecated API
       * doesn't fill it correctly. We'll calculate column positions from the text directly. */
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      line = vte_terminal_get_text_range (selection->terminal,
                                          absolute_row, 0,
                                          absolute_row, columns - 1,
                                          NULL, NULL, NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS

      if (line != NULL)
        {
          glong len = strlen (line);

          /* Remove trailing newline if present */
          if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

          /* Parse with the visual row number (0 = first visible row on screen) */
          if (*line != '\0')
            {
              /* Use the simpler parser that doesn't need VteCharAttributes */
              parse_line_for_files (selection, line, row);
            }
        }

      g_free (line);
    }

  /* Reverse to maintain order */
  selection->entries = g_list_reverse (selection->entries);

  /* Trigger redraw */
  gtk_widget_queue_draw (GTK_WIDGET (selection->terminal));

  return G_SOURCE_REMOVE;
}



/**
 * terminal_file_selection_update_from_contents:
 * @selection: A #TerminalFileSelection.
 *
 * Scans terminal contents for file/directory entries.
 */
void
terminal_file_selection_update_from_contents (TerminalFileSelection *selection)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  /* Debounce updates */
  if (selection->update_idle_id == 0)
    {
      selection->update_idle_id = g_idle_add (update_from_contents_idle, selection);
    }
}



/**
 * terminal_file_selection_set_selection_colors:
 * @selection: A #TerminalFileSelection.
 * @fg_color: Foreground color for selected items.
 * @bg_color: Background color for selected items.
 */
void
terminal_file_selection_set_selection_colors (TerminalFileSelection *selection,
                                              const GdkRGBA *fg_color,
                                              const GdkRGBA *bg_color)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  if (fg_color != NULL)
    selection->selection_fg = *fg_color;
  if (bg_color != NULL)
    selection->selection_bg = *bg_color;
}



/**
 * terminal_file_selection_set_hover_colors:
 * @selection: A #TerminalFileSelection.
 * @fg_color: Foreground color for hovered items.
 * @bg_color: Background color for hovered items.
 */
void
terminal_file_selection_set_hover_colors (TerminalFileSelection *selection,
                                          const GdkRGBA *fg_color,
                                          const GdkRGBA *bg_color)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  if (fg_color != NULL)
    selection->hover_fg = *fg_color;
  if (bg_color != NULL)
    selection->hover_bg = *bg_color;
}



/**
 * terminal_file_selection_open_selected_in_thunar:
 * @selection: A #TerminalFileSelection.
 *
 * Opens selected files in Thunar file manager.
 */
void
terminal_file_selection_open_selected_in_thunar (TerminalFileSelection *selection)
{
  GList *selected;
  GPtrArray *args;
  GError *error = NULL;

  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));

  selected = terminal_file_selection_get_selected (selection);
  if (selected == NULL)
    return;

  args = g_ptr_array_new ();
  g_ptr_array_add (args, "thunar");

  for (GList *l = selected; l != NULL; l = l->next)
    {
      TerminalFileEntry *entry = l->data;
      if (entry->full_path != NULL)
        g_ptr_array_add (args, entry->full_path);
    }

  g_ptr_array_add (args, NULL);

  g_spawn_async (NULL,
                 (gchar **) args->pdata,
                 NULL,
                 G_SPAWN_SEARCH_PATH,
                 NULL,
                 NULL,
                 NULL,
                 &error);

  if (error != NULL)
    {
      g_warning ("Failed to open Thunar: %s", error->message);
      g_error_free (error);
    }

  g_ptr_array_free (args, TRUE);
  g_list_free (selected);
}



/**
 * terminal_file_selection_is_app_using_mouse:
 * @selection: A #TerminalFileSelection.
 *
 * Checks if a terminal application (like vim, htop) is capturing mouse events.
 * We use multiple heuristics to detect this:
 * 1. Check if we're in alternate screen mode (used by vim, nano, less, etc.)
 * 2. Check cursor visibility (TUI apps often hide cursor)
 * 3. Track if previous mouse events were consumed by terminal app
 *
 * Returns: TRUE if mouse reporting is likely active.
 */
gboolean
terminal_file_selection_is_app_using_mouse (TerminalFileSelection *selection)
{
  gchar *text;
  gboolean is_tui_app = FALSE;

  g_return_val_if_fail (TERMINAL_IS_FILE_SELECTION (selection), FALSE);

  if (selection->terminal == NULL)
    return FALSE;

  /* Simple heuristic: if mouse_reporting_active was set, trust it */
  if (selection->mouse_reporting_active)
    return TRUE;

  /* Check for alternate screen buffer indicators
   * In alternate screen mode, the scrollback is typically disabled or different.
   * We can detect this by checking the adjustment range. */
  GtkAdjustment *vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (selection->terminal));
  if (vadj != NULL)
    {
      gdouble upper = gtk_adjustment_get_upper (vadj);
      gdouble page_size = gtk_adjustment_get_page_size (vadj);
      glong row_count = vte_terminal_get_row_count (selection->terminal);

      /* In alternate screen mode, upper == page_size == row_count
       * (no scrollback available) */
      if (upper <= page_size && upper <= row_count + 1)
        {
          /* Could be alternate screen mode - do additional check */
          /* Check if the screen content looks like a TUI app
           * (this is a very rough heuristic) */
          G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          text = vte_terminal_get_text (selection->terminal, NULL, NULL, NULL);
          G_GNUC_END_IGNORE_DEPRECATIONS
          if (text != NULL)
            {
              /* TUI apps often have top/bottom status bars, full-width content,
               * or special characters like box drawing characters */
              if (g_strstr_len (text, 200, "│") != NULL ||
                  g_strstr_len (text, 200, "─") != NULL ||
                  g_strstr_len (text, 200, "┌") != NULL ||
                  g_strstr_len (text, 200, "└") != NULL)
                {
                  is_tui_app = TRUE;
                }
              g_free (text);
            }
        }
    }

  return is_tui_app;
}



/**
 * terminal_file_selection_set_mouse_reporting:
 * @selection: A #TerminalFileSelection.
 * @active: Whether mouse reporting is active.
 *
 * Called externally to inform the file selection system that a mouse
 * event was consumed by the terminal application.
 */
void
terminal_file_selection_set_mouse_reporting (TerminalFileSelection *selection,
                                             gboolean active)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));
  selection->mouse_reporting_active = active;
}



/**
 * terminal_file_selection_set_double_click_opens:
 * @selection: A #TerminalFileSelection.
 * @enabled: Whether double-click should open files.
 */
void
terminal_file_selection_set_double_click_opens (TerminalFileSelection *selection,
                                                gboolean enabled)
{
  g_return_if_fail (TERMINAL_IS_FILE_SELECTION (selection));
  selection->double_click_opens = enabled;
  g_object_notify_by_pspec (G_OBJECT (selection), file_selection_props[PROP_DOUBLE_CLICK_OPENS]);
}
