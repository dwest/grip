/* gripcfg.c
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

#include <sys/stat.h>
#include <unistd.h>
#include <gconf/gconf-client.h>
#include "grip.h"
#include "gripcfg.h"
#include "dialog.h"
#include "parsecfg.h"

static void UseProxyChanged(GtkWidget *widget,gpointer data);
static void RipperSelected(GtkWidget *widget,gpointer data);
static void EncoderSelected(GtkWidget *widget,gpointer data);

static Ripper ripper_defaults[]={
#ifdef CDPAR
  {"grip (cdparanoia)",""},
#endif
#if defined(__linux__)
  {"cdparanoia","-d %c %t:[.%s]-%t:[.%e] %w"},
#endif
#if defined(__sun__)
  {"cdda2wav","-x -H -t %t -O wav %w"},
#else
  {"cdda2wav","-D %C -x -H -t %t -O wav %w"},
#endif
  {"other",""},
  {"",""}
};

static MP3Encoder encoder_defaults[]={{"bladeenc","-%b -QUIT %w %m","mp3"},
				      {"lame","-h -b %b %w %m","mp3"},
				      {"l3enc","-br %b %w %m","mp3"},
				      {"xingmp3enc","-B %b -Q %w","mp3"},
				      {"mp3encode","-p 2 -l 3 -b %b %w %m",
				       "mp3"},
				      {"gogo","-b %b %w %m","mp3"},
				      {"oggenc",
				       "-o %m -a %a -l %d -t %n -b %b -N %t -G %G -d %y %w",
				       "ogg"},
				      {"flac","-V -o %m %w","flac"},
				      {"other","",""},
				      {"",""}
};

static CFGEntry encoder_cfg_entries[]={
  {"name",CFG_ENTRY_STRING,256,NULL},
  {"cmdline",CFG_ENTRY_STRING,256,NULL},
  {"exe",CFG_ENTRY_STRING,256,NULL},
  {"extension",CFG_ENTRY_STRING,10,NULL}
};

static void UseProxyChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  ginfo->dbserver2.use_proxy=ginfo->dbserver.use_proxy=ginfo->use_proxy;
}

void MakeConfigPage(GripInfo *ginfo)
{
  GripGUI *uinfo;
  GtkWidget *vbox,*vbox2,*dbvbox;
  GtkWidget *entry;
  GtkWidget *realentry;
  GtkWidget *label;
  GtkWidget *page,*page2;
  GtkWidget *check;
  GtkWidget *notebook;
  GtkWidget *config_notebook;
  GtkWidget *configpage;
  GtkWidget *button;
#ifndef GRIPCD
  GtkWidget *hsep;
  GtkWidget *hbox;
  GtkWidget *menu,*optmenu;
  GtkWidget *item;
  MP3Encoder *enc;
  Ripper *rip;
#endif

  uinfo=&(ginfo->gui_info);

  configpage=MakeNewPage(uinfo->notebook,_("Config"));

  vbox2=gtk_vbox_new(FALSE,0);
  config_notebook=gtk_notebook_new();

  page=gtk_frame_new(NULL);
  vbox=gtk_vbox_new(FALSE,2);

  entry=MakeStrEntry(NULL,ginfo->cd_device,_("CDRom device"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  check=MakeCheckButton(NULL,&ginfo->no_interrupt,
			_("Don't interrupt playback on exit/startup"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);
 
  check=MakeCheckButton(NULL,&ginfo->stop_first,_("Rewind when stopped"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->play_first,
			_("Startup with first track if not playing"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->play_on_insert,
			_("Auto-play on disc insert"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->automatic_reshuffle,
			_("Reshuffle before each playback"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->faulty_eject,
			_("Work around faulty eject"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->poll_drive,
			_("Poll disc drive for new disc"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  entry=MakeNumEntry(NULL,&ginfo->poll_interval,_("Poll interval (seconds)"),
		     3);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  gtk_container_add(GTK_CONTAINER(page),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("CD"));
  gtk_notebook_append_page(GTK_NOTEBOOK(config_notebook),page,label);
  gtk_widget_show(page);

#ifndef GRIPCD
  page=gtk_frame_new(NULL);

  notebook=gtk_notebook_new();

  page2=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,4);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Ripper:"));
  gtk_box_pack_start(GTK_BOX(hbox),label,TRUE,TRUE,0);
  gtk_widget_show(label);

  menu=gtk_menu_new();

  rip=ripper_defaults;

  while(*(rip->name)) {
    item=gtk_menu_item_new_with_label(rip->name);
    gtk_object_set_user_data(GTK_OBJECT(item),(gpointer)rip);
    gtk_signal_connect(GTK_OBJECT(item),"activate",
    		       GTK_SIGNAL_FUNC(RipperSelected),(gpointer)ginfo);
    gtk_menu_append(GTK_MENU(menu),item);
    gtk_widget_show(item);

    rip++;
  }

  /* Make sure the selected ripper is active */
  gtk_menu_set_active(GTK_MENU(menu),ginfo->selected_ripper);

