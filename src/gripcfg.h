/* gripcfg.h
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

/* Ripper default info structure */

typedef struct _ripper {
  char name[20];
  char cmdline[256];
} Ripper;

/* Encoder default info structure */

typedef struct _mp3_encoder {
  char name[20];
  char cmdline[256];
  char extension[10];
} MP3Encoder;


void MakeConfigPage(GripInfo *ginfo);
gboolean LoadRipperConfig(GripInfo *ginfo,int ripcfg);
void SaveRipperConfig(GripInfo *ginfo,int ripcfg);
gboolean LoadEncoderConfig(GripInfo *ginfo,int encodecfg);
void SaveEncoderConfig(GripInfo *ginfo,int encodecfg);
char *FindExe(char *exename,char **paths);
void FindExeInPath(char *exename, char *buf, int bsize);
gboolean FileExists(char *filename);
