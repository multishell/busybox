/* dd -- convert a file while copying it.
   Copyright (C) 85, 90, 91, 95, 1996 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Rubin, David MacKenzie, and Stuart Kemp. */
/* Hacked to bare minimum for Debian boot floppies by Giuliano Procida */

/* Options:

   Numbers can be followed by a multiplier:
   b=512, c=1, k=1024, w=2, xm=number m

   bs=BYTES			Read and Write BYTES bytes at a time.
   count=BLOCKS			Copy only BLOCKS input blocks. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include "internal.h"

#define equal(p, q) (strcmp ((p),(q)) == 0)

/* The name this program was run with. */
static const char *program_name;

const char	dd_usage[] = "dd [OPTION]\n\
\n\
\tCopy from stdin to stdout according to the options.\n\
\n\
\t  bs=BYTES        read and write BYTES bytes at a time\n\
\t  count=BLOCKS    copy only BLOCKS input blocks\n\
\n\
\tBYTES may be suffixed: by xM for multiplication by M, by c for x1,\n\
\tby w for x2, by b for x512, by k for x1024.\n";

static int parse_integer (char *str);
static int copy (char *buf);

/* Default number of bytes in which atomic reads and writes are done. */
static long blocksize = 512;

/* Copy only this many records.  <0 means no limit. */
static int max_records = -1;

/* Number of partial blocks written. */
static unsigned w_partial = 0;

/* Number of full blocks written. */
static unsigned w_full = 0;

/* Number of partial blocks read. */
static unsigned r_partial = 0;

/* Number of full blocks read. */
static unsigned r_full = 0;

extern int
dd_main (struct FileInfo * unused, int argc, char **argv)
{
  int exit_status;
  unsigned char *buf;           /* Buffer. */
  int i, n;
  
  program_name = argv[0];

  /* Decode arguments. */
  for (i = 1; i < argc; i++)
    {
      char *name, *val;
      
      name = argv[i];
      val = strchr (name, '=');
      if (val == 0)
	{
	  fprintf (stderr, "%s: unrecognized option `%s'\n", program_name, name);
	  usage (dd_usage);
	  return 1;
	}
      *val++ = '\0';
      
      n = parse_integer (val);
      if (n < 0)
	{ 
	  fprintf (stderr, "%s: invalid number: `%s'\n", program_name, val);
	  return 1;
	}
      
      if (equal (name, "bs"))
	blocksize = n;
      else if (equal (name, "count"))
	max_records = n;
      else
	{
	  fprintf (stderr, "%s: unrecognized option `%s=%s'\n", program_name, name, val);
	  usage (dd_usage);
	  return 1;
	}
    }
  
  /* allocate buffer space */
  buf = (unsigned char *) malloc (blocksize);
  if ( buf == 0 ) {
    fprintf (stderr, "%s: Memory exhausted\n", program_name);
    exit_status = 1;
  } else {
    exit_status = copy (buf);
    free (buf);
  }
  
  fprintf (stderr, "%u+%u records in\n", r_full, r_partial);
  fprintf (stderr, "%u+%u records out\n", w_full, w_partial);
  
  return exit_status;
}

/* The main loop.  */

static int
copy (char *buf)
{
  int nread;			/* Bytes read in the current block. */
  int nwrite;			/* Bytes written in the current block. */
  
  while (1)
    {
      if (max_records >= 0 && r_partial + r_full >= max_records)
	break;

      nread = read (STDIN_FILENO, buf, blocksize);

      if (nread == 0)
	break;		/* EOF.  */

      if (nread < 0)
	return -2;

      if (nread < blocksize)
	r_partial++;
      else
	r_full++;

      nwrite = write (STDOUT_FILENO, buf, nread);

      if (nwrite < 0)
	return -1;

      if (nwrite == blocksize)
	w_full++;
      else
	w_partial++;
    }
  return 0;
}

/* Return the value of STR, interpreted as a non-negative decimal integer,
   optionally multiplied by various values.
   Return -1 if STR does not represent a number in this format. */

static int
parse_integer (char *str)
{
  register int n = 0;
  register int temp;
  char *p = str;

  n = strtoul (p,&p,10);

loop:
  switch (*p++)
    {
    case '\0':
      return n;
    case 'b':
      n *= 512;
      goto loop;
    case 'c':
      goto loop;
    case 'k':
      n *= 1024;
      goto loop;
    case 'w':
      n *= 2;
      goto loop;
    case 'x':
      temp = parse_integer (p);
      if (temp == -1)
	return -1;
      n *= temp;
      break;
    default:
      return -1;
    }
  return n;
}
