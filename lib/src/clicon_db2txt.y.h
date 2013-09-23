/*
 *  CVS Version: $Id: clicon_db2txt.y.h,v 1.6 2013/09/11 18:36:50 olof Exp $
 *
  Copyright (C) 2009-2013 Olof Hagsand and Benny Holmgren

  This file is part of CLICON.

  CLICON is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  CLICON is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with CLICON; see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>.
 */

#ifndef __CLICON_DB2TXT_Y_H__
#define __CLICON_DB2TXT_Y_H__

typedef struct state_stack {
    qelem_t	q;
    int		state;
    int		brackets;	/* Bracket counter in code state */
} state_t;


typedef int (cmpfn_t)(cg_var *cv1, cg_var *cv2);

struct keyvec {
    size_t	len;		/* Length of vector */
    char      **vec;		/* string vector */
    int		idx;		/* Current index */

};
typedef struct keyvec keyvec_t;

struct code_stack {
    qelem_t	q;
    int		codetype;	/* type of code; if, each etc.. */
    int		startline;	/* Start line of code section */
    int		processing;	/* Don't process/print anything if within @if
				   section (current or parent) that is false. */
    keyvec_t   *keyvec;		/* DB keys of @each statement */
    char       *loopbuf;	/* buffer to push back to flex */
    char       *loopvar;	/* loop key variable name */
    int		append_loopbuf;
};
typedef struct code_stack code_stack_t;

struct buffer_stack {
    qelem_t	q;
    void 	*buffer;	/* flex buffer */
};
typedef struct buffer_stack buffer_stack_t;

struct file_stack {
    qelem_t	q; 
    char	*name;		/* Name of file or buffer */
    int		line;
    FILE	*fp;		/* File pointer or buffer are used mutually exclusively */
    char	*buf;
};
typedef struct file_stack file_stack_t;

struct db2txt {
    char	*ya_db;			/* DB filename */
    char	*ya_ret;		/* Retirned output */
    clicon_handle ya_handle;		/* Clicon handle */
#if 0
    clicon_db2txtcb_t *ya_cb;		/* Upcall cb */
    void	*ya_cbarg;		/* Upcall arg */
#endif
    state_t	*ya_lex_state;		/* Lexer state stack */
    char	*ya_prevtok;		/* Previous token in lexer */
    int		 ya_fullcomment;	/* Is this a full line comment? */
    file_stack_t   *ya_file_stack;	/* Files processed */
    code_stack_t   *ya_code_stack;	/* Code section stack */
    buffer_stack_t *ya_buffer_stack;	/* Parser buffer stack */
    cg_var	*ya_null; 		/* A null variable */
    cvec        *ya_vars;		/* db2txt global variables */
};
typedef struct db2txt db2txt_t;

/*
 * Macros
 */
#define _YA ((db2txt_t *)_ya)


/*
 * Prototypes
 */
int clicon_db2txtlex(void *);
int clicon_db2txtparse(void *);

void clicon_db2txterror(void *_ya, char *, ...);
int clicon_db2txt_debug(int d);

int db2txt_pushtxt(db2txt_t *dbt, char *txt);
void *db2txt_popbuf(db2txt_t *dbt);
int db2txt_parser_cleanup(void *_ya);
file_stack_t *db2txt_file_new(char *file);
#if 0
char *db2txt_fread(void *_ya, const char *file);
#endif
int db2txt_pushfile(db2txt_t *dbt, file_stack_t *fs);
void db2txt_popfile(db2txt_t *dbt);

int db2txt_debug(const char *, ...);


#endif /* __CLICON_DB2TXT_Y_H__ */
