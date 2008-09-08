
/* $Id: puppy.c,v 1.25 2008/04/10 05:48:02 purbanec Exp $ */

/* Format using indent and the following options:
-bad -bap -bbb -i4 -bli0 -bl0 -cbi0 -cli4 -ss -npcs -nprs -nsaf -nsai -nsaw -nsc -nfca -nut -lp -npsl
*/

/*

  Copyright (C) 2004-2008 Peter Urbanec <toppy at urbanec.net>

  This file is part of puppy.

  puppy is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  puppy is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with puppy; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#define _LARGEFILE64_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>
#include <asm/byteorder.h>

#include "usb_io.h"
#include "tf_bytes.h"
#include "buffer.h"

#include "libpuppy.h"

#define PUT 0
#define GET 1

#define MAX_DEVICES_LINE_SIZE 128

extern time_t timezone;

int quiet = 0;
__u32 cmd = 0;
struct tf_packet packet;
struct tf_packet reply;

int parseArgs(int argc, char *argv[]);
int isToppy(struct usb_device_descriptor *desc);
char *findToppy(void);
void decode_dir(struct tf_packet *p, buffer_t buffer);
void progressStats(__u64 totalSize, __u64 bytes, time_t startTime);
void finalStats(__u64 bytes, time_t startTime);

#define E_INVALID_ARGS 1
#define E_READ_DEVICE 2
#define E_NOT_TF5000PVR 3
#define E_SET_INTERFACE 4
#define E_CLAIM_INTERFACE 5
#define E_DEVICE_LOCK 6
#define E_LOCK_FILE 7
#define E_GLOBAL_LOCK 8
#define E_RESET_DEVICE 9

struct puppy
{
    int lockFd;
    int fd;
    int error;
};

int puppy_ok(puppy_t p)
{
    return p->error == 0;
}

puppy_t puppy_open(const char *devPath)
{
    puppy_t p = malloc(sizeof(struct puppy));
    struct usb_device_descriptor devDesc;

    if(!devPath)
    {
        devPath = getenv("PUPPY");
    }
    if(!devPath)
    {
        devPath = "/dev/puppy";
    }

    p->lockFd = -1;
    p->fd = -1;
    p->error = 0;

    p->lockFd = open("/tmp/puppy", O_CREAT, S_IRUSR | S_IWUSR);
    if(p->lockFd < 0)
    {
        fprintf(stderr, "ERROR: Can not open lock file /tmp/puppy: %s\n",
                strerror(errno));
        p->error = E_LOCK_FILE;
        return p;
    }

    /* Create a lock, so that other instances of puppy can detect this one. */
    if(0 != flock(p->lockFd, LOCK_SH | LOCK_NB))
    {
        fprintf(stderr,
                "ERROR: Can not obtain shared lock on /tmp/puppy: %s\n",
                strerror(errno));
        p->error = E_GLOBAL_LOCK;
        close(p->lockFd);
        p->lockFd = -1;
        return p;
    }

    trace(2, fprintf(stderr, "cmd %04x on %s\n", cmd, devPath));

    if(devPath == NULL)
    {
        devPath = findToppy();
    }

    p->fd = open(devPath, O_RDWR);
    if(p->fd < 0)
    {
        close(p->lockFd);
        p->lockFd = -1;
        fprintf(stderr, "ERROR: Can not open %s for read/write: %s\n",
                devPath, strerror(errno));
        p->error = E_READ_DEVICE;
        return p;
    }

    if(0 != flock(p->fd, LOCK_EX | LOCK_NB))
    {
        close(p->lockFd);
        p->lockFd = -1;
        close(p->fd);
        p->fd = -1;
        fprintf(stderr, "ERROR: Can not get exclusive lock on %s\n", devPath);
        p->error = E_DEVICE_LOCK;
        return p;
    }

    {
        int r = read_device_descriptor(p->fd, &devDesc);

        if(r < 0)
        {
            close(p->lockFd);
            p->lockFd = -1;
            close(p->fd);
            p->fd = -1;
            p->error = E_READ_DEVICE;
            return p;
        }
    }

    if(!isToppy(&devDesc))
    {
        fprintf(stderr, "ERROR: Could not find a Topfield TF5000PVRt\n");
        close(p->lockFd);
        p->lockFd = -1;
        close(p->fd);
        p->fd = -1;
        p->error = E_NOT_TF5000PVR;
        return p;
    }

    trace(1, fprintf(stderr, "Found a Topfield TF5000PVRt\n"));

    trace(2, fprintf(stderr, "USBDEVFS_RESET\n"));
    {
        int r = ioctl(p->fd, USBDEVFS_RESET, NULL);

        if(r < 0)
        {
            fprintf(stderr, "ERROR: Can not reset device: %s\n",
                    strerror(errno));
            close(p->lockFd);
            p->lockFd = -1;
            close(p->fd);
            p->fd = -1;
            p->error = E_RESET_DEVICE;
            return p;
        }
    }

    {
        int interface = 0;
        int r;

        trace(2, fprintf(stderr, "USBDEVFS_CLAIMINTERFACE\n"));
        r = ioctl(p->fd, USBDEVFS_CLAIMINTERFACE, &interface);
        if(r < 0)
        {
            fprintf(stderr, "ERROR: Can not claim interface 0: %s\n",
                    strerror(errno));
            close(p->lockFd);
            p->lockFd = -1;
            close(p->fd);
            p->fd = -1;
            p->error = E_CLAIM_INTERFACE;
        }
    }

    {
        struct usbdevfs_setinterface interface0 = { 0, 0 };
        int r;

        trace(2, fprintf(stderr, "USBDEVFS_SETNTERFACE\n"));
        r = ioctl(p->fd, USBDEVFS_SETINTERFACE, &interface0);
        if(r < 0)
        {
            fprintf(stderr, "ERROR: Can not set interface zero: %s\n",
                    strerror(errno));
            close(p->lockFd);
            p->lockFd = -1;
            close(p->fd);
            p->fd = -1;
            p->error = E_SET_INTERFACE;
            return p;
        }
    }

    return p;
}

