
/* $Id: puppy.c,v 1.19 2005/03/01 13:23:23 purbanec Exp $ */

/* Format using indent and the following options:
-bad -bap -bbb -i4 -bli0 -bl0 -cbi0 -cli4 -ss -npcs -nprs -nsaf -nsai -nsaw -nsc -nfca -nut -lp -npsl
*/

/*

  Copyright (C) 2004 Peter Urbanec <toppy at urbanec.net>

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

#define trace(level, msg) if(verbose >= level) { msg; }
#define PUT 0
#define GET 1

#define MAX_DEVICES_LINE_SIZE 128

extern time_t timezone;

int lockFd = -1;
int verbose = 0;
int quiet = 0;
char *devPath = NULL;
int turbo_req = 0;
__u32 cmd = 0;
char *arg1 = NULL;
char *arg2 = NULL;
__u8 sendDirection = GET;
struct tf_packet packet;
struct tf_packet reply;

int parseArgs(int argc, char *argv[]);
int isToppy(struct usb_device_descriptor *desc);
char *findToppy(void);
void do_cancel(int fd);
void do_cmd_reset(int fd);
void do_hdd_size(int fd);
void do_hdd_dir(int fd, char *path);
void do_hdd_file_put(int fd, char *srcPath, char *dstPath, int turbo_on);
void do_hdd_file_get(int fd, char *srcPath, char *dstPath, int turbo_on);
void decode_dir(struct tf_packet *p);
void do_hdd_del(int fd, char *path);
void do_hdd_rename(int fd, char *srcPath, char *dstPath);
void do_hdd_mkdir(int fd, char *path);
void progressStats(__u64 totalSize, __u64 bytes, time_t startTime);
void finalStats(__u64 bytes, time_t startTime);

int main(int argc, char *argv[])
{
    struct usb_device_descriptor devDesc;
    struct usb_config_descriptor confDesc;
    int fd = -1;
    int r;

    /* Initialise timezone handling. */
    tzset();

    lockFd = open("/tmp/puppy", O_CREAT, S_IRUSR | S_IWUSR);
    if(lockFd < 0)
    {
        fprintf(stderr, "ERROR: Can not open lock file /tmp/puppy: %s\n",
                strerror(errno));
        return 7;
    }
    
    r = parseArgs(argc, argv);
    if(r != 0)
    {
        return 1;
    }

    /* Create a lock, so that other instances of puppy can detect this one. */
    if(0 != flock(lockFd, LOCK_SH | LOCK_NB))
    {
        fprintf(stderr, "ERROR: Can not obtain shared lock on /tmp/puppy: %s\n",
                strerror(errno));
        return 8;
    }

    trace(1, fprintf(stderr, "cmd %04x on %s\n", cmd, devPath));

    fd = open(devPath, O_RDWR);
    if(fd < 0)
    {
        fprintf(stderr, "ERROR: Can not open %s for read/write: %s\n",
                devPath, strerror(errno));
        return 1;
    }
    
    if(0 != flock(fd, LOCK_EX | LOCK_NB))
    {
        fprintf(stderr, "ERROR: Can not get exclusive lock on %s\n", devPath);
        close(fd);
        return 6;
    }

    {
        int interface = 0;

        r = ioctl(fd, USBDEVFS_CLAIMINTERFACE, &interface);
        if(r < 0)
        {
            fprintf(stderr, "ERROR: Can not claim interface 0: %s\n",
                    strerror(errno));
            close(fd);
            return 5;
        }
    }

    r = read_device_descriptor(fd, &devDesc);
    if(r < 0)
    {
        close(fd);
        return 2;
    }

    if(!isToppy(&devDesc))
    {
        fprintf(stderr, "ERROR: Could not find a Topfield TF5000PVRt\n");
        close(fd);
        return 3;
    }

    trace(1, fprintf(stderr, "Found a Topfield TF5000PVRt\n"));

    r = read_config_descriptor(fd, &confDesc);
    if(r < 0)
    {
        close(fd);
        return 4;
    }

    switch (cmd)
    {
        case CANCEL:
            do_cancel(fd);
            break;

        case CMD_RESET:
            do_cmd_reset(fd);
            break;

        case CMD_HDD_SIZE:
            do_hdd_size(fd);
            break;

        case CMD_HDD_DIR:
            do_hdd_dir(fd, arg1);
            break;

        case CMD_HDD_FILE_SEND:
            if(sendDirection == PUT)
            {
                do_hdd_file_put(fd, arg1, arg2, turbo_req);
            }
            else
            {
                do_hdd_file_get(fd, arg1, arg2, turbo_req);
            }
            break;

        case CMD_HDD_DEL:
            do_hdd_del(fd, arg1);
            break;

        case CMD_HDD_RENAME:
            do_hdd_rename(fd, arg1, arg2);
            break;

        case CMD_HDD_CREATE_DIR:
            do_hdd_mkdir(fd, arg1);
            break;

        default:
            fprintf(stderr, "BUG: Command 0x%08x not implemented\n", cmd);
    }
    return 0;
}

