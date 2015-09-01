/* cddev.c
 *
 * Based on code from libcdaudio 0.5.0 (Copyright (C)1998 Tony Arcieri)
 *
 * All changes copyright (c) 1998-2004  Mike Oliphant <grip@nostatic.org>
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <config.h>
#include <gio/gio.h>
#include "cddev.h"
#include "common.h"

/* Static callback functions for GIO drive operations */
static void CDEjectFinish(GObject *source, GAsyncResult *result, gpointer user_data);

static GVolumeMonitor *monitor = NULL;

/* We can check to see if the CD-ROM is mounted if this is available */
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif

/* For Linux */
#ifdef HAVE_LINUX_CDROM_H
#include <linux/cdrom.h>
#define NON_BLOCKING
#endif

/* For FreeBSD, OpenBSD, and Solaris */
#ifdef HAVE_SYS_CDIO_H
#include <sys/cdio.h>
#define NON_BLOCKING
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)
#define CDIOREADSUBCHANNEL CDIOCREADSUBCHANNEL
#endif

/* For Digital UNIX */
#ifdef HAVE_IO_CAM_CDROM_H
#include <io/cam/cdrom.h>
#endif

/* Initialize the CD-ROM for playing audio CDs */
gboolean CDInitDevice(char *device_name,DiscInfo *disc)
{
    GVolume *vol = GripGetVolumeByPath(device_name);
    if(vol != NULL){
        disc->volume = vol;
    }

    struct stat st;
#ifdef HAVE_MNTENT_H
    FILE *mounts;
    struct mntent *mnt;
    char devname[256];
#endif
#ifndef NON_BLOCKING
    const int OPEN_MODE = O_RDONLY;
#else
    const int OPEN_MODE = O_RDONLY|O_NONBLOCK;
#endif

    disc->have_info=FALSE;
    disc->disc_present=FALSE;

    if(lstat(device_name,&st)<0)
        return FALSE;
   
#ifdef HAVE_MNTENT_H
    if(S_ISLNK(st.st_mode))
        readlink(device_name,devname,256);
    else
        strncpy(devname,device_name,256);

    if((mounts=setmntent(MOUNTED, "r"))==NULL)
        return FALSE;
      
    while((mnt=getmntent(mounts))!=NULL) {
        if(strcmp(mnt->mnt_fsname,devname)==0) {
            endmntent(mounts);
            errno = EBUSY;
            return FALSE;
        }
    }

    endmntent(mounts);
#endif

    if (disc->devname 
        && disc->devname != device_name
        && strcmp(device_name, disc->devname)) {
        free(disc->devname);
        disc->devname = 0;
    }
    if (!disc->devname) {
        disc->devname = strdup(device_name);
    }

    disc->cd_desc=open(device_name, OPEN_MODE);

    if(disc->cd_desc==-1) return FALSE;

    return TRUE;
}

gboolean CDCloseDevice(DiscInfo *disc)
{
    close(disc->cd_desc);
    disc->cd_desc = -1;

    return TRUE;
}

/* Update a CD status structure... because operating system interfaces vary
   so does this function. */