void puppy_done(puppy_t p)
{
    if(p->fd > -1)
    {
        int interface = 0;

        ioctl(p->fd, USBDEVFS_RELEASEINTERFACE, &interface);
        close(p->fd);
        close(p->lockFd);

        free(p);
    }
}

int puppy_turbo(puppy_t p, int state)
{
    int r;
    int turbo_on = state;

    r = send_cmd_turbo(p->fd, turbo_on);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }

    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            trace(1,
                  fprintf(stderr, "Turbo mode: %s\n",
                          turbo_on ? "ON" : "OFF"));
            return 0;
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
    return -EPROTO;
}

int puppy_reset(puppy_t p)
{
    int r;

    r = send_cmd_reset(p->fd);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }

    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            printf("TF5000PVRt should now reboot\n");
            return 0;
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
    return -EPROTO;
}

int puppy_ready(puppy_t p)
{
    int r;

    r = send_cmd_ready(p->fd);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }

    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            printf("Device reports ready.\n");
            return 0;
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            get_u32(&reply.data);
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
            return -1;
    }
    return -EPROTO;
}

int puppy_cancel(puppy_t p)
{
    int r;

    r = send_cancel(p->fd);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }

    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            printf("In progress operation cancelled\n");
            return 0;
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
    return -EPROTO;
}

int puppy_hdd_size(puppy_t p)
{
    int r;

    r = send_cmd_hdd_size(p->fd);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }

    switch (get_u32(&reply.cmd))
    {
        case DATA_HDD_SIZE:
        {
            __u32 totalk = get_u32(&reply.data);
            __u32 freek = get_u32(&reply.data[4]);

            printf("Total %10u kiB %7u MiB %4u GiB\n", totalk, totalk / 1024,
                   totalk / (1024 * 1024));
            printf("Free  %10u kiB %7u MiB %4u GiB\n", freek, freek / 1024,
                   freek / (1024 * 1024));
            return 0;
            break;
        }

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
    return -EPROTO;
}

puppy_dir_entry_t *puppy_hdd_dir(puppy_t p, char *path)
{
    int r;
    buffer_t buf = buffer_init(10, sizeof(struct puppy_dir_entry));
    int finished = 0;

    r = send_cmd_hdd_dir(p->fd, path);
    if(r < 0)
    {
        p->error = EPROTO;
    }

    while(!finished && p->error == 0 && 0 < get_tf_packet(p->fd, &reply))
    {
        switch (get_u32(&reply.cmd))
        {
            case DATA_HDD_DIR:
                decode_dir(&reply, buf);
                send_success(p->fd);
                break;

            case DATA_HDD_DIR_END:
                finished = 1;
                break;

            case FAIL:
                fprintf(stderr, "ERROR: Device reports %s\n",
                        decode_error(&reply));
                p->error = EPROTO;
                break;

            default:
                fprintf(stderr, "ERROR: Unhandled packet\n");
                p->error = EPROTO;
        }
    }
    {
        puppy_dir_entry_t *entry = buffer_add(buf, NULL);
        puppy_dir_entry_t *ptr = buffer_dup_values(buf);

        entry->ftype = PUPPY_UNKNOWN;
        entry->size = 0;
        entry->time = 0;
        entry->name[0] = 0;
        buffer_done(buf);
        return ptr;
    }
}

