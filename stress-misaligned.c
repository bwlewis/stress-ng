/*
 * Copyright (C) 2013-2021 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */
#include "stress-ng.h"

#define MISALIGN_LOOPS		(65536)

#if defined(HAVE_ATOMIC_ADD_FETCH) &&	\
    defined(__ATOMIC_SEQ_CST)
#if defined(ATOMIC_INC)
#undef ATOMIC_INC
#endif
#define ATOMIC_INC(ptr) __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
#endif

static const stress_help_t help[] = {
	{ NULL,	"misaligned N",	   	"start N workers performing misaligned read/writes" },
	{ NULL,	"misaligned-ops N",	"stop after N misaligned bogo operations" },
	{ NULL,	"misaligned-method M",	"use misaligned memory read/write method" },
	{ NULL,	NULL,			NULL }
};

static sigjmp_buf jmp_env;
static int handled_signum = -1;

typedef void (*stress_misaligned_func)(uint8_t *buffer);

typedef struct {
	const char *name;
	const stress_misaligned_func func;
	bool disabled;
	bool exercised;
} stress_misaligned_method_info_t;

static stress_misaligned_method_info_t *current_method;

static void stress_misaligned_int16rd(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1 = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2 = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3 = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4 = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5 = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6 = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7 = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8 = (uint16_t *)(buffer + 15);

	while (--i) {
		(void)*ptr1;
		shim_mb();
		(void)*ptr2;
		shim_mb();
		(void)*ptr3;
		shim_mb();
		(void)*ptr4;
		shim_mb();
		(void)*ptr5;
		shim_mb();
		(void)*ptr6;
		shim_mb();
		(void)*ptr7;
		shim_mb();
		(void)*ptr8;
		shim_mb();
	}
}

static void stress_misaligned_int16wr(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1 = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2 = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3 = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4 = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5 = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6 = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7 = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8 = (uint16_t *)(buffer + 15);

	while (--i) {
		*ptr1 = i;
		shim_mb();
		*ptr2 = i;
		shim_mb();
		*ptr3 = i;
		shim_mb();
		*ptr4 = i;
		shim_mb();
		*ptr5 = i;
		shim_mb();
		*ptr6 = i;
		shim_mb();
		*ptr7 = i;
		shim_mb();
		*ptr8 = i;
		shim_mb();
	}
}

static void stress_misaligned_int16inc(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1 = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2 = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3 = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4 = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5 = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6 = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7 = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8 = (uint16_t *)(buffer + 15);

	while (--i) {
		(*ptr1)++;
		shim_mb();
		(*ptr2)++;
		shim_mb();
		(*ptr3)++;
		shim_mb();
		(*ptr4)++;
		shim_mb();
		(*ptr5)++;
		shim_mb();
		(*ptr6)++;
		shim_mb();
		(*ptr7)++;
		shim_mb();
		(*ptr8)++;
		shim_mb();
	}
}

#if defined(ATOMIC_INC)
static void stress_misaligned_int16atomic(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1 = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2 = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3 = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4 = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5 = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6 = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7 = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8 = (uint16_t *)(buffer + 15);

	while (--i) {
		ATOMIC_INC(ptr1);
		shim_mb();
		ATOMIC_INC(ptr2);
		shim_mb();
		ATOMIC_INC(ptr3);
		shim_mb();
		ATOMIC_INC(ptr4);
		shim_mb();
		ATOMIC_INC(ptr5);
		shim_mb();
		ATOMIC_INC(ptr6);
		shim_mb();
		ATOMIC_INC(ptr7);
		shim_mb();
		ATOMIC_INC(ptr8);
		shim_mb();
	}
}
#endif

static void stress_misaligned_int32rd(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);

	while (--i) {
		(void)*ptr1;
		shim_mb();
		(void)*ptr2;
		shim_mb();
		(void)*ptr3;
		shim_mb();
		(void)*ptr4;
		shim_mb();
	}
}

static void stress_misaligned_int32wr(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);

	while (--i) {
		*ptr1 = i;
		shim_mb();
		*ptr2 = i;
		shim_mb();
		*ptr3 = i;
		shim_mb();
		*ptr4 = i;
		shim_mb();
	}
}

static void stress_misaligned_int32inc(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);

	while (--i) {
		(*ptr1)++;
		shim_mb();
		(*ptr2)++;
		shim_mb();
		(*ptr3)++;
		shim_mb();
		(*ptr4)++;
		shim_mb();
	}
}

