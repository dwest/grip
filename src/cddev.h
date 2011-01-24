/* cddev.h
 *
 * Based on code from libcdaudio 0.5.0 (Copyright (C)1998 Tony Arcieri)
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
 *
 */

#ifndef GRIP_CDDEV_H
#define GRIP_CDDEV_H

#include <glib.h>

/* Used with disc_info */
#define CDAUDIO_PLAYING				0
#define CDAUDIO_PAUSED				1
#define CDAUDIO_COMPLETED			2
#define CDAUDIO_NOSTATUS			3

#define MAX_TRACKS				100
#define MAX_SLOTS				100 /* For CD changers */

#define CURRENT_CDDBREVISION			2


/* Used for keeping track of times */
typedef struct _disc_time {
   int mins;
   int secs;
} DiscTime;

/* Track specific information */
typedef struct _track_info {
  DiscTime length;
  DiscTime start_pos;
  int num_frames;
  int start_frame;
  unsigned char flags;
} TrackInfo;

/* Disc information such as current track, amount played, etc */
typedef struct _disc_info {
  int cd_desc;                              /* CD device file desc. */
  char *devname;                            /* CD device file pathname */
  gboolean have_info;                       /* Do we have disc info yet? */
  gboolean disc_present;	            /* Is disc present? */
  int disc_mode;		            /* Current disc mode */
  DiscTime track_time;		            /* Current track time */
  DiscTime disc_time;		            /* Current disc time */
  DiscTime length;		            /* Total disc length */
  int curr_frame;			    /* Current frame */
  int curr_track;		            /* Current track */
  int num_tracks;		            /* Number of tracks on disc */
  TrackInfo track[MAX_TRACKS];	            /* Track specific information */
} DiscInfo;

/* Channle volume structure */
typedef struct _channel_volume { 
  int left;
  int right;
} ChannelVolume;

/* Volume structure */
typedef struct _disc_volume {
   ChannelVolume vol_front;
   ChannelVolume vol_back;
} DiscVolume;


gboolean CDInitDevice(char *device_name,DiscInfo *disc);
gboolean CDCloseDevice(DiscInfo *disc);
gboolean CDStat(DiscInfo *disc,gboolean read_toc);
gboolean IsDataTrack(DiscInfo *disc,int track);
gboolean CDPlayFrames(DiscInfo *disc,int startframe,int endframe);
gboolean CDPlayTrackPos(DiscInfo *disc,int starttrack,
			int endtrack,int startpos);
gboolean CDPlayTrack(DiscInfo *disc,int starttrack,int endtrack);
gboolean CDAdvance(DiscInfo *disc,DiscTime *time);
gboolean CDStop(DiscInfo *disc);
gboolean CDPause(DiscInfo *disc);
gboolean CDResume(DiscInfo *disc);
gboolean TrayOpen(DiscInfo *disc);
gboolean CDEject(DiscInfo *disc);
gboolean CDClose(DiscInfo *disc);
gboolean CDGetVolume(DiscInfo *disc,DiscVolume *vol);
gboolean CDSetVolume(DiscInfo *disc,DiscVolume *vol);
gboolean CDChangerSelectDisc(DiscInfo *disc,int disc_num);
int CDChangerSlots(DiscInfo *disc);

#endif /* GRIP_CDDEV_H */
