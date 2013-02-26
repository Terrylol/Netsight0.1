// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

%{
#include "regexp.hh"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

static int yylex(void);
static void yyerror(const char*);
static Regexp *parsed_regexp;
static int nparen;

%}

%union {
	Regexp *re;
	int c;
	int nparen;
	char pf[MAX_PF_SIZE];
}

%token	<c> CHAR EOL POSTCARD
%type	<re>	alt concat repeat single line
%type	<nparen> count

%%

line: alt EOL
	{
		parsed_regexp = $1;
		return 1;
	}

alt:
	concat
|	alt '|' concat
	{
		$$ = reg(Alt, $1, $3);
	}
;

concat:
	repeat
|	concat repeat
	{
		$$ = reg(Cat, $1, $2);
	}
;

repeat:
	single
|	single '*'
	{
		$$ = reg(Star, $1, (Regexp *)nil);
	}
|	single '*' '?'
	{
		$$ = reg(Star, $1, (Regexp *)nil);
		$$->n = 1;
	}
|	single '+'
	{
		$$ = reg(Plus, $1, (Regexp *)nil);
	}
|	single '+' '?'
	{
		$$ = reg(Plus, $1, (Regexp *)nil);
		$$->n = 1;
	}
|	single '?'
	{
		$$ = reg(Quest, $1, (Regexp *)nil);
	}
|	single '?' '?'
	{
		$$ = reg(Quest, $1, (Regexp *)nil);
		$$->n = 1;
	}
;

count:
	{
		$$ = ++nparen;
	}
;

single:
	'(' count alt ')'
	{
		$$ = reg(Paren, $3, (Regexp *)nil);
		$$->n = $2;
	}
|	'(' '?' ':' alt ')'
	{
		$$ = $4;
	}
|	POSTCARD
	{
		$$ = reg(Lit, (Regexp *)nil, (Regexp *)nil);
		memcpy($$->pf, yylval.pf, sizeof yylval.pf);
	}
|	'.'
	{
		$$ = reg(Dot, (Regexp *)nil, (Regexp *)nil);
	}
;

%%

static char *input;
//static Regexp *parsed_regexp;
//static int nparen;
int gen;

static int
yylex(void)
{
	int c;
	char *aptr;

	if(input == NULL || *input == 0)
		return EOL;

	do {
		c = *input++;
		if(strchr("|*+?():.", c))
			return c;
	} while (c == ' ' || c == '\t');

	if(c == '{' && *input == '{') {
		aptr = yylval.pf;
		input++;
		while (!(*input == '}' && *(input + 1) == '}')) {
			*aptr++ = *input++;
		}
		*aptr = 0;
		input += 2;
		return POSTCARD;
	}

	yylval.c = c;
	return CHAR;
}

void
fatal(const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	fprintf(stderr, "fatal error: ");
	vfprintf(stderr, fmt, arg);
	fprintf(stderr, "\n");
	va_end(arg);
	exit(2);
}

static void
yyerror(const char *s)
{
	fatal("%s", s);
}


Regexp*
parse(char *s)
{
	Regexp *r, *dotstar;

	input = s;
	parsed_regexp = (Regexp *)nil;
	nparen = 0;
	if(yyparse() != 1) {
		yyerror("did not parse");
        }
	if(parsed_regexp == nil)
		yyerror("parser nil");

	r = reg(Paren, parsed_regexp, (Regexp *)nil);	// $0 parens
	dotstar = reg(Star, reg(Dot, (Regexp *)nil, (Regexp *)nil), (Regexp *)nil);
	dotstar->n = 1;	// non-greedy
	return reg(Cat, dotstar, r);
}

void*
mal(int n)
{
	void *v;

	v = malloc(n);
	if(v == nil)
		fatal("out of memory");
	memset(v, 0, n);
	return v;
}

Regexp*
reg(int type, Regexp *left, Regexp *right)
{
	Regexp *r;

	r = (Regexp *)mal(sizeof *r);
	r->type = type;
	r->left = left;
	r->right = right;
	return r;
}

