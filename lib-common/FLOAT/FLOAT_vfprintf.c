#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "FLOAT.h"

#ifdef LINUX_RT
#include <sys/mman.h>
#endif

extern char _vfprintf_internal;
extern char _fpmaxtostr;
extern char _ppfs_setargs;
extern int __stdio_fwrite(char *buf, int len, FILE *stream);

static uint8_t *find_bytes(uint8_t *start, int len,
		const uint8_t *pattern, int pattern_len) {
	int i;
	for (i = 0; i <= len - pattern_len; i ++) {
		if (memcmp(start + i, pattern, pattern_len) == 0) {
			return start + i;
		}
	}
	return NULL;
}

static void write_rel32(uint8_t *instruction, uint8_t opcode, void *target) {
	int32_t displacement = (uint8_t *)target - (instruction + 5);
	instruction[0] = opcode;
	memcpy(instruction + 1, &displacement, sizeof(displacement));
}

#ifdef LINUX_RT
static void make_writable(void *address) {
	uintptr_t page = (uintptr_t)address & ~(uintptr_t)0xfff;
	assert(mprotect((void *)page, 0x2000,
			PROT_READ | PROT_WRITE | PROT_EXEC) == 0);
}
#else
#define make_writable(address) ((void)(address))
#endif

__attribute__((used)) static int format_FLOAT(FILE *stream, FLOAT f) {
	/* TODO: Format a FLOAT argument `f' and write the formating
	 * result to `stream'. Keep the precision of the formating
	 * result with 6 by truncating. For example:
	 *              f          result
	 *         0x00010000    "1.000000"
	 *         0x00013333    "1.199996"
	 */

	char buf[80];
	int64_t value = f;
	uint64_t magnitude = value < 0 ? (uint64_t)(-value) : (uint64_t)value;
	uint32_t integer = magnitude >> 16;
	uint32_t fraction = ((magnitude & 0xffff) * 1000000ull) >> 16;
	int len;

	if (value < 0) {
		len = sprintf(buf, "-%u.%06u", integer, fraction);
	} else {
		len = sprintf(buf, "%u.%06u", integer, fraction);
	}
	return __stdio_fwrite(buf, len, stream);
}

