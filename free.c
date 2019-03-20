/* vi: set sw=4 ts=4: */
/*
 * Mini free implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "internal.h"
#include <stdio.h>
#include <sys/sysinfo.h>

#define DIVISOR	1024
extern int free_main(int argc, char **argv)
{
	struct sysinfo info;
	sysinfo(&info);
	info.totalram/=DIVISOR;
	info.freeram/=DIVISOR;
	info.totalswap/=DIVISOR;
	info.freeswap/=DIVISOR;
	info.sharedram/=DIVISOR;
	info.bufferram/=DIVISOR;


	printf("%6s%13s%13s%13s%13s%13s\n", "", "total", "used", "free", 
			"shared", "buffers");

	printf("%6s%13ld%13ld%13ld%13ld%13ld\n", "Mem:", info.totalram, 
			info.totalram-info.freeram, info.freeram, 
			info.sharedram, info.bufferram);

	printf("%6s%13ld%13ld%13ld\n", "Swap:", info.totalswap,
			info.totalswap-info.freeswap, info.freeswap);

	printf("%6s%13ld%13ld%13ld\n", "Total:", info.totalram+info.totalswap,
			(info.totalram-info.freeram)+(info.totalswap-info.freeswap),
			info.freeram+info.freeswap);
	exit(TRUE);
}