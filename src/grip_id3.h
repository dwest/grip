/* id3.h
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

#ifndef GRIP_ID3_H
#define GRIP_ID3_H

#include <config.h>
#include "glib.h"

typedef struct {
  char *name;
  int num;
} ID3Genre;

gboolean ID3v1TagFile(char *filename,char *title,char *artist,char *album,
		      char *year,char *comment,unsigned char genre,
		      unsigned char tracknum, char *id3_encoding);
#ifdef HAVE_ID3V2
gboolean ID3v2TagFile(char *filename, char *title, char *artist, char *album,
		      char *year, char *comment, unsigned char genre, unsigned
		      char tracknum,char *id3v2_encoding);
#endif
char *ID3GenreString(int genre);
ID3Genre *ID3GenreByNum(int num);
int ID3GenreValue(char *genre);
int ID3GenrePos(int genre);
int DiscDB2ID3(int genre);
int ID32DiscDB(int id3_genre);

#endif /* GRIP_ID3_H */
