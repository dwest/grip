/* discdb.c
 *
 * Based on code from libcdaudio 0.5.0 (Copyright (C)1998 Tony Arcieri)
 *
 * All changes Copyright (c) 1998-2004  Mike Oliphant <grip@nostatic.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__sun__)
#include <strings.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include "cddev.h"
#include "discdb.h"
#include "grip_id3.h"
#include "common.h"
#include "config.h"

extern char *Program;
static char *StrConvertEncoding(char *str,char *from,char *to,int max_len);
gboolean DiscDBUTF8Validate(const DiscInfo *disc,const DiscData *data);
static void DiscDBConvertEncoding(DiscInfo *disc,DiscData *data,
                                  char *from,char *to);
static int DiscDBSum(int val);
static char *DiscDBReadLine(char **dataptr);
static GString *DiscDBMakeURI(DiscDBServer *server,DiscDBHello *hello,
			      char *cmd);
static char *DiscDBMakeRequest(DiscDBServer *server,DiscDBHello *hello,
			       char *cmd);
static void DiscDBProcessLine(char *inbuffer,DiscData *data,
			    int numtracks);
static void DiscDBWriteLine(char *header,int num,char *data,FILE *outfile,
                            char *encoding);

static char *discdb_genres[]={"unknown","blues","classical","country",
			    "data","folk","jazz","misc","newage",
			    "reggae","rock","soundtrack"};

/* DiscDB sum function */

static int DiscDBSum(int val)
{
  char *bufptr, buf[16];
  int ret = 0;
   
  g_snprintf(buf,16,"%lu",(unsigned long int)val);

  for(bufptr = buf; *bufptr != '\0'; bufptr++)
     ret += (*bufptr - '0');
   
   return ret;
}

/* Produce DiscDB ID for CD currently in CD-ROM */

unsigned int DiscDBDiscid(DiscInfo *disc)
{
  int index, tracksum = 0, discid;
  
  if(!disc->have_info) CDStat(disc,TRUE);
  
  for(index = 0; index < disc->num_tracks; index++)
    tracksum += DiscDBSum(disc->track[index].start_pos.mins * 60 +
			disc->track[index].start_pos.secs);
  
  discid = (disc->length.mins * 60 + disc->length.secs) -
    (disc->track[0].start_pos.mins * 60 + disc->track[0].start_pos.secs);
  
  return (tracksum % 0xFF) << 24 | discid << 8 | disc->num_tracks;
}

/* Convert numerical genre to text */

char *DiscDBGenre(int genre)
{
  if(genre>11) return("unknown");

  return discdb_genres[genre];
}

/* Convert genre from text form into an integer value */

int DiscDBGenreValue(char *genre)
{
  int pos;
  
  for(pos=0;pos<12;pos++)
    if(!strcmp(genre,discdb_genres[pos])) return pos;
  
  return 0;
}

/* Read a single line from the buffer and move the pointer along */

static char *DiscDBReadLine(char **dataptr)
{
  char *data=*dataptr;
  char *pos;

  if(!data || !*data || *data=='.') {
    *dataptr=NULL;

    return NULL;
  }

  for(pos=data;*pos;pos++) {
    if(*pos=='\n') {
      *pos='\0';

      Debug("[%s]\n",data);

      *dataptr=pos+1;

      return data;
    }
  }

  Debug("[%s]\n",data);

  *dataptr=NULL;

  return data;
}

static GString *DiscDBMakeURI(DiscDBServer *server,DiscDBHello *hello,
			      char *cmd)
{
  GString *uri;

  uri=g_string_new(NULL);

  g_string_sprintf(uri,"http://%s/%s?cmd=%s&hello=private+free.the.cddb+%s+%s"
		   "&proto=%d",
		   server->name,server->cgi_prog,cmd,
		   hello->hello_program,hello->hello_version,
		   hello->proto_version);

  return uri;
}