void
printre(Regexp *r)
{
        if(!r) {
            return;
        }

	switch(r->type) {
	case Alt:
		printf("Alt(");
		printre(r->left);
		printf(", ");
		printre(r->right);
		printf(")");
		break;

	case Cat:
		printf("Cat(");
		printre(r->left);
		printf(", ");
		printre(r->right);
		printf(")");
		break;

	case Lit:
		printf("Lit(%s: ninst %d) ", r->pf, r->pfexp->program.bf_len);
		break;

	case Dot:
		printf("Dot");
		break;

	case Paren:
		printf("Paren(%d, ", r->n);
		printre(r->left);
		printf(")");
		break;

	case Star:
		if(r->n)
			printf("Ng");
		printf("Star(");
		printre(r->left);
		printf(")");
		break;

	case Plus:
		if(r->n)
			printf("Ng");
		printf("Plus(");
		printre(r->left);
		printf(")");
		break;

	case Quest:
		if(r->n)
			printf("Ng");
		printf("Quest(");
		printre(r->left);
		printf(")");
		break;

	default:
		printf("???");
		break;
	}
}

void
parse_postcard_filter(Regexp *r)
{
	int argc = 0;
	char argv[10][MAX_BPF_SIZE];
	char *ptr = r->pf;
	char *arg = argv[0];
	char delim = ' ';
	int i = 0;

	//printf("Parsing postcard filter: {{%s}}\n", r->pf);
	/* Parse from r->pf */
	while (argc < 10) {
		while (*ptr && *ptr == ' ') ptr++;
		if (*ptr == 0)
			break;
		if (*ptr == '"') {
			ptr++;
			delim = '"';
		} else {
			delim = ' ';
		}
		while (*ptr != delim) {
			*arg++ = *ptr++;
		}
		*arg++ = 0;
                ptr++;
		//printf("\narg[%d]: %s\n", argc, argv[argc]);
		argc++;
		arg = argv[argc];
	}

	r->pfexp = (PostcardFilter *)mal(sizeof (struct PostcardFilter));
	r->pfexp->dpid = -1;
	r->pfexp->inport = -1;
	r->pfexp->outport = -1;
	r->pfexp->bpf = -1;
	r->pfexp->version = -1;

	while (i < argc) {
		if (strcmp(argv[i], "--bpf") == 0) {
			char *bpf = argv[i+1];
			int ret;
			r->pfexp->bpf = 1;
#if BPF_JIT
                        ret = compile_to_bpf(bpf, &r->pfexp->program);
                        ret = compile_to_x86(&r->pfexp->program,
                            &r->pfexp->filter);
#else
			ret = pcap_compile_nopcap(1500, DLT_EN10MB,
						  &r->pfexp->program,
						  bpf,
						  0, 0);
#endif
			if (ret <= -1) {
				printf("Error %d when compiling %s\n",
				       ret, bpf);
				exit(-1);
			}
			i += 2;
		} else if (strcmp(argv[i], "--dpid") == 0) {
			r->pfexp->dpid = atoi(argv[i+1]);
			i += 2;
		} else if (strcmp(argv[i], "--inport") == 0) {
			r->pfexp->inport = atoi(argv[i+1]);
			i += 2;
		} else if (strcmp(argv[i], "--outport") == 0) {
			r->pfexp->outport = atoi(argv[i+1]);
			i += 2;
		} else if (strcmp(argv[i], "--version") == 0) {
			r->pfexp->version = atoi(argv[i+1]);
			i += 2;
		} else {
			printf("Unknown option %s\n", argv[i]);
			i += 1;
		}
	}
}

void
parse_postcard_filters(Regexp *r)
{
	switch(r->type) {
	case Alt:
		parse_postcard_filters(r->left);
		parse_postcard_filters(r->right);
		break;

	case Cat:
		parse_postcard_filters(r->left);
		parse_postcard_filters(r->right);
		break;

	case Lit:
		parse_postcard_filter(r);
		break;

	case Paren:
		parse_postcard_filters(r->left);
		break;

	case Star:
		parse_postcard_filters(r->left);
		break;

	case Plus:
		parse_postcard_filters(r->left);
		break;

	case Quest:
		parse_postcard_filters(r->left);
		break;
	}
}
