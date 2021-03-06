// -*- c-basic-offset: 4 -*-

/* Format using indent and the following options:
-bad -bap -bbb -i4 -bli0 -bl0 -cbi0 -cli4 -ss -npcs -nprs -nsaf -nsai -nsaw -nsc -nfca -nut -lp -npsl
*/

/* 
  Copyright (C) 2008 Erkki Seppala <flux at modeemi.fi>

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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fnmatch.h>
#include <utime.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tools.h"
#include "libpuppy.h"

void usage(void)
{
    puts("usage: puppy-get [-nm] DataFiles/Foo '*.rec'");
    puts("-n  no action - just say what we would do");
    puts("-m  move files: remove source files after succesfully copying them");
}

int main(int a_c, char **a_v)
{
    int ok = 1;

    int move = 0;
    int noact = 0;

    {
        int opt;
        
        while (ok && (opt = getopt(a_c, a_v, "mn")) != -1) {
            switch(opt)
            {
            case 'm':
                move = !move;
                break;
            case 'n':
                noact = !noact;
                break;
            default:
                ok = 0;
            }
        }
    }
    
    if (!ok || a_c - optind < 2)
    {
        usage();
        ok = 0;
    }
    else
    {
        puppy_t p = puppy_open(NULL);

        if(ok && puppy_ok(p))
        {
            char *dir = convert_slashes(a_v[optind]);
            char * const *globs = a_v + optind + 1;
            int num_globs = a_v + a_c - globs;

            puppy_dir_entry_t *entries = puppy_hdd_dir(p, dir);
            puppy_dir_entry_t *ptr = entries;

            if(!entries)
            {
                printf("Cannot retrieve contents");
            }
            else
            {
                while(ok && ptr->name[0])
                {
                    if(ptr->ftype == PUPPY_FILE)
                    {
                        int matches = 0;
                        {
                            int c;
                            for (c = 0; c < num_globs && !matches; ++c)
                            {
                                matches = fnmatch(globs[c], ptr->name, 0) == 0;
                            }
                        }
                        if (matches)
                        {
                            char tmStr[1024];
                            struct tm tm = *localtime(&ptr->time);
                            char src[256];
                            const char* dst = ptr->name;
                            struct stat statbuf;

                            snprintf(src, sizeof(src), "%s\\%s", dir, dst);
                
                            strftime(tmStr, sizeof(tmStr), "%Y-%m-%d %H:%M:%S", &tm);
                            printf("%s %12lld %s\n", tmStr, ptr->size, dst);

                            if(stat(dst, &statbuf) == 0 &&
                               statbuf.st_size == ptr->size)
                            {
                                printf("File already transferred in full, ");
                                if(move && !noact)
                                {
                                    printf("removing source file.\n");
                                    if(puppy_hdd_del(p, src) != 0)
                                    {
                                        printf("Cannot remove source file %s\n", dst);
                                    }
                                }
                                else
                                {
                                    printf("skipping.\n");
                                }
                            }
                            else 
                            {
                                if(!noact)
                                {
                                    if(puppy_hdd_file_get(p, src, dst) == 0)
                                    {
                                        if(stat(dst, &statbuf) == 0)
                                        {
                                            if(statbuf.st_size != ptr->size)
                                            {
                                                printf("Done, but file is not complete!\n");
                                                ok = 0;
                                            }
                                            else if(move && !noact)
                                            {
                                                if(puppy_hdd_del(p, src) != 0)
                                                {
                                                    printf("Cannot remove source file %s\n", dst);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            printf("Cannot stat file\n");
                                            ok = 0;
                                        }
                                    }
                                    else
                                    {
                                        printf("Cannot retrieve source file %s", src);
                                        ok = 0;
                                    }
                                }
                            }
                        }
                    }
                    ++ptr;
                }
                free(entries);
                free(dir);
            }
        }
        else
        {
            fprintf(stderr, "Failed to initialize puppy\n");
        }
        puppy_done(p);
    }
    return ok ? 0 : 1;
}
