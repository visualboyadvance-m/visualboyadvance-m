// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// adapted from gmon.c
/*-
 * Copyright (c) 1991, 1998 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. [rescinded 22 July 1999]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "gmon.h"
#include "gmon_out.h"

#include "../GBA.h"
#include "../NLS.h"

#include "prof.h" /* for struct profile_segment */

/*
 * froms is actually a bunch of unsigned shorts indexing tos
 */
static int  profiling = 3;

static profile_segment *first_segment = NULL;

static int hz = 0;
static int hist_num_bins = 0;
static char hist_dimension[16] = "seconds";
static char hist_dimension_abbrev = 's';

/* see profil(2) where this is describe (incorrectly) */
#define  SCALE_1_TO_1 0x10000L

void profPut32(char *b, u32 v)
{
  b[0] = v & 255;
  b[1] = (v >> 8) & 255;
  b[2] = (v >> 16) & 255;
  b[3] = (v >> 24) & 255;
}

void profPut16(char *b, u16 v)
{
  b[0] = v & 255;
  b[1] = (v >> 8) & 255;
}

int profWrite8(FILE *f, u8 b)
{
  if(fwrite(&b, 1, 1, f) != 1)
    return 1;
  return 0;
}

int profWrite32(FILE *f, u32 v)
{
  char buf[4];

  profPut32(buf, v);
  if(fwrite(buf, 1, 4, f) != 4)
    return 1;
  return 0;
}

int profWrite(FILE *f, char *buf, unsigned int n)
{
  if(fwrite(buf, 1, n, f) != n)
    return 1;
  return 0;
}

/* Control profiling;
   profiling is what mcount checks to see if
   all the data structures are ready.  */

void profControl(int mode)
{
  if (mode) {
    /* start */
#ifdef PROFILING
    cpuProfil(first_segment);
#endif
    profiling = 0;
  } else {
    /* stop */
#ifdef PROFILING
    cpuProfil(NULL);
#endif
    profiling = 3;
  }
}


#define MSG N_("No space for profiling buffer(s)\n")

void profStartup(u32 lowpc, u32 highpc)
{
  int monsize;
  char *buffer;
  int o;
  profile_segment *newseg = (profile_segment*)calloc(1,sizeof(profile_segment));
  
  if (newseg) {
    newseg->next = first_segment;
    first_segment = newseg;
  } else {
    systemMessage(0, MSG);
    return;
  }
  /*
   * round lowpc and highpc to multiples of the density we're using
   * so the rest of the scaling (here and in gprof) stays in ints.
   */
  lowpc = ROUNDDOWN(lowpc, HISTFRACTION*sizeof(HISTCOUNTER));
  newseg->s_lowpc = lowpc;
  highpc = ROUNDUP(highpc, HISTFRACTION*sizeof(HISTCOUNTER));
  newseg->s_highpc = highpc;
  newseg->s_textsize = highpc - lowpc;
  monsize = (newseg->s_textsize / HISTFRACTION);
  buffer = (char *)calloc(1, 2*monsize );
  if ( buffer == NULL ) {
    systemMessage(0, MSG);
    return;
  }
  newseg->froms = (unsigned short *) calloc(1, 4*newseg->s_textsize / HASHFRACTION );
  if ( newseg->froms == NULL ) {
    systemMessage(0, MSG);
    free(buffer);
    buffer = NULL;
    return;
  }
  newseg->tolimit = newseg->s_textsize * ARCDENSITY / 100;
  if ( newseg->tolimit < MINARCS ) {
    newseg->tolimit = MINARCS;
  } else if ( newseg->tolimit > 65534 ) {
    newseg->tolimit = 65534;
  }
  newseg->tos = (struct tostruct *) calloc(1, newseg->tolimit * sizeof( struct tostruct ) );
  if ( newseg->tos == NULL ) {
    systemMessage(0, MSG);
    
    free(buffer);
    buffer = NULL;
    
    free(newseg->froms);
    newseg->froms = NULL;
    
    return;
  }
  newseg->tos[0].link = 0;
  newseg->sbuf = buffer;
  newseg->ssiz = monsize;
  if ( monsize <= 0 )
    return;
  o = highpc - lowpc;
  if( monsize < o )
    newseg->s_scale = (int)(( (float) monsize / o ) * SCALE_1_TO_1);
  else
    newseg->s_scale = SCALE_1_TO_1;
  profControl(1);
}

