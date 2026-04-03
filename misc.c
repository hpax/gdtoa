/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 1998, 1999 by Lucent Technologies
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of Lucent or any of its entities
not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

****************************************************************/

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").	*/

#include "gdtoaimp.h"

#ifndef Omit_Private_Memory
#ifndef PRIVATE_MEM
#define PRIVATE_MEM 2304
#endif
#define PRIVATE_mem ((PRIVATE_MEM+sizeof(double)-1)/sizeof(double))
static double private_mem[PRIVATE_mem], *pmem_next = private_mem;
#endif

static ThInfo TI0;

#ifdef MULTIPLE_THREADS		/*{{ */
static unsigned int maxthreads = 0;
static ThInfo *TI1;
static int TI0_used;

void set_max_gdtoa_threads(unsigned int n)
{
	size_t L;

	if (n > maxthreads) {
		L = n * sizeof(ThInfo);
		if (TI1) {
			TI1 = (ThInfo *) REALLOC(TI1, L);
			memset(TI1 + maxthreads, 0,
			       (n - maxthreads) * sizeof(ThInfo));
		} else {
			TI1 = (ThInfo *) MALLOC(L);
			if (TI0_used) {
				memcpy(TI1, &TI0, sizeof(ThInfo));
				if (n > 1)
					memset(TI1 + 1, 0, L - sizeof(ThInfo));
				memset(&TI0, 0, sizeof(ThInfo));
			} else
				memset(TI1, 0, L);
		}
		maxthreads = n;
	}
}

static ThInfo *get_TI(void)
{
	unsigned int thno = dtoa_get_threadno();
	if (thno < maxthreads)
		return TI1 + thno;
	if (thno == 0)
		TI0_used = 1;
	return &TI0;
}

#define freelist TI->Freelist
#define p5s TI->P5s
#else				/*}{ */
#define freelist TI0.Freelist
#define p5s TI0.P5s
#endif				/*}} */

Bigint *Balloc(int k MTd) {
	int x;
	Bigint *rv;
#ifndef Omit_Private_Memory
	unsigned int len;
#endif

#ifdef MULTIPLE_THREADS
	ThInfo *TI;

	if (!(TI = *PTI))
		*PTI = TI = get_TI();
	if (TI == &TI0)
		ACQUIRE_DTOA_LOCK(0);
#endif
	/* The k > Kmax case does not need ACQUIRE_DTOA_LOCK(0), */
	/* but this case seems very unlikely. */
	if (k <= Kmax && (rv = freelist[k])) {
		freelist[k] = rv->next;
	} else {
		x = 1 << k;
#ifdef Omit_Private_Memory
		rv = (Bigint *) MALLOC(sizeof(Bigint) +
				       (x - 1) * sizeof(uint32_t));
#else
		len =
		    (sizeof(Bigint) + (x - 1) * sizeof(uint32_t) +
		     sizeof(double) - 1)
		    / sizeof(double);
		if (k <= Kmax
		    && pmem_next - private_mem + len - PRIVATE_mem <= 0
#ifdef MULTIPLE_THREADS
		    && TI == TI1
#endif
		    ) {
			rv = (Bigint *) pmem_next;
			pmem_next += len;
		} else
			rv = (Bigint *) MALLOC(len * sizeof(double));
#endif
		rv->k = k;
		rv->maxwds = x;
	}
#ifdef MULTIPLE_THREADS
	if (TI == &TI0)
		FREE_DTOA_LOCK(0);
#endif
	rv->sign = rv->wds = 0;
	return rv;
}

void
 Bfree(Bigint * v MTd) {
#ifdef MULTIPLE_THREADS
	ThInfo *TI;
#endif
	if (v) {
		if (v->k > Kmax)
#ifdef FREE
			FREE((void *)v);
#else
			free((void *)v);
#endif
		else {
#ifdef MULTIPLE_THREADS
			if (!(TI = *PTI))
				*PTI = TI = get_TI();
			if (TI == &TI0)
				ACQUIRE_DTOA_LOCK(0);
#endif
			v->next = freelist[v->k];
			freelist[v->k] = v;
#ifdef MULTIPLE_THREADS
			if (TI == &TI0)
				FREE_DTOA_LOCK(0);
#endif
		}
	}
}

