/*****************************************************************************\
 *  genledmap.c - generate LED segment lookup table 
 *****************************************************************************
 *  Copyright (C) 2010 Jim Garlick
 *  
 *  This file is part of Scrub, a program for erasing disks.
 *  For details, see <http://www.llnl.gov/linux/scrub/>.
 *  
 *  Scrub is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  Scrub is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with Scrub; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *ch;
    char *segstr;
} ledmap_t;

/*      a
 *      _
 *    f|_|b    (g in the middle)
 * p. e|_|c
 *      d
 */
ledmap_t tab7[] = {
    { "0", "abcdef" },
    { "1", "bc" },
    { "2", "abged" },
    { "3", "abcdg" },
    { "4", "bcgf" },
    { "5", "afgcd" },
    { "6", "afedcg" },
    { "7", "abc" },
    { "8", "abcdefg" },
    { "9", "abcdgf" },
    { ".0", "pabcdef" },
    { ".1", "pbc" },
    { ".2", "pabged" },
    { ".3", "pabcdg" },
    { ".4", "pbcgf" },
    { ".5", "pafgcd" },
    { ".6", "pafedcg" },
    { ".7", "pabc" },
    { ".8", "pabcdefg" },
    { ".9", "pabcdgf" },
    { "C", "afed" },
    { "H", "febcg" },
    { "H", "febc" }, /* N.B. malformed H is HP firmware bug?  make an alias */
    { "h", "fegc" },
    { "L", "fed" },
    { "E", "efedg" },
    { "r", "eg" },
    { " ", "" },
};

/*
 *  |d
 * --c  |a
 *  |e  |b  (decimal is not wired)
 */
ledmap_t tab5[] = {
    { "+", "dce" },
    { "-", "c" },
    { "1", "ab" },
    { "+1", "abcde" },
    { "-1", "abc" },
    { " ", "" },
};

unsigned char
encode7 (char *s)
{
    unsigned char c = 0;

    while (*s) {
        switch (*s++) {
            case 'a': c |= (1<<4); break;
            case 'b': c |= (1<<7); break;
            case 'c': c |= (1<<1); break;
            case 'd': c |= (1<<2); break;
            case 'e': c |= (1<<3); break;
            case 'f': c |= (1<<5); break;
            case 'g': c |= (1<<0); break;
            case 'p': c |= (1<<6); break;
        }
    } 
    return c;
}

unsigned char
encode5 (char *s)
{
    unsigned char c = 0;

    while (*s) {
        switch (*s++) {
            case 'a': c |= (1<<1); break;
            case 'b': c |= (1<<2); break;
            case 'c': c |= (1<<4); break;
            case 'd': c |= (1<<3); break;
            case 'e': c |= (1<<0); break;
        }
    } 
    return c;
}

int
main(int argc, char *argv[])
{
    int i;

    printf ("/* 7 segment displays */\n");
    for (i = 0; i < (sizeof(tab7)/sizeof(ledmap_t)); i++) {
        printf ("case 0x%-2.2x: return \"%s\";\n",
        (unsigned char)~encode7 (tab7[i].segstr), tab7[i].ch);
    }
    printf ("/* 5 segment displays */\n");
    for (i = 0; i < (sizeof(tab5)/sizeof(ledmap_t)); i++) {
        printf ("case 0x%-2.2x: return \"%s\";\n",
        (unsigned char)~encode5 (tab5[i].segstr), tab5[i].ch);
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

