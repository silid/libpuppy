// -*- c-basic-offset: 4 -*-

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fnmatch.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>

#include "libpuppy.h"
#include "tools.h"

typedef enum sort_field {
    SORT_UNSORTED,              /* don't do sorting */
    SORT_BY_NAME,
    SORT_BY_TIME,
    SORT_BY_SIZE,
    SORT_BY_NAME_TIME           /* sort by name, ignoring the trailing
                                   number; use time as the secondary
                                   sort criteria */
} sort_field_t;

void usage(void)
{
    puts("usage: puppy-dir [-ntNSU] DataFiles/Foo '*.rec'");
    puts(" -n  sort by name and time, ignoring the trailing number in the name");
    puts(" -N  sort by name (sorts numeric parts numerically)");
    puts(" -t  sort by time");
    puts(" -S  sort by size");
    puts(" -U  don't sort");
    puts(" The default is to sort by name and time (-N)");
}

/* Given a file name like foo-123.rec, finds the start and end
   pointers for 123

   Maybe I should just use a regular expressions, this is overly
   complicated versus the achieved results...
*/
void extract_parts(const char  *a_name,
                   int         *a_name_len,
                   int         *a_number)
{
    const char* dot = strrchr(a_name, '.');
    const char* dash = strrchr(a_name, '-');
    const char* plus = strrchr(a_name, '+');

    int name_len;
    int number;

    if(dot)
    {
        if (plus && plus < dot)
        {
            name_len = plus - a_name;
            number = 0;
        }
        else if(dash && dash < dot)
        {
            const char *ptr = dash + 1;
            name_len = dash - a_name;
            
            while(ptr < dot && isdigit(*ptr))
            {
                ++ptr;
            }
            if(ptr == dot)
            {
                number = atoi(dash + 1);
            }
            else
            {
                number = 0;
            }
        }
        else
        {
            name_len = dot - a_name;
            number = 0;
        }
    } else {
        name_len = strlen(a_name);
        number = 0;
    }
    *a_name_len = name_len;
    *a_number = number;
}

int strcmp_len(const char *a, int a_len,
               const char *b, int b_len)
{
    int cmp = 0;
    while (cmp == 0 && *a && *b && a_len > 0 && b_len > 0)
    {
        cmp = (int) *a - (int) *b;
        ++a;
        ++b;
        --a_len;
        --b_len;
    }
    return cmp;
}

typedef enum compare_by_name_and {
    COMPARE_BY_NUMBER,
    COMPARE_BY_TIME
} compare_by_name_and_t;

int compare_by_name_and(compare_by_name_and_t a_secondary,
                        const puppy_dir_entry_t *a,
                        const puppy_dir_entry_t *b)
{
    const char *a_name = a->name;
    const char *b_name = b->name;
    int         a_name_len;
    int         b_name_len;
    int         a_num;
    int         b_num;
    int         name_cmp;
    
    extract_parts(a_name, &a_name_len, &a_num);
    extract_parts(b_name, &b_name_len, &b_num);

    name_cmp = strcmp_len(a_name, a_name_len,
                          b_name, b_name_len);

    return
        name_cmp != 0 ? name_cmp :
        a_secondary == COMPARE_BY_NUMBER ? a_num - b_num :
        a_secondary == COMPARE_BY_TIME ? a->time - b->time :
        0 /* impossible */;
}

int compare_by_name(const puppy_dir_entry_t *a,
                    const puppy_dir_entry_t *b)
{
    return compare_by_name_and(COMPARE_BY_NUMBER, a, b);
}

int compare_by_time(const puppy_dir_entry_t *a,
                    const puppy_dir_entry_t *b)
{
    return a->time - b->time;
}

int compare_by_size(const puppy_dir_entry_t *a,
                    const puppy_dir_entry_t *b)
{
    return a->size - b->size;
}

int compare_by_name_time(const puppy_dir_entry_t *a,
                         const puppy_dir_entry_t *b)
{
    return compare_by_name_and(COMPARE_BY_TIME, a, b);
}

void sort_dir_entries(puppy_dir_entry_t *a_entries,
                      int a_len,
                      sort_field_t a_field)
{
    int(*comparator)(const puppy_dir_entry_t*, const puppy_dir_entry_t*) = NULL;
    switch(a_field)
    {
    case SORT_BY_NAME:
        comparator = compare_by_name;
        break;
                    
    case SORT_BY_TIME:
        comparator = compare_by_time;
        break;

    case SORT_BY_SIZE:
        comparator = compare_by_size;
        break;

    case SORT_BY_NAME_TIME:
        comparator = compare_by_name_time;
        break;

    case SORT_UNSORTED:
        comparator = NULL;
        break;
    }

    if(comparator)
    {
        qsort(a_entries, a_len, sizeof(puppy_dir_entry_t),
              (int(*)(const void*, const void*)) comparator);
    }
}

int main(int a_c, char **a_v)
{
    int ok = 1;
    sort_field_t sort_field = SORT_BY_NAME_TIME;
    
    {
        int opt;
        
        while (ok && (opt = getopt(a_c, a_v, "htUSn")) != -1) {
            switch(opt)
            {
            case 'h':
                usage();
                ok = 0;
                break;

            case 'U':
                sort_field = SORT_UNSORTED;
                break;

            case 't':
                sort_field = SORT_BY_TIME;
                break;

            case 'S':
                sort_field = SORT_BY_SIZE;
                break;

            case 'n':
                sort_field = SORT_BY_NAME;
                break;

            case 'N':
                sort_field = SORT_BY_NAME_TIME;
                break;
                
            default:
                ok = 0;
            }
        }
    }

    if(ok)
    {
        puppy_t p = puppy_open(NULL);

        if(puppy_ok(p))
        {
            char *dir = convert_slashes(a_c > optind ? a_v[optind] : "");
            const char *glob = a_c > optind + 1 ? a_v[optind + 1] : "*";

            puppy_dir_entry_t *entries = puppy_hdd_dir(p, dir);
            puppy_dir_entry_t *ptr = entries;
            int num_entries = 0;
            int c;

            while(ptr->name[0])
            {
                ++num_entries;
                ++ptr;
            }

            sort_dir_entries(entries, num_entries, sort_field);
            
            for(c = 0; c < num_entries; ++c)
            {
                puppy_dir_entry_t *entry = entries + c;
                if(fnmatch(glob, entry->name, 0) == 0)
                {
                    char tmStr[1024];
                    struct tm tm = *localtime(&entry->time);

                    strftime(tmStr, sizeof(tmStr), "%Y-%m-%d %H:%M:%S", &tm);
                    printf("%s %12lld  %s\n", tmStr, entry->size, entry->name);
                }
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
    return 0;
}
