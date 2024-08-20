/*****************************************************************************
 * lexicon.l
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 ****************************************************************************/
%option outfile="ipccodegen.lex.yy.c"
%option nounput
%option noyywrap


%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "ipccodegen.grammar.tab.h"

%}

%%

[ \t\n]+		;
"message"	{ return MESSAGE; }
"int64"|"uint64"|"int32"|"uint32"|"int16"|"uint16"|"int8"|"uint8"|"size"	{ yylval.string = strdup(yytext); return TYPE_GENERIC_T; }
"float"|"double"|"bool"	{ yylval.string = strdup(yytext); return TYPE_GENERIC; }
"string"	{ return TYPE_STRING; }
"buffer"	{ return TYPE_BUFFER; }

[a-zA-Z\_][a-zA-Z0-9\_]*	{ yylval.string = strdup(yytext); return IDENTIFIER; }
[0-9]+	{ sscanf(yytext, "%"PRId64, &yylval.integer); return INTEGER; }


[;{}\[\]]	{ return *yytext; }

%%
