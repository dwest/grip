/* rip.c
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

#include "grip.h"
#include <sys/stat.h>
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#elif defined (HAVE_SYS_VFS_H)
#include <sys/vfs.h>
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include "rip.h"
#include "dialog.h"
#include "cdplay.h"
#include "cddev.h"
#include "gripcfg.h"
#include "launch.h"
#include "grip_id3.h"
#include "config.h"
#include "common.h"
#ifdef CDPAR
#include "gain_analysis.h"
#include "cdpar.h"
extern int rip_smile_level;
#endif

static void RipPartialChanged(GtkWidget *widget,gpointer data);
static void PlaySegmentCB(GtkWidget *widget,gpointer data);
static GtkWidget *MakeRangeSelects(GripInfo *ginfo);
static void AddSQLEntry(GripInfo *ginfo,EncodeTrack *enc_track);
static void DBScan(GtkWidget *widget,gpointer data);
static char *MakeRelative(char *file1,char *file2);
static gboolean AddM3U(GripInfo *ginfo);
static void ID3Add(GripInfo *ginfo,char *file,EncodeTrack *enc_track);
static void DoWavFilter(GripInfo *ginfo);
static void DoDiscFilter(GripInfo *ginfo);
static void RipIsFinished(GripInfo *ginfo,gboolean aborted);
static void CheckDupNames(GripInfo *ginfo);
static void RipWholeCD(gpointer data);
static int NextTrackToRip(GripInfo *ginfo);
static gboolean RipNextTrack(GripInfo *ginfo);
#ifdef CDPAR
static void ThreadRip(void *arg);
#endif
static void AddToEncode(GripInfo *ginfo,int track);
static gboolean MP3Encode(GripInfo *ginfo);
static void CalculateAll(GripInfo *ginfo);
static size_t CalculateEncSize(GripInfo *ginfo, int track);
static size_t CalculateWavSize(GripInfo *ginfo, int track);

void MakeRipPage(GripInfo *ginfo)
{
  GripGUI *uinfo;
  GtkWidget *rippage;
  GtkWidget *rangesel;
  GtkWidget *vbox,*vbox2,*hbox,*hbox2;
  GtkWidget *button;
  GtkWidget *hsep;
  GtkWidget *check;
  GtkWidget *partial_rip_frame;
  int mycpu;
  int label_width;
  PangoLayout *layout;

  uinfo=&(ginfo->gui_info);

  rippage=MakeNewPage(uinfo->notebook,_("Rip"));

  vbox=gtk_vbox_new(FALSE,2);
  gtk_container_border_width(GTK_CONTAINER(vbox),3);

  hbox=gtk_hbox_new(FALSE,5);

  vbox2=gtk_vbox_new(FALSE,0);

  button=gtk_button_new_with_label(_("Rip+Encode"));
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Rip and encode selected tracks"),NULL);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoRipEncode),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(vbox2),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Rip Only"));
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Rip but do not encode selected tracks"),NULL);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DoRip),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(vbox2),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Abort Rip and Encode"));
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Kill all active rip and encode processes"),NULL);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(KillRip),(gpointer)ginfo);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(KillEncode),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(vbox2),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("Abort Ripping Only"));
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Kill rip process"),NULL);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(KillRip),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(vbox2),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  button=gtk_button_new_with_label(_("DDJ Scan"));
  gtk_tooltips_set_tip(MakeToolTip(),button,
		       _("Insert disc information into the DigitalDJ database"),
		       NULL);
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
		     GTK_SIGNAL_FUNC(DBScan),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(vbox2),button,FALSE,FALSE,0);
  gtk_widget_show(button);

  gtk_box_pack_start(GTK_BOX(hbox),vbox2,TRUE,TRUE,0);
  gtk_widget_show(vbox2);

  partial_rip_frame=gtk_frame_new(NULL);

  vbox2=gtk_vbox_new(FALSE,0);
  gtk_container_border_width(GTK_CONTAINER(vbox2),3);

  check=MakeCheckButton(NULL,&ginfo->rip_partial,_("Rip partial track"));
  gtk_signal_connect(GTK_OBJECT(check),"clicked",
  		     GTK_SIGNAL_FUNC(RipPartialChanged),(gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(vbox2),check,FALSE,FALSE,0);
  gtk_widget_show(check);

  uinfo->partial_rip_box=gtk_vbox_new(FALSE,0);
  gtk_widget_set_sensitive(uinfo->partial_rip_box,ginfo->rip_partial);

  hbox2=gtk_hbox_new(FALSE,5);

  button=gtk_button_new_with_label(_("Play"));
  gtk_signal_connect(GTK_OBJECT(button),"clicked",
  		     GTK_SIGNAL_FUNC(PlaySegmentCB),
  		     (gpointer)ginfo);
  gtk_box_pack_start(GTK_BOX(hbox2),button,TRUE,TRUE,0);
  gtk_widget_show(button);

  uinfo->play_sector_label=gtk_label_new(_("Current sector:      0"));
  gtk_box_pack_start(GTK_BOX(hbox2),uinfo->play_sector_label,FALSE,FALSE,0);
  gtk_widget_show(uinfo->play_sector_label);

  gtk_box_pack_start(GTK_BOX(uinfo->partial_rip_box),hbox2,FALSE,FALSE,0);
  gtk_widget_show(hbox2);

  rangesel=MakeRangeSelects(ginfo);
  gtk_box_pack_start(GTK_BOX(uinfo->partial_rip_box),rangesel,FALSE,FALSE,0);
  gtk_widget_show(rangesel);

  gtk_box_pack_start(GTK_BOX(vbox2),uinfo->partial_rip_box,TRUE,TRUE,0);
  gtk_widget_show(uinfo->partial_rip_box);

  gtk_container_add(GTK_CONTAINER(partial_rip_frame),vbox2);
  gtk_widget_show(vbox2);

  gtk_box_pack_start(GTK_BOX(hbox),partial_rip_frame,TRUE,TRUE,0);
  gtk_widget_show(partial_rip_frame);

  gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,0);
  gtk_widget_show(hbox);

  hsep=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox),hsep,TRUE,TRUE,0);
  gtk_widget_show(hsep);

  vbox2=gtk_vbox_new(FALSE,0);

  hbox=gtk_hbox_new(FALSE,3);

  uinfo->rip_prog_label=gtk_label_new(_("Rip: Idle"));

  /* This should be the largest this string can get */

  layout=gtk_widget_create_pango_layout(GTK_WIDGET(uinfo->app),
					_("Enc: Trk 99 (99.9x)"));


  pango_layout_get_size(layout,&label_width,NULL);

  label_width/=PANGO_SCALE;

  g_object_unref(layout);


  gtk_widget_set_usize(uinfo->rip_prog_label,label_width,0);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->rip_prog_label,FALSE,FALSE,0);
  gtk_label_set(GTK_LABEL(uinfo->rip_prog_label),_("Rip: Idle"));
  gtk_widget_show(uinfo->rip_prog_label);

  uinfo->ripprogbar=gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->ripprogbar,FALSE,FALSE,0);
  gtk_widget_show(uinfo->ripprogbar);

  uinfo->smile_indicator=NewBlankPixmap(uinfo->app);
  gtk_tooltips_set_tip(MakeToolTip(),uinfo->smile_indicator,
		       _("Rip status"),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->smile_indicator,FALSE,FALSE,0);
  gtk_widget_show(uinfo->smile_indicator);

  gtk_box_pack_start(GTK_BOX(vbox2),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);

  for(mycpu=0;mycpu<ginfo->num_cpu;mycpu++){
    hbox=gtk_hbox_new(FALSE,3);

    uinfo->mp3_prog_label[mycpu]=gtk_label_new(_("Enc: Idle"));
    gtk_widget_set_usize(uinfo->mp3_prog_label[mycpu],label_width,0);

    gtk_box_pack_start(GTK_BOX(hbox),uinfo->mp3_prog_label[mycpu],
		       FALSE,FALSE,0);
    gtk_widget_show(uinfo->mp3_prog_label[mycpu]);

    uinfo->mp3progbar[mycpu]=gtk_progress_bar_new();

    gtk_box_pack_start(GTK_BOX(hbox),uinfo->mp3progbar[mycpu],FALSE,FALSE,0);
    gtk_widget_show(uinfo->mp3progbar[mycpu]);

    gtk_box_pack_start(GTK_BOX(vbox2),hbox,FALSE,FALSE,0);
    gtk_widget_show(hbox);
  }
  
  gtk_box_pack_start(GTK_BOX(vbox),vbox2,TRUE,TRUE,0);
  gtk_widget_show(vbox2);

  hsep=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox),hsep,TRUE,TRUE,0);
  gtk_widget_show(hsep);
  
  vbox2=gtk_vbox_new(FALSE,0);
  uinfo->all_label=gtk_label_new(_("Overall indicators:"));
  gtk_box_pack_start(GTK_BOX(vbox2),uinfo->all_label,FALSE,FALSE,0);
  gtk_widget_show(uinfo->all_label);
  
  hbox=gtk_hbox_new(FALSE,2);
  uinfo->all_rip_label=gtk_label_new(_("Rip: Idle"));
  gtk_widget_set_usize(uinfo->all_rip_label,label_width,0);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->all_rip_label,FALSE,FALSE,0);
  gtk_widget_show(uinfo->all_rip_label);
  
  uinfo->all_ripprogbar=gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->all_ripprogbar,FALSE,FALSE,0);
  gtk_widget_show(uinfo->all_ripprogbar);

  gtk_box_pack_start(GTK_BOX(vbox2),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);
 
  hbox=gtk_hbox_new(FALSE,2);
  uinfo->all_enc_label=gtk_label_new(_("Enc: Idle"));
  gtk_widget_set_usize(uinfo->all_enc_label,label_width,0);
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->all_enc_label,FALSE,FALSE,0);
  gtk_widget_show(uinfo->all_enc_label);

  uinfo->all_encprogbar=gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(hbox),uinfo->all_encprogbar,FALSE,FALSE,0);
  gtk_widget_show(uinfo->all_encprogbar);
  
  gtk_box_pack_start(GTK_BOX(vbox2),hbox,FALSE,FALSE,0);
  gtk_widget_show(hbox);
  
  gtk_box_pack_start(GTK_BOX(vbox),vbox2,TRUE,TRUE,0);
  gtk_widget_show(vbox2);

  gtk_container_add(GTK_CONTAINER(rippage),vbox);
  gtk_widget_show(vbox);
}