int
 lo0bits(uint32_t * y) {
	int k;
	uint32_t x = *y;

	if (x & 7) {
		if (x & 1)
			return 0;
		if (x & 2) {
			*y = x >> 1;
			return 1;
		}
		*y = x >> 2;
		return 2;
	}
	k = 0;
	if (!(x & 0xffff)) {
		k = 16;
		x >>= 16;
	}
	if (!(x & 0xff)) {
		k += 8;
		x >>= 8;
	}
	if (!(x & 0xf)) {
		k += 4;
		x >>= 4;
	}
	if (!(x & 0x3)) {
		k += 2;
		x >>= 2;
	}
	if (!(x & 1)) {
		k++;
		x >>= 1;
		if (!x)
			return 32;
	}
	*y = x;
	return k;
}

Bigint *multadd(Bigint * b, int m, int a MTd) {	/* multiply by m and add a */
	int i, wds;
	uint32_t *x;
	uint64_t carry, y;
	Bigint *b1;

	wds = b->wds;
	x = b->x;
	i = 0;
	carry = a;
	do {
		y = *x * (uint64_t) m + carry;
		carry = y >> 32;
		*x++ = y & 0xffffffffUL;
	}
	while (++i < wds);
	if (carry) {
		if (wds >= b->maxwds) {
			b1 = Balloc(b->k + 1 MTa);
			Bcopy(b1, b);
			Bfree(b MTa);
			b = b1;
		}
		b->x[wds++] = carry;
		b->wds = wds;
	}
	return b;
}

int
 hi0bits_D2A(uint32_t x) {
	int k = 0;

	if (!(x & 0xffff0000)) {
		k = 16;
		x <<= 16;
	}
	if (!(x & 0xff000000)) {
		k += 8;
		x <<= 8;
	}
	if (!(x & 0xf0000000)) {
		k += 4;
		x <<= 4;
	}
	if (!(x & 0xc0000000)) {
		k += 2;
		x <<= 2;
	}
	if (!(x & 0x80000000)) {
		k++;
		if (!(x & 0x40000000))
			return 32;
	}
	return k;
}

Bigint *i2b(int i MTd) {
	Bigint *b;

	b = Balloc(1 MTa);
	b->x[0] = i;
	b->wds = 1;
	return b;
}

Bigint *mult(Bigint * a, Bigint * b MTd) {
	Bigint *c;
	int k, wa, wb, wc;
	uint32_t *x, *xa, *xae, *xb, *xbe, *xc, *xc0;
	uint32_t y;
	uint64_t carry, z;

	if (a->wds < b->wds) {
		c = a;
		a = b;
		b = c;
	}
	k = a->k;
	wa = a->wds;
	wb = b->wds;
	wc = wa + wb;
	if (wc > a->maxwds)
		k++;
	c = Balloc(k MTa);
	for (x = c->x, xa = x + wc; x < xa; x++)
		*x = 0;
	xa = a->x;
	xae = xa + wa;
	xb = b->x;
	xbe = xb + wb;
	xc0 = c->x;
	for (; xb < xbe; xc0++) {
		if ((y = *xb++) != 0) {
			x = xa;
			xc = xc0;
			carry = 0;
			do {
				z = *x++ * (uint64_t) y + *xc + carry;
				carry = z >> 32;
				*xc++ = z & 0xffffffffUL;
			}
			while (x < xae);
			*xc = carry;
		}
	}
	for (xc0 = c->x, xc = xc0 + wc; wc > 0 && !*--xc; --wc) ;
	c->wds = wc;
	return c;
}