static char *DiscDBMakeRequest(DiscDBServer *server,DiscDBHello *hello,
			       char *cmd)
{
  GString *uri;
  GString *proxy,*user;
  char user_agent[256];
  struct curl_slist *headers=NULL;
  FILE *outfile;
  char *data=NULL;
  int success;
  CURL *curl_handle;
  long filesize;

  curl_global_init(CURL_GLOBAL_ALL);
  curl_handle=curl_easy_init();
  
  if(curl_handle) {
    if(server->use_proxy) {
      proxy=g_string_new(NULL);

      g_string_sprintf(proxy,"%s:%d",server->proxy->name,
                       server->proxy->port);
      
      curl_easy_setopt(curl_handle,CURLOPT_PROXY,proxy->str);
      
      if(*server->proxy->username) {

        user=g_string_new(NULL);

        g_string_sprintf(user,"%s:%s",server->proxy->username,
                         server->proxy->pswd);
        
        curl_easy_setopt(curl_handle,CURLOPT_PROXYUSERPWD,user->str);
      }
    }
    
    uri=DiscDBMakeURI(server,hello,cmd);
    
    Debug(_("URI is %s\n"),uri->str);
    
    curl_easy_setopt(curl_handle,CURLOPT_URL,uri->str);
    
    g_snprintf(user_agent,256,"User-Agent: %s %s",
               hello->hello_program,hello->hello_version);
    
    headers=curl_slist_append(headers,user_agent);
    
    curl_easy_setopt(curl_handle,CURLOPT_HTTPHEADER,headers);
    
    outfile=tmpfile();
    
    if(outfile) {
      curl_easy_setopt(curl_handle,CURLOPT_FILE,outfile);
      
      success=curl_easy_perform(curl_handle);
      
      if(success==0) {
        filesize=ftell(outfile);
        
        rewind(outfile);
        
        data=(char *)malloc(filesize+1);
        
        if(data) {
          fread(data,filesize,1,outfile);

          data[filesize]='\0';
        }
      }
      
      fclose(outfile);
    }
    
    curl_slist_free_all(headers);

    curl_easy_cleanup(curl_handle);

    g_string_free(uri,TRUE);

    if(server->use_proxy) {
      g_string_free(proxy,TRUE);

      if(*server->proxy->username) {
        g_string_free(user,TRUE);
      }
    }
  }

  curl_global_cleanup();

  return data;
}


/* Query the DiscDB for the CD currently in the CD-ROM */

gboolean DiscDBDoQuery(DiscInfo *disc,DiscDBServer *server,
		       DiscDBHello *hello,DiscDBQuery *query)
{
  int index;
  GString *cmd;
  char *result,*inbuffer;
  char *dataptr;

  query->query_matches=0;

  if(!disc->have_info) CDStat(disc,TRUE);

  cmd=g_string_new(NULL);

  g_string_sprintfa(cmd,"cddb+query+%08x+%d",DiscDBDiscid(disc),
		    disc->num_tracks);

  for(index=0;index<disc->num_tracks;index++)
    g_string_sprintfa(cmd,"+%d",disc->track[index].start_frame);

  g_string_sprintfa(cmd,"+%d",disc->length.mins*60 + disc->length.secs);

  Debug(_("Query is [%s]\n"),cmd->str);

  result=DiscDBMakeRequest(server,hello,cmd->str);

  g_string_free(cmd,TRUE);

  if(!result) {
    return FALSE;
  }

  dataptr=result;

  inbuffer=DiscDBReadLine(&dataptr);

  switch(strtol(strtok(inbuffer," "),NULL,10)) {
    /* 200 - exact match */
  case 200:
    query->query_match=MATCH_EXACT;
    query->query_matches=1;
    
    query->query_list[0].list_genre=
      DiscDBGenreValue(g_strstrip(strtok(NULL," ")));
    
    sscanf(g_strstrip(strtok(NULL," ")),"%xd",
	   &query->query_list[0].list_id);
    
    DiscDBParseTitle(g_strstrip(strtok(NULL,"")),
		     query->query_list[0].list_title,
		     query->query_list[0].list_artist,"/");
    
    break;
    /* 210 - multiple exact matches */
  case 210: 
    query->query_match=MATCH_EXACT;
    query->query_matches=0;


    while(query->query_matches < MAX_INEXACT_MATCHES &&
          (inbuffer=DiscDBReadLine(&dataptr))) {
      query->query_list[query->query_matches].list_genre=
	DiscDBGenreValue(g_strstrip(strtok(inbuffer," ")));
      
      sscanf(g_strstrip(strtok(NULL," ")),"%xd",
	     &query->query_list[query->query_matches].list_id);
      
      DiscDBParseTitle(g_strstrip(strtok(NULL,"")),
		       query->query_list[query->query_matches].list_title,
		       query->query_list[query->query_matches].list_artist,
		       "/");
      
      query->query_matches++;
    }
   break;
    /* 211 - inexact match */
  case 211:
    query->query_match=MATCH_INEXACT;
    query->query_matches=0;

    while(query->query_matches < MAX_INEXACT_MATCHES &&
          (inbuffer=DiscDBReadLine(&dataptr))) {
      query->query_list[query->query_matches].list_genre=
	DiscDBGenreValue(g_strstrip(strtok(inbuffer," ")));
      
      sscanf(g_strstrip(strtok(NULL," ")),"%xd",
	     &query->query_list[query->query_matches].list_id);
      
      DiscDBParseTitle(g_strstrip(strtok(NULL,"")),
		       query->query_list[query->query_matches].list_title,
		       query->query_list[query->query_matches].list_artist,
		       "/");
      
      query->query_matches++;
    }
    
    break;
    /* No match */
  default:
    query->query_match=MATCH_NOMATCH;

    free(result);

    return FALSE;
  }

  free(result);

  return TRUE;
}

