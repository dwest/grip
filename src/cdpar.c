/* cdpar.c -- routines for interacting with the Paranoia library
 *
 * Based on main.c from the cdparanoia distribution
 *  (C) 1998 Monty <xiphmont@mit.edu>
 *
 * All changes Copyright 1999-2004 by Mike Oliphant (grip@nostatic.org)
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

#ifdef CDPAR

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <glib.h>
#include "gain_analysis.h"

#include "cdpar.h"

static void PutNum(long num,int f,int endianness,int bytes);
static void WriteWav(int f,long bytes);
static void CDPCallback(long inpos, int function);
static void GainCalc(char *buffer);
static long CDPWrite(int outf, char *buffer);

static inline int bigendianp(void){
  int test=1;
  char *hack=(char *)(&test);
  if(hack[0])return(0);
  return(1);
}

static inline size16 swap16(size16 x){
  return((((unsigned size16)x & 0x00ffU) <<  8) | 
         (((unsigned size16)x & 0xff00U) >>  8));
}

/* Ugly hack because we can't pass user data to the callback */
int *global_rip_smile_level;
FILE *global_output_fp;

static void PutNum(long num,int f,int endianness,int bytes)
{
  int i;
  unsigned char c;

  if(!endianness)
    i=0;
  else
    i=bytes-1;
  while(bytes--){
    c=(num>>(i<<3))&0xff;
    if(write(f,&c,1)==-1){
      perror("Could not write to output.");
      exit(1);
    }
    if(endianness)
      i--;
    else
      i++;
  }
}

static void WriteWav(int f,long bytes)
{
  /* quick and dirty */

  write(f,"RIFF",4);               /*  0-3 */
  PutNum(bytes+44-8,f,0,4);        /*  4-7 */
  write(f,"WAVEfmt ",8);           /*  8-15 */
  PutNum(16,f,0,4);                /* 16-19 */
  PutNum(1,f,0,2);                 /* 20-21 */
  PutNum(2,f,0,2);                 /* 22-23 */
  PutNum(44100,f,0,4);             /* 24-27 */
  PutNum(44100*2*2,f,0,4);         /* 28-31 */
  PutNum(4,f,0,2);                 /* 32-33 */
  PutNum(16,f,0,2);                /* 34-35 */
  write(f,"data",4);               /* 36-39 */
  PutNum(bytes,f,0,4);             /* 40-43 */
}

static void CDPCallback(long inpos,int function)
{
  static long c_sector=0,v_sector=0;
  static int last=0;
  static long lasttime=0;
  long sector,osector=0;
  struct timeval thistime;
  static char heartbeat=' ';
  static int overlap=0;
  static int slevel=0;
  static int slast=0;
  static int stimeout=0;
  long test;

  osector=inpos;
  sector=inpos/CD_FRAMEWORDS;
    
  if(function==-2){
    v_sector=sector;
    return;
  }

  if(function==-1){
    last=8;
    heartbeat='*';
    slevel=0;
    v_sector=sector;
  } else
    switch(function){
    case PARANOIA_CB_VERIFY:
      if(stimeout>=30) {
	if(overlap>CD_FRAMEWORDS)
	  slevel=2;
	else
	  slevel=1;
      }
      break;
    case PARANOIA_CB_READ:
      if(sector>c_sector)c_sector=sector;
      break;
      
    case PARANOIA_CB_FIXUP_EDGE:
      if(stimeout>=5) {
	if(overlap>CD_FRAMEWORDS)
	  slevel=2;
	else
	  slevel=1;
      }
      break;
    case PARANOIA_CB_FIXUP_ATOM:
      if(slevel<3 || stimeout>5)slevel=3;
      break;
    case PARANOIA_CB_READERR:
      slevel=6;
      break;
    case PARANOIA_CB_SKIP:
      slevel=8;
      break;
    case PARANOIA_CB_OVERLAP:
      overlap=osector;
      break;
    case PARANOIA_CB_SCRATCH:
      slevel=7;
      break;
    case PARANOIA_CB_DRIFT:
      if(slevel<4 || stimeout>5)slevel=4;
      break;
    case PARANOIA_CB_FIXUP_DROPPED:
    case PARANOIA_CB_FIXUP_DUPED:
      slevel=5;
      break;
    }
  
  
  gettimeofday(&thistime,NULL);
  test=thistime.tv_sec*10+thistime.tv_usec/100000;
  
  if(lasttime!=test || function==-1 || slast!=slevel){
    if(lasttime!=test || function==-1){
      last++;
      lasttime=test;
      if(last>7)last=0;
      stimeout++;
      switch(last){
      case 0:
	heartbeat=' ';
	break;
      case 1:case 7:
	heartbeat='.';
	break;
      case 2:case 6:
	heartbeat='o';
	break;
      case 3:case 5:  
	heartbeat='0';
	break;
      case 4:
	heartbeat='O';
	break;
      }

      if(function==-1)
	heartbeat='*';
      
    }
    if(slast!=slevel){
      stimeout=0;
    }
    slast=slevel;
  }

  if(slevel<8&&slevel>0) *global_rip_smile_level=slevel-1;
  else *global_rip_smile_level=0;
}

