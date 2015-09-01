/* tray.c
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

#include "tray.h"
#include "common.h"
#include "../pixmaps/rip1.xpm"
#include "../pixmaps/menuplay.xpm"
#include "../pixmaps/menupause.xpm"
#include "../pixmaps/menustop.xpm"
#include "../pixmaps/menuprev.xpm"
#include "../pixmaps/menunext.xpm"

static void MakeTrayIcon(GripInfo *ginfo);
static void PlayCB(GtkWidget *widget, gpointer data);
static void PauseCB(GtkWidget *widget, gpointer data);
static void NextCB(GtkWidget *widget, gpointer data);
static void PrevCB(GtkWidget *widget, gpointer data);
static void StopCB(GtkWidget *widget, gpointer data);
static void RipEncCB(GtkWidget *widget, gpointer data);
static void QuitCB(GtkWidget *widget, gpointer data);
static gboolean TrayIconButtonPress(GtkWidget *widget, GdkEventButton *event, gpointer data);

/*
 * UpdateTray
 *
 * Main functionality implemented here; handles:
 * - updating the tooltip
 * - if the tray icon is to be shown/built
 * - when to make menu sensitive/unsensitive
 * - NOTE: ginfo->show_tray_icon is set in the Config -> Misc tab
 */

void UpdateTray(GripInfo *ginfo)
{
	gchar *text, *riptext = NULL, *enctext = NULL;
	gchar *artist = strcmp(ginfo->ddata.data_artist, "") ? ginfo->ddata.data_artist : _("Artist");
	gchar *title = strcmp(ginfo->ddata.data_title, "") ? ginfo->ddata.data_title : _("Title");
	GripGUI *uinfo = &(ginfo->gui_info);
	
	int tmin, tsec, emin, esec;
	
	/* Decide if the tray icon is going to be displayed or not */
	if (ginfo->show_tray_icon) {
		if (!ginfo->tray_icon_made)
			MakeTrayIcon(ginfo);
	} else {
		if (uinfo->tray_icon) {
			gtk_widget_destroy(GTK_WIDGET(uinfo->tray_icon));
			uinfo->tray_icon = NULL;
		}
		ginfo->tray_icon_made = FALSE;
	}
	
	/* tray icon is present so we can make our tooltip */
	if (ginfo->show_tray_icon) {
		if (ginfo->playing) {
			TrayUnGrayMenu(ginfo);
			tmin = ginfo->disc.track[ginfo->current_track_index].length.mins;
			tsec = ginfo->disc.track[ginfo->current_track_index].length.secs;
			emin = ginfo->disc.track_time.mins;
			esec = ginfo->disc.track_time.secs;
			text = g_strdup_printf(_("%s - %s\n%02d:%02d of %02d:%02d"), artist, ginfo->ddata.data_track[ginfo->current_track_index].track_name, emin, esec, tmin, tsec);
		} else if (!ginfo->playing) {
			if (ginfo->ripping || ginfo->encoding) {
				TrayGrayMenu(ginfo);
				riptext = (ginfo->ripping) ? g_strdup_printf(_("Ripping  Track %02d:\t%6.2f%% (%6.2f%% )"), ginfo->rip_track + 1, ginfo->rip_percent * 100, ginfo->rip_tot_percent * 100) : g_strdup_printf("");
			    enctext = (ginfo->encoding) ? g_strdup_printf(_("Encoding Track %02d:\t%6.2f%% (%6.2f%% )"), ginfo->mp3_enc_track[0] + 1, ginfo->enc_percent * 100, ginfo->enc_tot_percent * 100) : g_strdup_printf("");
				text = g_strdup_printf(_("%s - %s\n%s%s%s"), artist, title, riptext, (ginfo->ripping && ginfo->encoding) ? "\n" : "", enctext);
			} else {
				TrayUnGrayMenu(ginfo);
				text = g_strdup_printf(_("%s - %s\nIdle"), artist, title);
			}
		}
	
		gtk_tooltips_set_tip(GTK_TOOLTIPS(uinfo->tray_tips), uinfo->tray_ebox, text, NULL);
	
		if (riptext) g_free(riptext);
		if (enctext) g_free(enctext);
		g_free(text);
	}
}