gboolean CDStat(DiscInfo *disc,gboolean read_toc)
{
    /* Since every platform does this a little bit differently this gets pretty
       complicated... */
#ifdef CDIOREADSUBCHANNEL
    struct ioc_read_subchannel cdsc;
    struct cd_sub_channel_info data;
#endif
#ifdef CDIOREADTOCENTRYS
    struct cd_toc_entry toc_buffer[MAX_TRACKS];
    struct ioc_read_toc_entry cdte;
#endif
#ifdef CDROMSUBCHNL
    struct cdrom_subchnl cdsc;
#endif
#ifdef CDROM_READ_SUBCHANNEL
    struct cd_sub_channel sch;
#endif
#ifdef CDROMREADTOCHDR
    struct cdrom_tochdr cdth;
#endif
#ifdef CDROMREADTOCENTRY
    struct cdrom_tocentry cdte;
#endif
#ifdef CDIOREADTOCHEADER
    struct ioc_toc_header cdth;
#endif
#ifdef CDROM_DRIVE_STATUS
    int retcode;
#endif

    int readtracks,frame[MAX_TRACKS],pos;

    if (disc->cd_desc < 0) {
        CDInitDevice(disc->devname, disc);
    }
    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDROM_DRIVE_STATUS
    retcode=ioctl(disc->cd_desc,CDROM_DRIVE_STATUS,CDSL_CURRENT);
    Debug(_("Drive status is %d\n"),retcode);
    if(retcode < 0) {
        Debug(_("Drive doesn't support drive status check (assume CDS_NO_INFO)\n"));
    }
    else if(retcode != CDS_DISC_OK && retcode != CDS_NO_INFO) {
        Debug(_("No disc\n"));
        disc->disc_present=FALSE;

        return FALSE;
    }

#endif

#ifdef CDIOREADSUBCHANNEL
    bzero(&cdsc,sizeof(cdsc));
    cdsc.data=&data;
    cdsc.data_len=sizeof(data);
    cdsc.data_format=CD_CURRENT_POSITION;
    cdsc.address_format=CD_MSF_FORMAT;
   
    if(ioctl(disc->cd_desc,CDIOCREADSUBCHANNEL,(char *)&cdsc)<0)
#endif
#ifdef CDROM_READ_SUBCHANNEL
        sch.sch_data_format=CDROM_CURRENT_POSITION;
  
    sch.sch_address_format=CDROM_MSF_FORMAT;
  
    if(ioctl(disc->cd_desc,CDROM_READ_SUBCHANNEL, &sch)<0)
#endif
#ifdef CDROMSUBCHNL
        cdsc.cdsc_format=CDROM_MSF;
  
    if(ioctl(disc->cd_desc,CDROMSUBCHNL,&cdsc)<0)
#endif
        {
            disc->disc_present=FALSE;
      
            return FALSE;
        }
  
#ifdef CDROMSUBCHNL
    if(cdsc.cdsc_audiostatus&&
       (cdsc.cdsc_audiostatus<0x11||cdsc.cdsc_audiostatus>0x15)) {
        disc->disc_present=FALSE;

        return FALSE;
    }
#endif

    disc->disc_present=TRUE;

#ifdef CDIOREADSUBCHANNEL

    disc->disc_time.mins=data.what.position.absaddr.msf.minute;
    disc->disc_time.secs=data.what.position.absaddr.msf.second;   
    disc->curr_frame=(data.what.position.absaddr.msf.minute * 60 +
                      data.what.position.absaddr.msf.second) * 75 +
        data.what.position.absaddr.msf.frame;
   
    switch(data.header.audio_status) {
    case CD_AS_AUDIO_INVALID:
        disc->disc_mode=CDAUDIO_NOSTATUS;
        break;
    case CD_AS_PLAY_IN_PROGRESS:
        disc->disc_mode=CDAUDIO_PLAYING;
        break;
    case CD_AS_PLAY_PAUSED:
        disc->disc_mode=CDAUDIO_PAUSED;
        break;
    case CD_AS_PLAY_COMPLETED:
        disc->disc_mode=CDAUDIO_COMPLETED;
        break;
    case CD_AS_PLAY_ERROR:
        disc->disc_mode=CDAUDIO_NOSTATUS;
        break;
    case CD_AS_NO_STATUS:
        disc->disc_mode=CDAUDIO_NOSTATUS;
    }
#endif
#ifdef CDROMSUBCHNL
    disc->disc_time.mins=cdsc.cdsc_absaddr.msf.minute;
    disc->disc_time.secs=cdsc.cdsc_absaddr.msf.second;
    disc->curr_frame=(cdsc.cdsc_absaddr.msf.minute * 60 +
                      cdsc.cdsc_absaddr.msf.second) * 75 +
        cdsc.cdsc_absaddr.msf.frame;

    switch(cdsc.cdsc_audiostatus) {
    case CDROM_AUDIO_INVALID:
        disc->disc_mode=CDAUDIO_NOSTATUS;
        break;
    case CDROM_AUDIO_PLAY:
        disc->disc_mode=CDAUDIO_PLAYING;
        break;
    case CDROM_AUDIO_PAUSED:
        disc->disc_mode=CDAUDIO_PAUSED;
        break;
    case CDROM_AUDIO_NO_STATUS:
        disc->disc_mode=CDAUDIO_NOSTATUS;
        break;
    case CDROM_AUDIO_COMPLETED:
        disc->disc_mode=CDAUDIO_COMPLETED;
        break;
    }
#endif

    if(read_toc) {
        /* Read the Table Of Contents header */

#ifdef CDIOREADTOCHEADER
        if(ioctl(disc->cd_desc,CDIOREADTOCHEADER,(char *)&cdth)<0) {
            g_print(_("Error: Failed to read disc contents\n"));
      
            return FALSE;
        }
    
        disc->num_tracks=cdth.ending_track;
#endif
#ifdef CDROMREADTOCHDR
        if(ioctl(disc->cd_desc,CDROMREADTOCHDR,&cdth)<0) {
            g_print(_("Error: Failed to read disc contents\n"));

            return FALSE;
        }
    
        disc->num_tracks=cdth.cdth_trk1;
#endif
    
        /* Read the table of contents */
    
#ifdef CDIOREADTOCENTRYS
        cdte.address_format=CD_MSF_FORMAT;
        cdte.starting_track=0;
        cdte.data=toc_buffer;
        cdte.data_len=sizeof(toc_buffer);
    
        if(ioctl(disc->cd_desc,CDIOREADTOCENTRYS,(char *)&cdte)<0) {
            g_print(_("Error: Failed to read disc contents\n"));

            return FALSE;
        }
    
        for(readtracks=0;readtracks<=disc->num_tracks;readtracks++) {
            disc->track[readtracks].start_pos.mins=
                cdte.data[readtracks].addr.msf.minute;
            disc->track[readtracks].start_pos.secs=
                cdte.data[readtracks].addr.msf.second;
            frame[readtracks]=cdte.data[readtracks].addr.msf.frame;

            /* I'm just guessing about this based on cdio.h -- should be tested */
            /* This compiles on freebsd, does it work? */
            disc->track[readtracks].flags=(cdte.data[readtracks].addr_type << 4) |
                (cdte.data[readtracks].control & 0x0f);
        }
#endif
#ifdef CDROMREADTOCENTRY
        for(readtracks=0;readtracks<=disc->num_tracks;readtracks++) {
            if(readtracks==disc->num_tracks)	
                cdte.cdte_track=CDROM_LEADOUT;
            else
                cdte.cdte_track=readtracks+1;
      
            cdte.cdte_format=CDROM_MSF;
            if(ioctl(disc->cd_desc,CDROMREADTOCENTRY,&cdte) < 0) {
                g_print(_("Error: Failed to read disc contents\n"));

                return FALSE;
            }
      
            disc->track[readtracks].start_pos.mins=cdte.cdte_addr.msf.minute;
            disc->track[readtracks].start_pos.secs=cdte.cdte_addr.msf.second;
            frame[readtracks]=cdte.cdte_addr.msf.frame;
      
            disc->track[readtracks].flags=(cdte.cdte_adr << 4) |
                (cdte.cdte_ctrl & 0x0f);
        }
#endif
    
        for(readtracks=0;readtracks<=disc->num_tracks;readtracks++) {
            disc->track[readtracks].start_frame=
                (disc->track[readtracks].start_pos.mins * 60 +
                 disc->track[readtracks].start_pos.secs) * 75 + frame[readtracks];
      
            if(readtracks>0) {
                pos=(disc->track[readtracks].start_pos.mins * 60 +
                     disc->track[readtracks].start_pos.secs) -
                    (disc->track[readtracks-1].start_pos.mins * 60 +
                     disc->track[readtracks -1].start_pos.secs);

                /* Compensate for the gap before a data track */
                if((readtracks<disc->num_tracks&&
                    IsDataTrack(disc,readtracks)&&
                    pos>152)) {
                    pos-=152;
                }

                disc->track[readtracks - 1].length.mins=pos / 60;
                disc->track[readtracks - 1].length.secs=pos % 60;
            }
        }
    
        disc->length.mins=
            disc->track[disc->num_tracks].start_pos.mins;
    
        disc->length.secs=
            disc->track[disc->num_tracks].start_pos.secs;
    }
   
    disc->curr_track=0;

    while(disc->curr_track<disc->num_tracks &&
          disc->curr_frame>=disc->track[disc->curr_track].start_frame)
        disc->curr_track++;

    pos=(disc->curr_frame-disc->track[disc->curr_track-1].start_frame) / 75;

    disc->track_time.mins=pos/60;
    disc->track_time.secs=pos%60;

    return TRUE;
}

