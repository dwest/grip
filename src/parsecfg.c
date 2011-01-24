/* parsecfg.c
 *
 * Copyright (c) 1998-200r  Mike Oliphant <grip@nostatic.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "grip.h"
#include "parsecfg.h"

static gboolean ParseLine(char *buf,CFGEntry *cfg);

static gboolean ParseLine(char *buf,CFGEntry *cfg)
{
  int cfgent;
  gboolean found=FALSE;
  char *tok;

  tok=strtok(buf," ");

  if(tok)
    for(cfgent=0;cfg[cfgent].type!=CFG_ENTRY_LAST;cfgent++) {
      if(!strcasecmp(tok,cfg[cfgent].name)) {
	tok=strtok(NULL,"");

	if(tok)
	  switch(cfg[cfgent].type) {
	  case CFG_ENTRY_STRING:
	    strncpy((char *)cfg[cfgent].destvar,
		    g_strstrip(tok),cfg[cfgent].length);
	    break;
	  case CFG_ENTRY_BOOL:
	    *((gboolean *)cfg[cfgent].destvar)=(atoi(tok)==1);
	    break;
	  case CFG_ENTRY_INT:
	    *((int *)cfg[cfgent].destvar)=atoi(tok);
	    break;
	  default:
	    g_print(_("Error: Bad entry type\n"));
	    break;
	  }
	
	found=TRUE;
	break;
      }
    }

  return found;
}

int LoadConfig(char *filename,char *name,int ver,int reqver,CFGEntry *cfg)
{
  char buf[1024];
  FILE *cfp;
  char *tok;

  cfp=fopen(filename,"r");
  if(!cfp) return 0;

  fgets(buf,1024,cfp);

  tok=strtok(buf," ");

  if(!tok||(strcasecmp(tok,name))) {
    g_print(_("Error: Invalid config file\n"));

    return -1;
  }

  tok=strtok(NULL,"");

  if(!tok||(atoi(tok)<reqver)) {
    return -2;
  }

  while(fgets(buf,1024,cfp)) {
    ParseLine(buf,cfg);
  }

  fclose(cfp);

  return 1;
}

gboolean SaveConfig(char *filename,char *name,int ver,CFGEntry *cfg)
{
  FILE *cfp;
  int cfgent;

  cfp=fopen(filename,"w");

  if(!cfp) return FALSE;

  fprintf(cfp,"%s %d\n",name,ver);

  for(cfgent=0;cfg[cfgent].type!=CFG_ENTRY_LAST;cfgent++) {
    fprintf(cfp,"%s ",cfg[cfgent].name);

    switch(cfg[cfgent].type) {
    case CFG_ENTRY_STRING:
      fprintf(cfp,"%s\n",(char *)cfg[cfgent].destvar);
      break;
    case CFG_ENTRY_INT:
      fprintf(cfp,"%d\n",*((int *)cfg[cfgent].destvar));
      break;
    case CFG_ENTRY_BOOL:
      fprintf(cfp,"%d\n",*((gboolean *)cfg[cfgent].destvar)==1);
      break;
    default:
      break;
    }
  }

  fclose(cfp);

  return TRUE;
}
