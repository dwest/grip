/* id3.c
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gnome.h>
#include "grip_id3.h"

static void ID3Put(char *dest,char *src,int len,char *encoding);

/* this array contains string representations of all known ID3 tags */
/* taken from mp3id3 in the mp3tools 0.7 package */

ID3Genre id3_genres[] = {
  {"Alternative",20},
  {"Anime",146},
  {"Blues",0},
  {"Classical",32},
  {"Country",2},
  {"Folk",80},
  {"Jazz",8},
  {"Metal",9},
  {"Pop",13},
  {"Rap",15},
  {"Reggae",16},
  {"Rock",17},
  {"Other",12},
  {"Acapella",123},
  {"Acid",34},
  {"Acid Jazz",74},
  {"Acid Punk",73},
  {"Acoustic",99},
  {"AlternRock",40},
  {"Ambient",26},
  {"Avantgarde",90},
  {"Ballad",116},
  {"Bass",41},
  {"Beat",135},
  {"Bebob",85},
  {"Big Band",96},
  {"Black Metal",138},
  {"Bluegrass",89},
  {"Booty Bass",107},
  {"BritPop",132},
  {"Cabaret",65},
  {"Celtic",88},
  {"Chamber Music",104},
  {"Chanson",102},
  {"Chorus",97},
  {"Christian Gangsta Rap",136},
  {"Christian Rap",61},
  {"Christian Rock",141},
  {"Classic Rock",1},
  {"Club",112},
  {"Club-House",128},
  {"Comedy",57},
  {"Contemporary Christian",140},
  {"Crossover",139},
  {"Cult",58},
  {"Dance",3},
  {"Dance Hall",125},
  {"Darkwave",50},
  {"Death Metal",22},
  {"Disco",4},
  {"Dream",55},
  {"Drum & Bass",127},
  {"Drum Solo",122},
  {"Duet",120},
  {"Easy Listening",98},
  {"Electronic",52},
  {"Ethnic",48},
  {"Eurodance",54},
  {"Euro-house",124},
  {"Euro-Techno",25},
  {"Fast Fusion",84},
  {"Folklore",115},
  {"Folk/Rock",81},
  {"Freestyle",119},
  {"Funk",5},
  {"Fusion",30},
  {"Game",36},
  {"Gangsta",59},
  {"Goa",126},
  {"Gospel",38},
  {"Gothic",49},
  {"Gothic Rock",91},
  {"Grunge",6},
  {"Hardcore",129},
  {"Hard Rock",79},
  {"Heavy Metal",137},
  {"Hip-Hop",7},
  {"House",35},
  {"Humour",100},
  {"Indie",131},
  {"Industrial",19},
  {"Instrumental",33},
  {"Instrumental Pop",46},
  {"Instrumental Rock",47},
  {"Jazz+Funk",29},
  {"JPop",145},
  {"Jungle",63},
  {"Latin",86},
  {"Lo-Fi",71},
  {"Meditative",45},
  {"Merengue",142},
  {"Musical",77},
  {"National Folk",82},
  {"Native American",64},
  {"Negerpunk",133},
  {"New Age",10},
  {"New Wave",66},
  {"Noise",39},
  {"Oldies",11},
  {"Opera",103},
  {"Polka",75},
  {"Polsk Punk",134},
  {"Pop-Folk",53},
  {"Pop/Funk",62},
  {"Porn Groove",109},
  {"Power Ballad",117},
  {"Pranks",23},
  {"Primus",108},
  {"Progressive Rock",92},
  {"Psychadelic",67},
  {"Psychedelic Rock",93},
  {"Punk",43},
  {"Punk Rock",121},
  {"Rave",68},
  {"R&B",14},
  {"Retro",76},
  {"Revival",87},
  {"Rhythmic Soul",118},
  {"Rock & Roll",78},
  {"Salsa",143},
  {"Samba",114},
  {"Satire",110},
  {"Showtunes",69},
  {"Ska",21},
  {"Slow Jam",111},
  {"Slow Rock",95},
  {"Sonata",105},
  {"Soul",42},
  {"Sound Clip",37},
  {"Soundtrack",24},
  {"Southern Rock",56},
  {"Space",44},
  {"Speech",101},
  {"Swing",83},
  {"Symphonic Rock",94},
  {"Symphony",106},
  {"SynthPop",147},
  {"Tango",113},
  {"Techno",18},
  {"Techno-Industrial",51},
  {"Terror",130},
  {"Top 40",60},
  {"Trailer",70},
  {"Trance",31},
  {"Trash Metal",144},
  {"Tribal",72},
  {"Trip-Hop",27},
  {"Vocal",28},
  {NULL,145}
};