void switch_turbo(int fd, int turbo_on)
{
    int r;

    r = send_cmd_turbo(fd, turbo_on);
    if(r < 0)
    {
        return;
    }

    r = get_tf_packet(fd, &reply);
    if(r < 0)
    {
        return;
    }

    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            trace(1,
                  fprintf(stderr, "Turbo mode: %s\n",
                          turbo_on ? "On" : "Off"));
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
}

void do_cmd_reset(int fd)
{
    int r;

    r = send_cmd_reset(fd);
    if(r < 0)
    {
        return;
    }

    r = get_tf_packet(fd, &reply);
    if(r < 0)
    {
        return;
    }

    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            printf("TF5000PVRt should now reboot\n");
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
}

void do_cancel(int fd)
{
    int r;

    r = send_cancel(fd);
    if(r < 0)
    {
        return;
    }

    r = get_tf_packet(fd, &reply);
    if(r < 0)
    {
        return;
    }

    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            printf("In progress operation cancelled\n");
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
}

void do_hdd_size(int fd)
{
    int r;

    r = send_cmd_hdd_size(fd);
    if(r < 0)
    {
        return;
    }

    r = get_tf_packet(fd, &reply);
    if(r < 0)
    {
        return;
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
            break;
        }

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
}

void do_hdd_dir(int fd, char *path)
{
    char getAnotherPacket = 1;
    int r;

    r = send_cmd_hdd_dir(fd, path);
    if(r < 0)
    {
        return;
    }

    while(getAnotherPacket && (0 < get_tf_packet(fd, &reply)))
    {
        switch (get_u32(&reply.cmd))
        {
            case DATA_HDD_DIR:
                decode_dir(&reply);
                send_success(fd);
                break;

            case DATA_HDD_DIR_END:
                getAnotherPacket = 0;
                break;

            case FAIL:
                fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
                getAnotherPacket = 0;
                break;

            default:
                fprintf(stderr, "ERROR: Unhandled packet\n");
        }
    }
}

void decode_dir(struct tf_packet *p)
{
    __u16 count =
        (get_u16(&p->length) - PACKET_HEAD_SIZE) / sizeof(struct typefile);
    struct typefile *entries = (struct typefile *) p->data;
    int i;
    time_t timestamp;

    for(i = 0; (i < count); i++)
    {
        char type;

        switch (entries[i].filetype)
        {
            case 1:
                type = 'd';
                break;

            case 2:
                type = 'f';
                break;

            default:
                type = '?';
        }

        /* This makes the assumption that the timezone of the Toppy and the system
         * that puppy runs on are the same. Given the limitations on the length of
         * USB cables, this condition is likely to be satisfied. */
        timestamp = tfdt_to_time(&entries[i].stamp);
        printf("%c %20llu %24.24s %s\n", type, get_u64(&entries[i].size),
               ctime(&timestamp), entries[i].name);
    }
}

