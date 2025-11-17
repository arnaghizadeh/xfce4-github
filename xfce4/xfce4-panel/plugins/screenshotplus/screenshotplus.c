/*
 * Copyright (C) 2024 XFCE Development Team
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <signal.h>



#define DEFAULT_DELAY (0)
#define DEFAULT_SCREENSHOT_DIR "Pictures/Screenshots"
#define DEFAULT_VIDEO_DIR "Videos/Recordings"

#define SCREENSHOTPLUS_TYPE_PLUGIN (screenshotplus_plugin_get_type ())
#define SCREENSHOTPLUS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCREENSHOTPLUS_TYPE_PLUGIN, ScreenshotPlusPlugin))
#define SCREENSHOTPLUS_IS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCREENSHOTPLUS_TYPE_PLUGIN))

typedef struct _ScreenshotPlusPlugin ScreenshotPlusPlugin;
typedef struct _ScreenshotPlusPluginClass ScreenshotPlusPluginClass;

struct _ScreenshotPlusPlugin
{
  XfcePanelPlugin __parent__;

  GtkWidget *button;
  GtkWidget *icon;
  GtkWidget *menu;

  guint delay;
  gchar *screenshot_dir;
  gchar *video_dir;

  /* Video recording state */
  GPid recording_pid;
  gboolean is_recording;
  gchar *current_video_file;

  /* Region overlay window for showing recording area */
  GtkWidget *region_overlay;
  gint region_x, region_y, region_width, region_height;
  gboolean show_region_outline;

  /* Pulsing animation for overlay */
  guint pulse_timeout_id;
  gdouble pulse_alpha;
  gboolean pulse_direction; /* TRUE = fading in, FALSE = fading out */
};

struct _ScreenshotPlusPluginClass
{
  XfcePanelPluginClass __parent__;
};

/* Helper to create image menu items */
static GtkWidget *
screenshotplus_image_menu_item_new (const gchar *label)
{
  GtkWidget *item;
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  item = gtk_image_menu_item_new_with_mnemonic (label);
  G_GNUC_END_IGNORE_DEPRECATIONS
  return item;
}

static void
screenshotplus_image_menu_item_set_image (GtkWidget *item, GtkWidget *image)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  G_GNUC_END_IGNORE_DEPRECATIONS
}



static void screenshotplus_plugin_construct (XfcePanelPlugin *panel_plugin);
static void screenshotplus_plugin_free_data (XfcePanelPlugin *panel_plugin);
static gboolean screenshotplus_plugin_size_changed (XfcePanelPlugin *panel_plugin, gint size);
static void screenshotplus_plugin_configure_plugin (XfcePanelPlugin *panel_plugin);
static void screenshotplus_plugin_menu (GtkWidget *button, ScreenshotPlusPlugin *plugin);

/* Forward declarations for video recording functions */
static void screenshotplus_record_fullscreen (ScreenshotPlusPlugin *plugin);
static void screenshotplus_record_window (ScreenshotPlusPlugin *plugin);
static void screenshotplus_record_region (ScreenshotPlusPlugin *plugin);

/* Define the plugin type */
XFCE_PANEL_DEFINE_PLUGIN (ScreenshotPlusPlugin, screenshotplus_plugin)



static void
screenshotplus_plugin_class_init (ScreenshotPlusPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = screenshotplus_plugin_construct;
  plugin_class->free_data = screenshotplus_plugin_free_data;
  plugin_class->size_changed = screenshotplus_plugin_size_changed;
  plugin_class->configure_plugin = screenshotplus_plugin_configure_plugin;
}



static void
screenshotplus_plugin_init (ScreenshotPlusPlugin *plugin)
{
  plugin->delay = DEFAULT_DELAY;
  plugin->screenshot_dir = g_strdup (DEFAULT_SCREENSHOT_DIR);
  plugin->video_dir = g_strdup (DEFAULT_VIDEO_DIR);
  plugin->menu = NULL;
  plugin->button = NULL;
  plugin->icon = NULL;
  plugin->recording_pid = 0;
  plugin->is_recording = FALSE;
  plugin->current_video_file = NULL;
  plugin->region_overlay = NULL;
  plugin->region_x = 0;
  plugin->region_y = 0;
  plugin->region_width = 0;
  plugin->region_height = 0;
  plugin->show_region_outline = TRUE;
  plugin->pulse_timeout_id = 0;
  plugin->pulse_alpha = 1.0;
  plugin->pulse_direction = FALSE;
}



static gchar *
screenshotplus_get_save_path (ScreenshotPlusPlugin *plugin, const gchar *suffix, gboolean is_video)
{
  gchar *dir_path;
  gchar *filename;
  gchar *full_path;
  time_t now;
  struct tm *tm_info;
  char timestamp[64];
  const gchar *base_dir;
  const gchar *extension;

  /* Choose directory based on type */
  base_dir = is_video ? plugin->video_dir : plugin->screenshot_dir;
  extension = is_video ? "mp4" : "png";

  /* Build the directory path */
  if (g_path_is_absolute (base_dir))
    dir_path = g_strdup (base_dir);
  else
    dir_path = g_build_filename (g_get_home_dir (), base_dir, NULL);

  /* Create directory if it doesn't exist */
  if (g_mkdir_with_parents (dir_path, 0755) != 0 && errno != EEXIST)
    {
      g_warning ("Failed to create directory: %s", dir_path);
      g_free (dir_path);
      dir_path = g_strdup (g_get_home_dir ());
    }

  /* Generate timestamp for filename */
  time (&now);
  tm_info = localtime (&now);
  strftime (timestamp, sizeof (timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);

  /* Create filename with ScreenshotPlus prefix */
  filename = g_strdup_printf ("ScreenshotPlus_%s%s.%s", timestamp, suffix ? suffix : "", extension);
  full_path = g_build_filename (dir_path, filename, NULL);

  g_free (dir_path);
  g_free (filename);

  return full_path;
}



static void
screenshotplus_show_notification (const gchar *title, const gchar *message, const gchar *icon_name)
{
  GError *error = NULL;
  gchar *escaped_title;
  gchar *escaped_message;
  gchar *command;

  escaped_title = g_shell_quote (title);
  escaped_message = g_shell_quote (message);

  command = g_strdup_printf ("notify-send %s %s -i %s",
                              escaped_title, escaped_message,
                              icon_name ? icon_name : "org.xfce.screenshooter");

  if (!g_spawn_command_line_async (command, &error))
    {
      g_warning ("Failed to show notification: %s", error->message);
      g_error_free (error);
    }

  g_free (escaped_title);
  g_free (escaped_message);
  g_free (command);
}



/* ============== SCREENSHOT FUNCTIONS ============== */

/* Data structure for delayed capture with path */
typedef struct {
  ScreenshotPlusPlugin *plugin;
  gchar *save_path;
} CaptureWithPathData;

static gboolean
screenshotplus_capture_fullscreen_with_path_delayed (gpointer user_data)
{
  CaptureWithPathData *data = (CaptureWithPathData *) user_data;
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *root_window;
  GdkPixbuf *screenshot;
  gint screen_width, screen_height;
  GError *error = NULL;

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  root_window = gdk_screen_get_root_window (screen);

  screen_width = gdk_screen_get_width (screen);
  screen_height = gdk_screen_get_height (screen);

  screenshot = gdk_pixbuf_get_from_window (root_window, 0, 0, screen_width, screen_height);

  if (screenshot == NULL)
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        "Failed to capture the screen",
                                        "dialog-error");
      g_free (data->save_path);
      g_free (data);
      return G_SOURCE_REMOVE;
    }

  if (!gdk_pixbuf_save (screenshot, data->save_path, "png", &error, NULL))
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
    }
  else
    {
      gchar *message = g_strdup_printf ("Saved to:\n%s", data->save_path);
      screenshotplus_show_notification ("Screenshot Captured", message, "org.xfce.screenshooter");
      g_free (message);
      gdk_display_beep (display);
    }

  g_object_unref (screenshot);
  g_free (data->save_path);
  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
