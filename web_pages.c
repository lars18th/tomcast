/*
 * mptsd internal web pages
 * Copyright (C) 2010-2011 Unix Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>

#include "libfuncs/io.h"
#include "libfuncs/log.h"
#include "libfuncs/list.h"
#include "libfuncs/http_response.h"

#include "config.h"

extern struct config *config;
extern int set_hibernate(int k, const char *channel);

void cmd_index(int clientsock) {
	send_200_ok(clientsock);
	send_header_textplain(clientsock);
	fdputs(clientsock, "\nHi from tomcast.\n");
}

void cmd_status(int clientsock) {
	send_200_ok(clientsock);
	send_header_textplain(clientsock);
	fdputs(clientsock, "\n");

	LNODE *l, *tmp;
	struct config *cfg = get_config();

	time_t now = time(NULL);
	fdputsf(clientsock, "%-10s %-20s %8s %10s %-18s %-64s %s\n",
		"# Status",
		"DestAddr",
		"ConnTime",
		"ReadBytes",
		"ChanName",
		"ChanSource",
		"ProxyStatus"
	);
	pthread_mutex_lock(&cfg->channels_lock);
	list_lock(cfg->restreamer);
	list_for_each(cfg->restreamer, l, tmp) {
		char dest[32];
		int conn_status = 0;
		RESTREAMER *r = l->data;
		pthread_rwlock_rdlock(&r->lock);
		snprintf(dest, sizeof(dest), "%s:%d", r->channel->dest_host, r->channel->dest_port);
		if (r->connected && r->conn_ts > 0 && r->read_bytes >= 1316 && !r->hibernate)
			conn_status = 1;
		fdputsf(clientsock, "%-10s %-20s %8lu %10llu %-18s %-64s %s\n",
			conn_status ? "CONN_OK" : r->hibernate ? "HIBERNATE" : "CONN_ERROR",
			dest,
			r->conn_ts ? now - r->conn_ts : 0,
			r->read_bytes,
			r->channel->name,
			r->channel->source,
			r->status
		);
		pthread_rwlock_unlock(&r->lock);
	}
	list_unlock(cfg->restreamer);
	pthread_mutex_unlock(&cfg->channels_lock);
}

void cmd_getconfig(int clientsock) {
	send_200_ok(clientsock);
	send_header_textplain(clientsock);
	fdputs(clientsock, "\n");

	LNODE *l, *tmp;
	struct config *cfg = get_config();

	pthread_mutex_lock(&cfg->channels_lock);
	list_lock(cfg->restreamer);
	list_for_each(cfg->restreamer, l, tmp) {
		RESTREAMER *r = l->data;
		pthread_rwlock_rdlock(&r->lock);
		int i;
		for (i = 0; i < r->channel->num_src; i++) {
			fdputsf(clientsock, "%s\t%s:%d\t%s\n",
				r->channel->name,
				r->channel->dest_host,
				r->channel->dest_port,
				r->channel->sources[i]
			);
		}
		pthread_rwlock_unlock(&r->lock);
	}
	list_unlock(cfg->restreamer);
	pthread_mutex_unlock(&cfg->channels_lock);
}

void cmd_reconnect(int clientsock) {
	send_200_ok(clientsock);
	send_header_textplain(clientsock);
	struct config *cfg = get_config();
	pthread_mutex_lock(&cfg->channels_lock);
	fdputsf(clientsock, "\nReconnecting %d inputs.\n", cfg->chanconf->items);
	pthread_mutex_unlock(&cfg->channels_lock);
	do_reconnect(1);
}

void cmd_reload(int clientsock) {
	send_200_ok(clientsock);
	send_header_textplain(clientsock);
	fdputs(clientsock, "\nReloading config\n");
	do_reconf(1);
}

void cmd_start(int clientsock, const char *uri) {
	if(uri[5] == '/') {
		struct config *cfg = get_config();
		pthread_mutex_lock(&cfg->channels_lock);
		int ret = set_hibernate(0,uri+6);
		pthread_mutex_unlock(&cfg->channels_lock);
		if (ret > 0) {
			send_200_ok(clientsock);
			send_header_textplain(clientsock);
			fdputs(clientsock, "\nok\n");
			return;
		}
		if (ret == 0) {
			send_403_forbidden(clientsock);
			return;
		}
	}
	send_404_not_found(clientsock);
}

void cmd_stop(int clientsock, const char *uri) {
	if(uri[4] == '/') {
		struct config *cfg = get_config();
		pthread_mutex_lock(&cfg->channels_lock);
		int ret = set_hibernate(1,uri+5);
		pthread_mutex_unlock(&cfg->channels_lock);
		if (ret > 0) {
			send_200_ok(clientsock);
			send_header_textplain(clientsock);
			fdputs(clientsock, "\nok\n");
			return;
		}
		if (ret == 0) {
			send_403_forbidden(clientsock);
			return;
		}
	}
	send_404_not_found(clientsock);
}