/* Split string into title/artist */

void DiscDBParseTitle(char *buf,char *title,char *artist,char *sep)
{
  char *tmp;

  tmp=strtok(buf,sep);

  if(!tmp) return;

  g_snprintf(artist,256,"%s",g_strstrip(tmp));

  tmp=strtok(NULL,"");

  if(tmp)
    g_snprintf(title,256,"%s",g_strstrip(tmp));
  else strcpy(title,artist);
}

/* Process a line of input data */

static void DiscDBProcessLine(char *inbuffer,DiscData *data,
                              int numtracks)
{
  int track;
  int len=0;
  char *st;

  strtok(inbuffer,"\n\r");

  if(!strncasecmp(inbuffer,"# Revision: ",12)) {
    data->revision=atoi(inbuffer+12);
  }
  else if(!strncasecmp(inbuffer,"DTITLE",6)) {
    len=strlen(data->data_title);

    g_snprintf(data->data_title+len,256-len,"%s",inbuffer+7);
  }
  else if(!strncasecmp(inbuffer,"DYEAR",5)) {
    strtok(inbuffer,"=");
    
    st = strtok(NULL, "");
    if(st == NULL)
        return;

    data->data_year=atoi(g_strstrip(st));
  }
  else if(!strncasecmp(inbuffer,"DGENRE",6)) {
    strtok(inbuffer,"=");
    
    st = strtok(NULL, "");
    if(st == NULL)
        return;

    st=g_strstrip(st);

    if(*st) {
      data->data_genre=DiscDBGenreValue(st);
      data->data_id3genre=ID3GenreValue(st);
    }
  }
  else if(!strncasecmp(inbuffer,"DID3",4)) {
    strtok(inbuffer,"=");
    
    st = strtok(NULL, "");
    if(st == NULL)
        return;
    
    data->data_id3genre=atoi(g_strstrip(st));
  }
  else if(!strncasecmp(inbuffer,"TTITLE",6)) {
    track=atoi(strtok(inbuffer+6,"="));
    
    if(track<numtracks)
      len=strlen(data->data_track[track].track_name);

    st = strtok(NULL, "");
    if(st == NULL)
        return;    
    
    g_snprintf(data->data_track[track].track_name+len,256-len,"%s",
	    st);
  }
  else if(!strncasecmp(inbuffer,"TARTIST",7)) {
    data->data_multi_artist=TRUE;

    track=atoi(strtok(inbuffer+7,"="));
    
    if(track<numtracks)
      len=strlen(data->data_track[track].track_artist);

    st = strtok(NULL, "");
    if(st == NULL)
        return;    
    
    g_snprintf(data->data_track[track].track_artist+len,256-len,"%s",
	    st);
  }
  else if(!strncasecmp(inbuffer,"EXTD",4)) {
    len=strlen(data->data_extended);

    g_snprintf(data->data_extended+len,4096-len,"%s",inbuffer+5);
  }
  else if(!strncasecmp(inbuffer,"EXTT",4)) {
    track=atoi(strtok(inbuffer+4,"="));
    
    if(track<numtracks)
      len=strlen(data->data_track[track].track_extended);

    st = strtok(NULL, "");
    if(st == NULL)
        return;
    
    g_snprintf(data->data_track[track].track_extended+len,4096-len,"%s",
	    st);
  }
  else if(!strncasecmp(inbuffer,"PLAYORDER",5)) {
    len=strlen(data->data_playlist);

    g_snprintf(data->data_playlist+len,256-len,"%s",inbuffer+10);
  }
}