screenshotplus_capture_fullscreen_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path)
{
  CaptureWithPathData *data = g_new0 (CaptureWithPathData, 1);
  data->plugin = plugin;
  data->save_path = g_strdup (save_path);

  if (plugin->delay > 0)
    g_timeout_add_seconds (plugin->delay, screenshotplus_capture_fullscreen_with_path_delayed, data);
  else
    g_timeout_add (100, screenshotplus_capture_fullscreen_with_path_delayed, data);
}

static void
screenshotplus_capture_fullscreen (ScreenshotPlusPlugin *plugin)
{
  gchar *save_path = screenshotplus_get_save_path (plugin, "_fullscreen", FALSE);
  screenshotplus_capture_fullscreen_with_path (plugin, save_path);
  g_free (save_path);
}



typedef struct
{
  ScreenshotPlusPlugin *plugin;
  Window selected_window;
  gchar *save_path;
} WindowSelectionData;



static Window
screenshotplus_select_window (Display *dpy)
{
  Cursor cursor;
  Window root, selected_window = None;
  XEvent event;
  int screen;
  int buttons = 0;

  screen = DefaultScreen (dpy);
  root = RootWindow (dpy, screen);

  cursor = XCreateFontCursor (dpy, XC_crosshair);

  if (XGrabPointer (dpy, root, False,
                     ButtonPressMask | ButtonReleaseMask,
                     GrabModeSync, GrabModeAsync,
                     root, cursor, CurrentTime) != GrabSuccess)
    {
      XFreeCursor (dpy, cursor);
      return None;
    }

  XGrabKeyboard (dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);

  while (selected_window == None || buttons != 0)
    {
      XAllowEvents (dpy, SyncPointer, CurrentTime);
      XWindowEvent (dpy, root, ButtonPressMask | ButtonReleaseMask | KeyPressMask, &event);

      switch (event.type)
        {
        case ButtonPress:
          if (event.xbutton.button == Button1)
            {
              selected_window = event.xbutton.subwindow;
              if (selected_window == None)
                selected_window = root;
            }
          else
            {
              selected_window = None;
              buttons = 0;
              goto done;
            }
          buttons++;
          break;

        case ButtonRelease:
          if (buttons > 0)
            buttons--;
          break;

        case KeyPress:
          if (XLookupKeysym (&event.xkey, 0) == XK_Escape)
            {
              selected_window = None;
              buttons = 0;
              goto done;
            }
          break;
        }
    }

done:
  XUngrabKeyboard (dpy, CurrentTime);
  XUngrabPointer (dpy, CurrentTime);
  XFreeCursor (dpy, cursor);
  XFlush (dpy);

  if (selected_window != None && selected_window != root)
    {
      Window parent, root_return;
      Window *children;
      unsigned int nchildren;

      while (TRUE)
        {
          if (XQueryTree (dpy, selected_window, &root_return, &parent, &children, &nchildren) == 0)
            break;

          if (children)
            XFree (children);

          if (parent == root_return || parent == None)
            break;

          selected_window = parent;
        }
    }

  return selected_window;
}



static gboolean
screenshotplus_capture_window_delayed (gpointer user_data)
{
  WindowSelectionData *data = (WindowSelectionData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  Window selected_window = data->selected_window;
  gchar *save_path = data->save_path;
  GdkDisplay *display;
  Display *xdisplay;
  XWindowAttributes attrs;
  GdkPixbuf *screenshot;
  GError *error = NULL;
  gint x, y;
  Window child;
  GdkWindow *root_window;

  g_free (data);

  if (selected_window == None)
    {
      g_free (save_path);
      return G_SOURCE_REMOVE;
    }

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  if (!XGetWindowAttributes (xdisplay, selected_window, &attrs))
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        "Failed to get window attributes",
                                        "dialog-error");
      g_free (save_path);
      return G_SOURCE_REMOVE;
    }

  if (attrs.map_state != IsViewable)
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        "Cannot capture minimized window",
                                        "dialog-error");
      g_free (save_path);
      return G_SOURCE_REMOVE;
    }

  XTranslateCoordinates (xdisplay, selected_window,
                          DefaultRootWindow (xdisplay),
                          0, 0, &x, &y, &child);

  gint capture_x = MAX (0, x);
  gint capture_y = MAX (0, y);
  gint capture_width = attrs.width;
  gint capture_height = attrs.height;

  if (x < 0)
    capture_width += x;
  if (y < 0)
    capture_height += y;

  GdkScreen *screen = gdk_display_get_default_screen (display);
  gint screen_width = gdk_screen_get_width (screen);
  gint screen_height = gdk_screen_get_height (screen);

  if (capture_x + capture_width > screen_width)
    capture_width = screen_width - capture_x;
  if (capture_y + capture_height > screen_height)
    capture_height = screen_height - capture_y;

  if (capture_width <= 0 || capture_height <= 0)
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        "Window is not visible on screen",
                                        "dialog-error");
      g_free (save_path);
      return G_SOURCE_REMOVE;
    }

  root_window = gdk_screen_get_root_window (screen);
  screenshot = gdk_pixbuf_get_from_window (root_window,
                                            capture_x, capture_y,
                                            capture_width, capture_height);

  if (screenshot == NULL)
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        "Failed to capture the window",
                                        "dialog-error");
      g_free (save_path);
      return G_SOURCE_REMOVE;
    }

  if (!gdk_pixbuf_save (screenshot, save_path, "png", &error, NULL))
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
    }
  else
    {
      gchar *message = g_strdup_printf ("Saved to:\n%s", save_path);
      screenshotplus_show_notification ("Screenshot Captured", message, "org.xfce.screenshooter");
      g_free (message);
      gdk_display_beep (display);
    }

  g_free (save_path);
  g_object_unref (screenshot);

  return G_SOURCE_REMOVE;
}

/* Window selection idle with path data */
typedef struct {
  ScreenshotPlusPlugin *plugin;
  gchar *save_path;
} WindowSelectionWithPathData;

static gboolean
screenshotplus_window_selection_with_path_idle (gpointer user_data)
{
  WindowSelectionWithPathData *path_data = (WindowSelectionWithPathData *) user_data;
  ScreenshotPlusPlugin *plugin = path_data->plugin;
  GdkDisplay *display;
  Display *xdisplay;
  Window selected_window;
  WindowSelectionData *data;

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  selected_window = screenshotplus_select_window (xdisplay);

  if (selected_window == None)
    {
      g_free (path_data->save_path);
      g_free (path_data);
      return G_SOURCE_REMOVE;
    }

  data = g_new0 (WindowSelectionData, 1);
  data->plugin = plugin;
  data->selected_window = selected_window;
  data->save_path = path_data->save_path;

  g_free (path_data);

  if (plugin->delay > 0)
    g_timeout_add_seconds (plugin->delay, screenshotplus_capture_window_delayed, data);
  else
    g_timeout_add (100, screenshotplus_capture_window_delayed, data);

  return G_SOURCE_REMOVE;
}

static void
screenshotplus_capture_window_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path)
{
  WindowSelectionWithPathData *data = g_new0 (WindowSelectionWithPathData, 1);
  data->plugin = plugin;
  data->save_path = g_strdup (save_path);
  g_idle_add (screenshotplus_window_selection_with_path_idle, data);
}

static gboolean
screenshotplus_window_selection_idle (gpointer user_data)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (user_data);
  GdkDisplay *display;
  Display *xdisplay;
  Window selected_window;
  WindowSelectionData *data;

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  selected_window = screenshotplus_select_window (xdisplay);

  if (selected_window == None)
    return G_SOURCE_REMOVE;

  data = g_new0 (WindowSelectionData, 1);
  data->save_path = screenshotplus_get_save_path (plugin, "_window", FALSE);
  data->plugin = plugin;
  data->selected_window = selected_window;

  if (plugin->delay > 0)
    g_timeout_add_seconds (plugin->delay, screenshotplus_capture_window_delayed, data);
  else
    g_timeout_add (100, screenshotplus_capture_window_delayed, data);

  return G_SOURCE_REMOVE;
}



