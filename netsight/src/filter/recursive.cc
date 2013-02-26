// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "regexp.hh"

int match(PostcardNode *sp, PostcardFilter *pf) {
    if(eo_postcard(sp))
        return 0;

        struct sk_buff skbuff;
        skbuff.len = sp->pkt->caplen;
        skbuff.data_len = sp->pkt->caplen;
        skbuff.data = (unsigned char *)sp->pkt->buff;

	if (pf->dpid > 0 && pf->dpid != sp->dpid)
		return 0;

	if (pf->inport > 0 && pf->inport != sp->inport)
		return 0;

	if (pf->outport > 0 && pf->outport != sp->outport)
		return 0;

	if (pf->version > 0 && pf->version != sp->version)
		return 0;

	if (pf->bpf > 0) {
		int ret;
#if BPF_JIT
                ret = pf->filter.bpf_func(&skbuff, &pf->filter.insns[0]);
#else
		ret = bpf_filter(pf->program.bf_insns, (unsigned char *)sp->pkt->buff,
				 sp->pkt->caplen, sp->pkt->caplen);
#endif
		if (!ret)
			return 0;
	}

	return 1;
}

int
recursive(Inst *pc, PostcardNode *sp, PostcardNode **subp, int nsubp)
{
    PostcardNode *old;

    switch(pc->opcode) {
        case Char:
            if(!match(sp, pc->pfexp))
                return 0;
        case Any:
            if(eo_postcard(sp))
                return 0;
            return recursive(pc+1, sp->next, subp, nsubp);
        case Match:
            return 1;
        case Jmp:
            return recursive(pc->x, sp, subp, nsubp);
        case Split:
            if(recursive(pc->x, sp, subp, nsubp))
                return 1;
            return recursive(pc->y, sp, subp, nsubp);
        case Save:
            if(pc->n >= nsubp)
                return recursive(pc+1, sp, subp, nsubp);
            old = subp[pc->n];
            subp[pc->n] = sp;
            if(recursive(pc+1, sp, subp, nsubp))
                return 1;
            subp[pc->n] = old;
            return 0;
    }
	fatal("recursive");
	return -1;
}

int
recursiveprog(Prog *prog, PostcardNode *input, PostcardNode **subp, int nsubp)
{
	return recursive(prog->start, input, subp, nsubp);
}

int
recursiveloop(Inst *pc, PostcardNode *sp, PostcardNode **subp, int nsubp)
{
    PostcardNode *old;

    for(;;) {
        switch(pc->opcode) {
            case Char:
                if(!match(sp, pc->pfexp)) {
                    return 0;
                }
            case Any:
                if (eo_postcard(sp))
                    return 0;
                pc++;
                sp = sp->next;
                continue;
            case Match:
                return 1;
            case Jmp:
                pc = pc->x;
                continue;
            case Split:
                if(recursiveloop(pc->x, sp, subp, nsubp))
                    return 1;
                pc = pc->y;
                continue;
            case Save:
                if(pc->n >= nsubp) {
                    pc++;
                    continue;
                }
                old = subp[pc->n];
                subp[pc->n] = sp;
                if(recursiveloop(pc+1, sp, subp, nsubp))
                    return 1;
                subp[pc->n] = old;
                return 0;
        }
        fatal("recursiveloop");
    }
}

int
recursiveloopprog(Prog *prog, PostcardNode *input, PostcardNode **subp, int nsubp)
{
	return recursiveloop(prog->start, input, subp, nsubp);
}