static void RipPartialChanged(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  gtk_widget_set_sensitive(ginfo->gui_info.partial_rip_box,ginfo->rip_partial);
}

static void PlaySegmentCB(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  PlaySegment(ginfo,CURRENT_TRACK);
}

static GtkWidget *MakeRangeSelects(GripInfo *ginfo)
{
  GtkWidget *vbox;
  GtkWidget *numentry;

  vbox=gtk_vbox_new(FALSE,0);

  numentry=MakeNumEntry(&(ginfo->gui_info.start_sector_entry),
			&ginfo->start_sector,_("Start sector"),10);
  gtk_box_pack_start(GTK_BOX(vbox),numentry,FALSE,FALSE,0);
  gtk_widget_show(numentry);
  
  numentry=MakeNumEntry(&(ginfo->gui_info.end_sector_entry),
			&ginfo->end_sector,_("End sector"),10);
  gtk_box_pack_start(GTK_BOX(vbox),numentry,FALSE,FALSE,0);
  gtk_widget_show(numentry);
  
  return vbox;
}

static void DBScan(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  int track;
  EncodeTrack enc_track;
  GString *str;

  ginfo=(GripInfo *)data;

  if(!ginfo->have_disc) return;

  for(track=0;track<ginfo->disc.num_tracks;track++) {
    FillInTrackInfo(ginfo,track,&enc_track);
    
    str=g_string_new(NULL);
    
    TranslateString(ginfo->mp3fileformat,str,TranslateSwitch,
		    &enc_track,TRUE,&(ginfo->sprefs));
    
    g_snprintf(enc_track.mp3_filename,256,"%s",str->str);
    g_string_free(str,TRUE);
    
    AddSQLEntry(ginfo,&enc_track);
  }
}

static void AddSQLEntry(GripInfo *ginfo,EncodeTrack *enc_track)
{
  int sqlpid;
  char track_str[4];
  char frame_str[11];
  char length_str[11];
  char playtime_str[6];
  char year_str[5];

  g_snprintf(track_str,4,"%d",enc_track->track_num);
  g_snprintf(frame_str,11,"%d",enc_track->start_frame);
  g_snprintf(length_str,11,"%d",enc_track->end_frame-enc_track->start_frame);
  g_snprintf(playtime_str,6,"%d:%d",enc_track->mins,enc_track->secs);
  g_snprintf(year_str,5,"%d",enc_track->song_year);

  LogStatus(ginfo,_("Inserting track %d into the ddj database\n"),
	    enc_track->track_num);

  sqlpid=fork();

  if(sqlpid==0) {
    CloseStuff(ginfo);

    if(*enc_track->song_artist)
      execlp("mp3insert","mp3insert",
	    "-p",enc_track->mp3_filename,
	    "-a",enc_track->disc_artist,
	    "-i",enc_track->song_artist,
	    "-t",enc_track->song_name,"-d",enc_track->disc_name,
	    "-g",ID3GenreString(enc_track->id3_genre),"-y",year_str,
	    "-n",track_str,
	    "-f",frame_str,"-l",length_str,"-m",playtime_str,NULL);
    else
      execlp("mp3insert","mp3insert",
	    "-p",enc_track->mp3_filename,
	    "-a",enc_track->disc_artist,
	    "-t",enc_track->song_name,"-d",enc_track->disc_name,
	    "-g",ID3GenreString(enc_track->id3_genre),"-y",year_str,
	    "-n",track_str,
	    "-f",frame_str,"-l",length_str,"-m",playtime_str,NULL);
    
    _exit(0);
  }

  waitpid(sqlpid,NULL,0);
}

gboolean IsDir(char *path)
{
  struct stat mystat;

  if(stat(path,&mystat)!=0) return FALSE;

  return S_ISDIR(mystat.st_mode);
}

unsigned long long BytesLeftInFS(char *path)
{
  unsigned long long bytesleft;
  int pos;
#ifdef HAVE_SYS_STATVFS_H
  struct statvfs stat;
#else
 struct statfs stat;
#endif

  if(!IsDir(path)) {
    for(pos=strlen(path);pos&&(path[pos]!='/');pos--);
    
    if(path[pos]!='/') return 0;
    
    path[pos]='\0';

#ifdef HAVE_SYS_STATVFS_H
    if(statvfs(path,&stat)!=0) return 0;
#else
    if(statfs(path,&stat)!=0) return 0;
#endif

    path[pos]='/';
  }
  else
#ifdef HAVE_SYS_STATVFS_H
    if(statvfs(path,&stat)!=0) return 0;
#else
    if(statfs(path,&stat)!=0) return 0;
#endif

  bytesleft=stat.f_bavail;
  bytesleft*=stat.f_bsize;

  return bytesleft;
}

/* Find the root filename of a path */
char *FindRoot(char *str)
{
  char *c;

  for(c=str+strlen(str);c>str;c--) {
    if(*c=='/') return c+1;
  }

  return c;
}