static void
screenshotplus_capture_window (ScreenshotPlusPlugin *plugin)
{
  g_idle_add (screenshotplus_window_selection_idle, plugin);
}



/* Region selection data */
typedef struct
{
  ScreenshotPlusPlugin *plugin;
  gint start_x, start_y;
  gint end_x, end_y;
  gboolean is_video;
} RegionSelectionData;



static gboolean
screenshotplus_capture_region_delayed (gpointer user_data)
{
  RegionSelectionData *data = (RegionSelectionData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *root_window;
  GdkPixbuf *screenshot;
  GdkPixbuf *preview;
  gchar *save_path;
  GError *error = NULL;
  gint x, y, width, height;
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *image;
  GtkWidget *vbox;
  GtkWidget *label;
  gint result;
  gint preview_width, preview_height;
  gdouble scale;

  /* Calculate rectangle */
  x = MIN (data->start_x, data->end_x);
  y = MIN (data->start_y, data->end_y);
  width = ABS (data->end_x - data->start_x);
  height = ABS (data->end_y - data->start_y);

  g_free (data);

  if (width <= 0 || height <= 0)
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        "Invalid region selected",
                                        "dialog-error");
      return G_SOURCE_REMOVE;
    }

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  root_window = gdk_screen_get_root_window (screen);

  screenshot = gdk_pixbuf_get_from_window (root_window, x, y, width, height);

  if (screenshot == NULL)
    {
      screenshotplus_show_notification ("Screenshot Failed",
                                        "Failed to capture the region",
                                        "dialog-error");
      return G_SOURCE_REMOVE;
    }

  /* Create preview dialog */
  dialog = gtk_dialog_new_with_buttons ("Save Screenshot?",
                                        NULL,
                                        GTK_DIALOG_MODAL,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Save", GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (content_area), vbox);

  /* Create scaled preview (max 400x300) */
  preview_width = width;
  preview_height = height;
  if (preview_width > 400 || preview_height > 300)
    {
      scale = MIN (400.0 / preview_width, 300.0 / preview_height);
      preview_width = (gint) (preview_width * scale);
      preview_height = (gint) (preview_height * scale);
    }

  preview = gdk_pixbuf_scale_simple (screenshot, preview_width, preview_height, GDK_INTERP_BILINEAR);
  image = gtk_image_new_from_pixbuf (preview);
  gtk_box_pack_start (GTK_BOX (vbox), image, TRUE, TRUE, 0);
  g_object_unref (preview);

  /* Add size label */
  gchar *size_text = g_strdup_printf ("Size: %d x %d pixels", width, height);
  label = gtk_label_new (size_text);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  g_free (size_text);

  gtk_widget_show_all (dialog);

  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (result == GTK_RESPONSE_ACCEPT)
    {
      save_path = screenshotplus_get_save_path (plugin, "_region", FALSE);

      if (!gdk_pixbuf_save (screenshot, save_path, "png", &error, NULL))
        {
          screenshotplus_show_notification ("Screenshot Failed",
                                            error->message,
                                            "dialog-error");
          g_error_free (error);
        }
      else
        {
          gchar *message = g_strdup_printf ("Saved to:\n%s", save_path);
          screenshotplus_show_notification ("Screenshot Captured", message, "org.xfce.screenshooter");
          g_free (message);
          gdk_display_beep (display);
        }

      g_free (save_path);
    }

  g_object_unref (screenshot);

  return G_SOURCE_REMOVE;
}



static void
screenshotplus_draw_selection_rectangle (Display *dpy, Window root, GC gc,
                                          int x, int y, int width, int height)
{
  if (width < 0)
    {
      x += width;
      width = -width;
    }
  if (height < 0)
    {
      y += height;
      height = -height;
    }

  XDrawRectangle (dpy, root, gc, x, y, width, height);
}



static gboolean
screenshotplus_region_selection_idle (gpointer user_data)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (user_data);
  GdkDisplay *display;
  Display *xdisplay;
  Window root;
  Cursor cursor;
  XEvent event;
  int screen_num;
  RegionSelectionData *data;
  gboolean selecting = TRUE;
  gboolean started = FALSE;
  int start_x = 0, start_y = 0, end_x = 0, end_y = 0;
  int prev_x = 0, prev_y = 0;
  GC gc;
  XGCValues gcval;

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  screen_num = DefaultScreen (xdisplay);
  root = RootWindow (xdisplay, screen_num);

  cursor = XCreateFontCursor (xdisplay, XC_crosshair);

  /* Create GC for drawing rectangle */
  gcval.foreground = XWhitePixel (xdisplay, screen_num);
  gcval.function = GXxor;
  gcval.background = XBlackPixel (xdisplay, screen_num);
  gcval.plane_mask = gcval.background ^ gcval.foreground;
  gcval.subwindow_mode = IncludeInferiors;
  gcval.line_width = 2;

  gc = XCreateGC (xdisplay, root,
                   GCFunction | GCForeground | GCBackground | GCSubwindowMode | GCLineWidth,
                   &gcval);

  if (XGrabPointer (xdisplay, root, False,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync,
                     root, cursor, CurrentTime) != GrabSuccess)
    {
      XFreeCursor (xdisplay, cursor);
      XFreeGC (xdisplay, gc);
      return G_SOURCE_REMOVE;
    }

  XGrabKeyboard (xdisplay, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);

  while (selecting)
    {
      XNextEvent (xdisplay, &event);

      switch (event.type)
        {
        case ButtonPress:
          if (event.xbutton.button == Button1)
            {
              start_x = event.xbutton.x_root;
              start_y = event.xbutton.y_root;
              end_x = start_x;
              end_y = start_y;
              prev_x = start_x;
              prev_y = start_y;
              started = TRUE;
            }
          else
            {
              selecting = FALSE;
              started = FALSE;
            }
          break;

        case ButtonRelease:
          if (event.xbutton.button == Button1 && started)
            {
              /* Erase the last rectangle */
              screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                        start_x, start_y,
                                                        prev_x - start_x, prev_y - start_y);
              end_x = event.xbutton.x_root;
              end_y = event.xbutton.y_root;
              selecting = FALSE;
            }
          break;

        case MotionNotify:
          if (started)
            {
              /* Erase the previous rectangle */
              screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                        start_x, start_y,
                                                        prev_x - start_x, prev_y - start_y);

              /* Draw the new rectangle */
              end_x = event.xmotion.x_root;
              end_y = event.xmotion.y_root;
              screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                        start_x, start_y,
                                                        end_x - start_x, end_y - start_y);
              prev_x = end_x;
              prev_y = end_y;
            }
          break;

        case KeyPress:
          if (XLookupKeysym (&event.xkey, 0) == XK_Escape)
            {
              if (started)
                {
                  /* Erase the rectangle */
                  screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                            start_x, start_y,
                                                            prev_x - start_x, prev_y - start_y);
                }
              selecting = FALSE;
              started = FALSE;
            }
          break;
        }
    }

  XUngrabKeyboard (xdisplay, CurrentTime);
  XUngrabPointer (xdisplay, CurrentTime);
  XFreeCursor (xdisplay, cursor);
  XFreeGC (xdisplay, gc);
  XFlush (xdisplay);

  if (!started)
    return G_SOURCE_REMOVE;

  data = g_new0 (RegionSelectionData, 1);
  data->plugin = plugin;
  data->start_x = start_x;
  data->start_y = start_y;
  data->end_x = end_x;
  data->end_y = end_y;
  data->is_video = FALSE;

  if (plugin->delay > 0)
    g_timeout_add_seconds (plugin->delay, screenshotplus_capture_region_delayed, data);
  else
    g_timeout_add (100, screenshotplus_capture_region_delayed, data);

  return G_SOURCE_REMOVE;
}