void decode_dir(struct tf_packet *p, buffer_t buf)
{
    __u16 count =
        (get_u16(&p->length) - PACKET_HEAD_SIZE) / sizeof(struct typefile);
    struct typefile *entries = (struct typefile *) p->data;
    int i;

    for(i = 0; (i < count); i++)
    {
        puppy_dir_entry_t *entry;

        entry = buffer_add(buf, NULL);;

        switch (entries[i].filetype)
        {
            case 1:
                entry->ftype = PUPPY_DIR;
                break;

            case 2:
                entry->ftype = PUPPY_FILE;
                break;

            default:
                entry->ftype = PUPPY_UNKNOWN;
        }
        /* This makes the assumption that the timezone of the Toppy and the system
         * that puppy runs on are the same. Given the limitations on the length of
         * USB cables, this condition is likely to be satisfied. */
        entry->time = tfdt_to_time(&entries[i].stamp);
        entry->size = get_u64(&entries[i].size);
        strcpy(entry->name, (char *) entries[i].name);

    }
}

int puppy_hdd_file_put(puppy_t p, char *srcPath, char *dstPath)
{
    int result = -EPROTO;
    time_t startTime = time(NULL);
    enum
    {
        START,
        DATA,
        END,
        FINISHED
    } state;
    int src = -1;
    int r;
    int update = 0;
    struct stat64 srcStat;
    __u64 fileSize;
    __u64 byteCount = 0;

    trace(4, fprintf(stderr, "%s\n", __func__));

    src = open64(srcPath, O_RDONLY);
    if(src < 0)
    {
        fprintf(stderr, "ERROR: Can not open source file: %s\n",
                strerror(errno));
        return errno;
    }

    if(0 != fstat64(src, &srcStat))
    {
        fprintf(stderr, "ERROR: Can not examine source file: %s\n",
                strerror(errno));
        result = errno;
        goto out;
    }

    fileSize = srcStat.st_size;
    if(fileSize == 0)
    {
        fprintf(stderr, "ERROR: Source file is empty - not transfering.\n");
        result = -ENODATA;
        goto out;
    }

    r = send_cmd_hdd_file_send(p->fd, PUT, dstPath);
    if(r < 0)
    {
        goto out;
    }

    state = START;
    while(0 < get_tf_packet(p->fd, &reply))
    {
        update = (update + 1) % 16;
        switch (get_u32(&reply.cmd))
        {
            case SUCCESS:
                switch (state)
                {
                    case START:
                    {
                        /* Send start */
                        struct typefile *tf = (struct typefile *) packet.data;

                        put_u16(&packet.length, PACKET_HEAD_SIZE + 114);
                        put_u32(&packet.cmd, DATA_HDD_FILE_START);
                        time_to_tfdt(srcStat.st_mtime, &tf->stamp);
                        tf->filetype = 2;
                        put_u64(&tf->size, srcStat.st_size);
                        strncpy((char *) tf->name, dstPath, 94);
                        tf->name[94] = '\0';
                        tf->unused = 0;
                        tf->attrib = 0;
                        trace(3,
                              fprintf(stderr, "%s: DATA_HDD_FILE_START\n",
                                      __func__));
                        r = send_tf_packet(p->fd, &packet);
                        if(r < 0)
                        {
                            fprintf(stderr, "ERROR: Incomplete send.\n");
                            goto out;
                        }
                        state = DATA;
                        break;
                    }

                    case DATA:
                    {
                        int payloadSize = sizeof(packet.data) - 9;
                        ssize_t w = read(src, &packet.data[8], payloadSize);

                        /* Detect a Topfield protcol bug and prevent the sending of packets
                           that are a multiple of 512 bytes. */
                        if((w > 4)
                           &&
                           (((((PACKET_HEAD_SIZE + 8 + w) +
                               1) & ~1) % 0x200) == 0))
                        {
                            lseek64(src, -4, SEEK_CUR);
                            w -= 4;
                            payloadSize -= 4;
                        }

                        put_u16(&packet.length, PACKET_HEAD_SIZE + 8 + w);
                        put_u32(&packet.cmd, DATA_HDD_FILE_DATA);
                        put_u64(packet.data, byteCount);
                        byteCount += w;

                        /* Detect EOF and transition to END */
                        if((w < 0) || (byteCount >= fileSize))
                        {
                            state = END;
                        }

                        if(w > 0)
                        {
                            trace(3,
                                  fprintf(stderr, "%s: DATA_HDD_FILE_DATA\n",
                                          __func__));
                            r = send_tf_packet(p->fd, &packet);
                            if(r < w)
                            {
                                fprintf(stderr, "ERROR: Incomplete send.\n");
                                goto out;
                            }
                        }

                        if(!update && !quiet)
                        {
                            progressStats(fileSize, byteCount, startTime);
                        }
                        break;
                    }

                    case END:
                        /* Send end */
                        put_u16(&packet.length, PACKET_HEAD_SIZE);
                        put_u32(&packet.cmd, DATA_HDD_FILE_END);
                        trace(3,
                              fprintf(stderr, "%s: DATA_HDD_FILE_END\n",
                                      __func__));
                        r = send_tf_packet(p->fd, &packet);
                        if(r < 0)
                        {
                            fprintf(stderr, "ERROR: Incomplete send.\n");
                            goto out;
                        }
                        state = FINISHED;
                        break;

                    case FINISHED:
                        result = 0;
                        goto out;
                        break;
                }
                break;

            case FAIL:
                fprintf(stderr, "ERROR: Device reports %s\n",
                        decode_error(&reply));
                goto out;
                break;

            default:
                fprintf(stderr, "ERROR: Unhandled packet\n");
                break;
        }
    }
    finalStats(byteCount, startTime);

  out:
    close(src);
    return result;
}