/* Check if a user has write access to a path */
gboolean CanWrite(char *path)
{
  char *c;
  gboolean can_write=FALSE;

  /* First find the filename part, if any */
  for(c=path+strlen(path);c>path;c--) {
    if(*c=='/') break;
  }

  /* This is kinda clumsy -- temporarily hack the string to get only the
   path part */
  if(c!=path) {
    *c='\0';
  }

  if(!access(path,W_OK)) {
    can_write=TRUE;
  }

  /* Put back the '/' */
  if(c!=path) {
    *c='/';
  }

  return can_write;
}

void MakeDirs(char *path)
{
  char dir[256];
  char *s;
  int len;

  for(len=0,s=path;*s;s++,len++) {
    if(*s=='/') {
      strncpy(dir,path,len);
      dir[len]='\0';

      if(!FileExists(dir))
	mkdir(dir,0777);
    }
  }
}

char *MakePath(char *str)
{
  int len;

  len=strlen(str)-1;

  if(str[len]!='/') {
    str[len+1]='/';
    str[len+2]='\0';
  }

  return str;
}

/* Make file1 relative to file2 */
static char *MakeRelative(char *file1,char *file2)
{
  int pos, pos2=0, slashcnt, i;
  char *rel=file1;
  char tem[PATH_MAX]="";

  slashcnt=0;

  /* This part finds relative names assuming m3u is not branched in a
     different directory from mp3 */
  for(pos=0;file2[pos];pos++) {
    if(pos&&(file2[pos]=='/')) {
      if(!strncmp(file1,file2,pos)) {
	rel=file1+pos+1;
	pos2=pos;
      }
    }
  }

  /* Now check to see if the m3u file branches to a different directory. */
  for(pos2=pos2+1;file2[pos2];pos2++) {
    if(file2[pos2]=='/'){             
      slashcnt++;
    }
  } 

  /* Now add correct number of "../"s to make the path relative */
  for(i=0;i<slashcnt;i++) {
    strcpy(tem,"../");
    strncat(tem,rel,strlen(rel));
    strcpy(rel,tem);
  }

  return rel; 
}

static gboolean AddM3U(GripInfo *ginfo)
{
  int i;
  EncodeTrack enc_track;
  FILE *fp;
  char tmp[PATH_MAX];
  char m3unam[PATH_MAX];
  char *relnam;
  GString *str;
  char *conv_str;
  gsize rb,wb;
  GtkWidget *dialog;

  if(!ginfo->have_disc) return FALSE;
  
  str=g_string_new(NULL);

  /* Use track 0 to fill in M3u switches */
  FillInTrackInfo(ginfo,0,&enc_track);

  TranslateString(ginfo->m3ufileformat,str,TranslateSwitch,
		    &enc_track,TRUE,&(ginfo->sprefs));

  conv_str=g_filename_from_utf8(str->str,strlen(str->str),&rb,&wb,NULL);

  if(!conv_str)
    conv_str=g_strdup(str->str);

  g_snprintf(m3unam,PATH_MAX,"%s",conv_str);

  MakeDirs(conv_str);

  fp=fopen(conv_str, "w");
  if(fp==NULL) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_WARNING, 
                                      GTK_BUTTONS_OK, 
                                      _("Error: can't open m3u file."));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      return FALSE;
  }
  g_free(conv_str);
  
  for(i=0;i<ginfo->disc.num_tracks;i++) {
    /* Only add to the m3u if the track is selected for ripping */
    if(TrackIsChecked(&(ginfo->gui_info),i)) {
      g_string_truncate(str,0);

      FillInTrackInfo(ginfo,i,&enc_track);
      TranslateString(ginfo->mp3fileformat,str,TranslateSwitch,
		      &enc_track,TRUE,&(ginfo->sprefs));

      conv_str=g_filename_from_utf8(str->str,strlen(str->str),&rb,&wb,NULL);

      if(!conv_str)
    	conv_str=g_strdup(str->str);

      if(ginfo->rel_m3u) {
	g_snprintf(tmp,PATH_MAX,"%s",conv_str);
	relnam=MakeRelative(tmp,m3unam);
	fprintf(fp,"%s\n",relnam);
      }
      else 
	fprintf(fp,"%s\n",conv_str);
      g_free(conv_str);
    }
  }

  g_string_free(str,TRUE);
  
  fclose(fp);

  return TRUE;
}

void KillRip(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  int track;

  Debug(_("In KillRip\n"));
  ginfo=(GripInfo *)data;

  if(!ginfo->ripping_a_disc) return;

  ginfo->all_ripsize=0;
  ginfo->all_ripdone=0;
  ginfo->all_riplast=0;

  ginfo->stop_rip=TRUE;
  ginfo->ripping_a_disc=FALSE;

  if(ginfo->ripping) {
    /* Need to decrement num_wavs since we didn't finish ripping
       the current track */
    if(ginfo->doencode && ginfo->num_wavs>0)
      ginfo->num_wavs--;
    
    /* Need to decrement all_mp3size */
    for (track=0;track<ginfo->disc.num_tracks;++track) {
      if ((!IsDataTrack(&(ginfo->disc),track)) &&
	  (TrackIsChecked(&(ginfo->gui_info),track))) {
	ginfo->all_encsize-=CalculateEncSize(ginfo,track);
      }
    }

    Debug(_("Now total enc size is: %d\n"),ginfo->all_encsize);
    
    if(ginfo->using_builtin_cdp) {
#ifdef CDPAR
      ginfo->stop_thread_rip_now=TRUE;
#endif
    }
    else kill(ginfo->rippid,SIGKILL);
  }
}

void KillEncode(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  int mycpu;
  GList *elist;
  EncodeTrack *enc_track;

  ginfo=(GripInfo *)data;

  if(!ginfo->encoding) return;

  ginfo->stop_encode=TRUE;
  ginfo->num_wavs=0;
  ginfo->all_encsize=0;
  ginfo->all_encdone=0;

  for(mycpu=0;mycpu<ginfo->num_cpu;mycpu++){
    if(ginfo->encoding&(1<<mycpu)) kill(ginfo->mp3pid[mycpu],SIGKILL);
      ginfo->all_enclast[mycpu]=0;
  }
  
  elist=ginfo->encode_list;
  
  /* Remove all entries in the encode list */
  while(elist) {
    enc_track=(EncodeTrack *)elist->data;
    elist=elist->next;
    
    ginfo->encode_list=g_list_remove(elist,enc_track);
    g_free(enc_track);
  }
}

static void ID3Add(GripInfo *ginfo,char *file,EncodeTrack *enc_track)
{
  char year[5];
  GString *comment;
  int len;

  /* If we only want to tag mp3 files, look for the correct extension */
  if(ginfo->tag_mp3_only) {
    len=strlen(file);

    if(len<4 || strcasecmp(file+(len-4),".mp3")) {
      return;
    }
  }

  comment=g_string_new(NULL);
  TranslateString(ginfo->id3_comment,comment,TranslateSwitch,enc_track,
		  FALSE,&(ginfo->sprefs));

  g_snprintf(year,5,"%d",enc_track->song_year);

  /* If we've got id3lib, we have the option of doing v2 tags */
#ifdef HAVE_ID3V2
  if(ginfo->doid3v2) {
    ID3v2TagFile(file,(*(enc_track->song_name))?enc_track->song_name:"Unknown",
		 (*(enc_track->song_artist))?enc_track->song_artist:
		 (*(enc_track->disc_artist))?enc_track->disc_artist:"Unknown",
		 (*(enc_track->disc_name))?enc_track->disc_name:"Unknown",
		 year,comment->str,enc_track->id3_genre,
		 enc_track->track_num+1,ginfo->id3v2_encoding);
  }
#endif
  if(ginfo->doid3) {
    ID3v1TagFile(file,(*(enc_track->song_name))?enc_track->song_name:"Unknown",
		 (*(enc_track->song_artist))?enc_track->song_artist:
		 (*(enc_track->disc_artist))?enc_track->disc_artist:"Unknown",
		 (*(enc_track->disc_name))?enc_track->disc_name:"Unknown",
		 year,comment->str,enc_track->id3_genre,
		 enc_track->track_num+1,ginfo->id3_encoding);
  }

  g_string_free(comment,TRUE);
}

