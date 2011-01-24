/* common.h
 *
 * Copyright (c) 1998-2002  Mike Oliphant <oliphant@gtk.org>
 *
 *   http://www.nostatic.org/grip
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gnome.h>

/* Routines from main.c */
void Debug(char *fmt,...);

/* Routines from uihelper.c */
GtkTooltips *MakeToolTip(void);
GdkColor *MakeColor(int red,int green,int blue);
GtkStyle *MakeStyle(GdkColor *fg,GdkColor *bg,gboolean do_grade);
GtkWidget *BuildMenuItemXpm(GtkWidget *xpm, gchar *text);
GtkWidget *BuildMenuItem(gchar *impath, gchar *text, gboolean stock);
GtkWidget *NewBlankPixmap(GtkWidget *widget);
GtkWidget *ImageButton(GtkWidget *widget,GtkWidget *image);
GtkWidget *Loadxpm(GtkWidget *widget,char **xpm);
void CopyPixmap(GtkPixmap *src,GtkPixmap *dest);
gint SizeInDubs(GdkFont *font,gint numchars);
void UpdateGTK(void);
