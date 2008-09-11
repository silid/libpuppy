// -*- c-basic-offset: 4 -*-

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

int main(int a_c, char **a_v)
{
    int ok = 1;

    if (a_c < 3)
    {
        printf("usage: %s dir/ec/to/ry '*'\n", a_v[0]);
        ok = 0;
    }
    else
    {
        puppy_t p = puppy_open(NULL);
        if(ok && puppy_ok(p))
        {
            char *dir = convert_slashes(a_v[1]);
            const char *glob = a_v[2];

            puppy_dir_entry_t *entries = puppy_hdd_dir(p, dir);
            puppy_dir_entry_t *ptr = entries;

            while(ok && ptr->name[0])
            {
                if(ptr->ftype == PUPPY_FILE &&
                   fnmatch(glob, ptr->name, 0) == 0)
                {
                    char tmStr[1024];
                    struct tm tm = *localtime(&ptr->time);
                    char src[256];
                    const char* dst = ptr->name;
                    struct stat statbuf;

                    snprintf(src, sizeof(src), "%s\\%s", dir, dst);
                
                    strftime(tmStr, sizeof(tmStr), "%Y-%m-%d %H:%M:%S", &tm);
                    printf("%s %12lld %s\n", tmStr, ptr->size, dst);

                    if (stat(dst, &statbuf) == 0 &&
                        statbuf.st_size == ptr->size)
                    {
                        printf("File already transferred in full, skipping.\n");
                    }
                    else 
                    {                        
                        struct utimbuf utimbuf;
                        
                        puppy_hdd_file_get(p, src, dst);

                        utimbuf.actime = ptr->time;
                        utimbuf.modtime = ptr->time;

                        if (stat(dst, &statbuf) == 0) {
                            if (statbuf.st_size != ptr->size)
                            {
                                printf("Done, but file is not complete!\n");
                                ok = 0;
                            }
                        }
                        else
                        {
                            printf("Cannot stat file\n");
                            ok = 0;
                        }

                        if(utime(dst, &utimbuf) != 0) {
                            printf("failed to set file time (%s)\n", strerror(errno));
                            ok = 0;
                        }
                    }
                }
                ++ptr;
            }
            free(entries);
            free(dir);
        }
        else
        {
            fprintf(stderr, "Failed to initialize puppy\n");
        }
        puppy_done(p);
    }
    return ok ? 0 : 1;
}