static void modify_vfprintf() {
	/* TODO: Implement this function to hijack the formating of "%f"
	 * argument during the execution of `_vfprintf_internal'. Below
	 * is the code section in _vfprintf_internal() relative to the
	 * hijack.
	 */

#if 0
	else if (ppfs->conv_num <= CONV_A) {  /* floating point */
		ssize_t nf;
		nf = _fpmaxtostr(stream,
				(__fpmax_t)
				(PRINT_INFO_FLAG_VAL(&(ppfs->info),is_long_double)
				 ? *(long double *) *argptr
				 : (long double) (* (double *) *argptr)),
				&ppfs->info, FP_OUT );
		if (nf < 0) {
			return -1;
		}
		*count += nf;

		return 0;
	} else if (ppfs->conv_num <= CONV_S) {  /* wide char or string */
#endif

	/* You should modify the run-time binary to let the code above
	 * call `format_FLOAT' defined in this source file, instead of
	 * `_fpmaxtostr'. When this function returns, the action of the
	 * code above should do the following:
	 */

#if 0
	else if (ppfs->conv_num <= CONV_A) {  /* floating point */
		ssize_t nf;
		nf = format_FLOAT(stream, *(FLOAT *) *argptr);
		if (nf < 0) {
			return -1;
		}
		*count += nf;

		return 0;
	} else if (ppfs->conv_num <= CONV_S) {  /* wide char or string */
#endif
	uint8_t *start = (uint8_t *)&_vfprintf_internal;
	uint8_t *call = NULL;
	uint8_t *p;
	int i;

	make_writable(start);
	for (i = 0; i < 0x600 - 5; i ++) {
		if (start[i] == 0xe8) {
			int32_t displacement;
			memcpy(&displacement, start + i + 1, sizeof(displacement));
			if (start + i + 5 + displacement == (uint8_t *)&_fpmaxtostr) {
				call = start + i;
				break;
			}
		}
	}
	assert(call != NULL);

	/* Replace both possible x87 loads before the conversion call with NOPs. */
	for (p = call - 40; p < call - 10; p ++) {
		if ((p[0] == 0xdb && p[1] == 0x2a) ||
				(p[0] == 0xdd && p[1] == 0x02)) {
			p[0] = p[1] = 0x90;
		}
	}

	/* Keep the original stack depth, but push the Q16.16 word from argptr. */
	assert(call[-13] == 0x83 && call[-12] == 0xec && call[-11] == 0x0c);
	assert(call[-10] == 0xdb && call[-9] == 0x3c && call[-8] == 0x24);
	call[-11] = 0x08;
	call[-10] = 0xff;
	call[-9] = 0x32;
	call[-8] = 0x90;
	write_rel32(call, 0xe8, format_FLOAT);
}

static void modify_ppfs_setargs() {
	/* TODO: Implement this function to modify the action of preparing
	 * "%f" arguments for _vfprintf_internal() in _ppfs_setargs().
	 * Below is the code section in _vfprintf_internal() relative to
	 * the modification.
	 */

#if 0
	enum {                          /* C type: */
		PA_INT,                       /* int */
		PA_CHAR,                      /* int, cast to char */
		PA_WCHAR,                     /* wide char */
		PA_STRING,                    /* const char *, a '\0'-terminated string */
		PA_WSTRING,                   /* const wchar_t *, wide character string */
		PA_POINTER,                   /* void * */
		PA_FLOAT,                     /* float */
		PA_DOUBLE,                    /* double */
		__PA_NOARG,                   /* non-glibc -- signals non-arg width or prec */
		PA_LAST
	};

	/* Flag bits that can be set in a type returned by `parse_printf_format'.  */
	/* WARNING -- These differ in value from what glibc uses. */
#define PA_FLAG_MASK		(0xff00)
#define __PA_FLAG_CHAR		(0x0100) /* non-gnu -- to deal with hh */
#define PA_FLAG_SHORT		(0x0200)
#define PA_FLAG_LONG		(0x0400)
#define PA_FLAG_LONG_LONG	(0x0800)
#define PA_FLAG_LONG_DOUBLE	PA_FLAG_LONG_LONG
#define PA_FLAG_PTR		(0x1000) /* TODO -- make dynamic??? */

	while (i < ppfs->num_data_args) {
		switch(ppfs->argtype[i++]) {
			case (PA_INT|PA_FLAG_LONG_LONG):
				GET_VA_ARG(p,ull,unsigned long long,ppfs->arg);
				break;
			case (PA_INT|PA_FLAG_LONG):
				GET_VA_ARG(p,ul,unsigned long,ppfs->arg);
				break;
			case PA_CHAR:	/* TODO - be careful */
				/* ... users could use above and really want below!! */
			case (PA_INT|__PA_FLAG_CHAR):/* TODO -- translate this!!! */
			case (PA_INT|PA_FLAG_SHORT):
			case PA_INT:
				GET_VA_ARG(p,u,unsigned int,ppfs->arg);
				break;
			case PA_WCHAR:	/* TODO -- assume int? */
				/* we're assuming wchar_t is at least an int */
				GET_VA_ARG(p,wc,wchar_t,ppfs->arg);
				break;
				/* PA_FLOAT */
			case PA_DOUBLE:
				GET_VA_ARG(p,d,double,ppfs->arg);
				break;
			case (PA_DOUBLE|PA_FLAG_LONG_DOUBLE):
				GET_VA_ARG(p,ld,long double,ppfs->arg);
				break;
			default:
				/* TODO -- really need to ensure this can't happen */
				assert(ppfs->argtype[i-1] & PA_FLAG_PTR);
			case PA_POINTER:
			case PA_STRING:
			case PA_WSTRING:
				GET_VA_ARG(p,p,void *,ppfs->arg);
				break;
			case __PA_NOARG:
				continue;
		}
		++p;
	}
#endif

	/* You should modify the run-time binary to let the `PA_DOUBLE'
	 * branch execute the code in the `(PA_INT|PA_FLAG_LONG_LONG)'
	 * branch. Comparing to the original `PA_DOUBLE' branch, the
	 * target branch will also prepare a 64-bit argument, without
	 * introducing floating point instructions. When this function
	 * returns, the action of the code above should do the following:
	 */

#if 0
	while (i < ppfs->num_data_args) {
		switch(ppfs->argtype[i++]) {
			case (PA_INT|PA_FLAG_LONG_LONG):
			here:
				GET_VA_ARG(p,ull,unsigned long long,ppfs->arg);
				break;
			// ......
				/* PA_FLOAT */
			case PA_DOUBLE:
				goto here;
				GET_VA_ARG(p,d,double,ppfs->arg);
				break;
			// ......
		}
		++p;
	}
#endif
	static const uint8_t double_branch[] = { 0x8d, 0x5a, 0x08, 0xdd, 0x02 };
	static const uint8_t long_long_branch[] = {
		0x8b, 0x3a, 0x8b, 0x6a, 0x04, 0x8d, 0x5a, 0x08
	};
	uint8_t *start = (uint8_t *)&_ppfs_setargs;
	uint8_t *from;
	uint8_t *to;

	make_writable(start);
	from = find_bytes(start, 0x200, double_branch, sizeof(double_branch));
	to = find_bytes(start, 0x200, long_long_branch, sizeof(long_long_branch));
	assert(from != NULL && to != NULL);
	write_rel32(from, 0xe9, to);
}

void init_FLOAT_vfprintf() {
	modify_vfprintf();
	modify_ppfs_setargs();
}