/* This array maps CDDB_ genre numbers to closest id3 genre */
int cddb_2_id3[] =
{
  12,         /* CDDB_UNKNOWN */
  0,          /* CDDB_BLUES */
  32,         /* CDDB_CLASSICAL */
  2,          /* CDDB_COUNTRY */
  12,         /* CDDB_DATA */
  80,         /* CDDB_FOLK */
  8,          /* CDDB_JAZZ */
  12,         /* CDDB_MISC */
  10,         /* CDDB_NEWAGE */
  16,         /* CDDB_REGGAE */
  17,         /* CDDB_ROCK */
  24,         /* CDDB_SOUNDTRACK */
};

/* ID3 tag structure */

typedef struct _id3_tag {
  char tag[3];
  char title[30];
  char artist[30];
  char album[30];
  char year[4];
  char comment[28];
  unsigned char id3v1_1_mark;
  unsigned char tracknum;
  unsigned char genre;
} ID3v1Tag;

#ifdef HAVE_ID3V2

#include <id3.h>

/* Things you might want to mess with. Surprisingly, the code will probably
   cope with you just messing with this section. */
#define NUM_FRAMES 7
static ID3_FrameID frameids[ NUM_FRAMES ] = {
    ID3FID_TITLE, ID3FID_LEADARTIST, ID3FID_ALBUM, ID3FID_YEAR,
    ID3FID_COMMENT, ID3FID_CONTENTTYPE, ID3FID_TRACKNUM
};
/* End of the section you're supposed to mess with */

gboolean ID3v2TagFile(char *filename, char *title, char *artist, char *album,
		      char *year, char *comment, unsigned char genre, unsigned
		      char tracknum,char *id3v2_encoding)
{
  ID3Tag *tag;
  ID3Field *field;
  ID3Frame *frames[ NUM_FRAMES ];
  int i;
  gboolean retval = TRUE;
  luint frm_offset;
  mode_t mask;
  char *conv_str;
  gsize rb,wb;

  tag = ID3Tag_New();

  if(tag) {
    frm_offset=ID3Tag_Link(tag,filename);
    /* GRR. No error. */
    
    for ( i = 0; i < NUM_FRAMES; i++ ) {
      frames[ i ] = ID3Frame_NewID( frameids[ i ] );
      
      if ( frames[ i ] ) {
	char *c_data = NULL;
	char gen[ 5 ] = "(   )";
	char trk[ 4 ] = "   ";
	
	switch( frameids[ i ] ) {
	case ID3FID_TITLE:
	  c_data = title;
	  break;
	  
	case ID3FID_LEADARTIST:
	  c_data = artist;
	  break;
	  
	case ID3FID_ALBUM:
	  c_data = album;
	  break;
	  
	case ID3FID_YEAR:
	  c_data = year;
	  break;
	  
	case ID3FID_COMMENT:
	  c_data = comment;
	  break;
	  
	case ID3FID_CONTENTTYPE:
	  c_data = gen;
	  sprintf( gen, "(%d)", genre ); /* XXX */
	  break;
	  
	case ID3FID_TRACKNUM:
	  c_data = trk;
	  sprintf( trk, "%d", tracknum ); /* XXX */
	  break;
	  
	default:
	  /* Doh! */
	  g_printerr(_("unknown ID3 field\n"));
	  break;
	}
	
	if(c_data != NULL) {
	  field = ID3Frame_GetField( frames[i], ID3FN_TEXT );

	  if(field) {
            /*            if(!strcasecmp(id3v2_encoding,"utf-8")) {
	      ID3Field_SetUNICODE(field,(unicode_t *)c_data);
	    }
	    else {
            */     

            /* Always encode pretending it is ascii */
            
            conv_str=g_convert_with_fallback(c_data,strlen(c_data),id3v2_encoding,
                               "utf-8",NULL,&rb,&wb,NULL);
            
            if(!conv_str) {
              printf("***convert failed\n");

              conv_str=strdup(c_data);
            }

            ID3Field_SetASCII(field,conv_str);
            
            g_free(conv_str);
	  } else {
	    retval = FALSE;
	  }
	}
      } else { /* Frame->new() failed */
	retval = FALSE;
	break;
      }
    }
    if ( retval != FALSE ) {
      /* It would be really nice if I could have done something like
	 ID3Tag_AddFrames( tag, frames, NUM_FRAMES ), but the
	 prototypes work against me one way or another. So, this will
	 do instead. */
      for ( i = 0; i < NUM_FRAMES; i++ ) {
	/* Strictly speaking I should look for existing tags and
	   delete them, but hey. We're making fresh mp3 files, right?
	*/
	ID3Tag_AddFrame( tag, frames[ i ] );
      }
    }
    
    if(ID3Tag_UpdateByTagType(tag,ID3TT_ID3V2) != ID3E_NoError ) {
      retval = FALSE;
    }
    
    ID3Tag_Delete( tag );

    /* Reset permissions based on users umask to work around a bug in the
       id3v2 library */
    mask = umask(0);
    umask(mask);
    chmod(filename, 0666 & ~mask);

  } else { /* Tag -> new() failed */
    retval = FALSE;
  }
  
  return retval;
}