#if defined(ATOMIC_INC)
static void stress_misaligned_int32atomic(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);

	while (--i) {
		ATOMIC_INC(ptr1);
		shim_mb();
		ATOMIC_INC(ptr2);
		shim_mb();
		ATOMIC_INC(ptr3);
		shim_mb();
		ATOMIC_INC(ptr4);
		shim_mb();
	}
}
#endif

static void stress_misaligned_int64rd(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);

	while (--i) {
		(void)*ptr1;
		shim_mb();
		(void)*ptr2;
		shim_mb();
	}
}

static void stress_misaligned_int64wr(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);

	while (--i) {
		*ptr1 = i;
		shim_mb();
		*ptr2 = i;
		shim_mb();
	}
}

static void stress_misaligned_int64inc(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);

	while (--i) {
		(*ptr1)++;
		shim_mb();
		(*ptr2)++;
		shim_mb();
	}
}

#if defined(ATOMIC_INC)
static void stress_misaligned_int64atomic(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);

	while (--i) {
		ATOMIC_INC(ptr1);
		shim_mb();
		ATOMIC_INC(ptr2);
		shim_mb();
	}
}
#endif

#if defined(HAVE_INT128_T)

/*
 *  Misaligned 128 bit fetches with SSE on x86 with some compilers
 *  with misaligned data may generate moveda rather than movdqu.
 *  For now, disable SSE optimization for x86 to workaround this
 *  even if it ends up generating two 64 bit reads.
 */
#if defined(__SSE__) && defined(STRESS_ARCH_X86)
#define TARGET_CLONE_NO_SSE __attribute__ ((target("no-sse")))
#else
#define TARGET_CLONE_NO_SSE
#endif
static void TARGET_CLONE_NO_SSE stress_misaligned_int128rd(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);

	while (--i) {
		(void)*ptr1;
		/* No need for shim_mb */
	}
}

static void stress_misaligned_int128wr(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);

	while (--i) {
		*ptr1 = i;
		/* No need for shim_mb */
	}
}

static void TARGET_CLONE_NO_SSE stress_misaligned_int128inc(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);

	while (--i) {
		(*ptr1)++;
		/* No need for shim_mb */
	}
}

#if defined(ATOMIC_INC)
static void TARGET_CLONE_NO_SSE stress_misaligned_int128atomic(uint8_t *buffer)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);

	while (--i) {
		ATOMIC_INC(ptr1);
		/* No need for shim_mb */
	}
}
#endif
#endif

static void stress_misaligned_all(uint8_t *buffer);

static stress_misaligned_method_info_t stress_misaligned_methods[] = {
	{ "all",	stress_misaligned_all,		false,	false },
	{ "int16rd",	stress_misaligned_int16rd,	false,	false },
	{ "int16wr",	stress_misaligned_int16wr,	false,	false },
	{ "int16inc",	stress_misaligned_int16inc,	false,	false },
#if defined(ATOMIC_INC)
	{ "int16atomic",stress_misaligned_int16atomic,	false,	false },
#endif
	{ "int32rd",	stress_misaligned_int32rd,	false,	false },
	{ "int32wr",	stress_misaligned_int32wr,	false,	false },
	{ "int32inc",	stress_misaligned_int32inc,	false,	false },
#if defined(ATOMIC_INC)
	{ "int32atomic",stress_misaligned_int32atomic,	false,	false },
#endif
	{ "int64rd",	stress_misaligned_int64rd,	false,	false },
	{ "int64wr",	stress_misaligned_int64wr,	false,	false },
	{ "int64inc",	stress_misaligned_int64inc,	false,	false },
#if defined(ATOMIC_INC)
	{ "int64atomic",stress_misaligned_int64atomic,	false,	false },
#endif
#if defined(HAVE_INT128_T)
	{ "int128rd",	stress_misaligned_int128rd,	false,	false },
	{ "int128wr",	stress_misaligned_int128wr,	false,	false },
	{ "int128inc",	stress_misaligned_int128inc,	false,	false },
#if defined(ATOMIC_INC)
	{ "int128atomic",stress_misaligned_int128atomic,false,	false },
#endif
#endif
	{ NULL,         NULL,				false,	false }
};

static void stress_misaligned_all(uint8_t *buffer)
{
	static bool exercised = false;
	stress_misaligned_method_info_t *info;

	for (info = &stress_misaligned_methods[1]; info->func; info++) {
		if (info->disabled)
			continue;
		current_method = info;
		info->func(buffer);
		info->exercised = true;
		exercised = true;
	}

	if (!exercised)
		stress_misaligned_methods[0].disabled = true;
}