/* Check if a track is a data track */
gboolean IsDataTrack(DiscInfo *disc,int track)
{
    return(disc->track[track].flags & 4);
}

/* Play frames from CD */
gboolean CDPlayFrames(DiscInfo *disc,int startframe,int endframe)
{
#ifdef CDIOCPLAYMSF
    struct ioc_play_msf cdmsf;
#endif
#ifdef CDROMPLAYMSF
    struct cdrom_msf cdmsf;
#endif

    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDIOCPLAYMSF
    cdmsf.start_m=startframe / (60 * 75);
    cdmsf.start_s=(startframe % (60 * 75)) / 75;
    cdmsf.start_f=startframe % 75;
    cdmsf.end_m=endframe / (60 * 75);
    cdmsf.end_s=(endframe % (60 * 75)) / 75;
    cdmsf.end_f=endframe % 75;
#endif
#ifdef CDROMPLAYMSF
    cdmsf.cdmsf_min0=startframe / (60 * 75);
    cdmsf.cdmsf_sec0=(startframe % (60 * 75)) / 75;
    cdmsf.cdmsf_frame0=startframe % 75;
    cdmsf.cdmsf_min1=endframe / (60 * 75);
    cdmsf.cdmsf_sec1=(endframe % (60 * 75)) / 75;
    cdmsf.cdmsf_frame1=endframe % 75;
#endif

#ifdef CDIOCSTART
    if(ioctl(disc->cd_desc,CDIOCSTART)<0)
        return FALSE;
#endif
#ifdef CDROMSTART
    if(ioctl(disc->cd_desc,CDROMSTART)<0)
        return FALSE;
#endif
#ifdef CDIOCPLAYMSF
    if(ioctl(disc->cd_desc,CDIOCPLAYMSF,(char *)&cdmsf)<0)
        return FALSE;
#endif
#ifdef CDROMPLAYMSF
    if(ioctl(disc->cd_desc,CDROMPLAYMSF,&cdmsf)<0)
        return FALSE;
#endif
   
    return TRUE;
}