#endif /* HAVE_ID3V2 */

/* Add an ID3v1 tag to a file */

gboolean ID3v1TagFile(char *filename,char *title,char *artist,char *album,
		      char *year,char *comment,unsigned char genre,
		      unsigned char tracknum, char *id3_encoding)
{
  FILE *fp;
  ID3v1Tag tag;

  fp=fopen(filename,"a");

  ID3Put(tag.tag,"TAG",3,id3_encoding);

  ID3Put(tag.title,title,30,id3_encoding);

  ID3Put(tag.artist,artist,30,id3_encoding);

  ID3Put(tag.album,album,30,id3_encoding);

  ID3Put(tag.year,year,4,NULL);

  ID3Put(tag.comment,comment,28,id3_encoding);

  tag.id3v1_1_mark = 0U;

  tag.tracknum=tracknum;
  tag.genre=genre;

  fwrite(&tag,sizeof(ID3v1Tag),1,fp);

  fclose(fp);

  return TRUE;
}

/* Copy a string padding with zeros */

static void ID3Put(char *dest,char *src,int len,char *encoding)
{
  int pos;
  int srclen;
  char *conv_str;
  gsize rb,wb;

  if(encoding&&strcasecmp(encoding,"utf-8")) {
    conv_str=g_convert_with_fallback(src,strlen(src),encoding,"utf-8",NULL,&rb,&wb,NULL);

    if(!conv_str) conv_str=strdup(src);
  }
  else conv_str=strdup(src);

  srclen=strlen(conv_str);

  for(pos=0;pos<len;pos++) {
    if(pos<srclen) dest[pos]=conv_str[pos];
    else dest[pos]=0;
  }

  g_free(conv_str);
}

char *ID3GenreString(int genre)
{
  int num;

  for(num=0;id3_genres[num].name;num++) {
    if(id3_genres[num].num==genre) return id3_genres[num].name;
  }

  return NULL;
}

ID3Genre *ID3GenreByNum(int num)
{
  if(!id3_genres[num].name) return NULL;

  return &(id3_genres[num]);
}

/* Return the id3 genre id from the text name */
int ID3GenreValue(char *genre)
{
  int pos;
  
  for(pos=0;id3_genres[pos].name;pos++)
    if(!strcasecmp(genre,id3_genres[pos].name))
      return id3_genres[pos].num;
  
  return 12;
}

int ID3GenrePos(int genre)
{
  int num;

  for(num=0;id3_genres[num].name;num++) {
    if(id3_genres[num].num==genre) return num;
  }

  return 0;
}

int DiscDB2ID3(int discdb_genre)
{
  return cddb_2_id3[discdb_genre];
}

int ID32DiscDB(int id3_genre)
{
  int discdb;

  for(discdb=0;discdb<12;discdb++) {
    if(cddb_2_id3[discdb]==id3_genre) return discdb;
  }

  return 12;
}
