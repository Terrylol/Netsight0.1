// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef REGEXP_H
#define REGEXP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <stdarg.h>
#include <assert.h>
#include <pcap.h>
#include "bpf-linux/bpf-jit-linux.h"
#include "helper.hh"
#include "postcard.hh"

#define MAX_REGEX_SIZE 50000
#define MAX_PF_SIZE 25500
#define MAX_BPF_SIZE 12800

#define BPF_JIT 1

using namespace std;

typedef struct Regexp Regexp;
typedef struct Prog Prog;
typedef struct Inst Inst;
typedef int (*RegexpMatcher)(Prog*, PostcardNode *, PostcardNode**, int);

struct PostcardFilter;
typedef struct PostcardFilter PostcardFilter;

struct Regexp
{
    int type;
    int n;
    char pf[MAX_PF_SIZE];
    PostcardFilter *pfexp;
    Regexp *left;
    Regexp *right;
};

enum	/* Regexp.type */
{
    Alt = 1,
    Cat,
    Lit,
    Dot,
    Paren,
    Quest,
    Star,
    Plus,
};

Regexp *parse(char*);
Regexp *reg(int type, Regexp *left, Regexp *right);
void free_reg(Regexp *reg);
void printre(Regexp*);
void fatal(const char*, ...);
void *mal(int);
void parse_postcard_filters(Regexp *r);

struct Prog
{
    Inst *start;
    int len;
};

struct Inst
{
    int opcode;
    char *pf;
    PostcardFilter *pfexp;
    int n;
    Inst *x;
    Inst *y;
    int gen;	// global state, oooh!
};

enum	/* Inst.opcode */
{
    Char = 1,
    Match,
    Jmp,
    Split,
    Any,
    Save,
};

Prog *compile(Regexp*);
void printprog(Prog*);

extern int gen;

enum {
    MAXSUB = 20
};

typedef struct Sub Sub;

struct Sub
{
    int ref;
    int nsub;
    PostcardNode *sub[MAXSUB];
};

struct PostcardFilter
{
    int bpf;
    struct bpf_program program;
    struct sk_filter filter;
    int dpid;
    int inport;
    int outport;
    int version;
};

Sub *newsub(int n);
Sub *incref(Sub*);
Sub *copy(Sub*);
Sub *update(Sub*, int, PostcardNode*);
void decref(Sub*);

int recursiveloop(Inst *pc, PostcardNode *sp, PostcardNode **subp, int nsubp);
int recursiveprog(Prog *prog, PostcardNode *input, PostcardNode **subp, int nsubp);
int recursiveloopprog(Prog *prog, PostcardNode *input, PostcardNode **subp, int nsubp);
int match(PostcardNode *sp, PostcardFilter *pf);

static inline int eo_postcard(PostcardNode *p) {
    return p == NULL;
}

class PacketHistoryFilter {
    private:
        RegexpMatcher match_fn;
        Regexp *re;
        Prog *prog;
        PostcardNode *sub[MAXSUB];
        char regex[MAX_REGEX_SIZE];

    public:
        PacketHistoryFilter(const char *regex_)
        {
            strcpy(regex, regex_);
            match_fn = recursiveloopprog;
            if(regex == NULL)
                return;
            else
                compile_(regex);
        }

        PacketHistoryFilter(const PacketHistoryFilter &phf)
        {
            this->match_fn = phf.match_fn;
            re = phf.re;
            prog = phf.prog;
            memcpy(sub, phf.sub, sizeof(PostcardNode*) * MAXSUB);
            strcpy(regex, phf.regex);
        }

        bool operator==(const char *re) const
        {
            return (strcmp(regex, re) == 0);
        }

        void clear()
        {
            free_reg(re);
            if(prog)
                free(prog);
        }

        string str()
        {
            return string(regex);
        }

        void compile_(char *regex)
        {
            re = parse(regex);
            parse_postcard_filters(re);
            prog = compile(re);
            memset(sub, 0, sizeof sub);
        }

        int match(PostcardList &pl)
        {
            DBG("Matching:\n");
            DBG("PHF:\"%s\"\n", regex);
            DBG("Packet History:\n%s\n", pl.str().c_str());
            return match_fn(prog, pl.head, sub, nelem(sub));
        }
};

#endif //REGEXP_H