static void DoWavFilter(GripInfo *ginfo)
{
  EncodeTrack enc_track;

  FillInTrackInfo(ginfo,ginfo->rip_track,&enc_track);
  strcpy(enc_track.wav_filename,ginfo->ripfile);

  TranslateAndLaunch(ginfo->wav_filter_cmd,TranslateSwitch,&enc_track,
		     FALSE,&(ginfo->sprefs),CloseStuff,(void *)ginfo);
}

static void DoDiscFilter(GripInfo *ginfo)
{
  EncodeTrack enc_track;

  FillInTrackInfo(ginfo,ginfo->rip_track,&enc_track);
  strcpy(enc_track.wav_filename,ginfo->ripfile);

  TranslateAndLaunch(ginfo->disc_filter_cmd,TranslateSwitch,&enc_track,
		     FALSE,&(ginfo->sprefs),CloseStuff,(void *)ginfo);
}

void UpdateRipProgress(GripInfo *ginfo)
{
  GripGUI *uinfo;
  struct stat mystat;
  int quarter;
  gfloat percent=0;
  int mycpu;
  char buf[PATH_MAX];
  time_t now;
  gfloat elapsed=0;
  gfloat speed;
  gboolean result=FALSE;
  char *conv_str;
  gsize rb,wb;
  
  uinfo=&(ginfo->gui_info);

  if(ginfo->ripping) {
    if(stat(ginfo->ripfile,&mystat)>=0) {
      percent=(gfloat)mystat.st_size/(gfloat)ginfo->ripsize;
      if(percent>1.0) percent=1.0;
    }
    else {
      percent=0;
    }
   
   	ginfo->rip_percent = percent;
   
    gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->ripprogbar),percent);

    now = time(NULL);
    elapsed = (gfloat)now - (gfloat)ginfo->rip_started;

    /* 1x is 44100*2*2 = 176400 bytes/sec */
    if(elapsed < 0.1f) /* 1/10 sec. */
      speed=0.0f; /* Avoid divide-by-0 at start */ 
    else
      speed=(gfloat)(mystat.st_size)/(176400.0f*elapsed);
    
    /* startup */
    if(speed >= 50.0f) {
      speed = 0.0f;
      ginfo->rip_started = now;
    }
    
    sprintf(buf,_("Rip: Trk %d (%3.1fx)"),ginfo->rip_track+1,speed);
	
    gtk_label_set(GTK_LABEL(uinfo->rip_prog_label),buf);

    quarter=(int)(percent*4.0);
	
    if(quarter<4)
      CopyPixmap(GTK_PIXMAP(uinfo->rip_pix[quarter]),
		 GTK_PIXMAP(uinfo->rip_indicator));

#ifdef CDPAR
    if(ginfo->using_builtin_cdp) {
      if(uinfo->minimized)
	CopyPixmap(GTK_PIXMAP(uinfo->smile_pix[ginfo->rip_smile_level]),
		   GTK_PIXMAP(uinfo->lcd_smile_indicator));
      else
	CopyPixmap(GTK_PIXMAP(uinfo->smile_pix[ginfo->rip_smile_level]),
		   GTK_PIXMAP(uinfo->smile_indicator));
    }
#endif
    
    /* Overall rip */
    if(ginfo->rip_started!=now && !ginfo->rip_partial && ginfo->ripping 
       && !ginfo->stop_rip) {
      ginfo->all_ripdone+=mystat.st_size-ginfo->all_riplast;
      ginfo->all_riplast=mystat.st_size;
      percent=(gfloat)(ginfo->all_ripdone)/(gfloat)(ginfo->all_ripsize);
      
      if(percent>1.0)
	percent=0.0;
      
      ginfo->rip_tot_percent = percent;
      
      sprintf(buf,_("Rip: %6.2f%%"),percent*100.0);
      gtk_label_set(GTK_LABEL(uinfo->all_rip_label),buf);
      gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->all_ripprogbar),percent);
    } else if (ginfo->stop_rip) {
      gtk_label_set(GTK_LABEL(uinfo->all_rip_label),_("Rip: Idle"));
	 
      gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->all_ripprogbar),0.0);
    }

    /* Check if a rip finished */
    if((ginfo->using_builtin_cdp&&!ginfo->in_rip_thread) ||
       (!ginfo->using_builtin_cdp&&waitpid(ginfo->rippid,NULL,WNOHANG))) {
      if(!ginfo->using_builtin_cdp) waitpid(ginfo->rippid,NULL,0);
      else {
	CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
		   GTK_PIXMAP(uinfo->lcd_smile_indicator));
	CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
		   GTK_PIXMAP(uinfo->smile_indicator));
      }

      LogStatus(ginfo,_("Rip finished\n"));
      ginfo->all_riplast=0;
      ginfo->ripping=FALSE;
      SetChecked(uinfo,ginfo->rip_track,FALSE);

#ifdef CDPAR
      /* Get the title gain */
      if(ginfo->using_builtin_cdp && ginfo->calc_gain) {
	ginfo->track_gain_adjustment=GetTitleGain();
      }