static void
screenshotplus_capture_region (ScreenshotPlusPlugin *plugin)
{
  g_idle_add (screenshotplus_region_selection_idle, plugin);
}

/* Stub for region capture with path - for now just use regular region capture */
static void
screenshotplus_capture_region_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path)
{
  /* TODO: implement proper path handling for region capture */
  screenshotplus_capture_region (plugin);
}



/* ============== REGION OVERLAY FUNCTIONS ============== */

static gboolean
screenshotplus_overlay_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (user_data);
  GtkAllocation allocation;
  gtk_widget_get_allocation (widget, &allocation);

  /* Clear background to transparent */
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  /* Draw red border with pulsing alpha */
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, plugin->pulse_alpha);
  cairo_set_line_width (cr, 3.0);
  cairo_rectangle (cr, 1.5, 1.5, allocation.width - 3, allocation.height - 3);
  cairo_stroke (cr);

  return FALSE;
}

static gboolean
screenshotplus_pulse_timeout (gpointer user_data)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (user_data);

  /* Update alpha for pulsing effect */
  if (plugin->pulse_direction)
    {
      plugin->pulse_alpha += 0.05;
      if (plugin->pulse_alpha >= 1.0)
        {
          plugin->pulse_alpha = 1.0;
          plugin->pulse_direction = FALSE;
        }
    }
  else
    {
      plugin->pulse_alpha -= 0.05;
      if (plugin->pulse_alpha <= 0.3)
        {
          plugin->pulse_alpha = 0.3;
          plugin->pulse_direction = TRUE;
        }
    }

  /* Redraw the overlay */
  if (plugin->region_overlay != NULL)
    gtk_widget_queue_draw (plugin->region_overlay);

  return G_SOURCE_CONTINUE;
}

static void
screenshotplus_create_region_overlay (ScreenshotPlusPlugin *plugin, gint x, gint y, gint width, gint height)
{
  GtkWidget *window;
  GdkScreen *screen;
  GdkVisual *visual;

  if (plugin->region_overlay != NULL)
    {
      gtk_widget_destroy (plugin->region_overlay);
      plugin->region_overlay = NULL;
    }

  /* Stop any existing pulse timer */
  if (plugin->pulse_timeout_id > 0)
    {
      g_source_remove (plugin->pulse_timeout_id);
      plugin->pulse_timeout_id = 0;
    }

  /* Reset pulse state */
  plugin->pulse_alpha = 1.0;
  plugin->pulse_direction = FALSE;

  /* Create a transparent, borderless window */
  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);
  gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
  gtk_widget_set_app_paintable (window, TRUE);

  /* Make window transparent */
  screen = gtk_widget_get_screen (window);
  visual = gdk_screen_get_rgba_visual (screen);
  if (visual != NULL)
    gtk_widget_set_visual (window, visual);

  /* Set size and position */
  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_window_resize (GTK_WINDOW (window), width, height);

  /* Connect draw signal */
  g_signal_connect (G_OBJECT (window), "draw",
                    G_CALLBACK (screenshotplus_overlay_draw), plugin);

  /* Make window click-through */
  GdkWindow *gdk_window;
  cairo_region_t *region;

  gtk_widget_show (window);

  gdk_window = gtk_widget_get_window (window);
  if (gdk_window != NULL)
    {
      region = cairo_region_create ();
      gdk_window_input_shape_combine_region (gdk_window, region, 0, 0);
      cairo_region_destroy (region);
    }

  plugin->region_overlay = window;
  plugin->region_x = x;
  plugin->region_y = y;
  plugin->region_width = width;
  plugin->region_height = height;

  /* Start pulsing animation - update every 50ms for smooth effect */
  plugin->pulse_timeout_id = g_timeout_add (50, screenshotplus_pulse_timeout, plugin);
}

static void
screenshotplus_destroy_region_overlay (ScreenshotPlusPlugin *plugin)
{
  /* Stop pulse timer */
  if (plugin->pulse_timeout_id > 0)
    {
      g_source_remove (plugin->pulse_timeout_id);
      plugin->pulse_timeout_id = 0;
    }

  if (plugin->region_overlay != NULL)
    {
      gtk_widget_destroy (plugin->region_overlay);
      plugin->region_overlay = NULL;
    }
}



/* ============== UNIFIED ACTION DIALOGS ============== */

typedef struct {
  GtkWidget *folder_label;
  GtkWidget *filename_entry;
  gchar *current_folder;
} UnifiedDialogData;

static void
screenshotplus_on_change_folder_clicked (GtkButton *button, gpointer user_data)
{
  UnifiedDialogData *data = (UnifiedDialogData *) user_data;
  GtkWidget *folder_dialog;
  gint result;

  folder_dialog = gtk_file_chooser_dialog_new ("Select Folder",
                                               GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
                                               GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                               "_Cancel", GTK_RESPONSE_CANCEL,
                                               "_Select", GTK_RESPONSE_ACCEPT,
                                               NULL);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (folder_dialog), data->current_folder);

  result = gtk_dialog_run (GTK_DIALOG (folder_dialog));

  if (result == GTK_RESPONSE_ACCEPT)
    {
      g_free (data->current_folder);
      data->current_folder = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (folder_dialog));

      /* Update label to show folder name */
      gchar *basename = g_path_get_basename (data->current_folder);
      gtk_label_set_text (GTK_LABEL (data->folder_label), basename);
      g_free (basename);
    }

  gtk_widget_destroy (folder_dialog);
}

/* Screenshot mode enum */
typedef enum {
  SCREENSHOT_MODE_FULLSCREEN = 0,
  SCREENSHOT_MODE_WINDOW,
  SCREENSHOT_MODE_REGION
} ScreenshotMode;

/* Video mode enum */
typedef enum {
  VIDEO_MODE_FULLSCREEN = 0,
  VIDEO_MODE_WINDOW,
  VIDEO_MODE_REGION
} VideoMode;

/* Forward declarations for capture/record functions that take save path */
static void screenshotplus_capture_fullscreen_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path);
static void screenshotplus_capture_window_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path);
static void screenshotplus_capture_region_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path);
static void screenshotplus_record_fullscreen_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path);
static void screenshotplus_record_window_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path);
static void screenshotplus_record_region_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path);