/* Play starttrack at position pos to endtrack */
gboolean CDPlayTrackPos(DiscInfo *disc,int starttrack,
                        int endtrack,int startpos)
{
    if (disc->cd_desc < 0) {
        return FALSE;
    }

    return CDPlayFrames(disc,disc->track[starttrack-1].start_frame +
                        startpos * 75,endtrack>=disc->num_tracks ?
                        (disc->length.mins * 60 +
                         disc->length.secs) * 75 :
                        disc->track[endtrack].start_frame - 1);
}

/* Play starttrack to endtrack */
gboolean CDPlayTrack(DiscInfo *disc,int starttrack,int endtrack)
{
    return CDPlayTrackPos(disc,starttrack,endtrack,0);
}

/* Advance (fastfwd) */

gboolean CDAdvance(DiscInfo *disc,DiscTime *time)
{
    if (disc->cd_desc < 0) {
        return FALSE;
    }

    disc->track_time.mins += time->mins;
    disc->track_time.secs += time->secs;

    if(disc->track_time.secs > 60) {
        disc->track_time.secs -= 60;
        disc->track_time.mins++;
    }

    if(disc->track_time.secs < 0) {
        disc->track_time.secs = 60 + disc->track_time.secs;
        disc->track_time.mins--;
    }
  
    /*  If we skip back past the beginning of a track, go to the end of
        the last track - DCV */
    if(disc->track_time.mins < 0) {
        disc->curr_track--;
    
        /*  Tried to skip past first track so go to the beginning  */
        if(disc->curr_track == 0) {
            disc->curr_track = 1;
            return CDPlayTrack(disc,disc->curr_track,disc->curr_track);
        }
    
        /*  Go to the end of the last track  */
        disc->track_time.mins=disc->track[(disc->curr_track)-1].
            length.mins;
        disc->track_time.secs=disc->track[(disc->curr_track)-1].
            length.secs;

        /*  Try again  */
        return CDAdvance(disc,time);
    }
   
    if((disc->track_time.mins ==
        disc->track[disc->curr_track].start_pos.mins &&
        disc->track_time.secs >
        disc->track[disc->curr_track].start_pos.secs)
       || disc->track_time.mins>
       disc->track[disc->curr_track].start_pos.mins) {
        disc->curr_track++;

        if(disc->curr_track>disc->num_tracks)
            disc->curr_track=disc->num_tracks;
      
        return CDPlayTrack(disc,disc->curr_track,disc->curr_track);
    }
   
    return CDPlayTrackPos(disc,disc->curr_track,disc->curr_track,
                          disc->track_time.mins * 60 +
                          disc->track_time.secs);
}