static MLOCKED_TEXT void stress_misaligned_handler(int signum)
{
	handled_signum = signum;

	if (current_method)
		current_method->disabled = true;

	siglongjmp(jmp_env, 1);		/* Ugly, bounce back */
}

static void stress_misaligned_enable_all(void)
{
	stress_misaligned_method_info_t *info;

	for (info = stress_misaligned_methods; info->func; info++) {
		info->disabled = false;
		info->exercised = false;
	}
}

/*
 *  stress_misaligned_exercised()
 *	report the methods that were successfully exercised
 */
static void stress_misaligned_exercised(const stress_args_t *args)
{
	stress_misaligned_method_info_t *info;
	char *str = NULL;
	ssize_t str_len = 0;

	if (args->instance != 0)
		return;

	for (info = &stress_misaligned_methods[1]; info->func; info++) {
		if (info->exercised) {
			char *tmp;
			const size_t name_len = strlen(info->name);

			tmp = realloc(str, str_len + name_len + 2);
			if (!tmp) {
				free(str);
				return;
			}
			str = tmp;
			if (str_len) {
				(void)shim_strlcpy(str + str_len, " ", 2);
				str_len++;
			}
			(void)shim_strlcpy(str + str_len, info->name, name_len + 1);
			str_len += name_len;
		}
	}

	if (str)
		pr_inf("%s: exercised %s\n", args->name, str);
	else
		pr_inf("%s: nothing exercised due to misalignment faults\n", args->name);

	free(str);
}

/*
 *  stress_set_misaligned_method()
 *      set default misaligned stress method
 */
static int stress_set_misaligned_method(const char *name)
{
	stress_misaligned_method_info_t const *info;

	for (info = stress_misaligned_methods; info->func; info++) {
		if (!strcmp(info->name, name)) {
			stress_set_setting("misaligned-method", TYPE_ID_UINTPTR_T, &info);
			return 0;
		}
	}

	(void)fprintf(stderr, "misaligned-method must be one of:");
	for (info = stress_misaligned_methods; info->func; info++) {
		(void)fprintf(stderr, " %s", info->name);
	}
	(void)fprintf(stderr, "\n");

	return -1;
}

static void stress_misaligned_set_default(void)
{
	stress_set_misaligned_method("all");
}

/*
 *  stress_misaligned()
 *	stress memory copies
 */
static int stress_misaligned(const stress_args_t *args)
{
	uint8_t *buffer;
	stress_misaligned_method_info_t *misaligned_method = &stress_misaligned_methods[0];
	int ret, rc;

	(void)stress_get_setting("misaligned-method", &misaligned_method);

	if (stress_sighandler(args->name, SIGBUS, stress_misaligned_handler, NULL) < 0)
		return EXIT_NO_RESOURCE;
	if (stress_sighandler(args->name, SIGILL, stress_misaligned_handler, NULL) < 0)
		return EXIT_NO_RESOURCE;
	if (stress_sighandler(args->name, SIGSEGV, stress_misaligned_handler, NULL) < 0)
		return EXIT_NO_RESOURCE;

	stress_misaligned_enable_all();

	buffer = (uint8_t *)mmap(NULL, args->page_size, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (buffer == MAP_FAILED) {
		pr_inf("%s: cannot allocate 1 page buffer, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	current_method = misaligned_method;
	ret = sigsetjmp(jmp_env, 1);
	if ((ret == 1) && (args->instance == 0)) {
		pr_inf("%s: skipping method %s, misaligned operations tripped %s\n",
			args->name, current_method->name,
			handled_signum == -1 ? "an error" :
			stress_strsignal(handled_signum));
	}

	rc = EXIT_SUCCESS;
	do {
		if (misaligned_method->disabled) {
			rc = EXIT_NO_RESOURCE;
			break;
		}
		misaligned_method->func(buffer);
		misaligned_method->exercised = true;
		inc_counter(args);
	} while (keep_stressing(args));

	(void)stress_sighandler_default(SIGBUS);
	(void)stress_sighandler_default(SIGILL);
	(void)stress_sighandler_default(SIGSEGV);

	stress_misaligned_exercised(args);

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	(void)munmap((void *)buffer, args->page_size);

	return rc;
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_misaligned_method,	stress_set_misaligned_method },
	{ 0,			NULL }
};

stressor_info_t stress_misaligned_info = {
	.stressor = stress_misaligned,
	.set_default = stress_misaligned_set_default,
	.class = CLASS_CPU_CACHE | CLASS_MEMORY,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
