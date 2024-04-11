#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../kernel-module/xt_RTPENGINE.h"

int main() {
	int fd = open("/proc/rtpengine/control", O_WRONLY);
	assert(fd >= 0);
	ssize_t ret = write(fd, "add 0\n", 6);
	assert(ret == 6 || (ret == -1 && errno == EEXIST));
	close(fd);

	fd = open("/proc/rtpengine/0/control", O_RDWR);
	assert(fd >= 0);

	struct rtpengine_command_noop noop = { .cmd = REMG_NOOP };

	noop.cmd = REMG_NOOP;

	noop.noop = (struct rtpengine_noop_info) {
		.last_cmd = __REMG_LAST,
		.msg_size = {
			[REMG_NOOP] = sizeof(struct rtpengine_command_noop),
			[REMG_ADD_TARGET] = sizeof(struct rtpengine_command_add_target),
			[REMG_DEL_TARGET_STATS] = sizeof(struct rtpengine_command_del_target_stats),
			[REMG_ADD_DESTINATION] = sizeof(struct rtpengine_command_destination),
			[REMG_ADD_CALL] = sizeof(struct rtpengine_command_add_call),
			[REMG_DEL_CALL] = sizeof(struct rtpengine_command_del_call),
			[REMG_ADD_STREAM] = sizeof(struct rtpengine_command_add_stream),
			[REMG_DEL_STREAM] = sizeof(struct rtpengine_command_del_stream),
			[REMG_PACKET] = sizeof(struct rtpengine_command_packet),
			[REMG_GET_RESET_STATS] = sizeof(struct rtpengine_command_stats),
			[REMG_SEND_RTCP] = sizeof(struct rtpengine_command_send_packet),
			[REMG_INIT_PLAY_STREAMS] = sizeof(struct rtpengine_command_init_play_streams),
			[REMG_GET_PACKET_STREAM] = sizeof(struct rtpengine_command_get_packet_stream),
			[REMG_PLAY_STREAM_PACKET] = sizeof(struct rtpengine_command_play_stream_packet),
			[REMG_PLAY_STREAM] = sizeof(struct rtpengine_command_play_stream),
			[REMG_STOP_STREAM] = sizeof(struct rtpengine_command_stop_stream),
			[REMG_FREE_PACKET_STREAM] = sizeof(struct rtpengine_command_free_packet_stream),
			[REMG_PLAY_STREAM_STATS] = sizeof(struct rtpengine_command_play_stream_stats),
		},
	};

	ret = write(fd, &noop, sizeof(noop));
	assert(ret == sizeof(noop));

	struct rtpengine_command_init_play_streams ips = {
		.cmd = REMG_INIT_PLAY_STREAMS,
		.num_packet_streams = 100,
		.num_play_streams = 40960,
	};
	ret = write(fd, &ips, sizeof(ips));
	assert(ret == sizeof(ips));

	struct rtpengine_command_get_packet_stream gps = { .cmd = REMG_GET_PACKET_STREAM };
	ret = read(fd, &gps, sizeof(gps));
	assert(ret == sizeof(gps));
	printf("packet stream idx %u\n", gps.packet_stream_idx);

	struct {
		struct rtpengine_command_play_stream_packet psp;
		char buf[160];
	} psp = {
		.psp = {
			.cmd = REMG_PLAY_STREAM_PACKET,
			.play_stream_packet = {
				.packet_stream_idx = gps.packet_stream_idx,
			},
		},
	};

	for (unsigned int i = 0; i < 256; i++) {
		psp.psp.play_stream_packet.delay_ms = i * 20;
		psp.psp.play_stream_packet.delay_ts = i * 160;
		memset(psp.psp.play_stream_packet.data, i, sizeof(psp.buf));
		ret = write(fd, &psp, sizeof(psp));
		assert(ret == sizeof(psp));
	}
	printf("packets ok\n");

	unsigned play_idx[4096];

	for (int i = 0; i < sizeof(play_idx)/sizeof(*play_idx); i++) {
		struct rtpengine_command_play_stream ps = {
			.cmd = REMG_PLAY_STREAM,
			.info = {
				.src_addr = {
					.family = AF_INET,
					.u = {
						.ipv4 = inet_addr("192.168.1.102"),
					},
					.port = 6666 + i,
				},
				.dst_addr = {
					.family = AF_INET,
					.u = {
						.ipv4 = inet_addr("192.168.1.66"),
					},
					.port = 9999,
				},
				.pt = 8,
				.ssrc = 0x12345678 + i,
				.ts = 76543210 + i,
				.seq = 5432 + i,
				.encrypt = {
					.cipher = REC_NULL,
					.hmac = REH_NULL,
				},
				.packet_stream_idx = gps.packet_stream_idx,
				.repeat = 50,
			},
		};
		ret = read(fd, &ps, sizeof(ps));
		assert(ret == sizeof(ps));
		printf("play stream idx %u\n", ps.play_idx);
		play_idx[i] = ps.play_idx;

		usleep(50000);
	}

	printf("sleep\n");
	sleep(10);

	printf("poll stats\n");
	for (int rep = 0; rep < 2000; rep++) {
		for (int i = 0; i < sizeof(play_idx)/sizeof(*play_idx); i++) {
			struct rtpengine_command_play_stream_stats pss = {
				.cmd = REMG_PLAY_STREAM_STATS,
				.play_idx = play_idx[i],
			};
			ret = read(fd, &pss, sizeof(pss));
			if (ret != sizeof(pss))
				printf("%i %i %zi %s\n", i, rep, ret, strerror(errno));
			//assert(ret == sizeof(pss));
			usleep(10000);
		}
	}

	printf("close fd, sleep\n");
	sleep(10);
	close(fd);

	printf("del table\n");
	fd = open("/proc/rtpengine/control", O_WRONLY);
	assert(fd >= 0);
	ret = write(fd, "del 0\n", 6);
	assert(ret == 6);
	close(fd);

	return 0;
}
