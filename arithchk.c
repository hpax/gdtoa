/****************************************************************
Copyright (C) 1997, 1998 Lucent Technologies
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

/* Try to deduce arith.h from arithmetic properties. */

#include <stdio.h>
#include <inttypes.h>

static int dalign;
typedef struct
    Akind {
	const char *name;
	int kind;
} Akind;

static const Akind IEEE_8087  = { "IEEE_8087", 1 };
static const Akind IEEE_MC68k = { "IEEE_MC68k", 2 };

static const Akind *Lcheck(void)
{
	union {
		double d;
		int32_t L[2];
	} u;
	struct {
		double d;
		int32_t L;
	} x[2];

	if (sizeof(double) != 2 * sizeof(int32_t))
		return NULL;

	if (sizeof(x) > 2 * (sizeof(double) + sizeof(int32_t)))
		dalign = 1;
	u.L[0] = u.L[1] = 0;
	u.d = 1e13;
	if (u.L[0] == 1117925532 && u.L[1] == -448790528)
		return &IEEE_MC68k;
	if (u.L[1] == 1117925532 && u.L[0] == -448790528)
		return &IEEE_8087;
	return NULL;
}

static int fzcheck(void)
{
	double a, b;
	int i;

	a = 1.;
	b = .1;
	for (i = 155;; b *= b, i >>= 1) {
		if (i & 1) {
			a *= b;
			if (i == 1)
				break;
		}
	}
	b = a * a;
	return b == 0.;
}

int main(void)
{
	const Akind *a = NULL;
	FILE *f;

#ifdef WRITE_ARITH_H		/* for Symantec's buggy "make" */
	f = fopen("arith.h", "w");
	if (!f) {
		printf("Cannot open arith.h\n");
		return 1;
	}
#else
	f = stdout;
#endif

	if (!a)
		a = Lcheck();
	if (a) {
		fprintf(f, "#define %s\n#define Arith_Kind_ASL %d\n",
			a->name, a->kind);
		if (dalign)
			fprintf(f, "#define Double_Align\n");
		if (a->kind <= 2 && fzcheck())
			fprintf(f, "#define Sudden_Underflow\n");
		return 0;
	}
	fprintf(f, "/* Unknown arithmetic */\n");
	return 1;
}
