#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fnmatch.h>

#include "libpuppy.h"

char *convert_slashes(const char *a_str)
{
    int escape = 0;
    char *buffer = NULL;
    char *write = NULL;

    for(int stage = 0; stage < 2; ++stage)
    {
        int len = 0;

        for(int c = 0; a_str[c]; ++c)
        {
            char ch = a_str[c];

            if(escape || ch != '\\')
            {
                if(stage == 1)
                {
                    buffer[len] = (ch == '/' ? '\\' : ch);
                }
                ++len;
                escape = 0;
            }
            else
            {
                escape = 1;
            }
        }
        if(stage == 0)
        {
            buffer = write = malloc(len + 1);
        }
        else
        {
            buffer[len] = 0;
        }
    }
    return buffer;
}

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
                printf("%s %s\n", tmStr, ptr->name);
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