int puppy_hdd_file_get(puppy_t p, char *srcPath, char *dstPath)
{
    int result = -EPROTO;
    time_t startTime = time(NULL);
    enum
    {
        START,
        DATA,
        ABORT
    } state;
    int dst = -1;
    int r;
    int update = 0;
    __u64 byteCount = 0;
    struct utimbuf mod_utime_buf = { 0, 0 };

    dst = open64(dstPath, O_WRONLY | O_CREAT | O_TRUNC,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if(dst < 0)
    {
        fprintf(stderr, "ERROR: Can not open destination file: %s\n",
                strerror(errno));
        return errno;
    }

    r = send_cmd_hdd_file_send(p->fd, GET, srcPath);
    if(r < 0)
    {
        goto out;
    }

    state = START;
    while(0 < (r = get_tf_packet(p->fd, &reply)))
    {
        update = (update + 1) % 16;
        switch (get_u32(&reply.cmd))
        {
            case DATA_HDD_FILE_START:
                if(state == START)
                {
                    struct typefile *tf = (struct typefile *) reply.data;

                    byteCount = get_u64(&tf->size);
                    mod_utime_buf.actime = mod_utime_buf.modtime =
                        tfdt_to_time(&tf->stamp);

                    send_success(p->fd);
                    state = DATA;
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: Unexpected DATA_HDD_FILE_START packet in state %d\n",
                            state);
                    send_cancel(p->fd);
                    state = ABORT;
                }
                break;

            case DATA_HDD_FILE_DATA:
                if(state == DATA)
                {
                    __u64 offset = get_u64(reply.data);
                    __u16 dataLen =
                        get_u16(&reply.length) - (PACKET_HEAD_SIZE + 8);
                    ssize_t w;

                    if(!update && !quiet)
                    {
                        progressStats(byteCount, offset + dataLen, startTime);
                    }

                    if(r < get_u16(&reply.length))
                    {
                        fprintf(stderr,
                                "ERROR: Short packet %d instead of %d\n", r,
                                get_u16(&reply.length));
                        /* TODO: Fetch the rest of the packet */
                    }

                    w = write(dst, &reply.data[8], dataLen);
                    if(w < dataLen)
                    {
                        /* Can't write data - abort transfer */
                        fprintf(stderr, "ERROR: Can not write data: %s\n",
                                strerror(errno));
                        send_cancel(p->fd);
                        state = ABORT;
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: Unexpected DATA_HDD_FILE_DATA packet in state %d\n",
                            state);
                    send_cancel(p->fd);
                    state = ABORT;
                }
                break;

            case DATA_HDD_FILE_END:
                send_success(p->fd);
                result = 0;
                goto out;
                break;

            case FAIL:
                fprintf(stderr, "ERROR: Device reports %s\n",
                        decode_error(&reply));
                send_cancel(p->fd);
                state = ABORT;
                break;

            case SUCCESS:
                goto out;
                break;

            default:
                fprintf(stderr, "ERROR: Unhandled packet (cmd 0x%x)\n",
                        get_u32(&reply.cmd));
        }
    }
    utime(dstPath, &mod_utime_buf);
    finalStats(byteCount, startTime);

  out:
    close(dst);
    return result;
}

