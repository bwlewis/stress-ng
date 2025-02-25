/*
 * Copyright (C) 2013-2021 Canonical, Ltd.
 * Copyright (C)      2022 Colin Ian King.
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
#include "core-capabilities.h"

#if defined(HAVE_LINUX_SOCKIOS_H)
#include <linux/sockios.h>
#endif

#if defined(HAVE_NETINET_IP_H)
#include <netinet/ip.h>
#endif

static const stress_help_t help[] = {
	{ NULL,	"rawsock N",		"start N workers performing raw socket send/receives " },
	{ NULL,	"rawsock-ops N",	"stop after N raw socket bogo operations" },
	{ NULL,	NULL,			NULL }
};

#if defined(SOCK_RAW) &&	\
    defined(IPPROTO_RAW) &&	\
    defined(HAVE_ICMPHDR) &&	\
    defined(__linux__)

typedef struct {
	struct iphdr	iph;
	uint32_t	data;
} stress_raw_packet_t;

static void *rawsock_lock;

static void stress_rawsock_init(void)
{
	rawsock_lock = stress_lock_create();
}

static void stress_rawsock_deinit(void)
{
	stress_lock_destroy(rawsock_lock);
}

/*
 *  stress_rawsock_supported()
 *      check if we can run this
 */
static int stress_rawsock_supported(const char *name)
{
	if (!stress_check_capability(SHIM_CAP_NET_RAW)) {
		pr_inf_skip("%s stressor will be skipped, "
			"need to be running with CAP_NET_RAW "
			"rights for this stressor\n", name);
		return -1;
	}
	return 0;
}

/*
 *  stress_rawsock
 *	stress by heavy raw udp ops
 */
static int stress_rawsock(const stress_args_t *args)
{
	pid_t pid;
	int rc = EXIT_SUCCESS;

	if (!rawsock_lock) {
		pr_inf("%s: failed to create rawsock lock, skipping stressor\n", args->name);
		return EXIT_NO_RESOURCE;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);
again:
	pid = fork();
	if (pid < 0) {
		if (stress_redo_fork(errno))
			goto again;
		if (!keep_stressing(args)) {
			rc = EXIT_SUCCESS;
			goto finish;
		}
		pr_fail("%s: fork failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_FAILURE;
	} else if (pid == 0) {
		/* Child, client */
		int fd;
		stress_raw_packet_t ALIGN64 pkt;
		struct sockaddr_in addr;

		stress_parent_died_alarm();
		(void)sched_settings_apply(true);

		if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
			pr_fail("%s: socket failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			/* failed, kick parent to finish */
			(void)kill(getppid(), SIGALRM);
			_exit(EXIT_FAILURE);
		}

		(void)memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = 45000;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		(void)memset(&pkt, 0, sizeof(pkt));
		pkt.iph.version = 4;
		pkt.iph.ihl = sizeof(struct iphdr) >> 2;
		pkt.iph.tos = 0;
		pkt.iph.tot_len = htons(40);
		pkt.iph.id = 0;
		pkt.iph.ttl = 64;
		pkt.iph.protocol = IPPROTO_RAW;
		pkt.iph.frag_off = 0;
		pkt.iph.check = 0;
		pkt.iph.saddr = addr.sin_addr.s_addr;
		pkt.iph.daddr = addr.sin_addr.s_addr;

		/* Wait for server to start */
		while (keep_stressing(args)) {
			uint32_t ready;

			(void)stress_lock_acquire(rawsock_lock);
			ready = g_shared->rawsock.ready;
			(void)stress_lock_release(rawsock_lock);

			if (ready == args->num_instances)
				break;
			shim_usleep(20000);
		}

		while (keep_stressing(args)) {
			ssize_t sret;

			sret = sendto(fd, &pkt, sizeof(pkt), 0,
				(const struct sockaddr *)&addr,
				(socklen_t)sizeof(addr));
			if (sret < 0)
				break;
			pkt.data++;
#if defined(SIOCOUTQ)
			/* Occasionally exercise SIOCINQ */
			if ((pkt.data & 0xff) == 0) {
				int queued;

				if (!keep_stressing(args))
					break;

				VOID_RET(int, ioctl(fd, SIOCOUTQ, &queued));
			}
#endif
		}
		(void)close(fd);

		(void)kill(getppid(), SIGALRM);
		_exit(EXIT_SUCCESS);
	} else {
		/* Parent, server */
		int fd = -1, status;
		struct sockaddr_in addr;

		if (stress_sig_stop_stressing(args->name, SIGALRM) < 0) {
			rc = EXIT_FAILURE;
			goto die;
		}
		if (!keep_stressing(args))
			goto die;

		if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
			pr_fail("%s: socket failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			rc = EXIT_FAILURE;
			goto die;
		}

		(void)memset(&addr, 0, sizeof(addr));

		(void)stress_lock_acquire(rawsock_lock);
		g_shared->rawsock.ready++;
		(void)stress_lock_release(rawsock_lock);

		while (keep_stressing(args)) {
			stress_raw_packet_t ALIGN64 pkt;
			socklen_t len = sizeof(addr);
			ssize_t n;

			n = recvfrom(fd, &pkt, sizeof(pkt), 0,
					(struct sockaddr *)&addr, &len);
			if (UNLIKELY(n == 0)) {
				break;
			} else if (UNLIKELY(n < 0)) {
				if (errno != EINTR)
					pr_fail("%s: recvfrom failed, errno=%d (%s)\n",
						args->name, errno, strerror(errno));
				break;
			}

#if defined(SIOCINQ)
			/* Occasionally exercise SIOCINQ */
			if ((pkt.data & 0xff) == 0) {
				int queued;

				if (!keep_stressing(args))
					break;

				VOID_RET(int, ioctl(fd, SIOCINQ, &queued));
			}
#endif
			inc_counter(args);
		}

die:
		if (pid) {
			(void)kill(pid, SIGKILL);
			(void)shim_waitpid(pid, &status, 0);
		}
		/* close recv socket after sender closed */
		if (fd > -1)
			(void)close(fd);
	}
finish:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	return rc;
}

stressor_info_t stress_rawsock_info = {
	.stressor = stress_rawsock,
	.class = CLASS_NETWORK | CLASS_OS,
	.supported = stress_rawsock_supported,
	.help = help,
	.init = stress_rawsock_init,
	.deinit = stress_rawsock_deinit,
};
#else
stressor_info_t stress_rawsock_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_NETWORK | CLASS_OS,
	.help = help
};
#endif