static void
screenshotplus_show_screenshot_dialog (ScreenshotPlusPlugin *plugin)
{
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *mode_combo;
  GtkWidget *folder_box;
  GtkWidget *folder_icon;
  GtkWidget *folder_button;
  GtkWidget *filename_entry;
  gchar *default_dir;
  gchar *filename;
  time_t now;
  struct tm *tm_info;
  char timestamp[64];
  gint result;
  UnifiedDialogData *data;

  /* Build the default directory path */
  if (g_path_is_absolute (plugin->screenshot_dir))
    default_dir = g_strdup (plugin->screenshot_dir);
  else
    default_dir = g_build_filename (g_get_home_dir (), plugin->screenshot_dir, NULL);

  /* Create directory if it doesn't exist */
  g_mkdir_with_parents (default_dir, 0755);

  /* Generate default filename */
  time (&now);
  tm_info = localtime (&now);
  strftime (timestamp, sizeof (timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);
  filename = g_strdup_printf ("ScreenshotPlus_%s.png", timestamp);

  /* Create dialog */
  dialog = gtk_dialog_new_with_buttons ("Take Screenshot",
                                        NULL,
                                        GTK_DIALOG_MODAL,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Capture", GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (content_area), grid);

  /* Setup data for callbacks */
  data = g_new0 (UnifiedDialogData, 1);
  data->current_folder = g_strdup (default_dir);

  /* Mode selection row */
  label = gtk_label_new ("Capture:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  mode_combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mode_combo), "Entire Screen");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mode_combo), "Select Window");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mode_combo), "Select Region");
  gtk_combo_box_set_active (GTK_COMBO_BOX (mode_combo), 2); /* Default to Select Region */
  gtk_grid_attach (GTK_GRID (grid), mode_combo, 1, 0, 1, 1);

  /* Folder row */
  label = gtk_label_new ("Folder:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  folder_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_grid_attach (GTK_GRID (grid), folder_box, 1, 1, 1, 1);

  folder_icon = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (folder_box), folder_icon, FALSE, FALSE, 0);

  gchar *folder_basename = g_path_get_basename (default_dir);
  data->folder_label = gtk_label_new (folder_basename);
  gtk_label_set_xalign (GTK_LABEL (data->folder_label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (data->folder_label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_max_width_chars (GTK_LABEL (data->folder_label), 25);
  gtk_box_pack_start (GTK_BOX (folder_box), data->folder_label, TRUE, TRUE, 0);
  g_free (folder_basename);

  folder_button = gtk_button_new_with_label ("Change...");
  g_signal_connect (G_OBJECT (folder_button), "clicked",
                    G_CALLBACK (screenshotplus_on_change_folder_clicked), data);
  gtk_box_pack_start (GTK_BOX (folder_box), folder_button, FALSE, FALSE, 0);

  /* Filename row */
  label = gtk_label_new ("Filename:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  filename_entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (filename_entry), filename);
  gtk_entry_set_width_chars (GTK_ENTRY (filename_entry), 35);
  gtk_entry_set_activates_default (GTK_ENTRY (filename_entry), TRUE);
  gtk_grid_attach (GTK_GRID (grid), filename_entry, 1, 2, 1, 1);

  gtk_widget_show_all (dialog);

  result = gtk_dialog_run (GTK_DIALOG (dialog));

  if (result == GTK_RESPONSE_ACCEPT)
    {
      ScreenshotMode mode = gtk_combo_box_get_active (GTK_COMBO_BOX (mode_combo));

      /* Update plugin's screenshot directory to use selected folder */
      g_free (plugin->screenshot_dir);
      plugin->screenshot_dir = g_strdup (data->current_folder);

      gtk_widget_destroy (dialog);

      switch (mode)
        {
        case SCREENSHOT_MODE_FULLSCREEN:
          screenshotplus_capture_fullscreen (plugin);
          break;
        case SCREENSHOT_MODE_WINDOW:
          screenshotplus_capture_window (plugin);
          break;
        case SCREENSHOT_MODE_REGION:
          screenshotplus_capture_region (plugin);
          break;
        }
    }
  else
    {
      gtk_widget_destroy (dialog);
    }

  g_free (data->current_folder);
  g_free (data);
  g_free (default_dir);
  g_free (filename);
}

static void
screenshotplus_show_video_dialog (ScreenshotPlusPlugin *plugin)
{
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *mode_combo;
  GtkWidget *folder_box;
  GtkWidget *folder_icon;
  GtkWidget *folder_button;
  GtkWidget *filename_entry;
  gchar *default_dir;
  gchar *filename;
  time_t now;
  struct tm *tm_info;
  char timestamp[64];
  gint result;
  UnifiedDialogData *data;

  /* Build the default directory path */
  if (g_path_is_absolute (plugin->video_dir))
    default_dir = g_strdup (plugin->video_dir);
  else
    default_dir = g_build_filename (g_get_home_dir (), plugin->video_dir, NULL);

  /* Create directory if it doesn't exist */
  g_mkdir_with_parents (default_dir, 0755);

  /* Generate default filename */
  time (&now);
  tm_info = localtime (&now);
  strftime (timestamp, sizeof (timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);
  filename = g_strdup_printf ("ScreenshotPlus_%s.mp4", timestamp);

  /* Create dialog */
  dialog = gtk_dialog_new_with_buttons ("Record Video",
                                        NULL,
                                        GTK_DIALOG_MODAL,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Record", GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (content_area), grid);

  /* Setup data for callbacks */
  data = g_new0 (UnifiedDialogData, 1);
  data->current_folder = g_strdup (default_dir);

  /* Mode selection row */
  label = gtk_label_new ("Record:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  mode_combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mode_combo), "Entire Screen");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mode_combo), "Select Window");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mode_combo), "Select Region");
  gtk_combo_box_set_active (GTK_COMBO_BOX (mode_combo), 2); /* Default to Select Region */
  gtk_grid_attach (GTK_GRID (grid), mode_combo, 1, 0, 1, 1);

  /* Folder row */
  label = gtk_label_new ("Folder:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  folder_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_grid_attach (GTK_GRID (grid), folder_box, 1, 1, 1, 1);

  folder_icon = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (folder_box), folder_icon, FALSE, FALSE, 0);

  gchar *folder_basename = g_path_get_basename (default_dir);
  data->folder_label = gtk_label_new (folder_basename);
  gtk_label_set_xalign (GTK_LABEL (data->folder_label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (data->folder_label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_max_width_chars (GTK_LABEL (data->folder_label), 25);
  gtk_box_pack_start (GTK_BOX (folder_box), data->folder_label, TRUE, TRUE, 0);
  g_free (folder_basename);

  folder_button = gtk_button_new_with_label ("Change...");
  g_signal_connect (G_OBJECT (folder_button), "clicked",
                    G_CALLBACK (screenshotplus_on_change_folder_clicked), data);
  gtk_box_pack_start (GTK_BOX (folder_box), folder_button, FALSE, FALSE, 0);

  /* Filename row */
  label = gtk_label_new ("Filename:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  filename_entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (filename_entry), filename);
  gtk_entry_set_width_chars (GTK_ENTRY (filename_entry), 35);
  gtk_entry_set_activates_default (GTK_ENTRY (filename_entry), TRUE);
  gtk_grid_attach (GTK_GRID (grid), filename_entry, 1, 2, 1, 1);

  gtk_widget_show_all (dialog);

  result = gtk_dialog_run (GTK_DIALOG (dialog));

  if (result == GTK_RESPONSE_ACCEPT)
    {
      VideoMode mode = gtk_combo_box_get_active (GTK_COMBO_BOX (mode_combo));

      /* Update plugin's video directory to use selected folder */
      g_free (plugin->video_dir);
      plugin->video_dir = g_strdup (data->current_folder);

      gtk_widget_destroy (dialog);

      switch (mode)
        {
        case VIDEO_MODE_FULLSCREEN:
          screenshotplus_record_fullscreen (plugin);
          break;
        case VIDEO_MODE_WINDOW:
          screenshotplus_record_window (plugin);
          break;
        case VIDEO_MODE_REGION:
          screenshotplus_record_region (plugin);
          break;
        }
    }
  else
    {
      gtk_widget_destroy (dialog);
    }

  g_free (data->current_folder);
  g_free (data);
  g_free (default_dir);
  g_free (filename);
}



/* ============== VIDEO RECORDING FUNCTIONS ============== */

static void
screenshotplus_stop_recording (ScreenshotPlusPlugin *plugin)
{
  if (plugin->is_recording && plugin->recording_pid > 0)
    {
      kill (plugin->recording_pid, SIGINT);
      g_spawn_close_pid (plugin->recording_pid);
      plugin->recording_pid = 0;
      plugin->is_recording = FALSE;

      /* Destroy region overlay */
      screenshotplus_destroy_region_overlay (plugin);

      if (plugin->current_video_file)
        {
          gchar *message = g_strdup_printf ("Saved to:\n%s", plugin->current_video_file);
          screenshotplus_show_notification ("Recording Stopped", message, "media-record");
          g_free (message);
          g_free (plugin->current_video_file);
          plugin->current_video_file = NULL;
        }

      /* Update icon back to normal */
      if (plugin->icon)
        gtk_image_set_from_icon_name (GTK_IMAGE (plugin->icon), "org.xfce.screenshooter", GTK_ICON_SIZE_BUTTON);
    }
}



static void
screenshotplus_record_fullscreen (ScreenshotPlusPlugin *plugin)
{
  GError *error = NULL;
  gchar *save_path;
  gchar *command;
  gchar **argv;
  gint argc;
  GdkDisplay *display;
  GdkScreen *screen;
  gint screen_width, screen_height;

  if (plugin->is_recording)
    {
      screenshotplus_stop_recording (plugin);
      return;
    }

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  screen_width = gdk_screen_get_width (screen);
  screen_height = gdk_screen_get_height (screen);

  save_path = screenshotplus_get_save_path (plugin, "_fullscreen", TRUE);

  command = g_strdup_printf ("ffmpeg -f x11grab -framerate 30 -video_size %dx%d -i :0.0 "
                              "-c:v libx264 -preset ultrafast -crf 23 -y %s",
                              screen_width, screen_height, save_path);

  if (!g_shell_parse_argv (command, &argc, &argv, &error))
    {
      screenshotplus_show_notification ("Recording Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
      g_free (save_path);
      g_free (command);
      return;
    }

  if (!g_spawn_async (NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &plugin->recording_pid, &error))
    {
      screenshotplus_show_notification ("Recording Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
      g_free (save_path);
      g_free (command);
      g_strfreev (argv);
      return;
    }

  plugin->is_recording = TRUE;
  plugin->current_video_file = save_path;

  /* Update icon to indicate recording */
  if (plugin->icon)
    gtk_image_set_from_icon_name (GTK_IMAGE (plugin->icon), "media-record", GTK_ICON_SIZE_BUTTON);

  screenshotplus_show_notification ("Recording Started",
                                    "Click plugin again to stop",
                                    "media-record");

  g_free (command);
  g_strfreev (argv);
}



static gboolean
screenshotplus_record_window_idle (gpointer user_data)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (user_data);
  GdkDisplay *display;
  Display *xdisplay;
  Window selected_window;
  XWindowAttributes attrs;
  gint x, y;
  Window child;
  GError *error = NULL;
  gchar *save_path;
  gchar *command;
  gchar **argv;
  gint argc;

  if (plugin->is_recording)
    {
      screenshotplus_stop_recording (plugin);
      return G_SOURCE_REMOVE;
    }

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  selected_window = screenshotplus_select_window (xdisplay);

  if (selected_window == None)
    return G_SOURCE_REMOVE;

  if (!XGetWindowAttributes (xdisplay, selected_window, &attrs))
    {
      screenshotplus_show_notification ("Recording Failed",
                                        "Failed to get window attributes",
                                        "dialog-error");
      return G_SOURCE_REMOVE;
    }

  XTranslateCoordinates (xdisplay, selected_window,
                          DefaultRootWindow (xdisplay),
                          0, 0, &x, &y, &child);

  save_path = screenshotplus_get_save_path (plugin, "_window", TRUE);

  command = g_strdup_printf ("ffmpeg -f x11grab -framerate 30 -video_size %dx%d -i :0.0+%d,%d "
                              "-c:v libx264 -preset ultrafast -crf 23 -y %s",
                              attrs.width, attrs.height, x, y, save_path);

  if (!g_shell_parse_argv (command, &argc, &argv, &error))
    {
      screenshotplus_show_notification ("Recording Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
      g_free (save_path);
      g_free (command);
      return G_SOURCE_REMOVE;
    }

  if (!g_spawn_async (NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &plugin->recording_pid, &error))
    {
      screenshotplus_show_notification ("Recording Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
      g_free (save_path);
      g_free (command);
      g_strfreev (argv);
      return G_SOURCE_REMOVE;
    }

  plugin->is_recording = TRUE;
  plugin->current_video_file = save_path;

  if (plugin->icon)
    gtk_image_set_from_icon_name (GTK_IMAGE (plugin->icon), "media-record", GTK_ICON_SIZE_BUTTON);

  screenshotplus_show_notification ("Recording Started",
                                    "Click plugin again to stop",
                                    "media-record");

  g_free (command);
  g_strfreev (argv);

  return G_SOURCE_REMOVE;
}



static void
screenshotplus_record_window (ScreenshotPlusPlugin *plugin)
{
  if (plugin->is_recording)
    {
      screenshotplus_stop_recording (plugin);
      return;
    }
  g_idle_add (screenshotplus_record_window_idle, plugin);
}



static gboolean
screenshotplus_record_region_idle (gpointer user_data)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (user_data);
  GdkDisplay *display;
  Display *xdisplay;
  Window root;
  Cursor cursor;
  XEvent event;
  int screen_num;
  gboolean selecting = TRUE;
  gboolean started = FALSE;
  int start_x = 0, start_y = 0, end_x = 0, end_y = 0;
  int prev_x = 0, prev_y = 0;
  GError *error = NULL;
  gchar *save_path;
  gchar *command;
  gchar **argv;
  gint argc;
  gint x, y, width, height;
  GC gc;
  XGCValues gcval;

  if (plugin->is_recording)
    {
      screenshotplus_stop_recording (plugin);
      return G_SOURCE_REMOVE;
    }

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  screen_num = DefaultScreen (xdisplay);
  root = RootWindow (xdisplay, screen_num);

  cursor = XCreateFontCursor (xdisplay, XC_crosshair);

  /* Create GC for drawing rectangle */
  gcval.foreground = XWhitePixel (xdisplay, screen_num);
  gcval.function = GXxor;
  gcval.background = XBlackPixel (xdisplay, screen_num);
  gcval.plane_mask = gcval.background ^ gcval.foreground;
  gcval.subwindow_mode = IncludeInferiors;
  gcval.line_width = 2;

  gc = XCreateGC (xdisplay, root,
                   GCFunction | GCForeground | GCBackground | GCSubwindowMode | GCLineWidth,
                   &gcval);

  if (XGrabPointer (xdisplay, root, False,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync,
                     root, cursor, CurrentTime) != GrabSuccess)
    {
      XFreeCursor (xdisplay, cursor);
      XFreeGC (xdisplay, gc);
      return G_SOURCE_REMOVE;
    }

  XGrabKeyboard (xdisplay, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);

  while (selecting)
    {
      XNextEvent (xdisplay, &event);

      switch (event.type)
        {
        case ButtonPress:
          if (event.xbutton.button == Button1)
            {
              start_x = event.xbutton.x_root;
              start_y = event.xbutton.y_root;
              end_x = start_x;
              end_y = start_y;
              prev_x = start_x;
              prev_y = start_y;
              started = TRUE;
            }
          else
            {
              selecting = FALSE;
              started = FALSE;
            }
          break;

        case ButtonRelease:
          if (event.xbutton.button == Button1 && started)
            {
              /* Erase the last rectangle */
              screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                        start_x, start_y,
                                                        prev_x - start_x, prev_y - start_y);
              end_x = event.xbutton.x_root;
              end_y = event.xbutton.y_root;
              selecting = FALSE;
            }
          break;

        case MotionNotify:
          if (started)
            {
              /* Erase the previous rectangle */
              screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                        start_x, start_y,
                                                        prev_x - start_x, prev_y - start_y);

              /* Draw the new rectangle */
              end_x = event.xmotion.x_root;
              end_y = event.xmotion.y_root;
              screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                        start_x, start_y,
                                                        end_x - start_x, end_y - start_y);
              prev_x = end_x;
              prev_y = end_y;
            }
          break;

        case KeyPress:
          if (XLookupKeysym (&event.xkey, 0) == XK_Escape)
            {
              if (started)
                {
                  /* Erase the rectangle */
                  screenshotplus_draw_selection_rectangle (xdisplay, root, gc,
                                                            start_x, start_y,
                                                            prev_x - start_x, prev_y - start_y);
                }
              selecting = FALSE;
              started = FALSE;
            }
          break;
        }
    }

  XUngrabKeyboard (xdisplay, CurrentTime);
  XUngrabPointer (xdisplay, CurrentTime);
  XFreeCursor (xdisplay, cursor);
  XFreeGC (xdisplay, gc);
  XFlush (xdisplay);

  if (!started)
    return G_SOURCE_REMOVE;

  x = MIN (start_x, end_x);
  y = MIN (start_y, end_y);
  width = ABS (end_x - start_x);
  height = ABS (end_y - start_y);

  if (width <= 0 || height <= 0)
    {
      screenshotplus_show_notification ("Recording Failed",
                                        "Invalid region selected",
                                        "dialog-error");
      return G_SOURCE_REMOVE;
    }

  save_path = screenshotplus_get_save_path (plugin, "_region", TRUE);

  command = g_strdup_printf ("ffmpeg -f x11grab -framerate 30 -video_size %dx%d -i :0.0+%d,%d "
                              "-c:v libx264 -preset ultrafast -crf 23 -y %s",
                              width, height, x, y, save_path);

  if (!g_shell_parse_argv (command, &argc, &argv, &error))
    {
      screenshotplus_show_notification ("Recording Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
      g_free (save_path);
      g_free (command);
      return G_SOURCE_REMOVE;
    }

  if (!g_spawn_async (NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &plugin->recording_pid, &error))
    {
      screenshotplus_show_notification ("Recording Failed",
                                        error->message,
                                        "dialog-error");
      g_error_free (error);
      g_free (save_path);
      g_free (command);
      g_strfreev (argv);
      return G_SOURCE_REMOVE;
    }

  plugin->is_recording = TRUE;
  plugin->current_video_file = save_path;

  /* Create region overlay to show the recording area */
  if (plugin->show_region_outline)
    screenshotplus_create_region_overlay (plugin, x, y, width, height);

  if (plugin->icon)
    gtk_image_set_from_icon_name (GTK_IMAGE (plugin->icon), "media-record", GTK_ICON_SIZE_BUTTON);

  screenshotplus_show_notification ("Recording Started",
                                    "Click plugin again to stop",
                                    "media-record");

  g_free (command);
  g_strfreev (argv);

  return G_SOURCE_REMOVE;
}



static void
screenshotplus_record_region (ScreenshotPlusPlugin *plugin)
{
  if (plugin->is_recording)
    {
      screenshotplus_stop_recording (plugin);
      return;
    }
  g_idle_add (screenshotplus_record_region_idle, plugin);
}

/* Stub implementations for _with_path variants - these use the existing functions for now */
static void
screenshotplus_record_fullscreen_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path)
{
  /* TODO: implement proper path handling */
  screenshotplus_record_fullscreen (plugin);
}

static void
screenshotplus_record_window_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path)
{
  /* TODO: implement proper path handling */
  screenshotplus_record_window (plugin);
}

static void
screenshotplus_record_region_with_path (ScreenshotPlusPlugin *plugin, const gchar *save_path)
{
  /* TODO: implement proper path handling */
  screenshotplus_record_region (plugin);
}



/* ============== MENU CALLBACKS ============== */

static void
screenshotplus_on_capture_fullscreen (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  screenshotplus_capture_fullscreen (plugin);
}

static void
screenshotplus_on_capture_window (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  screenshotplus_capture_window (plugin);
}

static void
screenshotplus_on_capture_region (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  screenshotplus_capture_region (plugin);
}

static void
screenshotplus_on_record_fullscreen (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  screenshotplus_record_fullscreen (plugin);
}

static void
screenshotplus_on_record_window (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  screenshotplus_record_window (plugin);
}

static void
screenshotplus_on_record_region (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  screenshotplus_record_region (plugin);
}

static void
screenshotplus_on_stop_recording (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  screenshotplus_stop_recording (plugin);
}

static void
screenshotplus_on_open_screenshot_folder (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  gchar *dir_path;
  gchar *command;
  GError *error = NULL;

  if (g_path_is_absolute (plugin->screenshot_dir))
    dir_path = g_strdup (plugin->screenshot_dir);
  else
    dir_path = g_build_filename (g_get_home_dir (), plugin->screenshot_dir, NULL);

  command = g_strdup_printf ("xdg-open %s", dir_path);

  if (!g_spawn_command_line_async (command, &error))
    {
      g_warning ("Failed to open folder: %s", error->message);
      g_error_free (error);
    }

  g_free (dir_path);
  g_free (command);
}

static void
screenshotplus_on_open_video_folder (GtkWidget *menuitem, ScreenshotPlusPlugin *plugin)
{
  gchar *dir_path;
  gchar *command;
  GError *error = NULL;

  if (g_path_is_absolute (plugin->video_dir))
    dir_path = g_strdup (plugin->video_dir);
  else
    dir_path = g_build_filename (g_get_home_dir (), plugin->video_dir, NULL);

  command = g_strdup_printf ("xdg-open %s", dir_path);

  if (!g_spawn_command_line_async (command, &error))
    {
      g_warning ("Failed to open folder: %s", error->message);
      g_error_free (error);
    }

  g_free (dir_path);
  g_free (command);
}



/* Helper to create a button with icon and label */
static GtkWidget *
screenshotplus_create_action_button (const gchar *icon_name, const gchar *label_text)
{
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;

  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (button), box);

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);

  label = gtk_label_new (label_text);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  return button;
}

/* Callback data for tabbed dialog buttons */
typedef struct {
  ScreenshotPlusPlugin *plugin;
  GtkWidget *dialog;
} TabButtonData;

static void
screenshotplus_tab_button_clicked (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  gtk_widget_destroy (data->dialog);
}

static void
screenshotplus_on_tab_capture_fullscreen (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_capture_fullscreen (plugin);
}

static void
screenshotplus_on_tab_capture_window (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_capture_window (plugin);
}

static void
screenshotplus_on_tab_capture_region (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_capture_region (plugin);
}

static void
screenshotplus_on_tab_open_screenshot_folder (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gchar *dir_path;
  gchar *command;
  GError *error = NULL;

  if (g_path_is_absolute (plugin->screenshot_dir))
    dir_path = g_strdup (plugin->screenshot_dir);
  else
    dir_path = g_build_filename (g_get_home_dir (), plugin->screenshot_dir, NULL);

  command = g_strdup_printf ("xdg-open %s", dir_path);

  if (!g_spawn_command_line_async (command, &error))
    {
      g_warning ("Failed to open folder: %s", error->message);
      g_error_free (error);
    }

  g_free (dir_path);
  g_free (command);
  gtk_widget_destroy (data->dialog);
}

static void
screenshotplus_on_tab_record_fullscreen (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_record_fullscreen (plugin);
}

static void
screenshotplus_on_tab_record_window (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_record_window (plugin);
}

static void
screenshotplus_on_tab_record_region (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_record_region (plugin);
}

static void
screenshotplus_on_tab_stop_recording (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_stop_recording (plugin);
}

static void
screenshotplus_on_tab_open_video_folder (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gchar *dir_path;
  gchar *command;
  GError *error = NULL;

  if (g_path_is_absolute (plugin->video_dir))
    dir_path = g_strdup (plugin->video_dir);
  else
    dir_path = g_build_filename (g_get_home_dir (), plugin->video_dir, NULL);

  command = g_strdup_printf ("xdg-open %s", dir_path);

  if (!g_spawn_command_line_async (command, &error))
    {
      g_warning ("Failed to open folder: %s", error->message);
      g_error_free (error);
    }

  g_free (dir_path);
  g_free (command);
  gtk_widget_destroy (data->dialog);
}

static void
screenshotplus_popup_dialog_destroy (GtkWidget *dialog, gpointer user_data)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (user_data);
  if (plugin->button != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), FALSE);
}

static void
screenshotplus_on_take_screenshot_clicked (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_show_screenshot_dialog (plugin);
}

static void
screenshotplus_on_record_video_clicked (GtkButton *button, gpointer user_data)
{
  TabButtonData *data = (TabButtonData *) user_data;
  ScreenshotPlusPlugin *plugin = data->plugin;
  gtk_widget_destroy (data->dialog);
  screenshotplus_show_video_dialog (plugin);
}

static void
screenshotplus_plugin_menu (GtkWidget *button, ScreenshotPlusPlugin *plugin)
{
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *btn;
  GtkWidget *separator;
  TabButtonData *data;
  gint x, y;
  GdkWindow *gdk_window;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    return;

  /* Create popup dialog */
  dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (dialog), "ScreenshotPlus");
  gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 8);

  g_signal_connect (G_OBJECT (dialog), "destroy",
                    G_CALLBACK (screenshotplus_popup_dialog_destroy), plugin);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add (GTK_CONTAINER (dialog), vbox);

  data = g_new0 (TabButtonData, 1);
  data->plugin = plugin;
  data->dialog = dialog;

  /* Take Screenshot button */
  btn = screenshotplus_create_action_button ("org.xfce.screenshooter", "Take Screenshot...");
  g_signal_connect_data (G_OBJECT (btn), "clicked",
                         G_CALLBACK (screenshotplus_on_take_screenshot_clicked),
                         g_memdup2 (data, sizeof (TabButtonData)), (GClosureNotify) g_free, 0);
  gtk_box_pack_start (GTK_BOX (vbox), btn, FALSE, FALSE, 0);

  /* Record Video button or Stop Recording */
  if (plugin->is_recording)
    {
      btn = screenshotplus_create_action_button ("media-playback-stop", "Stop Recording");
      g_signal_connect_data (G_OBJECT (btn), "clicked",
                             G_CALLBACK (screenshotplus_on_tab_stop_recording),
                             g_memdup2 (data, sizeof (TabButtonData)), (GClosureNotify) g_free, 0);
    }
  else
    {
      btn = screenshotplus_create_action_button ("camera-video", "Record Video...");
      g_signal_connect_data (G_OBJECT (btn), "clicked",
                             G_CALLBACK (screenshotplus_on_record_video_clicked),
                             g_memdup2 (data, sizeof (TabButtonData)), (GClosureNotify) g_free, 0);
    }
  gtk_box_pack_start (GTK_BOX (vbox), btn, FALSE, FALSE, 0);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 4);

  /* Open folders */
  btn = screenshotplus_create_action_button ("folder-pictures", "Open Screenshots");
  g_signal_connect_data (G_OBJECT (btn), "clicked",
                         G_CALLBACK (screenshotplus_on_tab_open_screenshot_folder),
                         g_memdup2 (data, sizeof (TabButtonData)), (GClosureNotify) g_free, 0);
  gtk_box_pack_start (GTK_BOX (vbox), btn, FALSE, FALSE, 0);

  btn = screenshotplus_create_action_button ("folder-videos", "Open Recordings");
  g_signal_connect_data (G_OBJECT (btn), "clicked",
                         G_CALLBACK (screenshotplus_on_tab_open_video_folder),
                         g_memdup2 (data, sizeof (TabButtonData)), (GClosureNotify) g_free, 0);
  gtk_box_pack_start (GTK_BOX (vbox), btn, FALSE, FALSE, 0);

  g_free (data);

  gtk_widget_show_all (dialog);

  /* Position near the button, keeping within screen bounds */
  gdk_window = gtk_widget_get_window (button);
  if (gdk_window != NULL)
    {
      GdkScreen *screen;
      gint screen_width, screen_height;
      gint dialog_width, dialog_height;
      gint button_height;

      gdk_window_get_origin (gdk_window, &x, &y);
      button_height = gtk_widget_get_allocated_height (button);

      /* Get dialog size */
      gtk_window_get_size (GTK_WINDOW (dialog), &dialog_width, &dialog_height);

      /* Get screen size */
      screen = gtk_widget_get_screen (dialog);
      screen_width = gdk_screen_get_width (screen);
      screen_height = gdk_screen_get_height (screen);

      /* Adjust x to keep dialog within screen */
      if (x + dialog_width > screen_width)
        x = screen_width - dialog_width;
      if (x < 0)
        x = 0;

      /* Position below button, or above if not enough space */
      if (y + button_height + dialog_height > screen_height)
        y = y - dialog_height;  /* Show above the button */
      else
        y = y + button_height;  /* Show below the button */

      if (y < 0)
        y = 0;

      gtk_window_move (GTK_WINDOW (dialog), x, y);
    }

  /* Grab focus */
  gtk_window_present (GTK_WINDOW (dialog));
}



static void
screenshotplus_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (panel_plugin);

  xfce_panel_plugin_menu_show_configure (XFCE_PANEL_PLUGIN (plugin));

  plugin->button = xfce_arrow_button_new (GTK_ARROW_NONE);
  gtk_widget_set_name (plugin->button, "screenshotplus-button");
  gtk_button_set_relief (GTK_BUTTON (plugin->button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (plugin->button, "ScreenshotPlus - Take screenshots and record videos");
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->button);
  g_signal_connect (G_OBJECT (plugin->button), "toggled",
                    G_CALLBACK (screenshotplus_plugin_menu), plugin);
  gtk_widget_show (plugin->button);

  plugin->icon = gtk_image_new_from_icon_name ("org.xfce.screenshooter", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (plugin->button), plugin->icon);
  gtk_widget_show (plugin->icon);

  screenshotplus_plugin_size_changed (panel_plugin, xfce_panel_plugin_get_size (panel_plugin));
}



static void
screenshotplus_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (panel_plugin);

  if (plugin->is_recording)
    screenshotplus_stop_recording (plugin);

  screenshotplus_destroy_region_overlay (plugin);

  g_free (plugin->screenshot_dir);
  g_free (plugin->video_dir);
  g_free (plugin->current_video_file);
}



static gboolean
screenshotplus_plugin_size_changed (XfcePanelPlugin *panel_plugin, gint size)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (panel_plugin);
  gint icon_size;

  icon_size = xfce_panel_plugin_get_icon_size (panel_plugin);

  if (plugin->icon != NULL)
    gtk_image_set_pixel_size (GTK_IMAGE (plugin->icon), icon_size);

  return TRUE;
}



static void
screenshotplus_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  ScreenshotPlusPlugin *plugin = SCREENSHOTPLUS_PLUGIN (panel_plugin);
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *spin;
  GtkWidget *entry_screenshot;
  GtkWidget *entry_video;
  GtkWidget *check_outline;
  gint result;

  dialog = gtk_dialog_new_with_buttons ("ScreenshotPlus Settings",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "_Close", GTK_RESPONSE_CLOSE,
                                        NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (content_area), grid);

  /* Delay setting */
  label = gtk_label_new_with_mnemonic ("_Delay (seconds):");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  spin = gtk_spin_button_new_with_range (0, 60, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), plugin->delay);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
  gtk_grid_attach (GTK_GRID (grid), spin, 1, 0, 1, 1);

  /* Screenshot directory setting */
  label = gtk_label_new_with_mnemonic ("_Screenshot folder:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  entry_screenshot = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry_screenshot), plugin->screenshot_dir);
  gtk_entry_set_width_chars (GTK_ENTRY (entry_screenshot), 30);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry_screenshot);
  gtk_grid_attach (GTK_GRID (grid), entry_screenshot, 1, 1, 1, 1);

  /* Video directory setting */
  label = gtk_label_new_with_mnemonic ("_Video folder:");
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  entry_video = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry_video), plugin->video_dir);
  gtk_entry_set_width_chars (GTK_ENTRY (entry_video), 30);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry_video);
  gtk_grid_attach (GTK_GRID (grid), entry_video, 1, 2, 1, 1);

  /* Show region outline checkbox */
  check_outline = gtk_check_button_new_with_mnemonic ("Show _region outline during recording");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_outline), plugin->show_region_outline);
  gtk_grid_attach (GTK_GRID (grid), check_outline, 0, 3, 2, 1);

  gtk_widget_show_all (dialog);

  result = gtk_dialog_run (GTK_DIALOG (dialog));

  if (result == GTK_RESPONSE_CLOSE)
    {
      plugin->delay = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
      g_free (plugin->screenshot_dir);
      plugin->screenshot_dir = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry_screenshot)));
      g_free (plugin->video_dir);
      plugin->video_dir = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry_video)));
      plugin->show_region_outline = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_outline));
    }

  gtk_widget_destroy (dialog);
}