#endif

      /* Do filtering of .wav file */

      if(*ginfo->wav_filter_cmd) DoWavFilter(ginfo);

      gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->ripprogbar),0.0);
      CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
		 GTK_PIXMAP(uinfo->rip_indicator));

      if(!ginfo->stop_rip) {
	if(ginfo->doencode) {
	  AddToEncode(ginfo,ginfo->rip_track);
	  MP3Encode(ginfo);
	}

	Debug(_("Rip partial %d  num wavs %d\n"),ginfo->rip_partial,
	      ginfo->num_wavs);

	Debug(_("Next track is %d, total is %d\n"),
	      NextTrackToRip(ginfo),ginfo->disc.num_tracks);

	if(!ginfo->rip_partial&&
	   (ginfo->num_wavs<ginfo->max_wavs||
	    NextTrackToRip(ginfo)==ginfo->disc.num_tracks)) {
	  Debug(_("Check if we need to rip another track\n"));
	  if(!RipNextTrack(ginfo)) RipIsFinished(ginfo,FALSE);
	  else { gtk_label_set(GTK_LABEL(uinfo->rip_prog_label),_("Rip: Idle"));  }
	}
	else { gtk_label_set(GTK_LABEL(uinfo->rip_prog_label),_("Rip: Idle"));  }
      }
      else {
	RipIsFinished(ginfo,TRUE);
      }
    }
  }
  else {
    if(ginfo->stop_rip) {
      RipIsFinished(ginfo,TRUE);
    }
  }
  
  /* Check if an encode finished */
  for(mycpu=0;mycpu<ginfo->num_cpu;mycpu++){
    if(ginfo->encoding&(1<<mycpu)) {
      if(stat(ginfo->mp3file[mycpu],&mystat)>=0) {
        percent=(gfloat)mystat.st_size/(gfloat)ginfo->mp3size[mycpu];
        if(percent>1.0) percent=1.0;
      }
      else {
        percent=0;
      } 

	  ginfo->enc_percent = percent;
	
      gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->mp3progbar[mycpu]),
			      percent);
       
      now = time(NULL);
      elapsed = (gfloat)now - (gfloat)ginfo->mp3_started[mycpu];

      if(elapsed < 0.1f) /* 1/10 sec. */
	speed=0.0f;
      else
	speed=(gfloat)mystat.st_size/
	  ((gfloat)ginfo->kbits_per_sec * 128.0f * elapsed);
      
      sprintf(buf,_("Enc: Trk %d (%3.1fx)"),
	      ginfo->mp3_enc_track[mycpu]+1,speed);
	  
      gtk_label_set(GTK_LABEL(uinfo->mp3_prog_label[mycpu]),buf);
 
      quarter=(int)(percent*4.0);
 
      if(quarter<4)
        CopyPixmap(GTK_PIXMAP(uinfo->mp3_pix[quarter]),
		   GTK_PIXMAP(uinfo->mp3_indicator[mycpu]));
      if (!ginfo->rip_partial && !ginfo->stop_encode && 
	  now!=ginfo->mp3_started[mycpu]) {
	ginfo->all_encdone+=mystat.st_size-ginfo->all_enclast[mycpu];
	ginfo->all_enclast[mycpu]=mystat.st_size;
	percent=(gfloat)(ginfo->all_encdone)/(gfloat)(ginfo->all_encsize);
	if (percent>1.0)
	  percent=1.0;
	ginfo->enc_tot_percent = percent;
	sprintf(buf,_("Enc: %6.2f%%"),percent*100.0);
	gtk_label_set(GTK_LABEL(uinfo->all_enc_label),buf);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->all_encprogbar),
				percent);
      }
      
      if(waitpid(ginfo->mp3pid[mycpu],NULL,WNOHANG)) {
        waitpid(ginfo->mp3pid[mycpu],NULL,0);
        ginfo->encoding&=~(1<<mycpu);

	LogStatus(ginfo,_("Finished encoding on cpu %d\n"),mycpu);
	ginfo->all_enclast[mycpu]=0;
        gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->mp3progbar[mycpu]),
				0.0);
        CopyPixmap(GTK_PIXMAP(uinfo->empty_image),
		   GTK_PIXMAP(uinfo->mp3_indicator[mycpu]));

        if(ginfo->doencode) ginfo->num_wavs--;

        if(!ginfo->stop_encode) {
	  if(ginfo->delete_wavs) {
            conv_str=g_filename_to_utf8(ginfo->rip_delete_file[mycpu],
                                        strlen(ginfo->rip_delete_file[mycpu]),
                                        &rb,&wb,NULL);

            if(!conv_str)
              conv_str=g_strdup(ginfo->rip_delete_file[mycpu]);

	    LogStatus(ginfo,_("Deleting [%s]\n"),conv_str,&rb,&wb,NULL);

            g_free(conv_str);

	    unlink(ginfo->rip_delete_file[mycpu]);
	  }

          if(ginfo->doid3 || ginfo->doid3v2)
	    ID3Add(ginfo,ginfo->mp3file[mycpu],
		   ginfo->encoded_track[mycpu]);

	  if(ginfo->add_to_db) AddSQLEntry(ginfo,ginfo->encoded_track[mycpu]);

	  if(*ginfo->mp3_filter_cmd)
	    TranslateAndLaunch(ginfo->mp3_filter_cmd,TranslateSwitch,
			       ginfo->encoded_track[mycpu],FALSE,
			       &(ginfo->sprefs),CloseStuff,(void *)ginfo);


          if(ginfo->ripping_a_disc&&!ginfo->rip_partial&&
	     !ginfo->ripping&&ginfo->num_wavs<ginfo->max_wavs) {
	    if(RipNextTrack(ginfo)) ginfo->doencode=TRUE;
	    else RipIsFinished(ginfo,FALSE);
          }

	  g_free(ginfo->encoded_track[mycpu]);
         
	  if(!ginfo->rip_partial&&ginfo->encode_list) {
	    MP3Encode(ginfo);
	  }
        }
	else ginfo->stop_encode=FALSE;

        if(!(ginfo->encoding&(1<<mycpu))) {
	  gtk_label_set(GTK_LABEL(uinfo->mp3_prog_label[mycpu]),
			_("Enc: Idle"));
 	    }
      }
    }  
  }
  /* Check if we have any encoding process (now or in future) */
  for (mycpu=0;mycpu<ginfo->num_cpu;++mycpu) {
    if (ginfo->encoding & (1<<mycpu)) {
      result=TRUE;
      break;
    }
  }
  if ((!result || ginfo->stop_encode) && 
      !ginfo->encode_list && !ginfo->ripping) {
    gtk_label_set(GTK_LABEL(uinfo->all_enc_label),_("Enc: Idle"));
    gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->all_encprogbar),0.0);
  }
}

static void RipIsFinished(GripInfo *ginfo,gboolean aborted)
{
  GripGUI *uinfo;
  LogStatus(ginfo,_("Ripping is finished\n"));

  uinfo=&(ginfo->gui_info);
  ginfo->all_ripsize=0;
  ginfo->all_ripdone=0;
  ginfo->all_riplast=0;

  gtk_label_set(GTK_LABEL(uinfo->rip_prog_label),_("Rip: Idle"));
  gtk_label_set(GTK_LABEL(uinfo->all_rip_label),_("Rip: Idle"));
  gtk_progress_bar_update(GTK_PROGRESS_BAR(uinfo->all_ripprogbar),0.0);

  ginfo->stop_rip=FALSE;
  ginfo->ripping=FALSE;
  ginfo->ripping_a_disc=FALSE;
  ginfo->rip_finished=time(NULL);

  /* Re-open the cdrom device if it was closed */
  if(ginfo->disc.cd_desc<0) CDInitDevice(ginfo->disc.devname,&(ginfo->disc));

  /* Do post-rip stuff only if we weren't explicitly aborted */
  if(!aborted) {
    if(ginfo->beep_after_rip) printf("%c%c",7,7);
  
#ifdef CDPAR
    if(ginfo->using_builtin_cdp && ginfo->calc_gain) {
      ginfo->disc_gain_adjustment=GetAlbumGain();
    }
#endif
  
    if(*ginfo->disc_filter_cmd)
      DoDiscFilter(ginfo);

    if(ginfo->delayed_encoding) {
      ginfo->encode_list = g_list_concat(ginfo->encode_list,
                                         ginfo->pending_list);
      ginfo->pending_list = NULL;
      
      /* Start an encoder on all free CPUs
       * This is really only useful the first time through */
      while (MP3Encode(ginfo));
    }
    
    if(ginfo->eject_after_rip) {
      /* Reset rip_finished since we're ejecting */
      ginfo->rip_finished=0;

      EjectDisc(NULL,ginfo);
      
      if(ginfo->eject_delay) ginfo->auto_eject_countdown=ginfo->eject_delay;
    }
  }
}

