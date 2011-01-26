/* grip.c
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

#include <pthread.h>
#include <config.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <time.h>
#include "grip.h"
#include <libgnomeui/gnome-window-icon.h>
#include "discdb.h"
#include "cdplay.h"
#include "discedit.h"
#include "rip.h"
#include "common.h"
#include "dialog.h"
#include "gripcfg.h"
#include "xpm.h"
#include "parsecfg.h"
#include "tray.h"

static void ReallyDie(gint reply,gpointer data);
static void MakeStatusPage(GripInfo *ginfo);
static void DoHelp(GtkWidget *widget,gpointer data);
static void MakeHelpPage(GripInfo *ginfo);
static void MakeAboutPage(GripGUI *uinfo);
static void MakeStyles(GripGUI *uinfo);
static void Homepage(void);
static void LoadImages(GripGUI *uinfo);
static void DoLoadConfig(GripInfo *ginfo);
void DoSaveConfig(GripInfo *ginfo);

#define BASE_CFG_ENTRIES \
{"grip_version",CFG_ENTRY_STRING,256,ginfo->version},\
{"cd_device",CFG_ENTRY_STRING,256,ginfo->cd_device},\
{"force_scsi",CFG_ENTRY_STRING,256,ginfo->force_scsi},\
{"ripexename",CFG_ENTRY_STRING,256,ginfo->ripexename},\
{"ripcmdline",CFG_ENTRY_STRING,256,ginfo->ripcmdline},\
{"wav_filter_cmd",CFG_ENTRY_STRING,256,ginfo->wav_filter_cmd},\
{"disc_filter_cmd",CFG_ENTRY_STRING,256,ginfo->disc_filter_cmd},\
{"mp3exename",CFG_ENTRY_STRING,256,ginfo->mp3exename},\
{"mp3cmdline",CFG_ENTRY_STRING,256,ginfo->mp3cmdline},\
{"dbserver",CFG_ENTRY_STRING,256,ginfo->dbserver.name},\
{"ripfileformat",CFG_ENTRY_STRING,256,ginfo->ripfileformat},\
{"mp3fileformat",CFG_ENTRY_STRING,256,ginfo->mp3fileformat},\
{"mp3extension",CFG_ENTRY_STRING,10,ginfo->mp3extension},\
{"m3ufileformat",CFG_ENTRY_STRING,256,ginfo->m3ufileformat},\
{"delete_wavs",CFG_ENTRY_BOOL,0,&ginfo->delete_wavs},\
{"add_m3u",CFG_ENTRY_BOOL,0,&ginfo->add_m3u},\
{"rel_m3u",CFG_ENTRY_BOOL,0,&ginfo->rel_m3u},\
{"add_to_db",CFG_ENTRY_BOOL,0,&ginfo->add_to_db},\
{"use_proxy",CFG_ENTRY_BOOL,0,&ginfo->use_proxy},\
{"proxy_name",CFG_ENTRY_STRING,256,ginfo->proxy_server.name},\
{"proxy_port",CFG_ENTRY_INT,0,&(ginfo->proxy_server.port)},\
{"proxy_user",CFG_ENTRY_STRING,80,ginfo->proxy_server.username},\
{"proxy_pswd",CFG_ENTRY_STRING,80,ginfo->proxy_server.pswd},\
{"cdupdate",CFG_ENTRY_STRING,256,ginfo->cdupdate},\
{"user_email",CFG_ENTRY_STRING,256,ginfo->user_email},\
{"ripnice",CFG_ENTRY_INT,0,&ginfo->ripnice},\
{"mp3nice",CFG_ENTRY_INT,0,&ginfo->mp3nice},\
{"mp3_filter_cmd",CFG_ENTRY_STRING,256,ginfo->mp3_filter_cmd},\
{"doid3",CFG_ENTRY_BOOL,0,&ginfo->doid3},\
{"doid3v2",CFG_ENTRY_BOOL,0,&ginfo->doid3v2},\
{"tag_mp3_only",CFG_ENTRY_BOOL,0,&ginfo->tag_mp3_only},\
{"id3_comment",CFG_ENTRY_STRING,30,ginfo->id3_comment},\
{"max_wavs",CFG_ENTRY_INT,0,&ginfo->max_wavs},\
{"auto_rip",CFG_ENTRY_BOOL,0,&ginfo->auto_rip},\
{"eject_after_rip",CFG_ENTRY_BOOL,0,&ginfo->eject_after_rip},\
{"eject_delay",CFG_ENTRY_INT,0,&ginfo->eject_delay},\
{"delayed_encoding",CFG_ENTRY_BOOL,0,&ginfo->delayed_encoding},\
{"delay_before_rip",CFG_ENTRY_BOOL,0,&ginfo->delay_before_rip},\
{"stop_between_tracks",CFG_ENTRY_BOOL,0,&ginfo->stop_between_tracks},\
{"beep_after_rip",CFG_ENTRY_BOOL,0,&ginfo->beep_after_rip},\
{"faulty_eject",CFG_ENTRY_BOOL,0,&ginfo->faulty_eject},\
{"poll_drive",CFG_ENTRY_BOOL,0,&ginfo->poll_drive},\
{"poll_interval",CFG_ENTRY_INT,0,&ginfo->poll_interval},\
{"use_proxy_env",CFG_ENTRY_BOOL,0,&ginfo->use_proxy_env},\
{"db_cgi",CFG_ENTRY_STRING,256,ginfo->dbserver.cgi_prog},\
{"cddb_submit_email",CFG_ENTRY_STRING,256,ginfo->discdb_submit_email},\
{"discdb_encoding",CFG_ENTRY_STRING,16,ginfo->discdb_encoding},\
{"submit_email_program",CFG_ENTRY_STRING,255,ginfo->discdb_encoding},\
{"id3_encoding",CFG_ENTRY_STRING,16,ginfo->id3_encoding},\
{"id3v2_encoding",CFG_ENTRY_STRING,16,ginfo->id3v2_encoding},\
{"db_use_freedb",CFG_ENTRY_BOOL,0,&ginfo->db_use_freedb},\
{"dbserver2",CFG_ENTRY_STRING,256,ginfo->dbserver2.name},\
{"db2_cgi",CFG_ENTRY_STRING,256,ginfo->dbserver2.cgi_prog},\
{"no_interrupt",CFG_ENTRY_BOOL,0,&ginfo->no_interrupt},\
{"stop_first",CFG_ENTRY_BOOL,0,&ginfo->stop_first},\
{"play_first",CFG_ENTRY_BOOL,0,&ginfo->play_first},\
{"play_on_insert",CFG_ENTRY_BOOL,0,&ginfo->play_on_insert},\
{"automatic_cddb",CFG_ENTRY_BOOL,0,&ginfo->automatic_discdb},\
{"automatic_reshuffle",CFG_ENTRY_BOOL,0,&ginfo->automatic_reshuffle},\
{"no_lower_case",CFG_ENTRY_BOOL,0,&ginfo->sprefs.no_lower_case},\
{"no_underscore",CFG_ENTRY_BOOL,0,&ginfo->sprefs.no_underscore},\
{"allow_high_bits",CFG_ENTRY_BOOL,0,&ginfo->sprefs.allow_high_bits},\
{"escape",CFG_ENTRY_BOOL,0,&ginfo->sprefs.escape},\
{"allow_these_chars",CFG_ENTRY_STRING,256,ginfo->sprefs.allow_these_chars},\
{"show_tray_icon",CFG_ENTRY_BOOL,0,&ginfo->show_tray_icon},\
{"num_cpu",CFG_ENTRY_INT,0,&ginfo->edit_num_cpu},\
{"kbits_per_sec",CFG_ENTRY_INT,0,&ginfo->kbits_per_sec},\
{"selected_encoder",CFG_ENTRY_INT,0,&ginfo->selected_encoder},\
{"selected_ripper",CFG_ENTRY_INT,0,&ginfo->selected_ripper},\
{"play_mode",CFG_ENTRY_INT,0,&ginfo->play_mode},\
{"playloop",CFG_ENTRY_BOOL,0,&ginfo->playloop},\
{"win_width",CFG_ENTRY_INT,0,&uinfo->win_width},\
{"win_height",CFG_ENTRY_INT,0,&uinfo->win_height},\
{"win_height_edit",CFG_ENTRY_INT,0,&uinfo->win_height_edit},\
{"win_width_min",CFG_ENTRY_INT,0,&uinfo->win_width_min},\
{"win_height_min",CFG_ENTRY_INT,0,&uinfo->win_height_min},\
{"vol_vis",CFG_ENTRY_BOOL,0,&uinfo->volvis},\
{"track_edit_vis",CFG_ENTRY_BOOL,0,&uinfo->track_edit_visible},\
{"track_prog_vis",CFG_ENTRY_BOOL,0,&uinfo->track_prog_visible},\
{"volume",CFG_ENTRY_INT,0,&ginfo->volume},

#define CDPAR_CFG_ENTRIES \
{"disable_paranoia",CFG_ENTRY_BOOL,0,&ginfo->disable_paranoia},\
{"disable_extra_paranoia",CFG_ENTRY_BOOL,0,&ginfo->disable_extra_paranoia},\
{"disable_scratch_detect",CFG_ENTRY_BOOL,0,&ginfo->disable_scratch_detect},\
{"disable_scratch_repair",CFG_ENTRY_BOOL,0,&ginfo->disable_scratch_repair},\
{"calc_gain",CFG_ENTRY_BOOL,0,&ginfo->calc_gain},

#ifdef CDPAR
#define CFG_ENTRIES BASE_CFG_ENTRIES CDPAR_CFG_ENTRIES
#else
#define CFG_ENTRIES BASE_CFG_ENTRIES
#endif

gboolean AppWindowStateCB(GtkWidget *widget, GdkEventWindowState *event, gpointer data)
{
  GripInfo *ginfo = (GripInfo*)data;
  GripGUI *uinfo = &(ginfo->gui_info);
  GdkWindowState state = event->new_window_state;
	
  if ((state & GDK_WINDOW_STATE_WITHDRAWN) || (state & GDK_WINDOW_STATE_ICONIFIED)) {
    ginfo->app_visible = FALSE;
    return TRUE;
  } else {
    ginfo->app_visible = TRUE;
    gtk_window_get_position(GTK_WINDOW(uinfo->app), &uinfo->x, &uinfo->y);
    return TRUE;
  }
	
  return FALSE;
}

GtkWidget *GripNew(const gchar* geometry,char *device,char *scsi_device,
		   char *config_filename,
		   gboolean force_small,
		   gboolean local_mode,gboolean no_redirect)
{
  GtkWidget *app;
  GripInfo *ginfo;
  GripGUI *uinfo;
  int major,minor,point;
  char buf[256];

  gnome_window_icon_set_default_from_file(GNOME_ICONDIR"/gripicon.png");

  app=gnome_app_new(PACKAGE,_("Grip"));
 
  ginfo=g_new0(GripInfo,1);

  gtk_object_set_user_data(GTK_OBJECT(app),(gpointer)ginfo);

  uinfo=&(ginfo->gui_info);
  uinfo->app=app;
  uinfo->status_window=NULL;
  uinfo->rip_status_window=NULL;
  uinfo->encode_status_window=NULL;
  uinfo->track_list=NULL;

  uinfo->win_width=WINWIDTH;
  uinfo->win_height=WINHEIGHT;
  uinfo->win_height_edit=WINHEIGHTEDIT;
  uinfo->win_width_min=MIN_WINWIDTH;
  uinfo->win_height_min=MIN_WINHEIGHT;

  /*  if(geometry != NULL) {
    gint x,y,w,h;
    
    if(gnome_parse_geometry(geometry, 
			    &x,&y,&w,&h)) {
      if(x != -1) {
	gtk_widget_set_uposition(app,x,y);
      }
      
      if(w != -1) {
        uinfo->win_width=w;
        uinfo->win_height=h;
      }
    }
    else {
      g_error(_("Could not parse geometry string `%s'"), geometry);
    }
  }
  */

  if(config_filename && *config_filename)
    g_snprintf(ginfo->config_filename,256,"%s",config_filename);
  else {
    strcpy(ginfo->config_filename,".grip");
  }

  Debug("Using config file [%s]\n",ginfo->config_filename);

  DoLoadConfig(ginfo);

  if(device) g_snprintf(ginfo->cd_device,256,"%s",device);
  if(scsi_device) g_snprintf(ginfo->force_scsi,256,"%s",scsi_device);

  uinfo->minimized=force_small;
  ginfo->local_mode=local_mode;
  ginfo->do_redirect=!no_redirect;

  if(!CDInitDevice(ginfo->cd_device,&(ginfo->disc))) {
    sprintf(buf,_("Error: Unable to initialize [%s]\n"),ginfo->cd_device);

    DisplayMsg(buf);
  }

  CDStat(&(ginfo->disc),TRUE);

  gtk_window_set_policy(GTK_WINDOW(app),FALSE,TRUE,FALSE);
  gtk_window_set_wmclass(GTK_WINDOW(app),"grip","Grip");
  g_signal_connect(G_OBJECT(app),"delete_event",
		   G_CALLBACK(GripDie),NULL);

  if(uinfo->minimized) {
    gtk_widget_set_size_request(GTK_WIDGET(app),MIN_WINWIDTH,
                                MIN_WINHEIGHT);

    gtk_window_resize(GTK_WINDOW(app),uinfo->win_width_min,
                                uinfo->win_height_min);
  }
  else {
    gtk_widget_set_size_request(GTK_WIDGET(app),WINWIDTH,
                                WINHEIGHT);

    if(uinfo->track_edit_visible) {
      gtk_window_resize(GTK_WINDOW(app),uinfo->win_width,
                        uinfo->win_height_edit);
    }
    else {
      gtk_window_resize(GTK_WINDOW(app),uinfo->win_width,
                        uinfo->win_height);
    }
  }

  gtk_widget_realize(app);

  uinfo->winbox=gtk_vbox_new(FALSE,3);
  if(!uinfo->minimized)
    gtk_container_border_width(GTK_CONTAINER(uinfo->winbox),3);

  uinfo->notebook=gtk_notebook_new();

  LoadImages(uinfo);
  MakeStyles(uinfo);
  MakeTrackPage(ginfo);
  MakeRipPage(ginfo);
  MakeConfigPage(ginfo);
  MakeStatusPage(ginfo);
  MakeHelpPage(ginfo);
  MakeAboutPage(uinfo);
  ginfo->tray_icon_made = FALSE;
  ginfo->tray_menu_sensitive = TRUE;

  gtk_box_pack_start(GTK_BOX(uinfo->winbox),uinfo->notebook,TRUE,TRUE,0);
  if(!uinfo->minimized) gtk_widget_show(uinfo->notebook);

  uinfo->track_edit_box=MakeEditBox(ginfo);
  gtk_box_pack_start(GTK_BOX(uinfo->winbox),uinfo->track_edit_box,
		     FALSE,FALSE,0);
  if(uinfo->track_edit_visible) gtk_widget_show(uinfo->track_edit_box);


  uinfo->playopts=MakePlayOpts(ginfo);
  gtk_box_pack_start(GTK_BOX(uinfo->winbox),uinfo->playopts,FALSE,FALSE,0);
  if(uinfo->track_prog_visible) gtk_widget_show(uinfo->playopts);
 
  uinfo->controls=MakeControls(ginfo);
  if(uinfo->minimized)
    gtk_box_pack_start(GTK_BOX(uinfo->winbox),uinfo->controls,TRUE,TRUE,0);
  else
    gtk_box_pack_start(GTK_BOX(uinfo->winbox),uinfo->controls,FALSE,FALSE,0);
  gtk_widget_show(uinfo->controls);
  
  gnome_app_set_contents(GNOME_APP(app),uinfo->winbox);
  gtk_widget_show(uinfo->winbox);

  CheckNewDisc(ginfo,FALSE);

  /* Check if we're running this version for the first time */
  if(strcmp(VERSION,ginfo->version)!=0) {
    strcpy(ginfo->version,VERSION);

    sscanf(VERSION,"%d.%d.%d",&major,&minor,&point);

    /* Check if we have a dev release */
    if(minor%2) {
      gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                        _("This is a development version of Grip. If you encounter problems, you are encouraged to revert to the latest stable version."));
    }
  }

  g_signal_connect(app, "window-state-event", G_CALLBACK(AppWindowStateCB), ginfo);

  LogStatus(ginfo,_("Grip started successfully\n"));

  return app;
}