/* Do the replay gain calculation on a sector */
static void GainCalc(char *buffer)
{
  static Float_t l_samples[588];
  static Float_t r_samples[588];
  long count;
  short *data;

  data=(short *)buffer;

  for(count=0;count<588;count++) {
    l_samples[count]=(Float_t)data[count*2];
    r_samples[count]=(Float_t)data[(count*2)+1];
  }

  AnalyzeSamples(l_samples,r_samples,588,2);
}

static long CDPWrite(int outf,char *buffer)
{
  long words=0,temp;
  long num=CD_FRAMESIZE_RAW;

  while(words<num){
    temp=write(outf,buffer+words,num-words);
    if(temp==-1){
      if(errno!=EINTR && errno!=EAGAIN)
	return(-1);
      temp=0;
    }
    words+=temp;
  }

  return(0);
}

gboolean CDPRip(char *device,char *generic_scsi_device,int track,
		long first_sector,long last_sector,
		char *outfile,int paranoia_mode,int *rip_smile_level,
		gfloat *rip_percent_done,gboolean *stop_thread_rip_now,
		gboolean do_gain_calc,FILE *output_fp)
{
  int force_cdrom_endian=-1;
  int force_cdrom_sectors=-1;
  int force_cdrom_overlap=-1;
  int output_endian=0; /* -1=host, 0=little, 1=big */

  /* full paranoia, but allow skipping */
  int out;
  int verbose=CDDA_MESSAGE_FORGETIT;
  int i;
  long cursor,offset;
  cdrom_drive *d=NULL;
  cdrom_paranoia *p=NULL;

  global_rip_smile_level=rip_smile_level;
  global_output_fp=output_fp;

  /* Query the cdrom/disc; */

  if(generic_scsi_device && *generic_scsi_device)
    d=cdda_identify_scsi(generic_scsi_device,device,verbose,NULL);
  else  d=cdda_identify(device,verbose,NULL);
  
  if(!d){
    if(!verbose)
      fprintf(output_fp,"\nUnable to open cdrom drive.\n");

    return FALSE;
  }

  if(verbose)
    cdda_verbose_set(d,CDDA_MESSAGE_PRINTIT,CDDA_MESSAGE_PRINTIT);
  else
    cdda_verbose_set(d,CDDA_MESSAGE_PRINTIT,CDDA_MESSAGE_FORGETIT);

  /* possibly force hand on endianness of drive, sector request size */
  if(force_cdrom_endian!=-1){
    d->bigendianp=force_cdrom_endian;
    switch(force_cdrom_endian){
    case 0:
      fprintf(output_fp,
	      "Forcing CDROM sense to little-endian; ignoring preset and autosense");
      break;
    case 1:
      fprintf(output_fp,
	      "Forcing CDROM sense to big-endian; ignoring preset and autosense");
      break;
    }
  }

  if(force_cdrom_sectors!=-1){
    if(force_cdrom_sectors<0 || force_cdrom_sectors>100){
      fprintf(output_fp,"Default sector read size must be 1<= n <= 100\n");
      cdda_close(d);

      return FALSE;
    }

    fprintf(output_fp,"Forcing default to read %d sectors; "
	    "ignoring preset and autosense",force_cdrom_sectors);

    d->nsectors=force_cdrom_sectors;
    d->bigbuff=force_cdrom_sectors*CD_FRAMESIZE_RAW;
  }

  if(force_cdrom_overlap!=-1){
    if(force_cdrom_overlap<0 || force_cdrom_overlap>75){
      fprintf(output_fp,"Search overlap sectors must be 0<= n <=75\n");
      cdda_close(d);

      return FALSE;
    }

    fprintf(output_fp,"Forcing search overlap to %d sectors; "
	    "ignoring autosense",force_cdrom_overlap);
  }

  switch(cdda_open(d)) {
  case -2:case -3:case -4:case -5:
    fprintf(output_fp,
	    "\nUnable to open disc.  Is there an audio CD in the drive?");
    cdda_close(d);
    return FALSE;
  case -6:
    fprintf(output_fp,
	    "\nCdparanoia could not find a way to read audio from this drive.");
    cdda_close(d);
    return FALSE;
  case 0:
    break;
  default:
    fprintf(output_fp,"\nUnable to open disc.");
    cdda_close(d);
    return FALSE;
  }

  if(d->interface==GENERIC_SCSI && d->bigbuff<=CD_FRAMESIZE_RAW) {
    fprintf(output_fp,
	    "WARNING: You kernel does not have generic SCSI 'SG_BIG_BUFF'\n"
	    "         set, or it is set to a very small value.  Paranoia\n"
	    "         will only be able to perform single sector reads\n"
	    "         making it very unlikely Paranoia can work.\n\n"
	    "         To correct this problem, the SG_BIG_BUFF define\n"
	    "         must be set in /usr/src/linux/include/scsi/sg.h\n"
	    "         by placing, for example, the following line just\n"
	    "         before the last #endif:\n\n"
	    "         #define SG_BIG_BUFF 65536\n\n"
	    "         and then recompiling the kernel.\n\n"
	    "         Attempting to continue...\n\n");
  }
  
  if(d->nsectors==1){
    fprintf(output_fp,
	    "WARNING: The autosensed/selected sectors per read value is\n"
	    "         one sector, making it very unlikely Paranoia can \n"
	    "         work.\n\n"
	    "         Attempting to continue...\n\n");
  }

  if(!cdda_track_audiop(d,track)) {
    fprintf(output_fp,
	    "Selected track is not an audio track. Aborting.\n\n");
    cdda_close(d);
    return FALSE;
  }

  offset=cdda_track_firstsector(d,track);
  first_sector+=offset;
  last_sector+=offset;

  p=paranoia_init(d);
  paranoia_modeset(p,paranoia_mode);

  if(force_cdrom_overlap!=-1) paranoia_overlapset(p,force_cdrom_overlap);
    
  if(verbose)
    cdda_verbose_set(d,CDDA_MESSAGE_LOGIT,CDDA_MESSAGE_LOGIT);
  else
    cdda_verbose_set(d,CDDA_MESSAGE_FORGETIT,CDDA_MESSAGE_FORGETIT);
    
  paranoia_seek(p,cursor=first_sector,SEEK_SET);      
    
  /* this is probably a good idea in general */
  /*  seteuid(getuid());
      setegid(getgid());*/
    
  out=open(outfile,O_RDWR|O_CREAT|O_TRUNC,0666);
  if(out==-1){
    fprintf(output_fp,"Cannot open default output file %s: %s",outfile,
	    strerror(errno));
    cdda_close(d);
    paranoia_free(p);

    return FALSE;
  }
      
  WriteWav(out,(last_sector-first_sector+1)*CD_FRAMESIZE_RAW);
      
  /* Off we go! */
      
  while(cursor<=last_sector){
    /* read a sector */
    gint16 *readbuf=paranoia_read(p,CDPCallback);
    char *err=cdda_errors(d);
    char *mes=cdda_messages(d);

    *rip_percent_done=(gfloat)cursor/(gfloat)last_sector;
	
    if(mes || err)
      fprintf(output_fp,"\r                               "
	      "                                           \r%s%s\n",
	      mes?mes:"",err?err:"");
	
    if(err)free(err);
    if(mes)free(mes);

    if(*stop_thread_rip_now) {
      *stop_thread_rip_now=FALSE;

      cdda_close(d);
      paranoia_free(p);

      return FALSE;
    }


    if(readbuf==NULL){
      fprintf(output_fp,"\nparanoia_read: Unrecoverable error, bailing.\n");
      cursor=last_sector+1;
      paranoia_seek(p,cursor,SEEK_SET);      
      break;
    }
	
    cursor++;
	
    if(output_endian!=bigendianp()){
      for(i=0;i<CD_FRAMESIZE_RAW/2;i++)
	readbuf[i]=swap16(readbuf[i]);
    }
	
    CDPCallback(cursor*(CD_FRAMEWORDS)-1,-2);

    if(do_gain_calc)
      GainCalc((char *)readbuf);
	
    if(CDPWrite(out,(char *)readbuf)){
      fprintf(output_fp,"Error writing output: %s",strerror(errno));
	  
      cdda_close(d);
      paranoia_free(p);

      return FALSE;
    }
	
    if(output_endian!=bigendianp()){
      for(i=0;i<CD_FRAMESIZE_RAW/2;i++)readbuf[i]=swap16(readbuf[i]);
    }
  }
  
  CDPCallback(cursor*(CD_FRAMESIZE_RAW/2)-1,-1);
  close(out);
    
  paranoia_free(p);
  
  cdda_close(d);

  return TRUE;
}

#endif /* CDPAR */