char *TranslateSwitch(char switch_char,void *data,gboolean *munge)
{
  static char res[PATH_MAX];
  EncodeTrack *enc_track;

  enc_track=(EncodeTrack *)data;

  switch(switch_char) {
  case 'b':
    g_snprintf(res,PATH_MAX,"%d",enc_track->ginfo->kbits_per_sec);
    *munge=FALSE;
    break;
  case 'c':
    g_snprintf(res,PATH_MAX,"%s",enc_track->ginfo->cd_device);
    *munge=FALSE;
    break;
  case 'C':
    if(*enc_track->ginfo->force_scsi) {
      g_snprintf(res,PATH_MAX,"%s",enc_track->ginfo->force_scsi);
    }
    else {
      g_snprintf(res,PATH_MAX,"%s",enc_track->ginfo->cd_device);
    }
    *munge=FALSE;
    break;
  case 'w':
    g_snprintf(res,PATH_MAX,"%s",enc_track->wav_filename);
    *munge=FALSE;
    break;
  case 'm':
    g_snprintf(res,PATH_MAX,"%s",enc_track->mp3_filename);
    *munge=FALSE;
    break;
  case 't':
    g_snprintf(res,PATH_MAX,"%02d",enc_track->track_num+1);
    *munge=FALSE;
    break;
  case 's':
    g_snprintf(res,PATH_MAX,"%d",enc_track->ginfo->start_sector);
    *munge=FALSE;
    break;
  case 'e':
    g_snprintf(res,PATH_MAX,"%d",enc_track->ginfo->end_sector);
    *munge=FALSE;
    break;
  case 'n':
    if(*(enc_track->song_name))
      g_snprintf(res,PATH_MAX,"%s",enc_track->song_name);
    else g_snprintf(res,PATH_MAX,"Track%02d",enc_track->track_num+1);
    break;
  case 'a':
    if(*(enc_track->song_artist))
      g_snprintf(res,PATH_MAX,"%s",enc_track->song_artist);
    else {
      if(*(enc_track->disc_artist))
	g_snprintf(res,PATH_MAX,"%s",enc_track->disc_artist);
      else strncpy(res,_("NoArtist"),PATH_MAX);
    }
    break;
  case 'A':
    if(*(enc_track->disc_artist))
      g_snprintf(res,PATH_MAX,"%s",enc_track->disc_artist);
    else strncpy(res,_("NoArtist"),PATH_MAX);	
    break;
  case 'd':
    if(*(enc_track->disc_name))
      g_snprintf(res,PATH_MAX,"%s",enc_track->disc_name);
    else strncpy(res,_("NoTitle"),PATH_MAX);
    break;
  case 'i':
    g_snprintf(res,PATH_MAX,"%08x",enc_track->discid);
    *munge=FALSE;
    break;
  case 'y':
    g_snprintf(res,PATH_MAX,"%d",enc_track->song_year);
    *munge=FALSE;
    break;
  case 'g':
    g_snprintf(res,PATH_MAX,"%d",enc_track->id3_genre);
    *munge=FALSE;
    break;
  case 'G':
    g_snprintf(res,PATH_MAX,"%s",ID3GenreString(enc_track->id3_genre));
    break;
#ifdef CDPAR
  case 'r':
    g_snprintf(res,PATH_MAX,"%+6.2f",enc_track->track_gain_adjustment);
    *munge=FALSE;
    break;
  case 'R':
    g_snprintf(res,PATH_MAX,"%+6.2f",enc_track->disc_gain_adjustment);
    *munge=FALSE;
    break;
#endif
  case 'x':
    g_snprintf(res,PATH_MAX,"%s",enc_track->ginfo->mp3extension);
    *munge=FALSE;
    break;
  default:
    *res='\0';
    break;
  }

  return res;
}


static void CheckDupNames(GripInfo *ginfo)
{
  int track,track2;
  int numdups[MAX_TRACKS];
  int count;
  char buf[256];

  for(track=0;track<ginfo->disc.num_tracks;track++)
    numdups[track]=0;

  for(track=0;track<(ginfo->disc.num_tracks-1);track++) {
    if(!numdups[track]) {
      count=0;

      for(track2=track+1;track2<ginfo->disc.num_tracks;track2++) {
	if(!strcmp(ginfo->ddata.data_track[track].track_name,
		   ginfo->ddata.data_track[track2].track_name))
	  numdups[track2]=++count;
      }
    }
  }

  for(track=0;track<ginfo->disc.num_tracks;track++) {
    if(numdups[track]) {
      g_snprintf(buf,260,"%s (%d)",ginfo->ddata.data_track[track].track_name,
		 numdups[track]+1);

      strcpy(ginfo->ddata.data_track[track].track_name,buf);
    }
  }
}

void DoRipEncode(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;

  ginfo=(GripInfo *)data;

  DoRip(NULL,(gpointer)ginfo);
}

void DoRip(GtkWidget *widget,gpointer data)
{
  GripInfo *ginfo;
  gboolean result;
  GtkWidget *dialog;
  gint response;

  ginfo=(GripInfo *)data;

  if(!ginfo->have_disc) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_WARNING, 
                                      GTK_BUTTONS_OK, 
                                      _("No disc was detected in the drive. If you have a disc in your drive, please check your CDRom device setting under Config->CD."));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      return;
  }

  if(widget) ginfo->doencode=FALSE;
  else ginfo->doencode=TRUE;

  if(!ginfo->using_builtin_cdp&&!FileExists(ginfo->ripexename)) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_WARNING, 
                                      GTK_BUTTONS_OK, 
                                      _("Invalid rip executable.\nCheck your rip config, and ensure it specifies the full path to the ripper executable."));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      ginfo->doencode=FALSE;
      return;
  }

  if(ginfo->doencode&&!FileExists(ginfo->mp3exename)) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_WARNING, 
                                      GTK_BUTTONS_OK, 
                                      _("Invalid encoder executable.\nCheck your encoder config, and ensure it specifies the full path to the encoder executable."));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      ginfo->doencode=FALSE;
      return;
  }

  CDStop(&(ginfo->disc));
  ginfo->stopped=TRUE;

  /* Close the device so as not to conflict with ripping */
  CDCloseDevice(&(ginfo->disc));
    
  if(ginfo->ripping) {
    ginfo->doencode=FALSE;
    return;
  }

#ifdef CDPAR
  /* Initialize gain calculation */
  if(ginfo->using_builtin_cdp && ginfo->calc_gain) 
    InitGainAnalysis(44100);
#endif

  CheckDupNames(ginfo);

  if(ginfo->rip_partial)
    ginfo->rip_track=CURRENT_TRACK;
  else {
    if(ginfo->add_m3u) AddM3U(ginfo);
    SetCurrentTrackIndex(ginfo,0);
    ginfo->rip_track=0;
  }

  if(NextTrackToRip(ginfo)==ginfo->disc.num_tracks) {
      Debug(_("No tracks selected.\n"));
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_QUESTION,
                                      GTK_BUTTONS_YES_NO,
                                      _("No tracks selected.\nRip whole CD?\n"));
      response = gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      if(response == GTK_RESPONSE_YES){
          RipWholeCD(ginfo);
          return;
      }else{
          return;
      }
  }
  
  ginfo->stop_rip=FALSE;

  CalculateAll(ginfo);

  result=RipNextTrack(ginfo);

  if(!result) {
    ginfo->doencode=FALSE;
  }
}

static void RipWholeCD(gpointer data)
{
  int track;
  GripInfo *ginfo;

  Debug(_("Ripping whole CD\n"));

  ginfo=(GripInfo *)data;
  
  for(track=0;track<ginfo->disc.num_tracks;++track)
    SetChecked(&(ginfo->gui_info),track,TRUE);

  if(ginfo->doencode) DoRip(NULL,(gpointer)ginfo);
  else DoRip((GtkWidget *)1,(gpointer)ginfo);
}

static int NextTrackToRip(GripInfo *ginfo)
{
  int track;

  for(track=0;(track<ginfo->disc.num_tracks)&&
	(!TrackIsChecked(&(ginfo->gui_info),track)||
	IsDataTrack(&(ginfo->disc),track));track++);

  return track;
}