void GripDie(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)gtk_object_get_user_data(GTK_OBJECT(widget));
  
#ifndef GRIPCD
  if(ginfo->ripping_a_disc || ginfo->encoding)
    gnome_app_ok_cancel_modal((GnomeApp *)ginfo->gui_info.app,
			      _("Work is in progress.\nReally shut down?"),
			      ReallyDie,(gpointer)ginfo);
  else ReallyDie(0,ginfo);
#else
  ReallyDie(0,ginfo);
#endif
}

static void ReallyDie(gint reply,gpointer data)
{
  GripInfo *ginfo;

  if(reply) return;

  ginfo=(GripInfo *)data;

#ifndef GRIPCD
  if(ginfo->ripping_a_disc) KillRip(NULL,ginfo);
  if(ginfo->encoding) KillEncode(NULL,ginfo);
#endif

  if(!ginfo->no_interrupt)
    CDStop(&(ginfo->disc));

  DoSaveConfig(ginfo);

  gtk_main_quit();
}

GtkWidget *MakeNewPage(GtkWidget *notebook,char *name)
{
  GtkWidget *page;
  GtkWidget *label;

  page=gtk_frame_new(NULL);
  gtk_widget_show(page);

  label=gtk_label_new(name);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page,label);

  return page;
}

static void MakeStatusPage(GripInfo *ginfo)
{
  GtkWidget *status_page;
  GtkWidget *vbox,*vbox2;
  GtkWidget *notebook;
  GtkWidget *page;
  GtkWidget *label;

  status_page=MakeNewPage(ginfo->gui_info.notebook,_("Status"));

  vbox2=gtk_vbox_new(FALSE,0);
  notebook=gtk_notebook_new();

  page=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,4);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  ginfo->gui_info.status_window=NewStatusWindow(vbox);

  gtk_container_add(GTK_CONTAINER(page),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("General"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page,label);
  gtk_widget_show(page);


  page=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,4);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  ginfo->gui_info.rip_status_window=NewStatusWindow(vbox);

  gtk_container_add(GTK_CONTAINER(page),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Rip"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page,label);
  /*  gtk_widget_show(page);*/


  page=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,4);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  ginfo->gui_info.encode_status_window=NewStatusWindow(vbox);

  gtk_container_add(GTK_CONTAINER(page),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Encode"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page,label);
  /*  gtk_widget_show(page);*/


  gtk_box_pack_start(GTK_BOX(vbox2),notebook,TRUE,TRUE,0);
  gtk_widget_show(notebook);

  gtk_container_add(GTK_CONTAINER(status_page),vbox2);
  gtk_widget_show(vbox2);
}