void profCleanup()
{
  FILE *fd;
  int fromindex;
  int endfrom;
  u32 frompc;
  int toindex;
  struct gmon_hdr ghdr;
  profile_segment *seg = first_segment;
  
  profControl(0);
  fd = fopen( "gmon.out" , "wb" );
  if ( fd == NULL ) {
    systemMessage( 0, "mcount: gmon.out" );
    return;
  }

  memcpy(&ghdr.cookie[0], GMON_MAGIC, 4);
  profPut32((char *)ghdr.version, GMON_VERSION);

  if(fwrite(&ghdr, sizeof(ghdr), 1, fd) != 1) {
    systemMessage(0, "mcount: gmon.out header");
    fclose(fd);
    return;
  }

  if(hz == 0)
    hz = 100;

  while(seg) {

    hist_num_bins = seg->ssiz;
  
  if(profWrite8(fd, GMON_TAG_TIME_HIST) ||
       profWrite32(fd, (u32)seg->s_lowpc) ||
       profWrite32(fd, (u32)seg->s_highpc) ||
     profWrite32(fd, hist_num_bins) ||
     profWrite32(fd, hz) ||
     profWrite(fd, hist_dimension, 15) ||
     profWrite(fd, &hist_dimension_abbrev, 1)) {
    systemMessage(0, "mcount: gmon.out hist");
    fclose(fd);
    return;
  }
    u16 *hist_sample = (u16 *)seg->sbuf;

  u16 count;
  int i;
  
  for(i = 0; i < hist_num_bins; ++i) {
    profPut16((char *)&count, hist_sample[i]);

    if(fwrite(&count, sizeof(count), 1, fd) != 1) {
      systemMessage(0, "mcount: gmon.out sample");
      fclose(fd);
      return;
    }
  }
  
    endfrom = seg->s_textsize / (HASHFRACTION * sizeof(*seg->froms));
  for ( fromindex = 0 ; fromindex < endfrom ; fromindex++ ) {
      if ( seg->froms[fromindex] == 0 ) {
      continue;
    }
      frompc = seg->s_lowpc + (fromindex * HASHFRACTION * sizeof(*seg->froms));
      for (toindex=seg->froms[fromindex]; toindex!=0; toindex=seg->tos[toindex].link) {
      if(profWrite8(fd, GMON_TAG_CG_ARC) ||
         profWrite32(fd, (u32)frompc) ||
	   profWrite32(fd, (u32)seg->tos[toindex].selfpc) ||
	   profWrite32(fd, seg->tos[toindex].count)) {
        systemMessage(0, "mcount: arc");
        fclose(fd);
        return;
      }
    }
  }
    seg = seg->next;
  }
  fclose(fd);
}

void profCount()
{
  register char *selfpc;
  register unsigned short *frompcindex;
  register struct tostruct *top;
  register struct tostruct *prevtop;
  register long toindex;
  profile_segment *seg = first_segment;

  /*
   * find the return address for mcount,
   * and the return address for mcount's caller.
   */
  
  /* selfpc = pc pushed by mcount call.
     This identifies the function that was just entered.  */
  selfpc = (char *) reg[14].I;
  /* frompcindex = pc in preceding frame.
     This identifies the caller of the function just entered.  */
  frompcindex = (unsigned short *) reg[12].I;
  /*
   * check that we are profiling
   * and that we aren't recursively invoked.
   */
  if (profiling) {
    goto out;
  }
  profiling++;
  /*
   * check that frompcindex is a reasonable pc value.
   * for example: signal catchers get called from the stack,
   *   not from text space.  too bad.
   */
  while(seg) {
    u32 index = (long)frompcindex - (long)seg->s_lowpc;
    if (index <= seg->s_textsize) {
      frompcindex = (unsigned short *) index;
      break;
    }
    seg = seg->next;
  }
  if ((unsigned long) frompcindex > seg->s_textsize) {
    goto done;
  }
  frompcindex =
    &(seg->froms[((long) frompcindex) / (HASHFRACTION * sizeof(*seg->froms))]);
  toindex = *frompcindex;
  if (toindex == 0) {
    /*
     * first time traversing this arc
     */
    toindex = ++seg->tos[0].link;
    if (toindex >= seg->tolimit) {
      goto overflow;
    }
    *frompcindex = (unsigned short)toindex;
    top = &seg->tos[toindex];
    top->selfpc = selfpc;
    top->count = 1;
    top->link = 0;
    goto done;
  }
  top = &seg->tos[toindex];
  if (top->selfpc == selfpc) {
    /*
     * arc at front of chain; usual case.
     */
    top->count++;
    goto done;
  }
  /*
   * have to go looking down chain for it.
   * top points to what we are looking at,
   * prevtop points to previous top.
   * we know it is not at the head of the chain.
   */
  for (; /* goto done */; ) {
    if (top->link == 0) {
      /*
       * top is end of the chain and none of the chain
       * had top->selfpc == selfpc.
       * so we allocate a new tostruct
       * and link it to the head of the chain.
       */
      toindex = ++seg->tos[0].link;
      if (toindex >= seg->tolimit) {
        goto overflow;
      }
      top = &seg->tos[toindex];
      top->selfpc = selfpc;
      top->count = 1;
      top->link = *frompcindex;
      *frompcindex = (unsigned short)toindex;
      goto done;
    }
    /*
     * otherwise, check the next arc on the chain.
     */
    prevtop = top;
    top = &seg->tos[top->link];
    if (top->selfpc == selfpc) {
      /*
       * there it is.
       * increment its count
       * move it to the head of the chain.
       */
      top->count++;
      toindex = prevtop->link;
      prevtop->link = top->link;
      top->link = *frompcindex;
      *frompcindex = (unsigned short)toindex;
      goto done;
    }
    
  }
 done:
  profiling--;
  /* and fall through */
 out:
  return;  /* normal return restores saved registers */
  
 overflow:
  profiling++; /* halt further profiling */
#define TOLIMIT "mcount: tos overflow\n"
  systemMessage(0, TOLIMIT);
  goto out;
}

void profSetHertz(int h)
{
  hz = h;
}