static gboolean RipNextTrack(GripInfo *ginfo)
{
  GripGUI *uinfo;
  char tmp[PATH_MAX];
  int arg;
  GString *args[100];
  char *char_args[101];
  unsigned long long bytesleft;
  struct stat mystat;
  GString *str;
  EncodeTrack enc_track;
  char *conv_str, *utf8_ripfile;
  gsize rb,wb;
  const char *charset;
  GtkWidget *dialog;

  uinfo=&(ginfo->gui_info);

  Debug(_("In RipNextTrack\n"));

  if(ginfo->ripping) return FALSE;

  if(!ginfo->rip_partial)
    ginfo->rip_track=NextTrackToRip(ginfo);

  Debug(_("First checked track is %d\n"),ginfo->rip_track+1);

  /* See if we are finished ripping */
  if(ginfo->rip_track==ginfo->disc.num_tracks) {
    return FALSE;
  }

  /* We have a track to rip */

  if(ginfo->have_disc&&ginfo->rip_track>=0) {
    Debug(_("Ripping away!\n"));

    CopyPixmap(GTK_PIXMAP(uinfo->rip_pix[0]),GTK_PIXMAP(uinfo->rip_indicator));

    if(ginfo->stop_between_tracks)
      CDStop(&(ginfo->disc));

    if(!ginfo->rip_partial) {
      ginfo->start_sector=0;
      ginfo->end_sector=(ginfo->disc.track[ginfo->rip_track+1].start_frame-1)-
	ginfo->disc.track[ginfo->rip_track].start_frame;

      /* Compensate for the gap before a data track */
      if((ginfo->rip_track<(ginfo->disc.num_tracks-1)&&
	  IsDataTrack(&(ginfo->disc),ginfo->rip_track+1)&&
	  (ginfo->end_sector-ginfo->start_sector)>11399))
	ginfo->end_sector-=11400;
    }

    ginfo->ripsize=44+((ginfo->end_sector-ginfo->start_sector)+1)*2352;

    str=g_string_new(NULL);
    FillInTrackInfo(ginfo,ginfo->rip_track,&enc_track);

    TranslateString(ginfo->ripfileformat,str,TranslateSwitch,
		    &enc_track,TRUE,&(ginfo->sprefs));

    g_get_charset(&charset);

    conv_str=g_filename_from_utf8(str->str,strlen(str->str),&rb,&wb,NULL);

    if(!conv_str) {
      conv_str=g_strdup(str->str);
    }

    g_snprintf(ginfo->ripfile,256,"%s",conv_str);

    g_free(conv_str);
    g_string_free(str,TRUE);

    MakeDirs(ginfo->ripfile);
    if(!CanWrite(ginfo->ripfile)) {
        Debug(_("No write access to write wave file.\n"));
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("No write access to write wave file.\n"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }

    /* Workaround for drives that spin up slowly */
    if(ginfo->delay_before_rip) {
      sleep(5);
    }

    utf8_ripfile=g_filename_to_utf8(ginfo->ripfile,strlen(ginfo->ripfile),
                                    &rb,&wb,NULL);

    if(!utf8_ripfile) utf8_ripfile=strdup(ginfo->ripfile);

    LogStatus(ginfo,_("Ripping track %d to %s\n"),
	      ginfo->rip_track+1,utf8_ripfile);

    ginfo->rip_started = time(NULL);
    sprintf(tmp,_("Rip: Trk %d (0.0x)"),ginfo->rip_track+1);
    gtk_label_set(GTK_LABEL(uinfo->rip_prog_label),tmp);

    if(stat(ginfo->ripfile,&mystat)>=0) {
      if(mystat.st_size == ginfo->ripsize) { 
	LogStatus(ginfo,_("File %s has already been ripped. Skipping...\n"),\
	          utf8_ripfile);

        g_free(utf8_ripfile);

	if(ginfo->doencode) ginfo->num_wavs++;
	ginfo->ripping=TRUE;
	ginfo->ripping_a_disc=TRUE;
	ginfo->all_ripdone+=CalculateWavSize(ginfo,ginfo->rip_track);
	ginfo->all_riplast=0;
	return TRUE;
      }
      else unlink(ginfo->ripfile);
    }

    g_free(utf8_ripfile);

    bytesleft=BytesLeftInFS(ginfo->ripfile);

    if(bytesleft<(ginfo->ripsize*1.5)) {
        Debug(_("Out of space in output directory.\n"));
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Out of space in output directory.\n"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }

#ifdef CDPAR
    if(ginfo->selected_ripper==0) {
      ginfo->in_rip_thread=TRUE;

      pthread_create(&ginfo->cdp_thread,NULL,(void *)ThreadRip,
		     (void *)ginfo);
      pthread_detach(ginfo->cdp_thread);
    }
    else {
#endif
      strcpy(enc_track.wav_filename,ginfo->ripfile);

      MakeTranslatedArgs(ginfo->ripcmdline,args,100,TranslateSwitch,
			 &enc_track,FALSE,&(ginfo->sprefs));

/*
      ArgsToLocale(args);
*/

      for(arg=0;args[arg];arg++) {
	char_args[arg+1]=args[arg]->str;
      }

      char_args[arg+1]=NULL;

      char_args[0]=FindRoot(ginfo->ripexename);

      ginfo->curr_pipe_fd=
	GetStatusWindowPipe(ginfo->gui_info.rip_status_window);

      ginfo->rippid=fork();
      
      if(ginfo->rippid==0) {
	CloseStuff(ginfo);
	nice(ginfo->ripnice);
	execv(ginfo->ripexename,char_args);
	
	LogStatus(ginfo,_("Exec failed\n"));
	_exit(0);
      }
      else {
	ginfo->curr_pipe_fd=-1;
      }

      for(arg=0;args[arg];arg++) {
	g_string_free(args[arg],TRUE);
      }
#ifdef CDPAR
    }
#endif

    ginfo->ripping=TRUE;
    ginfo->ripping_a_disc=TRUE;

    if(ginfo->doencode) ginfo->num_wavs++;

    return TRUE;
  }
  else return FALSE;
}

#ifdef CDPAR
static void ThreadRip(void *arg)
{
  GripInfo *ginfo;
  int paranoia_mode;
  int dup_output_fd;
  FILE *output_fp;

  ginfo=(GripInfo *)arg;

  Debug(_("Calling CDPRip\n"));

  paranoia_mode=PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP; 
  if(ginfo->disable_paranoia)
    paranoia_mode=PARANOIA_MODE_DISABLE;
  else if(ginfo->disable_extra_paranoia) {
    paranoia_mode|=PARANOIA_MODE_OVERLAP;
    paranoia_mode&=~PARANOIA_MODE_VERIFY;
  }
  if(ginfo->disable_scratch_detect)
    paranoia_mode&=
      ~(PARANOIA_MODE_SCRATCH|PARANOIA_MODE_REPAIR);
  if(ginfo->disable_scratch_repair) 
    paranoia_mode&=~PARANOIA_MODE_REPAIR;
  
  ginfo->rip_smile_level=0;
  
  nice(ginfo->ripnice);

  dup_output_fd=dup(GetStatusWindowPipe(ginfo->gui_info.rip_status_window));
  output_fp=fdopen(dup_output_fd,"w");
  setlinebuf(output_fp);

  CDPRip(ginfo->cd_device,ginfo->force_scsi,ginfo->rip_track+1,
	 ginfo->start_sector,
	 ginfo->end_sector,ginfo->ripfile,paranoia_mode,
	 &(ginfo->rip_smile_level),&(ginfo->rip_percent_done),
	 &(ginfo->stop_thread_rip_now),ginfo->calc_gain,
	 output_fp);

  fclose(output_fp);
 
  ginfo->in_rip_thread=FALSE;

  pthread_exit(0);
}
#endif /* ifdef CDPAR */

void FillInTrackInfo(GripInfo *ginfo,int track,EncodeTrack *new_track)
{
  new_track->ginfo=ginfo;

  new_track->wav_filename[0]='\0';
  new_track->mp3_filename[0]='\0';

  new_track->track_num=track;
  new_track->start_frame=ginfo->disc.track[track].start_frame;
  new_track->end_frame=ginfo->disc.track[track+1].start_frame-1;
#ifdef CDPAR
  new_track->track_gain_adjustment=ginfo->track_gain_adjustment;
  new_track->disc_gain_adjustment=ginfo->disc_gain_adjustment;
#endif

  /* Compensate for the gap before a data track */
  if((track<(ginfo->disc.num_tracks-1)&&
      IsDataTrack(&(ginfo->disc),track+1)&&
      (new_track->end_frame-new_track->start_frame)>11399))
    new_track->end_frame-=11400;

  new_track->mins=ginfo->disc.track[track].length.mins;
  new_track->secs=ginfo->disc.track[track].length.secs;
  new_track->song_year=ginfo->ddata.data_year;
  g_snprintf(new_track->song_name,256,"%s",
	     ginfo->ddata.data_track[track].track_name);
  g_snprintf(new_track->song_artist,256,"%s",
	     ginfo->ddata.data_track[track].track_artist);
  g_snprintf(new_track->disc_name,256,"%s",ginfo->ddata.data_title);
  g_snprintf(new_track->disc_artist,256,"%s",ginfo->ddata.data_artist);
  new_track->id3_genre=ginfo->ddata.data_id3genre;
  new_track->discid=ginfo->ddata.data_id;
}

static void AddToEncode(GripInfo *ginfo,int track)
{
  EncodeTrack *new_track;

  new_track=(EncodeTrack *)g_new(EncodeTrack,1);

  FillInTrackInfo(ginfo,track,new_track);
  strcpy(new_track->wav_filename,ginfo->ripfile);

  if (!ginfo->delayed_encoding)
    ginfo->encode_list=g_list_append(ginfo->encode_list,new_track);
  else
    ginfo->pending_list=g_list_append(ginfo->pending_list,new_track);

  Debug(_("Added track %d to %s list\n"),track+1,
          ginfo->delayed_encoding ? "pending" : "encoding");
}

static gboolean MP3Encode(GripInfo *ginfo)
{
  GripGUI *uinfo;
  char tmp[PATH_MAX];
  int arg;
  GString *args[100];
  char *char_args[101];
  unsigned long long bytesleft;
  EncodeTrack *enc_track;
  GString *str;
  int encode_track;
  int cpu;
  char *conv_str;
  gsize rb,wb;
  GtkWidget *dialog;

  uinfo=&(ginfo->gui_info);

  if(!ginfo->encode_list) return FALSE;

  for(cpu=0;(cpu<ginfo->num_cpu)&&(ginfo->encoding&(1<<cpu));cpu++);

  if(cpu==ginfo->num_cpu) {
    Debug(_("No free cpus\n"));
    return FALSE;
  }

  enc_track=(EncodeTrack *)(g_list_first(ginfo->encode_list)->data);
  encode_track=enc_track->track_num;

  ginfo->encode_list=g_list_remove(ginfo->encode_list,enc_track);
  ginfo->encoded_track[cpu]=enc_track;

  CopyPixmap(GTK_PIXMAP(uinfo->mp3_pix[0]),
	     GTK_PIXMAP(uinfo->mp3_indicator[cpu]));
  
  ginfo->mp3_started[cpu] = time(NULL);
  ginfo->mp3_enc_track[cpu] = encode_track;

  Debug(_("Enc track %d\n"),encode_track+1);

  strcpy(ginfo->rip_delete_file[cpu],enc_track->wav_filename);

  str=g_string_new(NULL);

  TranslateString(ginfo->mp3fileformat,str,TranslateSwitch,
		  enc_track,TRUE,&(ginfo->sprefs));

  conv_str=g_filename_from_utf8(str->str,strlen(str->str),&rb,&wb,NULL);

  if(!conv_str)
    conv_str=g_strdup(str->str);

  g_snprintf(ginfo->mp3file[cpu],256,"%s",conv_str);

  g_free(conv_str);
  g_string_free(str,TRUE);

  MakeDirs(ginfo->mp3file[cpu]);
  if(!CanWrite(ginfo->mp3file[cpu])) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_ERROR,
                                      GTK_BUTTONS_OK,
                                      _("No write access to write encoded file."));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      return FALSE;
  }
  
  bytesleft=BytesLeftInFS(ginfo->mp3file[cpu]);
  
  conv_str=g_filename_to_utf8(ginfo->mp3file[cpu],strlen(ginfo->mp3file[cpu]),
                              &rb,&wb,NULL);

  if(!conv_str) conv_str=strdup(ginfo->mp3file[cpu]);

  LogStatus(ginfo,_("%i: Encoding to %s\n"),cpu+1,conv_str);

  g_free(conv_str);
  
  sprintf(tmp,_("Enc: Trk %d (0.0x)"),encode_track+1);
  gtk_label_set(GTK_LABEL(uinfo->mp3_prog_label[cpu]),tmp);
  
  unlink(ginfo->mp3file[cpu]);
  
  ginfo->mp3size[cpu]=
    (int)((gfloat)((enc_track->end_frame-enc_track->start_frame)+1)*
	  (gfloat)(ginfo->kbits_per_sec*1024)/600.0);
  
  if(bytesleft<(ginfo->mp3size[cpu]*1.5)) {
        Debug(_("Out of space in output directory.\n"));
        dialog = gtk_message_dialog_new(GTK_WINDOW(ginfo->gui_info.app),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Out of space in output directory.\n"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
  }
  
  strcpy(enc_track->mp3_filename,ginfo->mp3file[cpu]);

  MakeTranslatedArgs(ginfo->mp3cmdline,args,100,TranslateSwitch,
		     enc_track,FALSE,&(ginfo->sprefs));

/*
  ArgsToLocale(args);
*/
  
  for(arg=0;args[arg];arg++) {
    char_args[arg+1]=args[arg]->str;
  }
  
  char_args[arg+1]=NULL;
  
  char_args[0]=FindRoot(ginfo->mp3exename);

  ginfo->curr_pipe_fd=
    GetStatusWindowPipe(ginfo->gui_info.encode_status_window);

  ginfo->mp3pid[cpu]=fork();
  
  if(ginfo->mp3pid[cpu]==0) {
    CloseStuff(ginfo);
    setsid();
    nice(ginfo->mp3nice);
    execv(ginfo->mp3exename,char_args);
    _exit(0);
  }
  else {
    ginfo->curr_pipe_fd=-1;
  }

  for(arg=0;args[arg];arg++) {
    g_string_free(args[arg],TRUE);
  }

  ginfo->encoding|=(1<<cpu);

  return TRUE;
}

void CalculateAll(GripInfo *ginfo)
{
  int cpu;
  int track;
  GripGUI *uinfo;

  uinfo=&(ginfo->gui_info);

  Debug(_("In CalculateAll\n"));

  ginfo->all_ripsize=0;
  ginfo->all_ripdone=0;
  ginfo->all_riplast=0;
  if (!ginfo->encoding) {
    Debug(_("We aren't ripping now, so let's zero encoding values\n"));
    ginfo->all_encsize=0;
    ginfo->all_encdone=0;
    for (cpu=0;cpu<ginfo->num_cpu;++cpu)
      ginfo->all_enclast[cpu]=0;
  }
  if (ginfo->rip_partial)
    return;
  
  for (track=0;track<ginfo->disc.num_tracks;++track) {
    if (!IsDataTrack(&(ginfo->disc),track) &&
	(TrackIsChecked(uinfo,track))) {
      ginfo->all_ripsize+=CalculateWavSize(ginfo,track);
      ginfo->all_encsize+=CalculateEncSize(ginfo,track);
    }
  }
  Debug(_("Total rip size is: %d\n"),ginfo->all_ripsize);
  Debug(_("Total enc size is: %d\n"),ginfo->all_encsize);
}

static size_t CalculateWavSize(GripInfo *ginfo, int track)
{
  int frames;
  
  frames=(ginfo->disc.track[track+1].start_frame-1)-
    ginfo->disc.track[track].start_frame;
  if ((track<(ginfo->disc.num_tracks)-1) &&
      (IsDataTrack(&(ginfo->disc),track+1)) &&
      (frames>11399))
    frames-=11400;
  return frames*2352;
}

static size_t CalculateEncSize(GripInfo *ginfo, int track)
{
  double tmp_encsize=0.0;
  /* It's not the best way, but i couldn't find anything better */
  tmp_encsize=(double)((ginfo->disc.track[track].length.mins*60+
			ginfo->disc.track[track].length.secs-2)*
		       ginfo->kbits_per_sec*1024/8);
  tmp_encsize-=tmp_encsize*0.0154;
  if (ginfo->add_m3u) 
    tmp_encsize+=128;
  return (size_t)tmp_encsize;
}
