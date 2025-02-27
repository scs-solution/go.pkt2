/*-
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 *      @(#)bpf_filter.c	8.1 (Berkeley) 6/10/93
 */

#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

#include <sys/cdefs.h>
#include <sys/param.h>

#include <netinet/in.h>

#ifndef __i386__
#define BPF_ALIGN
#endif

#define EXTRACT_BYTE(p)\
	((uint8_t)\
		((uint8_t)*((u_char *)p)))

#ifndef BPF_ALIGN
#define EXTRACT_SHORT(p)	((uint16_t)ntohs(*(uint16_t *)p))
#define EXTRACT_LONG(p)		(ntohl(*(uint32_t *)p))
#else
#define EXTRACT_SHORT(p)\
	((uint16_t)\
		((uint16_t)*((u_char *)p+0)<<8|\
		 (uint16_t)*((u_char *)p+1)<<0))
#define EXTRACT_LONG(p)\
		((uint32_t)*((u_char *)p+0)<<24|\
		 (uint32_t)*((u_char *)p+1)<<16|\
		 (uint32_t)*((u_char *)p+2)<<8|\
		 (uint32_t)*((u_char *)p+3)<<0)
#endif

#include "bpf_filter.h"

/*
 * Execute the filter program starting at pc on the packet p
 * wirelen is the length of the original packet
 * buflen is the amount of data present
 */
