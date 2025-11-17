/*-
 * Copyright (c) 2024 XFCE Development Team
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
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "terminal-breadcrumb-bar.h"

/* Signal identifiers */
enum
{
  PATH_CLICKED,
  LAST_SIGNAL
};

struct _TerminalBreadcrumbBarPrivate
{
  gchar *current_path;
  gchar *display_path;  /* Path to display in breadcrumb */
  GtkWidget *breadcrumb_box;
  GtkWidget *scrolled_window;
  GtkWidget *toggle_button;  /* Button to toggle between ~ and full path */
  gboolean enabled;
  gboolean click_in_progress;  /* True when click navigation is happening */
  gboolean show_full_path;  /* TRUE = show /home/user, FALSE = show ~ */
  gint active_segment_index;  /* Index of last clicked segment, -1 if none */
};

static void terminal_breadcrumb_bar_finalize (GObject *object);
static void terminal_breadcrumb_bar_update_breadcrumbs (TerminalBreadcrumbBar *bar);
static void terminal_breadcrumb_bar_segment_clicked (GtkButton *button,
                                                     TerminalBreadcrumbBar *bar);
static void terminal_breadcrumb_bar_toggle_clicked (GtkButton *button,
                                                    TerminalBreadcrumbBar *bar);

static guint breadcrumb_bar_signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (TerminalBreadcrumbBar, terminal_breadcrumb_bar, GTK_TYPE_BOX)