static char *StrConvertEncoding(char *str,char *from,char *to,int max_len)
{
  char *conv_str;
  gsize rb,wb;

  if(!str) return NULL;

  conv_str=g_convert_with_fallback(str,strlen(str),to,from,NULL,&rb,&wb,NULL);

  if(!conv_str) return str;

  g_snprintf(str,max_len,"%s",conv_str);

  g_free(conv_str);

  return str;
}

gboolean DiscDBUTF8Validate(const DiscInfo *disc,const DiscData *data)
{
  int track;

  if(data->data_title && !g_utf8_validate(data->data_title,-1,NULL))
    return FALSE;
  if(data->data_artist && !g_utf8_validate(data->data_artist,-1,NULL))
    return FALSE;
  if(data->data_extended && !g_utf8_validate(data->data_extended,-1,NULL))
    return FALSE;

  for(track=0;track<disc->num_tracks;track++) {
  if(data->data_track[track].track_name
     && !g_utf8_validate(data->data_track[track].track_name,-1,NULL))
    return FALSE;
  if(data->data_track[track].track_artist
     && !g_utf8_validate(data->data_track[track].track_artist,-1,NULL))
    return FALSE;
  if(data->data_track[track].track_extended
     && !g_utf8_validate(data->data_track[track].track_extended,-1,NULL))
    return FALSE;
  }
  return TRUE;
}


static void DiscDBConvertEncoding(DiscInfo *disc,DiscData *data,
                                  char *from,char *to)
{
  int track;

  StrConvertEncoding(data->data_title,from,to,256);
  StrConvertEncoding(data->data_artist,from,to,256);
  StrConvertEncoding(data->data_extended,from,to,4096);

  for(track=0;track<disc->num_tracks;track++) {
    StrConvertEncoding(data->data_track[track].track_name,from,to,256);
    StrConvertEncoding(data->data_track[track].track_artist,from,to,256);
    StrConvertEncoding(data->data_track[track].track_extended,from,to,4096);
  }
}

/* Read the actual DiscDB entry */

gboolean DiscDBRead(DiscInfo *disc,DiscDBServer *server,
		    DiscDBHello *hello,DiscDBEntry *entry,
		    DiscData *data,char *encoding)
{
  int index;
  GString *cmd;
  char *result,*inbuffer,*dataptr;
  
  if(!disc->have_info) CDStat(disc,TRUE);
  
  data->data_genre=entry->entry_genre;
  data->data_id=DiscDBDiscid(disc);
  *(data->data_extended)='\0';
  *(data->data_title)='\0';
  *(data->data_artist)='\0';
  *(data->data_playlist)='\0';
  data->data_multi_artist=FALSE;
  data->data_year=0;
  data->data_id3genre=-1;
  data->revision=-1;

  for(index=0;index<MAX_TRACKS;index++) {
    *(data->data_track[index].track_name)='\0';
    *(data->data_track[index].track_artist)='\0';
    *(data->data_track[index].track_extended)='\0';
  }

  cmd=g_string_new(NULL);

  g_string_sprintf(cmd,"cddb+read+%s+%08x",DiscDBGenre(entry->entry_genre),
		   entry->entry_id);

  result=DiscDBMakeRequest(server,hello,cmd->str);

  g_string_free(cmd,TRUE);

  if(!result) {
    return FALSE;
  }

  dataptr=result;

  inbuffer=DiscDBReadLine(&dataptr);

  while((inbuffer=DiscDBReadLine(&dataptr)))
    DiscDBProcessLine(inbuffer,data,disc->num_tracks);

  /* Both disc title and artist have been stuffed in the title field, so the
     need to be separated */
  
  DiscDBParseTitle(data->data_title,data->data_title,data->data_artist,"/");

  free(result);

  /* Don't allow the genre to be overwritten */
  data->data_genre=entry->entry_genre;

  if(strcasecmp(encoding,"utf-8")) {
    DiscDBConvertEncoding(disc,data,encoding,"utf-8");
  }

  return TRUE;
}

/* See if a disc is in the local database */