Bigint *pow5mult(Bigint * b, int k MTd) {
	Bigint *b1, *p5, *p51;
#ifdef MULTIPLE_THREADS
	ThInfo *TI;
#endif
	int i;
	static int p05[3] = { 5, 25, 125 };

	if ((i = k & 3) != 0)
		b = multadd(b, p05[i - 1], 0 MTa);

	if (!(k >>= 2))
		return b;
#ifdef  MULTIPLE_THREADS
	if (!(TI = *PTI))
		*PTI = TI = get_TI();
#endif
	if (!(p5 = p5s)) {
		/* first time */
#ifdef MULTIPLE_THREADS
		if (!(TI = *PTI))
			*PTI = TI = get_TI();
		if (TI == &TI0)
			ACQUIRE_DTOA_LOCK(1);
		if (!(p5 = p5s)) {
			p5 = p5s = i2b(625 MTa);
			p5->next = 0;
		}
		if (TI == &TI0)
			FREE_DTOA_LOCK(1);
#else
		p5 = p5s = i2b(625);
		p5->next = NULL;
#endif
	}
	for (;;) {
		if (k & 1) {
			b1 = mult(b, p5 MTa);
			Bfree(b MTa);
			b = b1;
		}
		if (!(k >>= 1))
			break;
		if (!(p51 = p5->next)) {
#ifdef MULTIPLE_THREADS
			if (!TI && !(TI = *PTI))
				*PTI = TI = get_TI();
			if (TI == &TI0)
				ACQUIRE_DTOA_LOCK(1);
			if (!(p51 = p5->next)) {
				p51 = p5->next = mult(p5, p5 MTa);
				p51->next = 0;
			}
			if (TI == &TI0)
				FREE_DTOA_LOCK(1);
#else
			p51 = p5->next = mult(p5, p5 MTa);
			p51->next = NULL;
#endif
		}
		p5 = p51;
	}
	return b;
}

Bigint *lshift(Bigint * b, int k MTd) {
	int i, k1, n, n1;
	Bigint *b1;
	uint32_t *x, *x1, *xe, z;

	n = k >> kshift;
	k1 = b->k;
	n1 = n + b->wds + 1;
	for (i = b->maxwds; n1 > i; i <<= 1)
		k1++;
	b1 = Balloc(k1 MTa);
	x1 = b1->x;
	for (i = 0; i < n; i++)
		*x1++ = 0;
	x = b->x;
	xe = x + b->wds;
	if (k &= kmask) {
		k1 = 32 - k;
		z = 0;
		do {
			*x1++ = *x << k | z;
			z = *x++ >> k1;
		}
		while (x < xe);
		if ((*x1 = z) != 0)
			++n1;
	} else
		do
			*x1++ = *x++;
		while (x < xe);
	b1->wds = n1 - 1;
	Bfree(b MTa);
	return b1;
}

int
 cmp(Bigint * a, Bigint * b) {
	uint32_t *xa, *xa0, *xb, *xb0;
	int i, j;

	i = a->wds;
	j = b->wds;
#ifdef DEBUG
	if (i > 1 && !a->x[i - 1])
		Bug("cmp called with a->x[a->wds-1] == 0");
	if (j > 1 && !b->x[j - 1])
		Bug("cmp called with b->x[b->wds-1] == 0");
#endif
	if (i -= j)
		return i;
	xa0 = a->x;
	xa = xa0 + j;
	xb0 = b->x;
	xb = xb0 + j;
	for (;;) {
		if (*--xa != *--xb)
			return *xa < *xb ? -1 : 1;
		if (xa <= xa0)
			break;
	}
	return 0;
}

Bigint *diff(Bigint * a, Bigint * b MTd) {
	Bigint *c;
	int i, wa, wb;
	uint32_t *xa, *xae, *xb, *xbe, *xc;
	uint64_t borrow, y;

	i = cmp(a, b);
	if (!i) {
		c = Balloc(0 MTa);
		c->wds = 1;
		c->x[0] = 0;
		return c;
	}
	if (i < 0) {
		c = a;
		a = b;
		b = c;
		i = 1;
	} else
		i = 0;
	c = Balloc(a->k MTa);
	c->sign = i;
	wa = a->wds;
	xa = a->x;
	xae = xa + wa;
	wb = b->wds;
	xb = b->x;
	xbe = xb + wb;
	xc = c->x;
	borrow = 0;
	do {
		y = (uint64_t) * xa++ - *xb++ - borrow;
		borrow = y >> 32 & 1UL;
		*xc++ = y & 0xffffffffUL;
	}
	while (xb < xbe);
	while (xa < xae) {
		y = *xa++ - borrow;
		borrow = y >> 32 & 1UL;
		*xc++ = y & 0xffffffffUL;
	}
	while (!*--xc)
		wa--;
	c->wds = wa;
	return c;
}

