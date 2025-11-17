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

#ifndef TERMINAL_BREADCRUMB_BAR_H
#define TERMINAL_BREADCRUMB_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_BREADCRUMB_BAR (terminal_breadcrumb_bar_get_type ())
#define TERMINAL_BREADCRUMB_BAR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TERMINAL_TYPE_BREADCRUMB_BAR, TerminalBreadcrumbBar))
#define TERMINAL_BREADCRUMB_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_BREADCRUMB_BAR, TerminalBreadcrumbBarClass))
#define TERMINAL_IS_BREADCRUMB_BAR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TERMINAL_TYPE_BREADCRUMB_BAR))
#define TERMINAL_IS_BREADCRUMB_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_BREADCRUMB_BAR))
#define TERMINAL_BREADCRUMB_BAR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_BREADCRUMB_BAR, TerminalBreadcrumbBarClass))

typedef struct _TerminalBreadcrumbBar TerminalBreadcrumbBar;
typedef struct _TerminalBreadcrumbBarClass TerminalBreadcrumbBarClass;
typedef struct _TerminalBreadcrumbBarPrivate TerminalBreadcrumbBarPrivate;

struct _TerminalBreadcrumbBar
{
  GtkBox parent_instance;

  TerminalBreadcrumbBarPrivate *priv;
};

struct _TerminalBreadcrumbBarClass
{
  GtkBoxClass parent_class;

  /* Signals */
  void (*path_clicked) (TerminalBreadcrumbBar *bar,
                        const gchar *path);
};

GType
terminal_breadcrumb_bar_get_type (void) G_GNUC_CONST;

GtkWidget *
terminal_breadcrumb_bar_new (void);

void
terminal_breadcrumb_bar_set_path (TerminalBreadcrumbBar *bar,
                                  const gchar *path);

const gchar *
terminal_breadcrumb_bar_get_path (TerminalBreadcrumbBar *bar);

void
terminal_breadcrumb_bar_set_enabled (TerminalBreadcrumbBar *bar,
                                     gboolean enabled);

gboolean
terminal_breadcrumb_bar_get_enabled (TerminalBreadcrumbBar *bar);

G_END_DECLS

#endif /* !TERMINAL_BREADCRUMB_BAR_H */