gboolean DiscDBStatDiscData(DiscInfo *disc)
{
  int index,id;
  struct stat st;
  char root_dir[256],file[256];
  
  if(!disc->have_info) CDStat(disc,TRUE);

  id=DiscDBDiscid(disc);
  
  g_snprintf(root_dir,256,"%s/.cddb",getenv("HOME"));
  
  if(stat(root_dir, &st) < 0)
    return FALSE;
  else {
    if(!S_ISDIR(st.st_mode))
      return FALSE;
  }
  
  g_snprintf(file,256,"%s/%08x",root_dir,id);
  if(stat(file,&st)==0) return TRUE;

  for(index=0;index<12;index++) {
    g_snprintf(file,256,"%s/%s/%08x",root_dir,DiscDBGenre(index),id);

    if(stat(file,&st) == 0)
      return TRUE;
  }
   
  return FALSE;
}

/* Read from the local database */

int DiscDBReadDiscData(DiscInfo *disc,DiscData *ddata, const char *encoding)
{
  FILE *discdb_data=NULL;
  int index,genre;
  char root_dir[256],file[256],inbuf[512];
  struct stat st;
  
  g_snprintf(root_dir,256,"%s/.cddb",getenv("HOME"));
  
  if(stat(root_dir, &st) < 0) {
    return -1;
  } else {
    if(!S_ISDIR(st.st_mode)) {
      errno = ENOTDIR;
      return -1;
    }
  }
  
  if(!disc->have_info) CDStat(disc,TRUE);

  ddata->data_id=DiscDBDiscid(disc);
  *(ddata->data_extended)='\0';
  *(ddata->data_title)='\0';
  *(ddata->data_artist)='\0';
  *(ddata->data_playlist)='\0';
  ddata->data_multi_artist=FALSE;
  ddata->data_year=0;
  ddata->data_genre=7;
  ddata->data_id3genre=-1;
  ddata->revision=-1;

  for(index=0;index<MAX_TRACKS;index++) {
    *(ddata->data_track[index].track_name)='\0';
    *(ddata->data_track[index].track_artist)='\0';
    *(ddata->data_track[index].track_extended)='\0';
  }

  g_snprintf(file,256,"%s/%08x",root_dir,ddata->data_id);
  if(stat(file,&st)==0) {
    discdb_data=fopen(file, "r");
  }
  else {
    for(genre=0;genre<12;genre++) {
      g_snprintf(file,256,"%s/%s/%08x",root_dir,DiscDBGenre(genre),
	       ddata->data_id);
      
      if(stat(file,&st)==0) {
	discdb_data=fopen(file, "r");
	
	ddata->data_genre=genre;
	break;
      }
    }

    if(genre==12) return -1;
  }

  while(fgets(inbuf,512,discdb_data))
    DiscDBProcessLine(inbuf,ddata,disc->num_tracks);

  /* Both disc title and artist have been stuffed in the title field, so the
     need to be separated */

  DiscDBParseTitle(ddata->data_title,ddata->data_title,ddata->data_artist,"/");

  if(!DiscDBUTF8Validate(disc,ddata)) {
    DiscDBConvertEncoding(disc,ddata,strcasecmp(encoding,"UTF-8")?
				     encoding:"ISO-8859-1","UTF-8");
  }

  fclose(discdb_data);
  
  return 0;
}

static void DiscDBWriteLine(char *header,int num,char *data,FILE *outfile,
                            char *encoding)
{
  char *offset, *next, *chunk;

  if(strcasecmp(encoding,"utf-8")) {
    StrConvertEncoding(data,"utf-8",encoding,512);
  }

  offset=data;

  do {
    for(next=offset; next-offset<65&&*next; ) {
      if (*next=='\\'&&*(next+1)) {
	next+=2;
      }
      else if(!strcasecmp(encoding,"utf-8")) {
	next=g_utf8_find_next_char(next,NULL);
      }
      else {
	next++;
      }
    }
    chunk=g_strndup(offset,(gsize)(next-offset));
    if(num==-1)
      fprintf(outfile,"%s=%s\n",header,chunk);
    else
      fprintf(outfile,"%s%d=%s\n",header,num,chunk);
    g_free(chunk);
    offset=next;
  } while (*offset);
}


/* Write to the local cache */