/* 
 * Stop the CD, if it is playing 
 */
gboolean CDStop(DiscInfo *disc)
{
    if (disc->cd_desc < 0) {
        return FALSE;
    }


#ifdef CDIOCSTOP
    if(ioctl(disc->cd_desc,CDIOCSTOP)<0)
        return FALSE;
#endif
#ifdef CDROMSTOP
    if(ioctl(disc->cd_desc,CDROMSTOP)<0)
        return FALSE;
#endif
   
    return TRUE;
}

/* Pause the CD */
gboolean CDPause(DiscInfo *disc)
{
    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDIOCPAUSE
    if(ioctl(disc->cd_desc,CDIOCPAUSE)<0)
        return FALSE;
#endif
#ifdef CDROMPAUSE
    if(ioctl(disc->cd_desc,CDROMPAUSE)<0)
        return FALSE;
#endif
   
    return TRUE;
}

/* Resume playing */
gboolean CDResume(DiscInfo *disc)
{
    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDIOCRESUME
    if(ioctl(disc->cd_desc,CDIOCRESUME)<0)
        return FALSE;
#endif
#ifdef CDROMRESUME
    if(ioctl(disc->cd_desc,CDROMRESUME)<0)
        return FALSE;
#endif
   
    return TRUE;
}

/* Check the tray status */
gboolean TrayOpen(DiscInfo *disc)
{
#ifdef CDROM_DRIVE_STATUS
    int status;
#endif

    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDROM_DRIVE_STATUS
    status=ioctl(disc->cd_desc,CDROM_DRIVE_STATUS,CDSL_CURRENT);
    Debug(_("Drive status is %d\n"), status);

    if(status < 0) {
        Debug(_("Drive doesn't support drive status check\n"));
        return FALSE;
    }

    return status==CDS_TRAY_OPEN;
#endif

    return FALSE;
}

/*
 * Eject the CD tray.
 */
gboolean CDEject(DiscInfo *disc)
{  
    if (disc->cd_desc < 0) {
        return FALSE;
    }

    if(disc->volume != NULL){
        g_volume_eject_with_operation(disc->volume, G_MOUNT_UNMOUNT_NONE, 
                                      NULL, NULL, CDEjectFinish, disc);
        return TRUE;
    }
    Debug("CDEject -- No volume to eject.\n");
    // TODO: Do we really need to return anything here?
    return FALSE;
}