unsigned int
bpf_filter(const struct bpf_insn *pc, char *p, unsigned int wirelen, unsigned int buflen)
{
	uint32_t A = 0, X = 0;
	uint32_t k;
	uint32_t mem[BPF_MEMWORDS];

	bzero(mem, sizeof(mem));

	if (pc == NULL)
		/*
		 * No filter means accept all.
		 */
		return ((unsigned int)-1);

	--pc;
	while (1) {
		++pc;
		switch (pc->code) {
		default:
			abort();

		case BPF_RET|BPF_K:
			return ((unsigned int)pc->k);

		case BPF_RET|BPF_A:
			return ((unsigned int)A);

		case BPF_LD|BPF_W|BPF_ABS:
			k = pc->k;
			if (k > buflen || sizeof(int32_t) > buflen - k) {
				return (0);
			}
#ifdef BPF_ALIGN
			if (((intptr_t)(p + k) & 3) != 0)
				A = EXTRACT_LONG(&p[k]);
			else
#endif
				A = ntohl(*(int32_t *)(p + k));
			continue;

		case BPF_LD|BPF_H|BPF_ABS:
			k = pc->k;
			if (k > buflen || sizeof(int16_t) > buflen - k) {
				return (0);
			}
			A = EXTRACT_SHORT(&p[k]);
			continue;

		case BPF_LD|BPF_B|BPF_ABS:
			k = pc->k;
			if (k >= buflen) {
				return (0);
			}
			A = EXTRACT_BYTE(&p[k]);
			continue;

		case BPF_LD|BPF_W|BPF_LEN:
			A = wirelen;
			continue;

		case BPF_LDX|BPF_W|BPF_LEN:
			X = wirelen;
			continue;

		case BPF_LD|BPF_W|BPF_IND:
			k = X + pc->k;
			if (pc->k > buflen || X > buflen - pc->k ||
			    sizeof(int32_t) > buflen - k) {
				return (0);
			}
#ifdef BPF_ALIGN
			if (((intptr_t)(p + k) & 3) != 0)
				A = EXTRACT_LONG(&p[k]);
			else
#endif
				A = ntohl(*(int32_t *)(p + k));
			continue;

		case BPF_LD|BPF_H|BPF_IND:
			k = X + pc->k;
			if (X > buflen || pc->k > buflen - X ||
			    sizeof(int16_t) > buflen - k) {
				return (0);
			}
			A = EXTRACT_SHORT(&p[k]);
			continue;

		case BPF_LD|BPF_B|BPF_IND:
			k = X + pc->k;
			if (pc->k >= buflen || X >= buflen - pc->k) {
				return (0);
			}
			A = EXTRACT_BYTE(&p[k]);
			continue;

		case BPF_LDX|BPF_MSH|BPF_B:
			k = pc->k;
			if (k >= buflen) {
				return (0);
			}
			X = (p[pc->k] & 0xf) << 2;
			continue;

		case BPF_LD|BPF_IMM:
			A = pc->k;
			continue;

		case BPF_LDX|BPF_IMM:
			X = pc->k;
			continue;

		case BPF_LD|BPF_MEM:
			A = mem[pc->k];
			continue;

		case BPF_LDX|BPF_MEM:
			X = mem[pc->k];
			continue;

		case BPF_ST:
			mem[pc->k] = A;
			continue;

		case BPF_STX:
			mem[pc->k] = X;
			continue;

		case BPF_JMP|BPF_JA:
			pc += pc->k;
			continue;

		case BPF_JMP|BPF_JGT|BPF_K:
			pc += (A > pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_K:
			pc += (A >= pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_K:
			pc += (A == pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_K:
			pc += (A & pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGT|BPF_X:
			pc += (A > X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_X:
			pc += (A >= X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_X:
			pc += (A == X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_X:
			pc += (A & X) ? pc->jt : pc->jf;
			continue;

		case BPF_ALU|BPF_ADD|BPF_X:
			A += X;
			continue;

		case BPF_ALU|BPF_SUB|BPF_X:
			A -= X;
			continue;

		case BPF_ALU|BPF_MUL|BPF_X:
			A *= X;
			continue;

		case BPF_ALU|BPF_DIV|BPF_X:
			if (X == 0)
				return (0);
			A /= X;
			continue;

		case BPF_ALU|BPF_MOD|BPF_X:
			if (X == 0)
				return 0;
			A %= X;
			continue;

		case BPF_ALU|BPF_AND|BPF_X:
			A &= X;
			continue;

		case BPF_ALU|BPF_OR|BPF_X:
			A |= X;
			continue;

		case BPF_ALU|BPF_XOR|BPF_X:
			A ^= X;
			continue;

		case BPF_ALU|BPF_LSH|BPF_X:
			A <<= X;
			continue;

		case BPF_ALU|BPF_RSH|BPF_X:
			A >>= X;
			continue;

		case BPF_ALU|BPF_ADD|BPF_K:
			A += pc->k;
			continue;

		case BPF_ALU|BPF_SUB|BPF_K:
			A -= pc->k;
			continue;

		case BPF_ALU|BPF_MUL|BPF_K:
			A *= pc->k;
			continue;

		case BPF_ALU|BPF_DIV|BPF_K:
			A /= pc->k;
			continue;

		case BPF_ALU|BPF_MOD|BPF_K:
			A %= pc->k;
			continue;

		case BPF_ALU|BPF_AND|BPF_K:
			A &= pc->k;
			continue;

		case BPF_ALU|BPF_OR|BPF_K:
			A |= pc->k;
			continue;

		case BPF_ALU|BPF_XOR|BPF_K:
			A ^= pc->k;
			continue;

		case BPF_ALU|BPF_LSH|BPF_K:
			A <<= pc->k;
			continue;

		case BPF_ALU|BPF_RSH|BPF_K:
			A >>= pc->k;
			continue;

		case BPF_ALU|BPF_NEG:
			A = -A;
			continue;

		case BPF_MISC|BPF_TAX:
			X = A;
			continue;

		case BPF_MISC|BPF_TXA:
			A = X;
			continue;
		}
	}
}

static const unsigned short	bpf_code_map[] = {
	0x10ff,	/* 0x00-0x0f: 1111111100001000 */
	0x3070,	/* 0x10-0x1f: 0000111000001100 */
	0x3131,	/* 0x20-0x2f: 1000110010001100 */
	0x3031,	/* 0x30-0x3f: 1000110000001100 */
	0x3131,	/* 0x40-0x4f: 1000110010001100 */
	0x1011,	/* 0x50-0x5f: 1000100000001000 */
	0x1013,	/* 0x60-0x6f: 1100100000001000 */
	0x1010,	/* 0x70-0x7f: 0000100000001000 */
	0x0093,	/* 0x80-0x8f: 1100100100000000 */
	0x1010,	/* 0x90-0x9f: 0000100000001000 */
	0x1010,	/* 0xa0-0xaf: 0000100000001000 */
	0x0002,	/* 0xb0-0xbf: 0100000000000000 */
	0x0000,	/* 0xc0-0xcf: 0000000000000000 */
	0x0000,	/* 0xd0-0xdf: 0000000000000000 */
	0x0000,	/* 0xe0-0xef: 0000000000000000 */
	0x0000	/* 0xf0-0xff: 0000000000000000 */
};

#define	BPF_VALIDATE_CODE(c)	\
    ((c) <= 0xff && (bpf_code_map[(c) >> 4] & (1 << ((c) & 0xf))) != 0)

/*
 * Return true if the 'fcode' is a valid filter program.
 * The constraints are that each jump be forward and to a valid
 * code.  The code must terminate with either an accept or reject.
 */
int
bpf_validate(const struct bpf_insn *f, int len)
{
	register int i;
	register const struct bpf_insn *p;

	/* Do not accept negative length filter. */
	if (len < 0)
		return (0);

	/* An empty filter means accept all. */
	if (len == 0)
		return (1);

	for (i = 0; i < len; ++i) {
		p = &f[i];
		/*
		 * Check that the code is valid.
		 */
		if (!BPF_VALIDATE_CODE(p->code))
			return (0);
		/*
		 * Check that that jumps are forward, and within
		 * the code block.
		 */
		if (BPF_CLASS(p->code) == BPF_JMP) {
			register unsigned int offset;

			if (p->code == (BPF_JMP|BPF_JA))
				offset = p->k;
			else
				offset = p->jt > p->jf ? p->jt : p->jf;
			if (offset >= (unsigned int)(len - i) - 1)
				return (0);
			continue;
		}
		/*
		 * Check that memory operations use valid addresses.
		 */
		if (p->code == BPF_ST || p->code == BPF_STX ||
		    p->code == (BPF_LD|BPF_MEM) ||
		    p->code == (BPF_LDX|BPF_MEM)) {
			if (p->k >= BPF_MEMWORDS)
				return (0);
			continue;
		}
		/*
		 * Check for constant division by 0.
		 */
		if ((p->code == (BPF_ALU|BPF_DIV|BPF_K) ||
		    p->code == (BPF_ALU|BPF_MOD|BPF_K)) && p->k == 0)
			return (0);
	}
	return (BPF_CLASS(f[len - 1].code) == BPF_RET);
}

int
bpf_append_insn(struct bpf_program *p, unsigned short code,
                unsigned char jt, unsigned char jf, unsigned int k)
{
	p->bf_insns = realloc(p->bf_insns, ++p->bf_len*sizeof(struct bpf_insn));
	if (p->bf_insns == NULL)
		return -1;

	p->bf_insns[p->bf_len - 1].code = code;
	p->bf_insns[p->bf_len - 1].jt   = jt;
	p->bf_insns[p->bf_len - 1].jf   = jf;
	p->bf_insns[p->bf_len - 1].k    = k;

	return 0;
}

int
bpf_get_len(struct bpf_program *p)
{
	return p->bf_len;
}

struct bpf_insn *
bpf_get_insn(struct bpf_program *p, int i)
{
	if (i > p->bf_len)
		return NULL;

	return &p->bf_insns[i];
}
