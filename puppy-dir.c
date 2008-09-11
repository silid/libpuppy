// -*- c-basic-offset: 4 -*-

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fnmatch.h>

#include "libpuppy.h"
#include "tools.h"

int main(int a_c, char **a_v)
{
    puppy_t p = puppy_open(NULL);

    if(puppy_ok(p))
    {
        char *dir = convert_slashes(a_c > 1 ? a_v[1] : "");
        const char *glob = a_c > 2 ? a_v[2] : "*";

        puppy_dir_entry_t *entries = puppy_hdd_dir(p, dir);
        puppy_dir_entry_t *ptr = entries;

        while(ptr->name[0])
        {
            if(fnmatch(glob, ptr->name, 0) == 0)
            {
                char tmStr[1024];
                struct tm tm = *localtime(&ptr->time);

                strftime(tmStr, sizeof(tmStr), "%Y-%m-%d %H:%M:%S", &tm);
                printf("%s %12lld  %s\n", tmStr, ptr->size, ptr->name);
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
    return 0;
}
