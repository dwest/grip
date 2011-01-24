/* rip.h
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

#ifndef GRIP_RIP_H
#define GRIP_RIP_H

/* Encode list structure */

typedef struct _encode_track {
  GripInfo *ginfo;
  int track_num;
  int start_frame;
  int end_frame;
  char song_name[256];
  char song_artist[256];
  char disc_name[256];
  char disc_artist[256];
  char wav_filename[256];
  char mp3_filename[256];
  int song_year;
  int id3_genre;
  int mins;
  int secs;
  int discid;
#ifdef CDPAR
  double track_gain_adjustment;
  double disc_gain_adjustment;
#endif
} EncodeTrack;


void MakeRipPage(GripInfo *ginfo);
gboolean FileExists(char *filename);
gboolean IsDir(char *path);
unsigned long long BytesLeftInFS(char *path);
char *FindExe(char *exename,char **paths);
char *FindRoot(char *str);
gboolean CanWrite(char *path);
void MakeDirs(char *path);
char *MakePath(char *str);
void KillRip(GtkWidget *widget,gpointer data);
void KillEncode(GtkWidget *widget,gpointer data);
void UpdateRipProgress(GripInfo *ginfo);
char *TranslateSwitch(char switch_char,void *data,gboolean *munge);
void DoRipEncode(GtkWidget *widget,gpointer data);
void DoRip(GtkWidget *widget,gpointer data);
void FillInTrackInfo(GripInfo *ginfo,int track,EncodeTrack *new_track);

#ifdef CDPAR
gboolean CDPRip(char *device,char *generic_scsi_device,int track,
		long first_sector,long last_sector,
		char *outfile,int paranoia_mode,int *rip_smile_level,
		gfloat *rip_percent_done,gboolean *stop_thread_rip_now,
		gboolean do_gain_calc,FILE *output_fp);
#endif

#endif /* ifndef GRIP_RIP_H */