void do_hdd_file_put(int fd, char *srcPath, char *dstPath, int turbo_on)
{
    time_t startTime = time(NULL);
    enum
    {
        START,
        DATA,
        END,
        FINISHED,
        EXIT
    } state;
    int src = -1;
    int r;
    int update = 0;
    struct stat64 srcStat;
    __u64 byteCount = 0;

    src = open64(srcPath, O_RDONLY);
    if(src < 0)
    {
        fprintf(stderr, "ERROR: Can not open source file: %s\n", strerror(errno));
        return;
    }

    if(0 != fstat64(src, &srcStat))
    {
        fprintf(stderr, "ERROR: Can not examine source file: %s\n", strerror(errno));
        close(src);
        return;
    }
    
    if(srcStat.st_size == 0)
    {
        fprintf(stderr, "ERROR: Source file is empty - not transfering.\n");
        close(src);
        return;
    }

    switch_turbo(fd, turbo_on);

    r = send_cmd_hdd_file_send(fd, PUT, dstPath);
    if(r < 0)
    {
        return;
    }

    state = START;
    while((state != EXIT) && (0 < get_tf_packet(fd, &reply)))
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
                        r = send_tf_packet(fd, &packet);
                        if(r < 0)
                        {
                            return;
                        }
                        state = DATA;
                        break;
                    }

                    case DATA:
                    {
                        int payloadSize = sizeof(packet.data) - 9;
                        ssize_t w = read(src, &packet.data[8], payloadSize);

                        put_u16(&packet.length, PACKET_HEAD_SIZE + 8 + w);
                        put_u32(&packet.cmd, DATA_HDD_FILE_DATA);
                        put_u64(packet.data, byteCount);
                        byteCount += w;

                        /* Detect EOF and transition to END */
                        if(w < payloadSize)
                        {
                            state = END;
                        }

                        if(w > 0)
                        {
                            r = send_tf_packet(fd, &packet);
                            if(r < w)
                            {
                                fprintf(stderr, "ERROR: Incomplete send.\n");
                                return;
                            }
                        }

                        if(!update && !quiet)
                        {
                            progressStats(srcStat.st_size, byteCount,
                                          startTime);
                        }
                        break;
                    }

                    case END:
                        /* Send end */
                        put_u16(&packet.length, PACKET_HEAD_SIZE);
                        put_u32(&packet.cmd, DATA_HDD_FILE_END);
                        r = send_tf_packet(fd, &packet);
                        if(r < 0)
                        {
                            return;
                        }
                        state = FINISHED;
                        break;

                    case FINISHED:
                        state = EXIT;
                        break;

                    case EXIT:
                        /* Should not get here. */
                        break;
                }
                break;

            case FAIL:
                fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
                state = EXIT;
                break;

            default:
                fprintf(stderr, "ERROR: Unhandled packet\n");
                break;
        }
    }
    close(src);
    fprintf(stderr, "\n");
    switch_turbo(fd, 0);
    finalStats(byteCount, startTime);
}

void do_hdd_file_get(int fd, char *srcPath, char *dstPath, int turbo_on)
{
    time_t startTime = time(NULL);
    enum
    {
        START,
        DATA,
        ABORT,
        EXIT
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
        return;
    }

    switch_turbo(fd, turbo_on);
    r = send_cmd_hdd_file_send(fd, GET, srcPath);
    if(r < 0)
    {
        return;
    }

    state = START;
    while((state != EXIT) && (0 < (r = get_tf_packet(fd, &reply))))
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

                    send_success(fd);
                    state = DATA;
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: Unexpected DATA_HDD_FILE_START packet in state %d\n",
                            state);
                    send_cancel(fd);
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
                        fprintf(stderr, "ERROR: Short packet %d instead of %d\n", r,
                                get_u16(&reply.length));
                        /* TODO: Fetch the rest of the packet */
                    }

                    w = write(dst, &reply.data[8], dataLen);
                    if(w < dataLen)
                    {
                        /* Can't write data - abort transfer */
                        fprintf(stderr, "ERROR: Can not write data: %s\n",
                                strerror(errno));
                        send_cancel(fd);
                        state = ABORT;
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: Unexpected DATA_HDD_FILE_DATA packet in state %d\n",
                            state);
                    send_cancel(fd);
                    state = ABORT;
                }
                break;

            case DATA_HDD_FILE_END:
                send_success(fd);
                state = EXIT;
                break;

            case FAIL:
                fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
                send_cancel(fd);
                state = ABORT;
                break;

            case SUCCESS:
                state = EXIT;
                break;

            default:
                fprintf(stderr, "ERROR: Unhandled packet\n");
        }
    }
    close(dst);
    fprintf(stderr, "\n");
    utime(dstPath, &mod_utime_buf);
    switch_turbo(fd, 0);
    finalStats(byteCount, startTime);
}

void do_hdd_del(int fd, char *path)
{
    int r;

    r = send_cmd_hdd_del(fd, path);
    if(r < 0)
    {
        return;
    }

    r = get_tf_packet(fd, &reply);
    if(r < 0)
    {
        return;
    }
    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
}

void do_hdd_rename(int fd, char *srcPath, char *dstPath)
{
    int r;

    r = send_cmd_hdd_rename(fd, srcPath, dstPath);
    if(r < 0)
    {
        return;
    }

    r = get_tf_packet(fd, &reply);
    if(r < 0)
    {
        return;
    }
    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
}

void do_hdd_mkdir(int fd, char *path)
{
    int r;

    r = send_cmd_hdd_create_dir(fd, path);
    if(r < 0)
    {
        return;
    }

    r = get_tf_packet(fd, &reply);
    if(r < 0)
    {
        return;
    }
    switch (get_u32(&reply.cmd))
    {
        case SUCCESS:
            break;

        case FAIL:
            fprintf(stderr, "ERROR: Device reports %s\n", decode_error(&reply));
            break;

        default:
            fprintf(stderr, "ERROR: Unhandled packet\n");
    }
}