int puppy_hdd_del(puppy_t p, char *path)
{
    int r;

    r = send_cmd_hdd_del(p->fd, path);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }
    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            return 0;
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
    return -EPROTO;
}

int puppy_hdd_rename(puppy_t p, char *srcPath, char *dstPath)
{
    int r;

    r = send_cmd_hdd_rename(p->fd, srcPath, dstPath);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }
    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            return 0;
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
    return -EPROTO;
}

int puppy_hdd_mkdir(puppy_t p, char *path)
{
    int r;

    r = send_cmd_hdd_create_dir(p->fd, path);
    if(r < 0)
    {
        return -EPROTO;
    }

    r = get_tf_packet(p->fd, &reply);
    if(r < 0)
    {
        return -EPROTO;
    }
    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            return 0;
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n",
                    decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
    return -EPROTO;
}

void progressStats(__u64 totalSize, __u64 bytes, time_t startTime)
{
    int delta = time(NULL) - startTime;

    if(quiet)
        return;

    if(delta > 0)
    {
        double rate = bytes / delta;
        int eta = (totalSize - bytes) / rate;

        fprintf(stderr,
                "\r%6.2f%%, %5.2f Mbits/s, %02d:%02d:%02d elapsed, %02d:%02d:%02d remaining",
                100.0 * ((double) (bytes) / (double) totalSize),
                ((bytes * 8.0) / delta) / (1000 * 1000), delta / (60 * 60),
                (delta / 60) % 60, delta % 60, eta / (60 * 60),
                (eta / 60) % 60, eta % 60);
    }
}

void finalStats(__u64 bytes, time_t startTime)
{
    int delta = time(NULL) - startTime;

    if(quiet)
        return;

    if(delta > 0)
    {
        fprintf(stderr, "\n%.2f Mbytes in %02d:%02d:%02d (%.2f Mbits/s)\n",
                (double) bytes / (1000.0 * 1000.0),
                delta / (60 * 60), (delta / 60) % 60, delta % 60,
                ((bytes * 8.0) / delta) / (1000.0 * 1000.0));
    }
}

int isToppy(struct usb_device_descriptor *desc)
{
    return (desc->idVendor == 0x11db) && (desc->idProduct == 0x1000);
}

char *findToppy(void)
{
    FILE *toppy;
    char buffer[MAX_DEVICES_LINE_SIZE];
    int bus, device;
    unsigned int vendor, prodid;
    int found = 0;
    static char pathBuffer[32];

    /* Open the /proc/bus/usb/devices file, and read it to find candidate Topfield devices. */
    if((toppy = fopen("/proc/bus/usb/devices", "r")) == NULL)
    {
        fprintf(stderr,
                "ERROR: Can not perform autodetection.\n"
                "ERROR: /proc/bus/usb/devices can not be open for reading.\n"
                "ERROR: %s\n", strerror(errno));
        return NULL;
    }

    /* Scan the devices file, line by line, looking for Topfield devices. */
    while(fgets(buffer, MAX_DEVICES_LINE_SIZE, toppy))
    {

        /* Store the information from topology lines. */
        if(sscanf
           (buffer, "T: Bus=%d Lev=%*d Prnt=%*d Port=%*d Cnt=%*d Dev#=%d",
            &bus, &device))
        {
            trace(2,
                  fprintf(stderr, "Found USB device at bus=%d, device=%d\n",
                          bus, device));
        }

        /* Look for Topfield vendor/product lines, and also check for multiple devices. */
        else if(sscanf(buffer, "P: Vendor=%x ProdID=%x", &vendor, &prodid)
                && (vendor == 0x11db) && (prodid == 0x1000))
        {
            trace(1,
                  fprintf(stderr,
                          "Recognised Topfield device at bus=%d, device=%d\n",
                          bus, device));

            /* If we've already found one, then there are multiple devices present. */
            if(found)
            {
                fprintf(stderr,
                        "ERROR: Multiple Topfield devices recognised.\n"
                        "ERROR: Please use the -d option to specify a device.\n");
                fclose(toppy);
                return NULL;
            }

            /* Construct the device path according to the topology found. */
            sprintf(pathBuffer, "/proc/bus/usb/%03d/%03d", bus, device);
            found = 1;
        }
    }

    fclose(toppy);

    if(found)
    {
        return pathBuffer;
    }
    else
    {
        fprintf(stderr, "ERROR: Can not autodetect a Topfield TF5000PVRt\n");
        return NULL;
    }
}