/*
 * TrayMenuShowPlay/TrayMenuShowPause
 *
 * - set whether the Play menu item or Pause menu item is displayed
 */

void TrayMenuShowPlay(GripInfo *ginfo)
{
	GripGUI *uinfo = &(ginfo->gui_info);
	
	gtk_widget_hide(GTK_WIDGET(uinfo->tray_menu_pause));
	gtk_widget_show(GTK_WIDGET(uinfo->tray_menu_play));
}

void TrayMenuShowPause(GripInfo *ginfo)
{
	GripGUI *uinfo = &(ginfo->gui_info);
	
	gtk_widget_hide(GTK_WIDGET(uinfo->tray_menu_play));
	gtk_widget_show(GTK_WIDGET(uinfo->tray_menu_pause));
}

/*
 * TrayGrayMenu/TrayUnGrayMenu
 *
 * - sets sensitivity of the menu items
 */

static void ToggleMenuItemSensitive(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), (gboolean)data);
}

void TrayGrayMenu(GripInfo *ginfo)
{
	GripGUI *uinfo = &(ginfo->gui_info);
	
	if (ginfo->tray_menu_sensitive) {
		gtk_container_foreach(GTK_CONTAINER(uinfo->tray_menu), ToggleMenuItemSensitive, (gpointer)FALSE);
		ginfo->tray_menu_sensitive = FALSE;
	}
}

void TrayUnGrayMenu(GripInfo *ginfo)
{
	GripGUI *uinfo = &(ginfo->gui_info);
	
	if (!ginfo->tray_menu_sensitive) {
		gtk_container_foreach(GTK_CONTAINER(uinfo->tray_menu), ToggleMenuItemSensitive, (gpointer)TRUE);
		ginfo->tray_menu_sensitive = TRUE;
	}	
}

/*
 * MakeTrayIcon
 *
 * - tray icon and menu is built here
 * - NOTE: I added the function BuildMenuItemXpm to uihelper.c
 */

static void MakeTrayIcon(GripInfo *ginfo)
{
	GtkWidget *image, *mentry, *hb, *img;
	GripGUI *uinfo = &(ginfo->gui_info);
	
	uinfo->tray_icon = egg_tray_icon_new("Grip");	
	
	uinfo->tray_ebox = gtk_event_box_new();
	
	gtk_container_set_border_width(GTK_CONTAINER(uinfo->tray_icon), 0);
	
	gtk_container_add(GTK_CONTAINER(uinfo->tray_icon), uinfo->tray_ebox);
		
	image = gtk_image_new_from_file(GNOME_ICONDIR"/griptray.png");
	
	gtk_container_add(GTK_CONTAINER(uinfo->tray_ebox), image);
	
	uinfo->tray_tips = gtk_tooltips_new();
	
	uinfo->tray_menu = gtk_menu_new();
	
	img = (GtkWidget*)Loadxpm(uinfo->app, menuplay_xpm);
	uinfo->tray_menu_play = (GtkWidget*)BuildMenuItemXpm(img, _("Play"));
	g_signal_connect(uinfo->tray_menu_play, "activate", G_CALLBACK(PlayCB), ginfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), uinfo->tray_menu_play);
	
	img = (GtkWidget*)Loadxpm(uinfo->app, menupause_xpm);
	uinfo->tray_menu_pause = (GtkWidget*)BuildMenuItemXpm(img, _("Pause"));
	g_signal_connect(uinfo->tray_menu_pause, "activate", G_CALLBACK(PauseCB), ginfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), uinfo->tray_menu_pause);
	
	img = (GtkWidget*)Loadxpm(uinfo->app, menustop_xpm);
	mentry = (GtkWidget*)BuildMenuItemXpm(img, _("Stop"));
	g_signal_connect(mentry, "activate", G_CALLBACK(StopCB), ginfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), mentry);
	
	hb = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), hb);
	
	img = (GtkWidget*)Loadxpm(uinfo->app, menuprev_xpm);
	mentry = (GtkWidget*)BuildMenuItemXpm(img, _("Previous"));
	g_signal_connect(mentry, "activate", G_CALLBACK(PrevCB), ginfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), mentry);
	
	img = (GtkWidget*)Loadxpm(uinfo->app, menunext_xpm);
	mentry = (GtkWidget*)BuildMenuItemXpm(img, _("Next"));
	g_signal_connect(mentry, "activate", G_CALLBACK(NextCB), ginfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), mentry);
	
	hb = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), hb);
	
	img = (GtkWidget*)Loadxpm(uinfo->app, rip1_xpm);
	mentry = (GtkWidget*)BuildMenuItemXpm(img, _("Rip and Encode"));
	g_signal_connect(mentry, "activate", G_CALLBACK(RipEncCB), ginfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), mentry);
	
	hb = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), hb);

	mentry = (GtkWidget*)BuildMenuItem(GTK_STOCK_QUIT, _("Quit"), TRUE);
	g_signal_connect(mentry, "activate", G_CALLBACK(QuitCB), ginfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(uinfo->tray_menu), mentry);
		
	g_signal_connect(uinfo->tray_ebox, "button-press-event", G_CALLBACK(TrayIconButtonPress), ginfo);
	
	gtk_widget_show_all(uinfo->tray_menu);
	
	gtk_widget_hide(uinfo->tray_menu_pause);
	
	gtk_widget_show_all(GTK_WIDGET(uinfo->tray_icon));	
		
	ginfo->tray_icon_made = TRUE;
}

