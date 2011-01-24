/* uihelper.c
 *
 * Copyright (c) 1998-2004  Mike Oliphant <grip@nostatic.org>
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

#include "common.h"

GtkTooltips *MakeToolTip(void)
{
  GtkTooltips *tip;

  tip=gtk_tooltips_new();

  gtk_tooltips_set_delay(tip,1250);

  return tip;
}

GdkColor *MakeColor(int red,int green,int blue)
{
  GdkColor *c;

  c=(GdkColor *)g_malloc(sizeof(GdkColor));
  c->red=red;
  c->green=green;
  c->blue=blue;

  gdk_color_alloc(gdk_colormap_get_system(),c);

  return c;
}

static gfloat style_color_mods[5]={0.0,-0.1,0.2,-0.2};

GtkStyle *MakeStyle(GdkColor *fg,GdkColor *bg,gboolean do_grade)
{
  GtkStyle *def;
  GtkStyle *sty;
  int state;

  def=gtk_widget_get_default_style();
  sty=gtk_style_copy(def);

  for(state=0;state<5;state++) {
    if(fg) sty->fg[state]=*fg;

    if(bg) sty->bg[state]=*bg;

    if(bg && do_grade) {
      sty->bg[state].red+=sty->bg[state].red*style_color_mods[state];
      sty->bg[state].green+=sty->bg[state].green*style_color_mods[state];
      sty->bg[state].blue+=sty->bg[state].blue*style_color_mods[state];
    }
  }

  return sty;
}

GtkWidget *BuildMenuItem(gchar *impath, gchar *text, gboolean stock)
{
  GtkWidget *item, *image;

  if (impath != NULL) {
    item = gtk_image_menu_item_new_with_mnemonic(text);
	  if (!stock) {
	    image = gtk_image_new_from_file(impath);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	  } else {
		image = gtk_image_new_from_stock(impath, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	  }
  } else {
    item = gtk_menu_item_new_with_mnemonic(text);
  }
	
  gtk_widget_show_all(item);
	
  return item;
}

GtkWidget *BuildMenuItemXpm(GtkWidget *xpm, gchar *text)
{
  GtkWidget *item;
	
  item = gtk_image_menu_item_new_with_mnemonic(text);
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), xpm);
	
  gtk_widget_show_all(item);
	
  return item;
}

static char * empty_xpm[] = {
  "1 1 1 1",
  "       c None",
  " "
};

GtkWidget *NewBlankPixmap(GtkWidget *widget)
{
  return Loadxpm(widget,empty_xpm);
}

GtkWidget *ImageButton(GtkWidget *widget,GtkWidget *image)
{
  GtkWidget *button;

  button=gtk_button_new();

  gtk_container_add(GTK_CONTAINER(button),image);
  gtk_widget_show(image);

  return button;
}

GtkWidget *Loadxpm(GtkWidget *widget,char **xpm)
{
  GdkBitmap *mask;
  GtkStyle *style;
  GtkWidget *pixmapwid;
  GdkPixmap *pixmap;

  style=gtk_widget_get_style(widget);

  pixmap=gdk_pixmap_create_from_xpm_d(widget->window,&mask,
				      &style->bg[GTK_STATE_NORMAL],
				      (gchar **)xpm);

  pixmapwid=gtk_pixmap_new(pixmap,mask);

  return pixmapwid;
}

void CopyPixmap(GtkPixmap *src,GtkPixmap *dest)
{
  GdkPixmap *gdkpix;
  GdkBitmap *mask;

  gtk_pixmap_get(src,&gdkpix,&mask);
  gtk_pixmap_set(dest,gdkpix,mask);
}

gint SizeInDubs(GdkFont *font,gint numchars)
{
  return gdk_string_width(font,"W")*numchars;
}

void UpdateGTK(void)
{
  while(gtk_events_pending())
    gtk_main_iteration();
}
