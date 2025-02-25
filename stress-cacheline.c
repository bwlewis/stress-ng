/*
 * Copyright (C) 2022      Colin Ian King.
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
 */
#include "stress-ng.h"
#include "core-cache.h"

#define DEFAULT_L1_SIZE		(64)

#if defined(HAVE_ATOMIC_FETCH_ADD) &&	\
    defined(__ATOMIC_RELAXED)
#define SHIM_ATOMIC_INC(ptr)       \
	do { __atomic_fetch_add(ptr, 1, __ATOMIC_RELAXED); } while (0)
#endif

/*
 *  8 bit rotate right
 */
#define ROR8(val)				\
do {						\
	uint8_t tmpval = (val);			\
	const uint8_t bit0 = (tmpval & 1) << 7;	\
	tmpval >>= 1;				\
	tmpval |= bit0;				\
	(val) = tmpval;				\
} while (0)

/*
 *  8 bit rotate left
 */
#define ROL8(val)				\
do {						\
	uint8_t tmpval = (val);			\
	const uint8_t bit7 = (tmpval & 0x80) >> 7;\
	tmpval <<= 1;				\
	tmpval |= bit7;				\
	(val) = tmpval;				\
} while (0)

#define EXERCISE(data)	\
do {			\
	(data)++;	\
	shim_mb();	\
	ROL8(data);	\
	shim_mb();	\
	ROR8(data);	\
	shim_mb();	\
} while (0)

static const stress_help_t help[] = {
	{ NULL,	"cacheline N",		"start N workers that exercise cachelines" },
	{ NULL,	"cacheline-ops N",	"stop after N cacheline bogo operations" },
	{ NULL,	"cacheline-affinity",	"modify CPU affinity" },
	{ NULL,	"cacheline-method M",	"use cacheline stressing method M" },
	{ NULL,	NULL,			NULL }
};

typedef int (*stress_cacheline_func)(
        const stress_args_t *args,
        const int index,
        const bool parent,
        const size_t l1_cacheline_size);

typedef struct {
	const char *name;
	const stress_cacheline_func	func;
} stress_cacheline_method_t;

static uint64_t get_L1_line_size(const stress_args_t *args)
{
	uint64_t cache_size = DEFAULT_L1_SIZE;
#if defined(__linux__)
	stress_cpus_t *cpu_caches;
	stress_cpu_cache_t *cache = NULL;

	cpu_caches = stress_get_all_cpu_cache_details();
	if (!cpu_caches) {
		if (!args->instance)
			pr_inf("%s: using built-in defaults as unable to "
				"determine cache line details\n", args->name);
		return cache_size;
	}

	cache = stress_get_cpu_cache(cpu_caches, 1);
	if (!cache) {
		if (!args->instance)
			pr_inf("%s: using built-in defaults as no suitable "
				"cache found\n", args->name);
		stress_free_cpu_caches(cpu_caches);
		return cache_size;
	}
	if (!cache->line_size) {
		if (!args->instance)
			pr_inf("%s: using built-in defaults as unable to "
				"determine cache line size\n", args->name);
		stress_free_cpu_caches(cpu_caches);
		return cache_size;
	}
	cache_size = cache->line_size;

	stress_free_cpu_caches(cpu_caches);
#else
	if (!args->instance)
		pr_inf("%s: using built-in defaults as unable to "
			"determine cache line details\n", args->name);
#endif
	return cache_size;
}