static void
terminal_breadcrumb_bar_class_init (TerminalBreadcrumbBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = terminal_breadcrumb_bar_finalize;

  /**
   * TerminalBreadcrumbBar::path-clicked:
   * @bar       : A #TerminalBreadcrumbBar.
   * @path      : The full path that was clicked.
   *
   * Emitted when a breadcrumb segment is clicked.
   **/
  breadcrumb_bar_signals[PATH_CLICKED] =
    g_signal_new ("path-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalBreadcrumbBarClass, path_clicked),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);
}

static void
terminal_breadcrumb_bar_init (TerminalBreadcrumbBar *bar)
{
  GtkCssProvider *css_provider;
  const gchar *css_data;

  bar->priv = terminal_breadcrumb_bar_get_instance_private (bar);
  bar->priv->current_path = NULL;
  bar->priv->display_path = NULL;
  bar->priv->enabled = TRUE;
  bar->priv->click_in_progress = FALSE;
  bar->priv->show_full_path = FALSE;  /* Default: show ~ instead of /home/user */
  bar->priv->active_segment_index = -1;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (bar), 0);
  gtk_widget_set_margin_start (GTK_WIDGET (bar), 3);
  gtk_widget_set_margin_end (GTK_WIDGET (bar), 3);
  gtk_widget_set_margin_top (GTK_WIDGET (bar), 2);
  gtk_widget_set_margin_bottom (GTK_WIDGET (bar), 2);

  /* Add CSS for breadcrumb styling */
  css_provider = gtk_css_provider_new ();
  css_data = ".root-path { color: #cc0000; font-weight: bold; }";
  gtk_css_provider_load_from_data (css_provider, css_data, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                              GTK_STYLE_PROVIDER (css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  /* Create toggle button for ~ vs full path */
  bar->priv->toggle_button = gtk_button_new_with_label ("~/...");  /* Show we're in short mode */
  gtk_button_set_relief (GTK_BUTTON (bar->priv->toggle_button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (bar->priv->toggle_button, "Show full path");
  g_signal_connect (bar->priv->toggle_button, "clicked",
                    G_CALLBACK (terminal_breadcrumb_bar_toggle_clicked), bar);
  gtk_box_pack_start (GTK_BOX (bar), bar->priv->toggle_button, FALSE, FALSE, 0);

  /* Create scrolled window for horizontal scrolling of long paths */
  bar->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (bar->priv->scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (bar->priv->scrolled_window),
                                       GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (bar), bar->priv->scrolled_window, TRUE, TRUE, 0);

  /* Create container for breadcrumb buttons */
  bar->priv->breadcrumb_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (bar->priv->scrolled_window), bar->priv->breadcrumb_box);

  gtk_widget_show_all (GTK_WIDGET (bar));
}

static void
terminal_breadcrumb_bar_finalize (GObject *object)
{
  TerminalBreadcrumbBar *bar = TERMINAL_BREADCRUMB_BAR (object);

  g_free (bar->priv->current_path);
  g_free (bar->priv->display_path);

  G_OBJECT_CLASS (terminal_breadcrumb_bar_parent_class)->finalize (object);
}

/**
 * terminal_breadcrumb_bar_new:
 *
 * Creates a new #TerminalBreadcrumbBar.
 *
 * Return value: A new #TerminalBreadcrumbBar.
 **/
GtkWidget *
terminal_breadcrumb_bar_new (void)
{
  return g_object_new (TERMINAL_TYPE_BREADCRUMB_BAR, NULL);
}

static void
terminal_breadcrumb_bar_toggle_clicked (GtkButton *button,
                                        TerminalBreadcrumbBar *bar)
{
  /* Toggle between ~ and full path */
  bar->priv->show_full_path = !bar->priv->show_full_path;

  /* Update button label and tooltip based on new state */
  if (bar->priv->show_full_path)
    {
      gtk_button_set_label (GTK_BUTTON (bar->priv->toggle_button), "/...");
      gtk_widget_set_tooltip_text (bar->priv->toggle_button, "Show short path (~)");
    }
  else
    {
      gtk_button_set_label (GTK_BUTTON (bar->priv->toggle_button), "~/...");
      gtk_widget_set_tooltip_text (bar->priv->toggle_button, "Show full path");
    }

  /* Refresh breadcrumb display */
  terminal_breadcrumb_bar_update_breadcrumbs (bar);
}

static void
terminal_breadcrumb_bar_segment_clicked (GtkButton *button,
                                         TerminalBreadcrumbBar *bar)
{
  const gchar *path;

  path = g_object_get_data (G_OBJECT (button), "breadcrumb-path");
  if (path != NULL)
    {
      /* Mark that a click navigation is happening */
      bar->priv->click_in_progress = TRUE;

      /* Store which segment was clicked */
      bar->priv->active_segment_index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "breadcrumb-index"));

      /* Update visual states */
      terminal_breadcrumb_bar_update_breadcrumbs (bar);

      /* Emit signal */
      g_signal_emit (bar, breadcrumb_bar_signals[PATH_CLICKED], 0, path);
    }
}

static void
terminal_breadcrumb_bar_clear_breadcrumbs (TerminalBreadcrumbBar *bar)
{
  GList *children, *iter;

  children = gtk_container_get_children (GTK_CONTAINER (bar->priv->breadcrumb_box));
  for (iter = children; iter != NULL; iter = iter->next)
    {
      gtk_widget_destroy (GTK_WIDGET (iter->data));
    }
  g_list_free (children);
}

static gchar *
terminal_breadcrumb_bar_expand_home (const gchar *path)
{
  const gchar *home;

  if (path == NULL)
    return NULL;

  /* Check if path starts with ~ */
  if (path[0] == '~' && (path[1] == '/' || path[1] == '\0'))
    {
      home = g_get_home_dir ();
      if (path[1] == '\0')
        return g_strdup (home);
      else
        return g_build_filename (home, path + 2, NULL);
    }

  return g_strdup (path);
}

static gchar *
terminal_breadcrumb_bar_compress_home (const gchar *path)
{
  const gchar *home;
  gsize home_len;

  if (path == NULL)
    return NULL;

  home = g_get_home_dir ();
  home_len = strlen (home);

  /* Check if path starts with home directory */
  if (strncmp (path, home, home_len) == 0 && (path[home_len] == '/' || path[home_len] == '\0'))
    {
      if (path[home_len] == '\0')
        return g_strdup ("~");
      else
        return g_strconcat ("~", path + home_len, NULL);
    }

  return g_strdup (path);
}

static void
terminal_breadcrumb_bar_update_breadcrumbs (TerminalBreadcrumbBar *bar)
{
  gchar *expanded_path;
  gchar *display_path;
  gchar **path_parts;
  gint i;
  gchar *accumulated_path;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;
  GtkStyleContext *context;

  if (bar->priv->current_path == NULL || !bar->priv->enabled)
    {
      terminal_breadcrumb_bar_clear_breadcrumbs (bar);
      return;
    }

  /* Clear existing breadcrumbs */
  terminal_breadcrumb_bar_clear_breadcrumbs (bar);

  /* Use display_path if set, otherwise current_path */
  const gchar *path_to_render = bar->priv->display_path ? bar->priv->display_path : bar->priv->current_path;

  /* Expand ~ to full home path for processing */
  expanded_path = terminal_breadcrumb_bar_expand_home (path_to_render);

  /* Compress or keep full based on toggle setting */
  if (bar->priv->show_full_path)
    display_path = g_strdup (expanded_path);  /* Show full path like /home/mehran/... */
  else
    display_path = terminal_breadcrumb_bar_compress_home (expanded_path);  /* Show ~ */

  /* Check if path is actually under home directory (for styling purposes) */
  const gchar *home = g_get_home_dir ();
  gboolean is_under_home = g_str_has_prefix (expanded_path, home);

  /* Handle root separately */
  gboolean starts_with_home = (display_path[0] == '~');
  const gchar *parse_path = starts_with_home ? display_path + 1 : display_path;

  /* Split path into components */
  if (parse_path[0] == '/')
    parse_path++;

  path_parts = g_strsplit (parse_path, "/", -1);

  /* Add root or home segment */
  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  if (starts_with_home)
    {
      label = gtk_label_new ("~");
      accumulated_path = g_strdup (g_get_home_dir ());
    }
  else
    {
      label = gtk_label_new ("/");
      accumulated_path = g_strdup ("/");
      /* Mark root paths with special styling (only if NOT under home) */
      if (!is_under_home)
        {
          context = gtk_widget_get_style_context (label);
          gtk_style_context_add_class (context, "root-path");
        }
    }

  gtk_container_add (GTK_CONTAINER (button), label);
  g_object_set_data_full (G_OBJECT (button), "breadcrumb-path",
                          g_strdup (accumulated_path), g_free);
  g_object_set_data (G_OBJECT (button), "breadcrumb-index", GINT_TO_POINTER (0));
  g_signal_connect (button, "clicked",
                    G_CALLBACK (terminal_breadcrumb_bar_segment_clicked), bar);

  /* Apply gray style if this segment is inactive (to the right of clicked segment) */
  if (bar->priv->active_segment_index >= 0 && 0 > bar->priv->active_segment_index)
    {
      context = gtk_widget_get_style_context (label);
      gtk_style_context_add_class (context, "dim-label");
    }

  gtk_box_pack_start (GTK_BOX (bar->priv->breadcrumb_box), button, FALSE, FALSE, 0);

  /* Add path segments */
  for (i = 0; path_parts[i] != NULL; i++)
    {
      if (strlen (path_parts[i]) == 0)
        continue;

      /* Add separator */
      separator = gtk_label_new ("›");
      context = gtk_widget_get_style_context (separator);
      gtk_style_context_add_class (context, "breadcrumb-separator");
      gtk_box_pack_start (GTK_BOX (bar->priv->breadcrumb_box), separator, FALSE, FALSE, 2);

      /* Add segment button */
      button = gtk_button_new ();
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      label = gtk_label_new (path_parts[i]);
      gtk_container_add (GTK_CONTAINER (button), label);

      /* Build accumulated path */
      gchar *temp = accumulated_path;
      if (g_str_has_suffix (accumulated_path, "/"))
        accumulated_path = g_strconcat (accumulated_path, path_parts[i], NULL);
      else
        accumulated_path = g_strconcat (accumulated_path, "/", path_parts[i], NULL);
      g_free (temp);

      g_object_set_data_full (G_OBJECT (button), "breadcrumb-path",
                              g_strdup (accumulated_path), g_free);
      g_object_set_data (G_OBJECT (button), "breadcrumb-index", GINT_TO_POINTER (i + 1));
      g_signal_connect (button, "clicked",
                        G_CALLBACK (terminal_breadcrumb_bar_segment_clicked), bar);

      /* Mark root-level paths with special styling (only if NOT under home) */
      if (!is_under_home)
        {
          context = gtk_widget_get_style_context (label);
          gtk_style_context_add_class (context, "root-path");
        }

      /* Apply gray style if this segment is inactive (to the right of clicked segment) */
      if (bar->priv->active_segment_index >= 0 && (i + 1) > bar->priv->active_segment_index)
        {
          context = gtk_widget_get_style_context (label);
          gtk_style_context_add_class (context, "dim-label");
        }

      gtk_box_pack_start (GTK_BOX (bar->priv->breadcrumb_box), button, FALSE, FALSE, 0);
    }

  g_strfreev (path_parts);
  g_free (accumulated_path);
  g_free (expanded_path);
  g_free (display_path);

  gtk_widget_show_all (bar->priv->breadcrumb_box);
}

/**
 * terminal_breadcrumb_bar_set_path:
 * @bar   : A #TerminalBreadcrumbBar.
 * @path  : The current working directory path.
 *
 * Sets the current working directory path and updates the breadcrumb display.
 **/
void
terminal_breadcrumb_bar_set_path (TerminalBreadcrumbBar *bar,
                                  const gchar *path)
{
  g_return_if_fail (TERMINAL_IS_BREADCRUMB_BAR (bar));

  /* Only update if path actually changed */
  if (g_strcmp0 (bar->priv->current_path, path) == 0)
    return;

  g_free (bar->priv->current_path);
  bar->priv->current_path = g_strdup (path);

  /* If this is from a click, keep the display_path unchanged */
  if (bar->priv->click_in_progress)
    {
      /* Reset the flag after a short delay */
      bar->priv->click_in_progress = FALSE;
    }
  else
    {
      /* Manual cd - update display_path to match and reset active segment */
      g_free (bar->priv->display_path);
      bar->priv->display_path = g_strdup (path);
      bar->priv->active_segment_index = -1;  /* Reset graying */
    }

  terminal_breadcrumb_bar_update_breadcrumbs (bar);
}

/**
 * terminal_breadcrumb_bar_get_path:
 * @bar   : A #TerminalBreadcrumbBar.
 *
 * Returns the current path.
 *
 * Return value: The current path, or %NULL.
 **/
const gchar *
terminal_breadcrumb_bar_get_path (TerminalBreadcrumbBar *bar)
{
  g_return_val_if_fail (TERMINAL_IS_BREADCRUMB_BAR (bar), NULL);

  return bar->priv->current_path;
}

/**
 * terminal_breadcrumb_bar_set_enabled:
 * @bar     : A #TerminalBreadcrumbBar.
 * @enabled : Whether the breadcrumb bar is enabled.
 *
 * Enables or disables the breadcrumb bar display.
 **/
void
terminal_breadcrumb_bar_set_enabled (TerminalBreadcrumbBar *bar,
                                     gboolean enabled)
{
  g_return_if_fail (TERMINAL_IS_BREADCRUMB_BAR (bar));

  if (bar->priv->enabled != enabled)
    {
      bar->priv->enabled = enabled;
      terminal_breadcrumb_bar_update_breadcrumbs (bar);

      if (enabled)
        gtk_widget_show (GTK_WIDGET (bar));
      else
        gtk_widget_hide (GTK_WIDGET (bar));
    }
}

/**
 * terminal_breadcrumb_bar_get_enabled:
 * @bar   : A #TerminalBreadcrumbBar.
 *
 * Returns whether the breadcrumb bar is enabled.
 *
 * Return value: %TRUE if enabled.
 **/
gboolean
terminal_breadcrumb_bar_get_enabled (TerminalBreadcrumbBar *bar)
{
  g_return_val_if_fail (TERMINAL_IS_BREADCRUMB_BAR (bar), FALSE);

  return bar->priv->enabled;
}
