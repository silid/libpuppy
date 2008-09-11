#include <stdio.h>
#include <stdlib.h>

#include "libpuppy.h"

int main(int a_c, char **a_v)
{
    puppy_t p = puppy_open(NULL);

    if(puppy_ok(p))
    {
        const char *dir = a_c > 1 ? a_v[1] : "";
        const char* gl
        puppy_dir_entry_t *entries = puppy_hdd_dir(p, dir);
        puppy_dir_entry_t *ptr = entries;

        while(ptr->name[0])
        {
            printf("%d %s\n", ptr->time, ptr->name);
            ++ptr;
        }
        free(entries);
    }
    else
    {
        fprintf(stderr, "Failed to initialize puppy\n");
    }
    puppy_done(p);
    return 0;
}