int DiscDBWriteDiscData(DiscInfo *disc,DiscData *ddata,FILE *outfile,
                        gboolean gripext,gboolean freedbext,char *encoding)
{
  FILE *discdb_data;
  int track;
  char root_dir[256],file[256],tmp[512];
  struct stat st;

  if(!disc->have_info) CDStat(disc,TRUE);

  if(!outfile) {
    g_snprintf(root_dir,256,"%s/.cddb",getenv("HOME"));
    g_snprintf(file,256,"%s/%08x",root_dir,ddata->data_id);
  
    if(stat(root_dir,&st)<0) {
      if(errno != ENOENT) {
	Debug(_("Stat error %d on %s\n"),errno,root_dir);
	return -1;
      }
      else {
	Debug(_("Creating directory %s\n"),root_dir);
	mkdir(root_dir,0777);
      }
    } else {
      if(!S_ISDIR(st.st_mode)) {
	Debug(_("Error: %s exists, but is a file\n"),root_dir);
	errno=ENOTDIR;
	return -1;
      }   
    }
      
    if((discdb_data=fopen(file,"w"))==NULL) {
      Debug(_("Error: Unable to open %s for writing\n"),file);
      return -1;
    }
  }
  else discdb_data=outfile;

#ifndef GRIPCD
  fprintf(discdb_data,"# xmcd CD database file generated by Grip %s\n",
	  VERSION);
#else
  fprintf(discdb_data,"# xmcd CD database file generated by GCD %s\n",
	  VERSION);
#endif
  fputs("# \n",discdb_data);
  fputs("# Track frame offsets:\n",discdb_data);

  for(track=0;track<disc->num_tracks;track++)
    fprintf(discdb_data, "#       %d\n",disc->track[track].start_frame);

  fputs("# \n",discdb_data);
  fprintf(discdb_data,"# Disc length: %d seconds\n",disc->length.mins *
	  60 + disc->length.secs);
  fputs("# \n",discdb_data);

  if(gripext) fprintf(discdb_data,"# Revision: %d\n",ddata->revision);
  else fprintf(discdb_data,"# Revision: %d\n",ddata->revision+1);

  fprintf(discdb_data,"# Submitted via: Grip %s\n",VERSION);
  fputs("# \n",discdb_data);
  fprintf(discdb_data,"DISCID=%08x\n",ddata->data_id);

  g_snprintf(tmp,512,"%s / %s",ddata->data_artist,ddata->data_title);

  DiscDBWriteLine("DTITLE",-1,tmp,discdb_data,encoding);

  if(gripext||freedbext) {
    if(ddata->data_year)
      fprintf(discdb_data,"DYEAR=%d\n",ddata->data_year);
    else fprintf(discdb_data,"DYEAR=\n");
  }

  if(gripext) {
    fprintf(discdb_data,"DGENRE=%s\n",DiscDBGenre(ddata->data_genre));
    fprintf(discdb_data,"DID3=%d\n",ddata->data_id3genre);
  }
  else if(freedbext) {
    fprintf(discdb_data,"DGENRE=%s\n",ID3GenreString(ddata->data_id3genre));
  }

  for(track=0;track<disc->num_tracks;track++) {
    if(gripext||!*(ddata->data_track[track].track_artist)) {
      DiscDBWriteLine("TTITLE",track,ddata->data_track[track].track_name,
		      discdb_data,encoding);
    }
    else {
      g_snprintf(tmp,512,"%s / %s",ddata->data_track[track].track_artist,
		 ddata->data_track[track].track_name);
      DiscDBWriteLine("TTITLE",track,tmp,discdb_data,encoding);
    }

    if(gripext&&*(ddata->data_track[track].track_artist))
      DiscDBWriteLine("TARTIST",track,ddata->data_track[track].track_artist,
		      discdb_data,encoding);
  }

  DiscDBWriteLine("EXTD",-1,ddata->data_extended,discdb_data,encoding);
   
  for(track=0;track<disc->num_tracks;track++)
    DiscDBWriteLine("EXTT",track,
                    ddata->data_track[track].track_extended,discdb_data,
                    encoding);
  
  if(outfile)
    fprintf(discdb_data,"PLAYORDER=\n");
  else {
    fprintf(discdb_data,"PLAYORDER=%s\n",ddata->data_playlist);
    fclose(discdb_data);
  }
  
  return 0;
}