static int stress_cacheline_adjacent(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	register uint8_t val8 = *(data8);
	volatile uint8_t *data8adjacent = (volatile uint8_t *)(((uintptr_t)data8) ^ 1);

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		(*data8)++;
		(void)(*data8adjacent);
		shim_mb();
		(*data8)++;
		(void)(*data8adjacent);
		shim_mb();
		(*data8)++;
		(void)(*data8adjacent);
		shim_mb();
		(*data8)++;
		(void)(*data8adjacent);
		shim_mb();
		(*data8)++;
		(void)(*data8adjacent);
		shim_mb();
		(*data8)++;
		(void)(*data8adjacent);
		shim_mb();
		(*data8)++;
		(void)(*data8adjacent);
		shim_mb();
		val8 += 7;

		if (UNLIKELY(*data8 != val8)) {
			pr_fail("%s: adjacent method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_copy(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	const volatile uint8_t *data8adjacent = (volatile uint8_t *)(((uintptr_t)data8) ^ 1);

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		register uint8_t val8;

		(*data8) = (*data8adjacent);
		(*data8) = (*data8adjacent);
		(*data8) = (*data8adjacent);
		(*data8) = (*data8adjacent);
		(*data8) = (*data8adjacent);
		(*data8) = (*data8adjacent);
		(*data8) = (*data8adjacent);
		(*data8) = (*data8adjacent);
		val8 = *data8;

		if (UNLIKELY(*data8 != val8)) {
			pr_fail("%s: copy method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_inc(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	register uint8_t val8 = *(data8);

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		(*data8)++;
		shim_mb();
		(*data8)++;
		shim_mb();
		(*data8)++;
		shim_mb();
		(*data8)++;
		shim_mb();
		(*data8)++;
		shim_mb();
		(*data8)++;
		shim_mb();
		(*data8)++;
		shim_mb();
		val8 += 7;

		if (UNLIKELY(*data8 != val8)) {
			pr_fail("%s: inc method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_rdwr(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	register uint8_t val8 = *(data8);

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();
		(void)*data8;
		*data8 = *data8;
		shim_mb();

		if (UNLIKELY(*data8 != val8)) {
			pr_fail("%s: rdwr method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_mix(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	static uint8_t tmp = 0xa5;

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		register uint8_t val8;

		*(data8) = tmp;
		EXERCISE((*data8));
		val8 = tmp;
		EXERCISE(val8);
		if (UNLIKELY(val8 != *data8)) {
			pr_fail("%s: mix method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
		tmp = val8;
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_rdrev64(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	const size_t cacheline_size = g_shared->cacheline_size;
	volatile uint8_t *aligned_cacheline = (volatile uint8_t *)
		((intptr_t)cacheline & ~(l1_cacheline_size - 1));

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		register ssize_t j;
		uint8_t val8;

		(*data8)++;
		val8 = *data8;

		/* read cache line backwards */
		for (j = (ssize_t)cacheline_size - 8; j >= 0; j -= 8) {
			volatile uint64_t *data64 = (volatile uint64_t *)(aligned_cacheline + j);

			(void)*data64;
			shim_mb();
		}
		if (UNLIKELY(val8 != *data8)) {
			pr_fail("%s: rdrev64 method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_rdfwd64(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	const size_t cacheline_size = g_shared->cacheline_size;
	volatile uint8_t *aligned_cacheline = (volatile uint8_t *)
		((intptr_t)cacheline & ~(l1_cacheline_size - 1));

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		register size_t j;
		uint8_t val8;

		(*data8)++;
		val8 = *data8;

		/* read cache line forwards */
		for (j = 0; j < cacheline_size; j += 8) {
			volatile uint64_t *data64 = (volatile uint64_t *)(aligned_cacheline + j);

			(void)*data64;
			shim_mb();
		}
		if (UNLIKELY(val8 != *data8)) {
			pr_fail("%s: rdfwd64: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_rdints(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	volatile uint16_t *data16 = (uint16_t *)(((uintptr_t)data8) & ~(uintptr_t)1);
	volatile uint32_t *data32 = (uint32_t *)(((uintptr_t)data8) & ~(uintptr_t)3);
	volatile uint64_t *data64 = (uint64_t *)(((uintptr_t)data8) & ~(uintptr_t)7);
#if defined(HAVE_INT128_T)
        volatile __uint128_t *data128 = (__uint128_t *)(((uintptr_t)data8) & ~(uintptr_t)15);
#endif

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		uint8_t val8;

		/* 1 byte increment and read */
		(*data8)++;
		val8 = *data8;
		shim_mb();

		/* 2 byte reads from same location */
		(void)*(data16);
		shim_mb();

		/* 4 byte reads from same location */
		(void)*(data32);
		shim_mb();

		/* 8 byte reads from same location */
		(void)*(data64);
		shim_mb();

#if defined(HAVE_INT128_T)
		/* 116 byte reads from same location */
		(void)*(data128);
		shim_mb();
#endif
		if (UNLIKELY(val8 != *data8)) {
			pr_fail("%s: rdints method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

static int stress_cacheline_bits(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		register uint8_t val8;

		(void)*(data8);

		val8 = 1U << (i & 7);
		*data8 = val8;
		shim_mb();
		if (*data8 != val8) {
			pr_fail("%s: bits method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
		val8 ^= 0xff;
		*data8 = val8;
		shim_mb();
		if (*data8 != val8) {
			pr_fail("%s: bits method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

#if defined(SHIM_ATOMIC_INC)
static int stress_cacheline_atomicinc(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	register int i;
	volatile uint8_t *cacheline = (volatile uint8_t *)g_shared->cacheline;
	volatile uint8_t *data8 = cacheline + index;
	register uint8_t val8 = *(data8);

	(void)parent;
	(void)l1_cacheline_size;

	for (i = 0; i < 1024; i++) {
		SHIM_ATOMIC_INC(data8);
		SHIM_ATOMIC_INC(data8);
		SHIM_ATOMIC_INC(data8);
		SHIM_ATOMIC_INC(data8);
		SHIM_ATOMIC_INC(data8);
		SHIM_ATOMIC_INC(data8);
		SHIM_ATOMIC_INC(data8);
		val8 += 7;

		if (UNLIKELY(*data8 != val8)) {
			pr_fail("%s: atomicinc method: cache line error in offset 0x%x, expected %2" PRIx8 ", got %2" PRIx8 "\n",
				args->name, index, val8, *data8);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
#endif

static int stress_cacheline_all(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size);

static const stress_cacheline_method_t cacheline_methods[] = {
	{ "all",	stress_cacheline_all },
	{ "adjacent",	stress_cacheline_adjacent },
#if defined (SHIM_ATOMIC_INC)
	{ "atomicinc",	stress_cacheline_atomicinc },
#endif
	{ "bits",	stress_cacheline_bits },
	{ "copy",	stress_cacheline_copy },
	{ "inc",	stress_cacheline_inc },
	{ "mix",	stress_cacheline_mix },
	{ "rdfwd64",	stress_cacheline_rdfwd64 },
	{ "rdints",	stress_cacheline_rdints },
	{ "rdrev64",	stress_cacheline_rdrev64 },
	{ "rdwr",	stress_cacheline_rdwr },
};

static int stress_cacheline_all(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size)
{
	size_t i;
	const size_t n = SIZEOF_ARRAY(cacheline_methods);

	for (i = 1; keep_stressing(args) && (i < n); i++) {
		int rc;

		rc = cacheline_methods[i].func(args, index, parent, l1_cacheline_size);
		if (rc != EXIT_SUCCESS)
			return rc;
	}
	return EXIT_SUCCESS;
}

static int stress_set_cacheline_affinity(const char *opt)
{
	return stress_set_setting_true("cacheline-affinity", opt);
}

/*
 *  stress_set_cacheline_method()
 *	set the default cachline stress method
 */
static int stress_set_cacheline_method(const char *name)
{
	size_t i;

	for (i = 0; i < SIZEOF_ARRAY(cacheline_methods); i++) {
		if (!strcmp(cacheline_methods[i].name, name)) {
			stress_set_setting("cacheline-method", TYPE_ID_SIZE_T, &i);
			return 0;
		}
	}

	(void)fprintf(stderr, "cacheline-method must be one of:");
	for (i = 0; i < SIZEOF_ARRAY(cacheline_methods); i++) {
		(void)fprintf(stderr, " %s", cacheline_methods[i].name);
	}
	(void)fprintf(stderr, "\n");

	return -1;
}

#if defined(HAVE_AFFINITY) && \
    defined(HAVE_SCHED_GETAFFINITY)
/*
 *  stress_cacheline_change_affinity()
 *	pin process to CPU based on clock time * 100, instance number
 *	and parent/child offset modulo number of CPUs
 */
static inline void stress_cacheline_change_affinity(
	const stress_args_t *args,
	const uint32_t cpus,
	bool parent)
{
	cpu_set_t mask;
	double now = stress_time_now() * 100;
	uint32_t cpu = ((uint32_t)args->instance + (uint32_t)parent + (uint32_t)now) % cpus;

	CPU_ZERO(&mask);
	CPU_SET((int)cpu, &mask);
	VOID_RET(int, sched_setaffinity(0, sizeof(mask), &mask));
}
#endif

static int stress_cacheline_child(
	const stress_args_t *args,
	const int index,
	const bool parent,
	const size_t l1_cacheline_size,
	stress_cacheline_func func,
	const bool cacheline_affinity)
{
	int rc;
#if defined(HAVE_AFFINITY) && \
    defined(HAVE_SCHED_GETAFFINITY)
	const uint32_t cpus = (int)stress_get_processors_configured();
#endif

	(void)cacheline_affinity;

	do {
		rc = func(args, index, parent, l1_cacheline_size);
		if (parent)
			inc_counter(args);

#if defined(HAVE_AFFINITY) && \
    defined(HAVE_SCHED_GETAFFINITY)
		if (cacheline_affinity)
			stress_cacheline_change_affinity(args, cpus, parent);
#endif
	} while ((rc == EXIT_SUCCESS) && keep_stressing(args));

	/* Child tell parent it has finished */
	if (!parent)
		(void)kill(getppid(), SIGALRM);

	return rc;
}

/*
 *  stress_cacheline()
 *	execise a cacheline by multiple processes
 */
static int stress_cacheline(const stress_args_t *args)
{
	size_t l1_cacheline_size = (size_t)get_L1_line_size(args);
	const int index = (int)(args->instance * 2);
	pid_t pid;
	int rc = EXIT_SUCCESS;
	size_t cacheline_method = 0;
	stress_cacheline_func func;
	bool cacheline_affinity = false;

	(void)stress_get_setting("cacheline-affinity", &cacheline_affinity);
	(void)stress_get_setting("cacheline-method", &cacheline_method);

	if (args->instance == 0) {
		pr_dbg("%s: L1 cache line size %zd bytes\n", args->name, l1_cacheline_size);

		if ((args->num_instances * 2) < l1_cacheline_size) {
			pr_inf("%s: to fully exercise a %zd byte cache line, %zd instances are required\n",
				args->name, l1_cacheline_size, l1_cacheline_size / 2);
		}
	}

	pr_dbg("%s: using method '%s'\n", args->name, cacheline_methods[cacheline_method].name);
	func = cacheline_methods[cacheline_method].func;

	stress_set_proc_state(args->name, STRESS_STATE_RUN);
again:
	pid = fork();
	if (pid < 0) {
		if (stress_redo_fork(errno))
			goto again;
		if (!keep_stressing(args))
			goto finish;
		pr_err("%s: fork failed: errno=%d: (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	} else if (pid == 0) {
		rc = stress_cacheline_child(args, index + 1, false, l1_cacheline_size, func, cacheline_affinity);
		_exit(rc);
	} else {
		int status;

		stress_cacheline_child(args, index, true, l1_cacheline_size, func, cacheline_affinity);

		(void)kill(pid, SIGALRM);
		(void)shim_waitpid(pid, &status, 0);

		if (WIFEXITED(status) && (WEXITSTATUS(status) != EXIT_SUCCESS))
			rc = WEXITSTATUS(status);
	}

finish:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	return rc;
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_cacheline_affinity,	stress_set_cacheline_affinity },
	{ OPT_cacheline_method,		stress_set_cacheline_method },
	{ 0,				NULL },
};

stressor_info_t stress_cacheline_info = {
	.stressor = stress_cacheline,
	.class = CLASS_CPU_CACHE,
	.verify = VERIFY_ALWAYS,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