#ifdef CDPAR
  if(ginfo->selected_ripper==0) ginfo->using_builtin_cdp=TRUE;
  else ginfo->using_builtin_cdp=FALSE;
#else
  ginfo->using_builtin_cdp=FALSE;
#endif

  optmenu=gtk_option_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu),menu);
  gtk_widget_show(menu);
  gtk_box_pack_start(GTK_BOX(hbox),optmenu,TRUE,TRUE,0);
  gtk_widget_show(optmenu);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hsep=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox),hsep,FALSE,FALSE,0);
  gtk_widget_show(hsep);

  uinfo->rip_exe_box=gtk_vbox_new(FALSE,2);

  entry=MakeStrEntry(&(uinfo->ripexename_entry),ginfo->ripexename,
		     _("Ripping executable"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(uinfo->rip_exe_box),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(&(uinfo->ripcmdline_entry),ginfo->ripcmdline,
		     _("Rip command-line"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(uinfo->rip_exe_box),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_box_pack_start(GTK_BOX(vbox),uinfo->rip_exe_box,FALSE,FALSE,0);
  if(!ginfo->using_builtin_cdp)
    gtk_widget_show(uinfo->rip_exe_box);

#ifdef CDPAR
  uinfo->rip_builtin_box=gtk_vbox_new(FALSE,2);

  check=MakeCheckButton(NULL,&ginfo->disable_paranoia,_("Disable paranoia"));
  gtk_box_pack_start(GTK_BOX(uinfo->rip_builtin_box),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->disable_extra_paranoia,
			_("Disable extra paranoia"));
  gtk_box_pack_start(GTK_BOX(uinfo->rip_builtin_box),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Disable scratch"));
  gtk_box_pack_start(GTK_BOX(hbox),label,TRUE,TRUE,0);
  gtk_widget_show(label);
  
  check=MakeCheckButton(NULL,&ginfo->disable_scratch_detect,_("detection"));
  gtk_box_pack_start(GTK_BOX(hbox),check,TRUE,TRUE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->disable_scratch_repair,_("repair"));
  gtk_box_pack_start(GTK_BOX(hbox),check,TRUE,TRUE,0);
  gtk_widget_show(check);

  gtk_box_pack_start(GTK_BOX(uinfo->rip_builtin_box),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);
  
  check=MakeCheckButton(NULL,&ginfo->calc_gain,
			_("Calculate gain adjustment"));
  gtk_box_pack_start(GTK_BOX(uinfo->rip_builtin_box),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  gtk_box_pack_start(GTK_BOX(vbox),uinfo->rip_builtin_box,FALSE,FALSE,0);
  if(ginfo->using_builtin_cdp) gtk_widget_show(uinfo->rip_builtin_box);
#endif

  entry=MakeStrEntry(NULL,ginfo->ripfileformat,_("Rip file format"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,ginfo->force_scsi,_("Generic SCSI device"),
		     255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_container_add(GTK_CONTAINER(page2),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Ripper"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page2,label);
  gtk_widget_show(page2);

  page2=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  entry=MakeNumEntry(NULL,&ginfo->ripnice,_("Rip 'nice' value"),3);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  entry=MakeNumEntry(NULL,&ginfo->max_wavs,_("Max non-encoded .wav's"),3);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  hbox=gtk_hbox_new(FALSE,3);

  check=MakeCheckButton(NULL,&ginfo->auto_rip,_("Auto-rip on insert"));
  gtk_box_pack_start(GTK_BOX(hbox),check,TRUE,TRUE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->beep_after_rip,_("Beep after rip"));
  gtk_box_pack_start(GTK_BOX(hbox),check,TRUE,TRUE,0);
  gtk_widget_show(check);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hbox=gtk_hbox_new(FALSE,3);

  check=MakeCheckButton(NULL,&ginfo->eject_after_rip,
			_("Auto-eject after rip"));
  gtk_box_pack_start(GTK_BOX(hbox),check,TRUE,TRUE,0);
  gtk_widget_show(check);

  entry=MakeNumEntry(NULL,&ginfo->eject_delay,_("Auto-eject delay"),3);
  gtk_box_pack_start(GTK_BOX(hbox),entry,TRUE,TRUE,0);
  gtk_widget_show(entry);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  check=MakeCheckButton(NULL,&ginfo->delay_before_rip,
			_("Delay before ripping"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton (NULL, &ginfo->delayed_encoding,
                        _("Delay encoding until disc is ripped"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->stop_between_tracks,
			_("Stop cdrom drive between tracks"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  entry=MakeStrEntry(NULL,ginfo->wav_filter_cmd,_("Wav filter command"),
						  255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  entry=MakeStrEntry(NULL,ginfo->disc_filter_cmd,_("Disc filter command"),
						  255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  gtk_container_add(GTK_CONTAINER(page2),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Options"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page2,label);
  gtk_widget_show(page2);

  gtk_container_add(GTK_CONTAINER(page),notebook);
  gtk_widget_show(notebook);

  label=gtk_label_new(_("Rip"));
  gtk_notebook_append_page(GTK_NOTEBOOK(config_notebook),page,label);
  gtk_widget_show(page);

  page=gtk_frame_new(NULL);

  notebook=gtk_notebook_new();

  page2=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,4);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  hbox=gtk_hbox_new(FALSE,3);

  label=gtk_label_new(_("Encoder:"));
  gtk_box_pack_start(GTK_BOX(hbox),label,TRUE,TRUE,0);
  gtk_widget_show(label);

  menu=gtk_menu_new();

  enc=encoder_defaults;

  while(*(enc->name)) {
    item=gtk_menu_item_new_with_label(enc->name);
    gtk_object_set_user_data(GTK_OBJECT(item),(gpointer)enc);
    gtk_signal_connect(GTK_OBJECT(item),"activate",
    		       GTK_SIGNAL_FUNC(EncoderSelected),(gpointer)ginfo);
    gtk_menu_append(GTK_MENU(menu),item);
    gtk_widget_show(item);

    enc++;
  }

  gtk_menu_set_active(GTK_MENU(menu),ginfo->selected_encoder);

  optmenu=gtk_option_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu),menu);
  gtk_widget_show(menu);
  gtk_box_pack_start(GTK_BOX(hbox),optmenu,TRUE,TRUE,0);
  gtk_widget_show(optmenu);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  hsep=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox),hsep,FALSE,FALSE,0);
  gtk_widget_show(hsep);

  entry=MakeStrEntry(&(uinfo->mp3exename_entry),ginfo->mp3exename,
		     _("Encoder executable"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(&(uinfo->mp3cmdline_entry),ginfo->mp3cmdline,
		     _("Encoder command-line"),
		     255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(&(uinfo->mp3extension_entry),ginfo->mp3extension,
		     _("Encode file extension"),
		     10,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  entry=MakeStrEntry(NULL,ginfo->mp3fileformat,_("Encode file format"),
		     255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_container_add(GTK_CONTAINER(page2),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Encoder"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page2,label);
  gtk_widget_show(page2);

  page2=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  check=MakeCheckButton(NULL,&ginfo->delete_wavs,
			_("Delete .wav after encoding"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->add_to_db,
			_("Insert info into SQL database"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->add_m3u,_("Create .m3u files"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->rel_m3u,
			_("Use relative paths in .m3u files"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  entry=MakeStrEntry(NULL,ginfo->m3ufileformat,_("M3U file format"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeNumEntry(NULL,&ginfo->kbits_per_sec,
		     _("Encoding bitrate (kbits/sec)"),3);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  entry=MakeNumEntry(NULL,&ginfo->edit_num_cpu,_("Number of CPUs to use"),3);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  entry=MakeNumEntry(NULL,&ginfo->mp3nice,_("Encode 'nice' value"),3);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  entry=MakeStrEntry(NULL,ginfo->mp3_filter_cmd,_("Encode filter command"),
		     255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_container_add(GTK_CONTAINER(page2),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Options"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page2,label);
  gtk_widget_show(page2);

  gtk_container_add(GTK_CONTAINER(page),notebook);
  gtk_widget_show(notebook);

  label=gtk_label_new(_("Encode"));
  gtk_notebook_append_page(GTK_NOTEBOOK(config_notebook),page,label);
  gtk_widget_show(page);

  page=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  check=MakeCheckButton(NULL,&ginfo->doid3,_("Add ID3 tags to encoded files"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

#ifdef HAVE_ID3V2
  check=MakeCheckButton(NULL,&ginfo->doid3v2,
			_("Add ID3v2 tags to encoded files"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);
#endif

  check=MakeCheckButton(NULL,&ginfo->tag_mp3_only,
			_("Only tag files ending in '.mp3'"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  entry=MakeStrEntry(NULL,ginfo->id3_comment,_("ID3 comment field"),29,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,ginfo->id3_encoding,
		     _("ID3v1 Character set encoding"),16,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

#ifdef HAVE_ID3V2
  entry=MakeStrEntry(NULL,ginfo->id3v2_encoding,
		     _("ID3v2 Character set encoding"),16,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
#endif

  gtk_container_add(GTK_CONTAINER(page),vbox);
  gtk_widget_show(vbox);

  label = gtk_label_new(_("ID3"));
  gtk_notebook_append_page(GTK_NOTEBOOK(config_notebook),page,label);
  gtk_widget_show(page);
#endif

  page=gtk_frame_new(NULL);

  dbvbox=gtk_vbox_new(FALSE,4);
  gtk_container_border_width(GTK_CONTAINER(dbvbox),3);

  notebook=gtk_notebook_new();

  page2=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  entry=MakeStrEntry(NULL,ginfo->dbserver.name,_("DB server"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,ginfo->dbserver.cgi_prog,_("CGI path"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_container_add(GTK_CONTAINER(page2),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Primary Server"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page2,label);
  gtk_widget_show(page2);

  page2=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  entry=MakeStrEntry(NULL,ginfo->dbserver2.name,_("DB server"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,ginfo->dbserver2.cgi_prog,_("CGI path"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_container_add(GTK_CONTAINER(page2),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Secondary Server"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page2,label);
  gtk_widget_show(page2);


  gtk_box_pack_start(GTK_BOX(dbvbox),notebook,FALSE,FALSE,0);
  gtk_widget_show(notebook);


  entry=MakeStrEntry(NULL,ginfo->discdb_submit_email,
		     _("DB Submit email"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(dbvbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,ginfo->discdb_encoding,
		     _("DB Character set encoding"),16,TRUE);
  gtk_box_pack_start(GTK_BOX(dbvbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,
		     ginfo->submit_email_program,
		     _("Email Client"),
		     255,
		     TRUE);
  gtk_box_pack_start(GTK_BOX(dbvbox),
		     entry,
		     FALSE,
		     FALSE,
		     0);
  gtk_widget_show(entry);

  check=MakeCheckButton(NULL,&ginfo->db_use_freedb,
			_("Use freedb extensions"));
  gtk_box_pack_start(GTK_BOX(dbvbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->automatic_discdb,
			_("Perform disc lookups automatically"));
  gtk_box_pack_start(GTK_BOX(dbvbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  gtk_container_add(GTK_CONTAINER(page),dbvbox);
  gtk_widget_show(dbvbox);


  label=gtk_label_new(_("DiscDB"));
  gtk_notebook_append_page(GTK_NOTEBOOK(config_notebook),page,label);
  gtk_widget_show(page);

  page=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  check=MakeCheckButton(&button,&ginfo->use_proxy,_("Use proxy server"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
  		     GTK_SIGNAL_FUNC(UseProxyChanged),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->use_proxy_env,
			_("Get server from 'http_proxy' env. var"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  entry=MakeStrEntry(NULL,ginfo->proxy_server.name,_("Proxy server"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeNumEntry(NULL,&(ginfo->proxy_server.port),_("Proxy port"),5);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,ginfo->proxy_server.username,_("Proxy username"),
		     80,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(&realentry,ginfo->proxy_server.pswd,
		     _("Proxy password"),80,TRUE);
  gtk_entry_set_visibility(GTK_ENTRY(realentry),FALSE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  gtk_container_add(GTK_CONTAINER(page),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Proxy"));
  gtk_notebook_append_page(GTK_NOTEBOOK(config_notebook),page,label);
  gtk_widget_show(page);

  page=gtk_frame_new(NULL);

  vbox=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  entry=MakeStrEntry(NULL,ginfo->user_email,_("Email address"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  entry=MakeStrEntry(NULL,ginfo->cdupdate,_("CD update program"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);

  check=MakeCheckButton(NULL,&ginfo->sprefs.no_lower_case,
			_("Do not lowercase filenames"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);
   
  check=MakeCheckButton(NULL,&ginfo->sprefs.allow_high_bits,
			_("Allow high bits in filenames"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->sprefs.escape,
			_("Replace incompatible characters by hexadecimal numbers"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  check=MakeCheckButton(NULL,&ginfo->sprefs.no_underscore,
			_("Do not change spaces to underscores"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  entry=MakeStrEntry(NULL,ginfo->sprefs.allow_these_chars,
		     _("Characters to not strip\nin filenames"),255,TRUE);
  gtk_box_pack_start(GTK_BOX(vbox),entry,FALSE,FALSE,0);
  gtk_widget_show(entry);
  
  check=MakeCheckButton(NULL,&ginfo->show_tray_icon,
  			_("Show tray icon"));
  gtk_box_pack_start(GTK_BOX(vbox),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  gtk_container_add(GTK_CONTAINER(page),vbox);
  gtk_widget_show(vbox);

  label=gtk_label_new(_("Misc"));
  gtk_notebook_append_page(GTK_NOTEBOOK(config_notebook),page,label);
  gtk_widget_show(page);

  gtk_box_pack_start(GTK_BOX(vbox2),config_notebook,FALSE,FALSE,0);
  gtk_widget_show(config_notebook);

  gtk_container_add(GTK_CONTAINER(configpage),vbox2);
  gtk_widget_show(vbox2);
}

static void RipperSelected(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;
  Ripper *rip;
  char buf[256];
  int selected_ripper;

  ginfo=(GripInfo *)data;
  uinfo=&(ginfo->gui_info);
  rip=(Ripper *)gtk_object_get_user_data(GTK_OBJECT(widget));

  SaveRipperConfig(ginfo,ginfo->selected_ripper);

  selected_ripper=rip-ripper_defaults;

  /* Don't overwrite if the selection hasn't changed */
  if(ginfo->selected_ripper == selected_ripper) {
    return;
  }

  ginfo->selected_ripper = selected_ripper;

#ifdef CDPAR
  if(ginfo->selected_ripper==0) {
    ginfo->using_builtin_cdp=TRUE;

    gtk_widget_hide(uinfo->rip_exe_box);
    gtk_widget_show(uinfo->rip_builtin_box);
  }
  else {
    ginfo->using_builtin_cdp=FALSE;

    gtk_widget_show(uinfo->rip_exe_box);
    gtk_widget_hide(uinfo->rip_builtin_box);
  }
#endif

  if(!ginfo->using_builtin_cdp) {
    if(LoadRipperConfig(ginfo,ginfo->selected_ripper)) {
      strcpy(buf,ginfo->ripexename);
      gtk_entry_set_text(GTK_ENTRY(uinfo->ripexename_entry),buf);
      strcpy(buf,ginfo->ripcmdline);
      gtk_entry_set_text(GTK_ENTRY(uinfo->ripcmdline_entry),buf);
    }
    else {
      if(strcmp(rip->name,"other")) {
        FindExeInPath(rip->name, buf, sizeof(buf));
        gtk_entry_set_text(GTK_ENTRY(uinfo->ripexename_entry), buf);
      }
      else gtk_entry_set_text(GTK_ENTRY(uinfo->ripexename_entry),"");
      
      gtk_entry_set_text(GTK_ENTRY(uinfo->ripcmdline_entry),rip->cmdline);
    }
  }
}

#define RIP_CFG_ENTRIES \
    {"exe",CFG_ENTRY_STRING,256,ginfo->ripexename},\
    {"cmdline",CFG_ENTRY_STRING,256,ginfo->ripcmdline},\
    {"",CFG_ENTRY_LAST,0,NULL}

gboolean LoadRipperConfig(GripInfo *ginfo,int ripcfg)
{
  char buf[256];
  CFGEntry rip_cfg_entries[]={
    RIP_CFG_ENTRIES
  };

#ifdef CDPAR
  if(ripcfg==0) return;
#endif

  sprintf(buf,"%s/%s-%s",getenv("HOME"),ginfo->config_filename,
          ripper_defaults[ripcfg].name);

  return (LoadConfig(buf,"GRIP",2,2,rip_cfg_entries)==1);
}

void SaveRipperConfig(GripInfo *ginfo,int ripcfg)
{
  char buf[256];
  CFGEntry rip_cfg_entries[]={
    RIP_CFG_ENTRIES
  };
  GtkWidget *dialog;

#ifdef CDPAR
  if(ripcfg==0) return;
#endif

  sprintf(buf,"%s/%s-%s",getenv("HOME"),ginfo->config_filename,
          ripper_defaults[ripcfg].name);

  if(!SaveConfig(buf,"GRIP",2,rip_cfg_entries)){
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_ERROR,
                                      GTK_BUTTONS_OK,
                                      _("Unable to save ripper config."));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
  }
}

static void EncoderSelected(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  GripGUI *uinfo;
  MP3Encoder *enc;
  char buf[256];

  ginfo=(GripInfo *)data;
  uinfo=&(ginfo->gui_info);
  enc=(MP3Encoder *)gtk_object_get_user_data(GTK_OBJECT(widget));

  SaveEncoderConfig(ginfo,ginfo->selected_encoder);

  ginfo->selected_encoder=enc-encoder_defaults;

  if(LoadEncoderConfig(ginfo,ginfo->selected_encoder)) {
    strcpy(buf,ginfo->mp3exename);
    gtk_entry_set_text(GTK_ENTRY(uinfo->mp3exename_entry),buf);

    strcpy(buf,ginfo->mp3cmdline);
    gtk_entry_set_text(GTK_ENTRY(uinfo->mp3cmdline_entry),buf);

    strcpy(buf,ginfo->mp3extension);
    gtk_entry_set_text(GTK_ENTRY(uinfo->mp3extension_entry),buf);    
  }
  else {
    if(strcmp(enc->name,"other")) {
      FindExeInPath(enc->name, buf, sizeof(buf));
      gtk_entry_set_text(GTK_ENTRY(uinfo->mp3exename_entry),buf);
    }
    else gtk_entry_set_text(GTK_ENTRY(uinfo->mp3exename_entry),"");
    
    gtk_entry_set_text(GTK_ENTRY(uinfo->mp3cmdline_entry),enc->cmdline);
    gtk_entry_set_text(GTK_ENTRY(uinfo->mp3extension_entry),enc->extension);
  }
}

#define ENCODE_CFG_ENTRIES \
    {"exe",CFG_ENTRY_STRING,256,ginfo->mp3exename},\
    {"cmdline",CFG_ENTRY_STRING,256,ginfo->mp3cmdline},\
    {"extension",CFG_ENTRY_STRING,10,ginfo->mp3extension},\
    {"",CFG_ENTRY_LAST,0,NULL}

gboolean LoadEncoderConfig(GripInfo *ginfo,int encodecfg)
{
  char buf[256];
  CFGEntry encode_cfg_entries[]={
    ENCODE_CFG_ENTRIES
  };

  sprintf(buf,"%s/%s-%s",getenv("HOME"),ginfo->config_filename,
          encoder_defaults[encodecfg].name);

  return (LoadConfig(buf,"GRIP",2,2,encode_cfg_entries)==1);
}

void SaveEncoderConfig(GripInfo *ginfo,int encodecfg)
{
  char buf[256];
  CFGEntry encode_cfg_entries[]={
    ENCODE_CFG_ENTRIES
  };
  GtkWidget *dialog;

  sprintf(buf,"%s/%s-%s",getenv("HOME"),ginfo->config_filename,
          encoder_defaults[encodecfg].name);

  if(!SaveConfig(buf,"GRIP",2,encode_cfg_entries)){
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_ERROR,
                                      GTK_BUTTONS_OK,
                                      _("Unable to save encoder config."));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
  }
                      
}

void FindExeInPath(char *exename, char *buf, int bsize)
{
  char *path;
  static char **PATH = 0;

  if(!PATH) {
    const char *env = g_getenv("PATH");

    PATH = g_strsplit(env ? env : "/usr/local/bin:/usr/bin:/bin", ":", 0);
  }

  path = FindExe(exename, PATH);

  if(!path) {
    g_snprintf(buf, bsize, "%s", exename);
  }
  else {
    g_snprintf(buf, bsize, "%s/%s", path, exename);
  }
}

char *FindExe(char *exename,char **paths)
{
  char **path;
  char buf[256];

  path=paths;

  while(*path) {
    g_snprintf(buf,256,"%s/%s",*path,exename);

    if(FileExists(buf)) return *path;

    path++;
  }

  return NULL;
}

gboolean FileExists(char *filename)
{
  struct stat mystat;

  return (stat(filename,&mystat)>=0);
}
