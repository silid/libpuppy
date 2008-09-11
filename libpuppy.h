// -*- c-basic-offset: 4 -*-

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

#define PUPPY_RELEASE "1.14"

struct puppy;

typedef enum puppy_ftype
{
    PUPPY_DIR,
    PUPPY_FILE,
    PUPPY_UNKNOWN
} puppy_ftype_t;

typedef struct puppy_dir_entry
{
    puppy_ftype_t ftype;
    time_t time;
    long long int size;
    char name[95];
} puppy_dir_entry_t;

typedef struct puppy *puppy_t;

puppy_t puppy_open(const char *devPath);
void puppy_done(puppy_t p);
int puppy_ok(puppy_t p);
int puppy_turbo(puppy_t p, int state);
int puppy_reset(puppy_t p);
int puppy_ready(puppy_t p);
int puppy_cancel(puppy_t p);
int puppy_hdd_size(puppy_t p);
puppy_dir_entry_t *puppy_hdd_dir(puppy_t p, const char *path);
int puppy_hdd_file_put(puppy_t p, const char *srcPath, const char *dstPath);
int puppy_hdd_file_get(puppy_t p, const char *srcPath, const char *dstPath);
int puppy_hdd_del(puppy_t p, const char *path);
int puppy_hdd_rename(puppy_t p, const char *srcPath, const char *dstPath);
int puppy_hdd_mkdir(puppy_t p, const char *path);