/*
 * TrayIconButtonPress
 *
 * - handles the showing/hiding of the main window and displaying the menu
 * - NOTE: ginfo->app_visible is set by AppWindowStateCB in grip.c
 */

static gboolean TrayIconButtonPress(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	GripInfo *ginfo = (GripInfo*)data;
	GripGUI *uinfo = &(ginfo->gui_info);
		
	if (event->button == 1) {
		if (ginfo->app_visible) {
			gtk_window_get_position(GTK_WINDOW(uinfo->app), &uinfo->x, &uinfo->y);
			gtk_widget_hide(GTK_WIDGET(uinfo->app));
		} else {
			gtk_window_move(GTK_WINDOW(uinfo->app), uinfo->x, uinfo->y);
			gtk_window_present(GTK_WINDOW(uinfo->app));
		}
				
		return TRUE;
	}
		
	if (event->button == 3) {
		gtk_menu_popup(GTK_MENU(uinfo->tray_menu), NULL, NULL, NULL, NULL, event->button, event->time);
		
		return TRUE;
	}
	
	return FALSE;
}

/*
 * Callbacks for the menu entries; pretty self-explanatory
 */

static void PlayCB(GtkWidget *widget, gpointer data)
{
	PlayTrackCB(NULL, data);
}

static void PauseCB(GtkWidget *widget, gpointer data)
{
	PlayTrackCB(NULL, data);
}	

static void StopCB(GtkWidget *widget, gpointer data)
{
	StopPlayCB(NULL, data);
}

static void NextCB(GtkWidget *widget, gpointer data)
{
	NextTrackCB(NULL, data);
}

static void PrevCB(GtkWidget *widget, gpointer data)
{
	PrevTrackCB(NULL, data);
}

static void RipEncCB(GtkWidget *widget, gpointer data)
{
	int i;
	GripInfo *ginfo = (GripInfo*)data;
	
	/* this gets rid of the annoying 'Rip Whole CD?' dialog box */
	for (i = 0; i < ginfo->disc.num_tracks; i++)
		SetChecked(&(ginfo->gui_info), i, TRUE);
		
	DoRip(NULL, data);
}

static void QuitCB(GtkWidget *widget, gpointer data)
{
	GripInfo *ginfo = (GripInfo*)data;
	GripGUI *uinfo = &(ginfo->gui_info);
	
	GripDie(uinfo->app, NULL);
}