/*
 * This is the callback function for g_volume_eject_with_operation inside
 * CDEject. Prints error if volume cannot be ejected.
 */
static void CDEjectFinish(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    g_volume_eject_with_operation_finish((GVolume *)source, result, &error);
    if(error != NULL){
        Debug("CDEject -- %s\n", error->message);
    }else{
        Debug("CD Ejected.\n");
        source = NULL;
    }
}

/* Close the tray */

gboolean CDClose(DiscInfo *disc)
{
    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDIOCCLOSE
    if(ioctl(disc->cd_desc,CDIOCCLOSE)<0)
        return FALSE;
#endif
#ifdef CDROMCLOSETRAY
    if(ioctl(disc->cd_desc,CDROMCLOSETRAY)<0)
        return FALSE;
#endif
   
    return TRUE;
}

gboolean CDGetVolume(DiscInfo *disc,DiscVolume *vol)
{
#ifdef CDIOCGETVOL
    struct ioc_vol volume;
#endif
#ifdef CDROMVOLREAD
    struct cdrom_volctrl volume;
#endif
   
    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDIOCGETVOL
    if(ioctl(disc->cd_desc,CDIOCGETVOL,&volume)<0)
        return FALSE;
   
    vol->vol_front.left=volume.vol[0];
    vol->vol_front.right=volume.vol[1];
    vol->vol_back.left=volume.vol[2];
    vol->vol_back.right=volume.vol[3];
#endif
#ifdef CDROMVOLREAD
    if(ioctl(disc->cd_desc,CDROMVOLREAD, &volume)<0)
        return FALSE;
      
    vol->vol_front.left=volume.channel0;
    vol->vol_front.right=volume.channel1;
    vol->vol_back.left=volume.channel2;
    vol->vol_back.right=volume.channel3;
#endif
   
    return TRUE;
}

gboolean CDSetVolume(DiscInfo *disc,DiscVolume *vol)
{
#ifdef CDIOCSETVOL
    struct ioc_vol volume;
#endif
#ifdef CDROMVOLCTRL
    struct cdrom_volctrl volume;
#endif
   
    if (disc->cd_desc < 0) {
        return FALSE;
    }

    if(vol->vol_front.left > 255 || vol->vol_front.left < 0 ||
       vol->vol_front.right > 255 || vol->vol_front.right < 0 ||
       vol->vol_back.left > 255 || vol->vol_back.left < 0 ||
       vol->vol_back.right > 255 || vol->vol_back.right < 0)
        return -1;

#ifdef CDIOCSETVOL
    volume.vol[0]=vol->vol_front.left;
    volume.vol[1]=vol->vol_front.right;
    volume.vol[2]=vol->vol_back.left;
    volume.vol[3]=vol->vol_back.right;
   
    if(ioctl(disc->cd_desc,CDIOCSETVOL,&volume)<0)
        return FALSE;
#endif
#ifdef CDROMVOLCTRL
    volume.channel0=vol->vol_front.left;
    volume.channel1=vol->vol_front.right;
    volume.channel2=vol->vol_back.left;
    volume.channel3=vol->vol_back.right;
   
    if(ioctl(disc->cd_desc,CDROMVOLCTRL,&volume)<0)
        return FALSE;
#endif
   
    return TRUE;
}


/* CD Changer routines */

/* Choose a particular disc from the CD changer */
gboolean CDChangerSelectDisc(DiscInfo *disc,int disc_num)
{
    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDROM_SELECT_DISC
    if(ioctl(disc->cd_desc,CDROM_SELECT_DISC,disc_num)<0)
        return FALSE;
   
    return TRUE;
#else
    errno = ENOSYS;
   
    return FALSE;
#endif
}

