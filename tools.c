// -*- c-basic-offset: 4 -*-
#include <stdlib.h>

#include "tools.h"

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
