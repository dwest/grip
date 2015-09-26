/* launch.c
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

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "grip.h"
#include "common.h"
#include "launch.h"

/* Split a string into an array of arguments */
int MakeArgs(char *str,GString **args,int maxargs)
{
  int arg;
  gboolean inquotes=FALSE;
  gboolean escaped=FALSE;
  gboolean inspace=TRUE;

  for(arg=0;;str++) {
    if(inspace && *str && *str!=' ') {
      args[arg]=g_string_new(NULL);
      inspace=FALSE;
    }

    if(inspace && *str==' ') continue;

    if(!escaped && *str=='\\') {
      escaped=TRUE;
      continue;
    }

    if(!escaped && *str=='"') {
      inquotes=!inquotes;
      continue;
    }

    /* A null or a space outside quotes indicates the end of an argument */
    if(!*str || (!inquotes && *str==' ')) {
      inquotes=escaped=FALSE;

      if(!inspace) arg++;

      /* Check to see if we are finished */
      if(!*str || !*(str+1) || arg==(maxargs-1)) break;

      inspace=TRUE;

      continue;
    }

    g_string_append_c(args[arg],*str);

    escaped=FALSE;
  }

  args[arg]=NULL;

  return arg;
}

/* Translate all '%' switches in a string using the specified function */
void TranslateString(char *instr,GString *outstr,
		     char *(*trans_func)(char,void *,gboolean *),
		     void *user_data,gboolean do_munge_default,
		     StrTransPrefs *prefs)
{
  gboolean do_munge;
  char *trans_result;
  char *tok;
  struct passwd *pwd;
  char *munge_str;

  if(*instr=='~') {
    instr++;
    
    /* Expand ~ in dir -- modeled loosely after code from gtkfind by
       Matthew Grossman */

    if((*instr=='\0') || (*instr=='/')) {   /* This user's dir */
      g_string_sprintf(outstr,"%s",getenv("HOME"));
    }
    else {  /* Another user's dir */
      tok=strchr(instr,'/');
      
      if(tok) {   /* Ugly, but it works */
	*tok='\0';
	pwd=getpwnam(instr);
	instr+=strlen(instr);
	*tok='/';
      }
      else {
	pwd=getpwnam(instr);
	instr+=strlen(instr);
      }

      if(!pwd)
	g_print(_("Error: unable to translate filename. No such user as %s\n"),
	       tok);
      else {
	g_string_sprintf(outstr,"%s",pwd->pw_dir);
      }
    }
  }

  for(;*instr;instr++) {
    do_munge=do_munge_default;

    if(*instr=='%') {
      instr++;

      if(*instr=='*') {
	do_munge=FALSE;
	instr++;
      }

      if(*instr=='!') {
	do_munge=TRUE;
	instr++;
      }

      if(*instr=='%')
	g_string_append_c(outstr,*instr);
      else {
	trans_result=trans_func(*instr,user_data,&do_munge);

	if(do_munge && (munge_str=MungeString(trans_result,prefs))) {
          g_string_append(outstr,munge_str);
          
          free(munge_str);
        }
        else
          g_string_append(outstr,trans_result);
      }
    }
    else {
      g_string_append_c(outstr,*instr);
    }
  }
}

/*
  Concatenate two strings with reallocation
*/

char *ReallocStrcat(char *dest, const char *src)
{
  if(src) {
    dest = g_realloc (dest, strlen(dest)+strlen(src)+sizeof(char));
    if (dest) {
      strcat(dest,src);
    }
  }
  return dest;
}

/*
  Munge a string to be suitable for filenames
*/

char *MungeString(char *str,StrTransPrefs *prefs)
{
  gunichar *src, *c;
  gchar *filename_char;
  char *dst;
  glong ri,wi;
  gsize rb,wb;
  char escape[11];
  char utf8_char[7];
  gint utf8_char_len;

  src=g_utf8_to_ucs4(str,strlen(str),&ri,&wi,NULL);
  if(!src) {
    return NULL;
  }
  
  dst=g_strdup("");
  for(c=src;*c;c++) {
    utf8_char_len=g_unichar_to_utf8(prefs->no_lower_case?
				    *c:g_unichar_tolower(*c),utf8_char);
    utf8_char[utf8_char_len]='\0';
    filename_char=g_filename_from_utf8(utf8_char,-1,&rb,&wb,NULL);
    g_free(filename_char);
    if (!filename_char || (!prefs->allow_high_bits && *c >> 7)) {
      if (prefs->escape) {
	g_snprintf(escape,11,"(%x)",*c);
	dst=ReallocStrcat(dst,escape);
      }
    }
    else if(*c==' ') {
      dst=ReallocStrcat(dst,prefs->no_underscore?" ":"_");
    }
    else if(!g_unichar_isalnum(*c)&&
	    !g_utf8_strchr(prefs->allow_these_chars,
	    strlen(prefs->allow_these_chars),*c)) {
      continue;
    }
    else {
      dst=ReallocStrcat(dst,utf8_char);
    }
  }
  
  g_free(src);
  
  return dst;
}

int MakeTranslatedArgs(char *str,GString **args,int maxargs,
		       char *(*trans_func)(char,void *,gboolean *),
		       void *user_data,gboolean do_munge_default,
		       StrTransPrefs *prefs)
{
  int num_args;
  int arg;
  GString *out,*tmp;

  num_args=MakeArgs(str,args,maxargs);

  for(arg=0;args[arg];arg++) {
    out=g_string_new(NULL);

    TranslateString(args[arg]->str,out,trans_func,user_data,
		    do_munge_default,prefs);

    tmp=args[arg];
    args[arg]=out;
    g_string_free(tmp,TRUE);
  }

  return num_args;
}

extern char *FindRoot(char *);

/*
void ArgsToLocale(GString **args)
{
  char *new_str;
  GString *new_arg;
  int pos;
  int len;

  for(pos=1;args[pos];pos++) {
    new_str=g_locale_from_utf8(args[pos]->str,-1,NULL,&len,NULL);

    if(new_str) {
      new_arg=g_string_new(new_str);
      
      g_string_free(args[pos],TRUE);
      
      args[pos]=new_arg;
    }
    g_free(new_str);
  }
}
*/

void TranslateAndLaunch(char *cmd,char *(*trans_func)(char,void *,gboolean *),
			void *user_data,gboolean do_munge_default,
			StrTransPrefs *prefs,void (*close_func)(void *),
			void *close_user_data)
{
  GString *args[100];
  char *char_args[21];
  int pid;
  int arg;

  MakeTranslatedArgs(cmd,args,100,trans_func,user_data,do_munge_default,prefs);

/*
  ArgsToLocale(args);
*/

  for(arg=1;args[arg];arg++) {
    char_args[arg]=args[arg]->str;
  }

  char_args[arg]=NULL;
  
  char_args[0]=FindRoot(args[0]->str);

  pid=fork();
  
  if(pid==0) {
    if(close_func) close_func(close_user_data);

    execv(args[0]->str,char_args);
    
    Debug(_("Exec failed\n"));
    _exit(0);
  }
  
  waitpid(pid,NULL,0);

  for(arg=0;args[arg];arg++) {
    g_string_free(args[arg],TRUE);
  }
}