void progressStats(__u64 totalSize, __u64 bytes, time_t startTime)
{
    int delta = abs(difftime(startTime, time(NULL)));

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
    int delta = abs(difftime(startTime, time(NULL)));

    fprintf(stderr, "%.2f Mbytes in %02d:%02d:%02d",
            (double) bytes / (1000.0 * 1000.0),
            delta / (60 * 60), (delta / 60) % 60, delta % 60);
    if(delta > 0)
    {
        fprintf(stderr, " (%.2f Mbits/s)",
                ((bytes * 8.0) / delta) / (1000.0 * 1000.0));
    }
    fprintf(stderr, "\n");
}

void usage(char *myName)
{
    char *usageString =
        "Usage: %s [-pPtv] [-d <device>] -c <command> [args]\n"
        " -p             - packet header output to stderr\n"
        " -P             - full packet dump output to stderr\n"
        " -t             - turbo mode on for file xfers\n"
        " -v             - verbose output to stderr\n"
        " -q             - quiet transfers - no progress updates\n"
        " -d <device>    - USB device (must be usbfs)\n"
        "                  for example /proc/bus/usb/001/003\n"
        " -c <command>   - one of size, dir, get, put, rename, delete, mkdir, reboot, cancel\n"
        " args           - optional arguments, as required by each command\n";
    fprintf(stderr, usageString, myName);
}

int parseArgs(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind;
    int c;

    while((c = getopt(argc, argv, "pPqtvd:c:")) != -1)
    {
        switch (c)
        {
            case 'v':
                verbose = 1;
                break;

            case 'p':
                packet_trace = 1;
                break;

            case 'P':
                packet_trace = 2;
                break;

            case 'q':
                quiet = 1;
                break;

            case 't':
                turbo_req = 1;
                break;

            case 'd':
                devPath = optarg;
                break;

            case 'c':
                if(!strcasecmp(optarg, "dir"))
                    cmd = CMD_HDD_DIR;
                else if(!strcasecmp(optarg, "cancel"))
                    cmd = CANCEL;
                else if(!strcasecmp(optarg, "size"))
                    cmd = CMD_HDD_SIZE;
                else if(!strcasecmp(optarg, "reboot"))
                    cmd = CMD_RESET;
                else if(!strcasecmp(optarg, "put"))
                {
                    cmd = CMD_HDD_FILE_SEND;
                    sendDirection = PUT;
                }
                else if(!strcasecmp(optarg, "get"))
                {
                    cmd = CMD_HDD_FILE_SEND;
                    sendDirection = GET;
                }
                else if(!strcasecmp(optarg, "delete"))
                    cmd = CMD_HDD_DEL;
                else if(!strcasecmp(optarg, "rename"))
                    cmd = CMD_HDD_RENAME;
                else if(!strcasecmp(optarg, "mkdir"))
                    cmd = CMD_HDD_CREATE_DIR;
                break;

            default:
                usage(argv[0]);
                return -1;
        }
    }

    if(cmd == 0)
    {
        usage(argv[0]);
        return -1;
    }

    /* Search for a Toppy if the device is not specified */
    if(devPath == NULL)
    {
        devPath = findToppy();
    }

    if(devPath == NULL)
    {
        return -1;
    }

    if(cmd == CMD_HDD_DIR)
    {
        if(optind < argc)
        {
            arg1 = argv[optind];
        }
        else
        {
            arg1 = "\\";
        }
    }

    if(cmd == CMD_HDD_FILE_SEND)
    {
        if((optind + 1) < argc)
        {
            arg1 = argv[optind];
            arg2 = argv[optind + 1];
        }
        else
        {
            fprintf(stderr,
                    "ERROR: Need both source and destination names.\n");
            return -1;
        }
    }

    if(cmd == CMD_HDD_DEL)
    {
        if(optind < argc)
        {
            arg1 = argv[optind];
        }
        else
        {
            fprintf(stderr,
                    "ERROR: Specify name of file or directory to delete.\n");
            return -1;
        }
    }

    if(cmd == CMD_HDD_RENAME)
    {
        if((optind + 1) < argc)
        {
            arg1 = argv[optind];
            arg2 = argv[optind + 1];
        }
        else
        {
            fprintf(stderr,
                    "ERROR: Specify both source and destination paths for rename.\n");
            return -1;
        }
    }

    if(cmd == CMD_HDD_CREATE_DIR)
    {
        if(optind < argc)
        {
            arg1 = argv[optind];
        }
        else
        {
            fprintf(stderr, "ERROR: Specify name of directory to create.\n");
            return -1;
        }
    }

    return 0;
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

    /* Refuse to scan while another instance is running. */
    if(0 != flock(lockFd, LOCK_EX | LOCK_NB))
    {
        fprintf(stderr, "ERROR: Can not scan for devices while another instance of puppy is running.\n");
        return NULL;
    }
    
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
            trace(1,
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