double
 b2d(Bigint * a, int *e) {
	uint32_t *xa, *xa0, w, y, z;
	int k;
	U d;
#define d0 word0(&d)
#define d1 word1(&d)

	xa0 = a->x;
	xa = xa0 + a->wds;
	y = *--xa;
#ifdef DEBUG
	if (!y)
		Bug("zero y in b2d");
#endif
	k = hi0bits(y);
	*e = 32 - k;
	if (k < Ebits) {
		d0 = Exp_1 | y >> (Ebits - k);
		w = xa > xa0 ? *--xa : 0;
		d1 = y << ((32 - Ebits) + k) | w >> (Ebits - k);
		goto ret_d;
	}
	z = xa > xa0 ? *--xa : 0;
	if (k -= Ebits) {
		d0 = Exp_1 | y << k | z >> (32 - k);
		y = xa > xa0 ? *--xa : 0;
		d1 = z << k | y >> (32 - k);
	} else {
		d0 = Exp_1 | y;
		d1 = z;
	}
 ret_d:
	return dval(&d);
}

#undef d0
#undef d1

Bigint *d2b(double dd, int *e, int *bits MTd) {
	Bigint *b;
	U d;
#ifndef Sudden_Underflow
	int i;
#endif
	int de, k;
	uint32_t *x, y, z;
#define d0 word0(&d)
#define d1 word1(&d)
	d.d = dd;

	b = Balloc(1 MTa);
	x = b->x;

	z = d0 & Frac_mask;
	d0 &= 0x7fffffff;	/* clear sign bit, which we ignore */
#ifdef Sudden_Underflow
	de = (int)(d0 >> Exp_shift);
	z |= Exp_msk11;
#else
	if ((de = (int)(d0 >> Exp_shift)) != 0)
		z |= Exp_msk1;
#endif
	if ((y = d1) != 0) {
		if ((k = lo0bits(&y)) != 0) {
			x[0] = y | z << (32 - k);
			z >>= k;
		} else
			x[0] = y;
#ifndef Sudden_Underflow
		i =
#endif
		    b->wds = (x[1] = z) != 0 ? 2 : 1;
	} else {
		k = lo0bits(&z);
		x[0] = z;
#ifndef Sudden_Underflow
		i =
#endif
		    b->wds = 1;
		k += 32;
	}
#ifndef Sudden_Underflow
	if (de) {
#endif
		*e = de - Bias - (P - 1) + k;
		*bits = P - k;
#ifndef Sudden_Underflow
	} else {
		*e = de - Bias - (P - 1) + 1 + k;
		*bits = 32 * i - hi0bits(x[i - 1]);
	}
#endif
	return b;
}

#undef d0
#undef d1

const double
 bigtens[] = { 1e16, 1e32, 1e64, 1e128, 1e256 };

const double tinytens[] = { 1e-16, 1e-32, 1e-64, 1e-128, 1e-256
};

const double
 tens[] = {
	1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
	1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
	1e20, 1e21, 1e22
};

char *strcp_D2A(char *a, const char *b)
{
	while ((*a = *b++))
		a++;
	return a;
}

#ifdef NO_STRING_H

void *memcpy_D2A(void *a1, void *b1, size_t len)
{
	char *a = (char *)a1, *ae = a + len;
	char *b = (char *)b1, *a0 = a;
	while (a < ae)
		*a++ = *b++;
	return a0;
}

#endif				/* NO_STRING_H */
