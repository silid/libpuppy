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