/* Identify how many CD-ROMs the changer can hold */
int CDChangerSlots(DiscInfo *disc)
{
#ifdef CDROM_CHANGER_NSLOTS
    int slots;
#endif

    if (disc->cd_desc < 0) {
        return FALSE;
    }

#ifdef CDROM_CHANGER_NSLOTS
    if((slots=ioctl(disc->cd_desc, CDROM_CHANGER_NSLOTS))<0)
        slots=1;
   
    if(slots==0)
        return 1;
   
    return slots;
#else
    return 1;
#endif
}

/*
 * Loop over all connected drives and find 
 * the one with the matching path.
 */
GVolume *GripGetVolumeByPath(gchar *device_name)
{
    GVolumeMonitor *monitor;
    GVolume *volume         = NULL;
    GList *list;
    gboolean finished       = FALSE;
    gchar *name             = NULL;
    char *devpath;

    devpath = realpath(device_name, NULL);
    if (devpath == NULL) {
        Debug("GripGetVolumeByPath: realpath(%s) failed: %s\n",
	      device_name, strerror(errno));
        return NULL;
    }
    Debug("GripGetVolumByPath: searching for %s\n", devpath);

    monitor = GripGetVolumeMonitor();
    list =    g_volume_monitor_get_volumes(monitor);

    // Loop over volumes and see if any match out needs.
    while(list != NULL && !finished){
        volume = list->data;
        name = g_volume_get_identifier(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        Debug("Considering volume %s\n", name ? name : "<noname>");
        if(name != NULL && strcmp(name, devpath) == 0){
            Debug("Found volume %s.\n", name);
            finished = TRUE;
        }else{
            list = list->next;
        }
    }

    free(devpath);

    // We didn't find proper volume
    if(!finished){
        Debug("No volume found.\n");
        return NULL;
    }else{
        return volume;
    }

    // Cleanup
    list = g_list_first(list);
    g_list_foreach(list, g_object_unref, NULL);
    g_list_free(list);
}

/**
 * This function is called when the GVolumeMonitor emits 
 * the "volume-added" signal. When this happens we want
 * update the GVolume in the DiscInfo struct.
 */
void GripVolumeAdded(GVolumeMonitor *monitor, GVolume *new_volume, gpointer info)
{
    // Make sure that the new volume has the correct path.
    //
    // TODO: Un-hardcode this when Daniel
    //       finished the GConf stuff.
    gchar *device_name = g_volume_get_identifier(new_volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    DiscInfo *disc = info;
    char *devpath;

    if (!disc->devname) {
        Debug("GripVolumeAdded: we have no devicename\n");
        return;
    }

    devpath = realpath(disc->devname, NULL);

    if (devpath == NULL) {
        Debug("GripVolumeAdded: can't find realpath for %s: %s\n",
              disc->devname, strerror(errno));
        devpath = strdup(disc->devname);
        if (devpath == NULL) {
            Debug("and out of memory\n");
            return;
        }
    }

    Debug("GripVolumeAdded: device is %s notification is for %s\n",
          devpath, device_name);
    if(device_name != NULL && strcmp(device_name, devpath) == 0){
        disc->volume = new_volume;
        Debug("Volume added. DiscInfo updated.\n");
    }else{
        Debug("No volume found at %s.\n", devpath);
    }
    free(devpath);
}

/**
 * This function is called when the GVolumeMonitor emits
 * a "volume-removed" signal. We want to set the GVolume
 * in the DiscInfo struct to NULL.
 */
void GripVolumeRemoved(GVolumeMonitor *monitor, GVolume *old_volume, gpointer info)
{
    DiscInfo *disc = info;

    if (old_volume == disc->volume) {
        Debug("Our volume was removed\n");
        disc->volume = NULL;
    } else {
        Debug("Some other volume was removed.\n");
    }
}

// Singleton....?
// TODO: How hacked is this? Are there any other ways?
GVolumeMonitor *GripGetVolumeMonitor()
{
    if(monitor == NULL){
        monitor = g_volume_monitor_get();
    }
    return monitor;
}