void LogStatus(GripInfo *ginfo,char *fmt,...)
{
  va_list args;
  char *buf;

  if(!ginfo->gui_info.status_window) return;

  va_start(args,fmt);

  buf=g_strdup_vprintf(fmt,args);

  va_end(args);

  StatusWindowWrite(ginfo->gui_info.status_window,buf);

  g_free(buf);
}

static void DoHelp(GtkWidget *widget,gpointer data)
{
  char *section;

  section=(char *)data;

  gnome_help_display("grip.xml",section,NULL);
}

static void MakeHelpPage(GripInfo *ginfo)
{
  GtkWidget *help_page;
  GtkWidget *button;
  GtkWidget *vbox;

  help_page=MakeNewPage(ginfo->gui_info.notebook,_("Help"));

  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  button=gtk_button_new_with_label(_("Table Of Contents"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoHelp),NULL);
  gtk_box_pack_start(GTK_BOX(vbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Playing CDs"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoHelp),(gpointer)"cdplayer");
  gtk_box_pack_start(GTK_BOX(vbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Ripping CDs"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoHelp),(gpointer)"ripping");
  gtk_box_pack_start(GTK_BOX(vbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Configuring Grip"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoHelp),(gpointer)"configure");
  gtk_box_pack_start(GTK_BOX(vbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("FAQ"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoHelp),(gpointer)"faq");
  gtk_box_pack_start(GTK_BOX(vbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Getting More Help"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoHelp),(gpointer)"morehelp");
  gtk_box_pack_start(GTK_BOX(vbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Reporting Bugs"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoHelp),(gpointer)"bugs");
  gtk_box_pack_start(GTK_BOX(vbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  gtk_container_add(GTK_CONTAINER(help_page),vbox);
  gtk_widget_show(vbox);
}

void MakeAboutPage(GripGUI *uinfo)
{
  GtkWidget *aboutpage;
  GtkWidget *vbox,*vbox2,*hbox;
  GtkWidget *label;
  GtkWidget *logo;
  GtkWidget *ebox;
  GtkWidget *button;
  char versionbuf[20];

  aboutpage=MakeNewPage(uinfo->notebook,_("About"));

  ebox=gtk_event_box_new();
  gtk_widget_set_style(ebox,uinfo->style_wb);

  vbox=gtk_vbox_new(TRUE,5);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

#ifndef GRIPCD
  logo=Loadxpm(GTK_WIDGET(uinfo->app),grip_xpm);
#else
  logo=Loadxpm(GTK_WIDGET(uinfo->app),gcd_xpm);
#endif

  gtk_box_pack_start(GTK_BOX(vbox),logo,FALSE,FALSE,0);
  gtk_widget_show(logo);

  vbox2=gtk_vbox_new(TRUE,0);

  sprintf(versionbuf,_("Version %s"),VERSION);
  label=gtk_label_new(versionbuf);
  gtk_widget_set_style(label,uinfo->style_wb);
  gtk_box_pack_start(GTK_BOX(vbox2),label,FALSE,FALSE,0);
  gtk_widget_show(label);

  label=gtk_label_new("Copyright 1998-2005, Mike Oliphant");
  gtk_widget_set_style(label,uinfo->style_wb);
  gtk_box_pack_start(GTK_BOX(vbox2),label,FALSE,FALSE,0);
  gtk_widget_show(label);

#if defined(__sun__)
  label=gtk_label_new("Solaris Port, David Meleedy");
  gtk_widget_set_style(label,uinfo->style_wb);
  gtk_box_pack_start(GTK_BOX(vbox2),label,FALSE,FALSE,0);
  gtk_widget_show(label);
#endif

  hbox=gtk_hbox_new(TRUE,0);

  button=gtk_button_new_with_label("http://www.nostatic.org/grip");
  gtk_widget_set_style(button,uinfo->style_dark_grey);
  gtk_widget_set_style(GTK_BIN(button)->child,
		       uinfo->style_dark_grey);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(Homepage),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  gtk_box_pack_start(GTK_BOX(vbox2),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);
  

  gtk_container_add(GTK_CONTAINER(vbox),vbox2);
  gtk_widget_show(vbox2);

  gtk_container_add(GTK_CONTAINER(ebox),vbox);
  gtk_widget_show(vbox);

  gtk_container_add(GTK_CONTAINER(aboutpage),ebox);
  gtk_widget_show(ebox);
}

static void MakeStyles(GripGUI *uinfo)
{
  GdkColor gdkblack;
  GdkColor gdkwhite;
  GdkColor *color_LCD;
  GdkColor *color_dark_grey;

  gdk_color_white(gdk_colormap_get_system(),&gdkwhite);
  gdk_color_black(gdk_colormap_get_system(),&gdkblack);
  
  color_LCD=MakeColor(33686,38273,29557);
  color_dark_grey=MakeColor(0x4444,0x4444,0x4444);
  
  uinfo->style_wb=MakeStyle(&gdkwhite,&gdkblack,FALSE);
  uinfo->style_LCD=MakeStyle(color_LCD,color_LCD,FALSE);
  uinfo->style_dark_grey=MakeStyle(&gdkwhite,color_dark_grey,TRUE);
}

static void Homepage(void)
{
  system("gnome-moz-remote http://www.nostatic.org/grip");
}

static void LoadImages(GripGUI *uinfo)
{
  uinfo->check_image=Loadxpm(uinfo->app,check_xpm);
  uinfo->eject_image=Loadxpm(uinfo->app,eject_xpm);
  uinfo->cdscan_image=Loadxpm(uinfo->app,cdscan_xpm);
  uinfo->ff_image=Loadxpm(uinfo->app,ff_xpm);
  uinfo->lowleft_image=Loadxpm(uinfo->app,lowleft_xpm);
  uinfo->lowright_image=Loadxpm(uinfo->app,lowright_xpm);
  uinfo->minmax_image=Loadxpm(uinfo->app,minmax_xpm);
  uinfo->nexttrk_image=Loadxpm(uinfo->app,nexttrk_xpm);
  uinfo->playpaus_image=Loadxpm(uinfo->app,playpaus_xpm);
  uinfo->prevtrk_image=Loadxpm(uinfo->app,prevtrk_xpm);
  uinfo->loop_image=Loadxpm(uinfo->app,loop_xpm);
  uinfo->noloop_image=Loadxpm(uinfo->app,noloop_xpm);
  uinfo->random_image=Loadxpm(uinfo->app,random_xpm);
  uinfo->playlist_image=Loadxpm(uinfo->app,playlist_xpm);
  uinfo->playnorm_image=Loadxpm(uinfo->app,playnorm_xpm);
  uinfo->quit_image=Loadxpm(uinfo->app,quit_xpm);
  uinfo->rew_image=Loadxpm(uinfo->app,rew_xpm);
  uinfo->stop_image=Loadxpm(uinfo->app,stop_xpm);
  uinfo->upleft_image=Loadxpm(uinfo->app,upleft_xpm);
  uinfo->upright_image=Loadxpm(uinfo->app,upright_xpm);
  uinfo->vol_image=Loadxpm(uinfo->app,vol_xpm);
  uinfo->discdbwht_image=Loadxpm(uinfo->app,discdbwht_xpm);
  uinfo->rotate_image=Loadxpm(uinfo->app,rotate_xpm);
  uinfo->edit_image=Loadxpm(uinfo->app,edit_xpm);
  uinfo->progtrack_image=Loadxpm(uinfo->app,progtrack_xpm);
  uinfo->mail_image=Loadxpm(uinfo->app,mail_xpm);
  uinfo->save_image=Loadxpm(uinfo->app,save_xpm);

  uinfo->empty_image=NewBlankPixmap(uinfo->app);

  uinfo->discdb_pix[0]=Loadxpm(uinfo->app,discdb0_xpm);
  uinfo->discdb_pix[1]=Loadxpm(uinfo->app,discdb1_xpm);

  uinfo->play_pix[0]=Loadxpm(uinfo->app,playnorm_xpm);
  uinfo->play_pix[1]=Loadxpm(uinfo->app,random_xpm);
  uinfo->play_pix[2]=Loadxpm(uinfo->app,playlist_xpm);

#ifndef GRIPCD
  uinfo->rip_pix[0]=Loadxpm(uinfo->app,rip0_xpm);
  uinfo->rip_pix[1]=Loadxpm(uinfo->app,rip1_xpm);
  uinfo->rip_pix[2]=Loadxpm(uinfo->app,rip2_xpm);
  uinfo->rip_pix[3]=Loadxpm(uinfo->app,rip3_xpm);

  uinfo->mp3_pix[0]=Loadxpm(uinfo->app,enc0_xpm);
  uinfo->mp3_pix[1]=Loadxpm(uinfo->app,enc1_xpm);
  uinfo->mp3_pix[2]=Loadxpm(uinfo->app,enc2_xpm);
  uinfo->mp3_pix[3]=Loadxpm(uinfo->app,enc3_xpm);

  uinfo->smile_pix[0]=Loadxpm(uinfo->app,smile1_xpm);
  uinfo->smile_pix[1]=Loadxpm(uinfo->app,smile2_xpm);
  uinfo->smile_pix[2]=Loadxpm(uinfo->app,smile3_xpm);
  uinfo->smile_pix[3]=Loadxpm(uinfo->app,smile4_xpm);
  uinfo->smile_pix[4]=Loadxpm(uinfo->app,smile5_xpm);
  uinfo->smile_pix[5]=Loadxpm(uinfo->app,smile6_xpm);
  uinfo->smile_pix[6]=Loadxpm(uinfo->app,smile7_xpm);
  uinfo->smile_pix[7]=Loadxpm(uinfo->app,smile8_xpm);
#endif
}

void GripUpdate(GtkWidget *app)
{
  GripInfo *ginfo;
  time_t secs;

  ginfo=(GripInfo *)gtk_object_get_user_data(GTK_OBJECT(app));

  if(ginfo->ffwding) FastFwd(ginfo);
  if(ginfo->rewinding) Rewind(ginfo);

  secs=time(NULL);

  /* Make sure we don't mod by zero */
  if(!ginfo->poll_interval)
    ginfo->poll_interval=1;

  if(ginfo->ripping|ginfo->encoding) UpdateRipProgress(ginfo);

  if(!ginfo->ripping_a_disc) {
    if(ginfo->poll_drive && !(secs%ginfo->poll_interval)) {
      if(!ginfo->have_disc)
	CheckNewDisc(ginfo,FALSE);
    }

    UpdateDisplay(ginfo);
  }
  
  UpdateTray(ginfo);
}

void Busy(GripGUI *uinfo)
{
  gdk_window_set_cursor(uinfo->app->window,uinfo->wait_cursor);

  UpdateGTK();
}

void UnBusy(GripGUI *uinfo)
{
  gdk_window_set_cursor(uinfo->app->window,NULL);

  UpdateGTK();
}

static void DoLoadConfig(GripInfo *ginfo)
{
  GripGUI *uinfo=&(ginfo->gui_info);
  char filename[256];
  char renamefile[256];
  char *proxy_env,*tok;
  char outputdir[256];
  int confret;
  CFGEntry cfg_entries[]={
    CFG_ENTRIES
    {"outputdir",CFG_ENTRY_STRING,256,outputdir},
    {"",CFG_ENTRY_LAST,0,NULL}
  };
  gchar *email_client;

  outputdir[0]='\0';

  uinfo->minimized=FALSE;
  uinfo->volvis=FALSE;
  uinfo->track_prog_visible=FALSE;
  uinfo->track_edit_visible=FALSE;

  uinfo->wait_cursor=gdk_cursor_new(GDK_WATCH);

  uinfo->tray_icon=NULL;

  uinfo->id3_genre_item_list=NULL;

  *ginfo->version='\0';

  strcpy(ginfo->cd_device,"/dev/cdrom");
  *ginfo->force_scsi='\0';

  ginfo->local_mode=FALSE;
  ginfo->have_disc=FALSE;
  ginfo->tray_open=FALSE;
  ginfo->faulty_eject=FALSE;
  ginfo->looking_up=FALSE;
  ginfo->play_mode=PM_NORMAL;
  ginfo->playloop=TRUE;
  ginfo->automatic_reshuffle=TRUE;
  ginfo->ask_submit=FALSE;
  ginfo->is_new_disc=FALSE;
  ginfo->first_time=TRUE;
  ginfo->automatic_discdb=TRUE;
  ginfo->auto_eject_countdown=0;
  ginfo->current_discid=0;
  ginfo->volume=255;
#if defined(__FreeBSD__) || defined(__NetBSD__)
  ginfo->poll_drive=FALSE;
  ginfo->poll_interval=15;
#else
  ginfo->poll_drive=TRUE;
  ginfo->poll_interval=1;
#endif

  ginfo->changer_slots=0;
  ginfo->current_disc=0;

  ginfo->proxy_server.name[0]='\0';
  ginfo->proxy_server.port=8000;
  ginfo->use_proxy=FALSE;
  ginfo->use_proxy_env=FALSE;

  strcpy(ginfo->dbserver.name,"freedb.freedb.org");
  strcpy(ginfo->dbserver.cgi_prog,"~cddb/cddb.cgi");
  ginfo->dbserver.port=80;
  ginfo->dbserver.use_proxy=0;
  ginfo->dbserver.proxy=&(ginfo->proxy_server);

  strcpy(ginfo->dbserver2.name,"");
  strcpy(ginfo->dbserver2.cgi_prog,"~cddb/cddb.cgi");
  ginfo->dbserver2.port=80;
  ginfo->dbserver2.use_proxy=0;
  ginfo->dbserver2.proxy=&(ginfo->proxy_server);

  strcpy(ginfo->discdb_submit_email,"freedb-submit@freedb.org");

  //load the email client from gconf
  email_client = GetDefaultEmailClient();

  strcpy(ginfo->submit_email_program, email_client);

  ginfo->db_use_freedb=TRUE;
  *ginfo->user_email='\0';

  strcpy(ginfo->discdb_encoding,"UTF-8");
  strcpy(ginfo->id3_encoding,"UTF-8");
  strcpy(ginfo->id3v2_encoding,"UTF-8");

  ginfo->local_mode=FALSE;
  ginfo->update_required=FALSE;
  ginfo->looking_up=FALSE;
  ginfo->ask_submit=FALSE;
  ginfo->is_new_disc=FALSE;
  ginfo->automatic_discdb=TRUE;
  ginfo->play_first=TRUE;
  ginfo->play_on_insert=FALSE;
  ginfo->stop_first=FALSE;
  ginfo->no_interrupt=FALSE;
  ginfo->playing=FALSE;
  ginfo->stopped=FALSE;
  ginfo->ffwding=FALSE;
  ginfo->rewinding=FALSE;

  strcpy(ginfo->title_split_chars,"/");

  ginfo->curr_pipe_fd=-1;

  ginfo->num_cpu=1;
  ginfo->ripping=FALSE;
  ginfo->ripping_a_disc=FALSE;
  ginfo->encoding=FALSE;
  ginfo->rip_partial=FALSE;
  ginfo->stop_rip=FALSE;
  ginfo->stop_encode=FALSE;
  ginfo->rip_finished=0;
  ginfo->num_wavs=0;
  ginfo->doencode=FALSE;
  ginfo->encode_list=NULL;
  ginfo->pending_list=NULL;
  ginfo->delayed_encoding = 0;
  ginfo->do_redirect=TRUE;
  ginfo->selected_ripper=0;
#ifdef CDPAR
  ginfo->stop_thread_rip_now=FALSE;
  ginfo->using_builtin_cdp=TRUE;
  ginfo->disable_paranoia=FALSE;
  ginfo->disable_extra_paranoia=FALSE;
  ginfo->disable_scratch_detect=FALSE;
  ginfo->disable_scratch_repair=FALSE;
  ginfo->calc_gain=FALSE;
#else
  ginfo->using_builtin_cdp=FALSE;
#endif
  ginfo->in_rip_thread=FALSE;
  strcpy(ginfo->ripfileformat,"~/mp3/%A/%d/%n.wav");
#ifdef __linux__
  FindExeInPath("cdparanoia", ginfo->ripexename, sizeof(ginfo->ripexename));
  strcpy(ginfo->ripcmdline,"-d %c %t:[.%s]-%t:[.%e] %w");
#else
  FindExeInPath("cdda2wav", ginfo->ripexename, sizeof(ginfo->ripexename));
#ifdef __sun__
  strcpy(ginfo->ripcmdline,"-x -H -t %t -O wav %w");
#else
  strcpy(ginfo->ripcmdline,"-D %C -x -H -t %t -O wav %w");
#endif /* not sun */
#endif /* not linux */

  ginfo->ripnice=0;
  ginfo->max_wavs=99;
  ginfo->auto_rip=FALSE;
  ginfo->beep_after_rip=TRUE;
  ginfo->eject_after_rip=TRUE;
  ginfo->eject_delay=0;
  ginfo->delay_before_rip=FALSE;
  ginfo->stop_between_tracks=FALSE;
  *ginfo->wav_filter_cmd='\0';
  *ginfo->disc_filter_cmd='\0';
  ginfo->selected_encoder=1;
  strcpy(ginfo->mp3cmdline,"-h -b %b %w %m");
  FindExeInPath("lame", ginfo->mp3exename, sizeof(ginfo->mp3exename));
  strcpy(ginfo->mp3fileformat,"~/mp3/%A/%d/%n.%x");
  strcpy(ginfo->mp3extension,"mp3");
  ginfo->mp3nice=0;
  *ginfo->mp3_filter_cmd='\0';
  ginfo->delete_wavs=TRUE;
  ginfo->add_to_db=FALSE;
  ginfo->add_m3u=TRUE;
  ginfo->rel_m3u=TRUE;
  strcpy(ginfo->m3ufileformat,"~/mp3/%A-%d.m3u");
  ginfo->kbits_per_sec=128;
  ginfo->edit_num_cpu=1;
  ginfo->doid3=TRUE;
  ginfo->doid3=FALSE;
  ginfo->tag_mp3_only=TRUE;
  strcpy(ginfo->id3_comment,_("Created by Grip"));
  *ginfo->cdupdate='\0';
  ginfo->sprefs.no_lower_case=FALSE;
  ginfo->sprefs.allow_high_bits=FALSE;
  ginfo->sprefs.escape=FALSE;
  ginfo->sprefs.no_underscore=FALSE;
  *ginfo->sprefs.allow_these_chars='\0';
  ginfo->show_tray_icon=TRUE;

  sprintf(filename,"%s/%s",getenv("HOME"),ginfo->config_filename);

  confret=LoadConfig(filename,"GRIP",2,2,cfg_entries);

  if(confret<0) {
    /* Check if the config is out of date */
    if(confret==-2) {
      gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                        _("Your config file is out of date -- "
                          "resetting to defaults.\n"
                          "You will need to re-configure Grip.\n"
                          "Your old config file has been saved with -old appended."));

      sprintf(renamefile,"%s-old",filename);

      rename(filename,renamefile);
    }

    DoSaveConfig(ginfo);
  }

  LoadRipperConfig(ginfo,ginfo->selected_ripper);
  LoadEncoderConfig(ginfo,ginfo->selected_encoder);

#ifndef GRIPCD
  /* Phase out 'outputdir' variable */

  if(*outputdir) {
    strcpy(filename,outputdir);
    MakePath(filename);
    strcat(filename,ginfo->mp3fileformat);
    strcpy(ginfo->mp3fileformat,filename);

    strcpy(filename,outputdir);
    MakePath(filename);
    strcat(filename,ginfo->ripfileformat);
    strcpy(ginfo->ripfileformat,filename);

    *outputdir='\0';
  }
#endif

  ginfo->dbserver2.use_proxy=ginfo->dbserver.use_proxy=ginfo->use_proxy;
  ginfo->dbserver2.proxy=ginfo->dbserver.proxy;

  ginfo->num_cpu=ginfo->edit_num_cpu;

  if(!*ginfo->user_email) {
    char *host;
    char *user;

    host = getenv("HOST");
    if(!host)
      host = getenv("HOSTNAME");
    if(!host)
      host = "localhost";

    user = getenv("USER");
    if(!user) 
      user = getenv("USERNAME");
    if(!user)
      user = "user";

    g_snprintf(ginfo->user_email,256,"%s@%s",user,host);
  }

  if(ginfo->use_proxy_env) {   /* Get proxy info from "http_proxy" */
    proxy_env=getenv("http_proxy");

    if(proxy_env) {
      
      /* Skip the "http://" if it's present */
      
      if(!strncasecmp(proxy_env,"http://",7)) proxy_env+=7;
      
      tok=strtok(proxy_env,":");
      if(tok) strncpy(ginfo->proxy_server.name,tok,256);
      
      tok=strtok(NULL,"/");
      if(tok) ginfo->proxy_server.port=atoi(tok);
      
      Debug(_("server is %s, port %d\n"),ginfo->proxy_server.name,
	    ginfo->proxy_server.port);
    }
  }
}

void DoSaveConfig(GripInfo *ginfo)
{
  char filename[256];
  GripGUI *uinfo=&(ginfo->gui_info);
  CFGEntry cfg_entries[]={
    CFG_ENTRIES
    {"",CFG_ENTRY_LAST,0,NULL}
  };

  if(ginfo->edit_num_cpu>MAX_NUM_CPU) ginfo->edit_num_cpu=MAX_NUM_CPU;

  g_snprintf(filename,256,"%s/%s",getenv("HOME"),ginfo->config_filename);

  if(!SaveConfig(filename,"GRIP",2,cfg_entries))
    gnome_app_warning((GnomeApp *)ginfo->gui_info.app,
                      _("Error: Unable to save config file."));

  SaveRipperConfig(ginfo,ginfo->selected_ripper);
  SaveEncoderConfig(ginfo,ginfo->selected_encoder);
}

/* Shut down stuff (generally before an exec) */
void CloseStuff(void *user_data)
{
  GripInfo *ginfo;
  int fd;

  ginfo=(GripInfo *)user_data;

  close(ConnectionNumber(GDK_DISPLAY()));
  close(ginfo->disc.cd_desc);

  fd=open("/dev/null",O_RDWR);
  dup2(fd,0);

  if(ginfo->do_redirect) {
    if(ginfo->curr_pipe_fd>0) {
      dup2(ginfo->curr_pipe_fd,1);
      dup2(ginfo->curr_pipe_fd,2);

      ginfo->curr_pipe_fd=-1;
    }
    else {
      dup2(fd,1);
      dup2(fd,2);
    }
  }

  /* Close any other filehandles that might be around */
  for(fd=3;fd<NOFILE;fd++) {
    close(fd);
  }
}

gchar *GetDefaultEmailClient()
{
  GConfClient *client;
  gchar *value;
  GError *error = NULL;

  client = gconf_client_get_default();
  gconf_client_add_dir(client, 
		       "/desktop/gnome/url-handlers/mailto",
		       GCONF_CLIENT_PRELOAD_NONE,
		       NULL);

  value = gconf_client_get_string(client,
			  "/desktop/gnome/url-handlers/mailto/command",
			  &error);

  g_object_unref(client);

  return value;
}
