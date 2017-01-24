/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <errno.h>
#include <poll.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"

struct sync_cb_data {
	int is_finished;
	int status;
	void *ptr;
};

static int wait_for_reply(struct smb2_context *smb2,
                          struct sync_cb_data *cb_data)
{
        while (!cb_data->is_finished) {
                struct pollfd pfd;

		pfd.fd = smb2_get_fd(smb2);
		pfd.events = smb2_which_events(smb2);

		if (poll(&pfd, 1, 1000) < 0) {
			smb2_set_error(smb2, "Poll failed");
			return -1;
		}
                if (pfd.revents == 0) {
                        continue;
                }
		if (smb2_service(smb2, pfd.revents) < 0) {
			smb2_set_error(smb2, "smb2_service failed with : "
                                       "%s\n", smb2_get_error(smb2));
                        return -1;
		}
	}

        return 0;
}

/*
 * Connect to the server and mount the share.
 */
static void connect_cb(struct smb2_context *smb2, int status,
                       void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->is_finished = 1;
        cb_data->status = status;
}

int smb2_connect_share(struct smb2_context *smb2,
                       const char *server, const char *share)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_connect_share_async(smb2, server, share,
                                     connect_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_connect_share_async failed");
		return -ENOMEM;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -EIO;
        }

	return cb_data.status;
}

/*
 * opendir()
 */
static void opendir_cb(struct smb2_context *smb2, int status,
                       void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->is_finished = 1;
        cb_data->ptr = command_data;
}

struct smb2dir *smb2_opendir(struct smb2_context *smb2, const char *path)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_opendir_async(smb2, path,
                               opendir_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_opendir_async failed");
		return NULL;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return NULL;
        }

	return cb_data.ptr;
}

/*
 * open()
 */
static void open_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->is_finished = 1;
        cb_data->ptr = command_data;
}

struct smb2fh *smb2_open(struct smb2_context *smb2, const char *path, int flags)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_open_async(smb2, path, flags,
                               open_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_open_async failed");
		return NULL;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return NULL;
        }

	return cb_data.ptr;
}

/*
 * close()
 */
static void close_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->is_finished = 1;
        cb_data->status = status;
}

int smb2_close(struct smb2_context *smb2, struct smb2fh *fh)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_close_async(smb2, fh, close_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_close_async failed");
		return -1;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -1;
        }

	return cb_data.status;
}

/*
 * pread()
 */
static void generic_status_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->is_finished = 1;
        cb_data->status = status;
}

int smb2_pread(struct smb2_context *smb2, struct smb2fh *fh,
               char *buf, uint32_t count, uint64_t offset)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_pread_async(smb2, fh, buf, count, offset,
                             generic_status_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_pread_async failed");
		return -1;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -1;
        }

	return cb_data.status;
}

int smb2_read(struct smb2_context *smb2, struct smb2fh *fh,
               char *buf, uint32_t count)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_read_async(smb2, fh, buf, count,
                            generic_status_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_read_async failed");
		return -1;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -1;
        }

	return cb_data.status;
}

int smb2_unlink(struct smb2_context *smb2, const char *path)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_unlink_async(smb2, path,
                            generic_status_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_unlink_async failed");
		return -1;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -1;
        }

	return cb_data.status;
}

int smb2_rmdir(struct smb2_context *smb2, const char *path)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_rmdir_async(smb2, path,
                            generic_status_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_rmdir_async failed");
		return -1;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -1;
        }

	return cb_data.status;
}

int smb2_mkdir(struct smb2_context *smb2, const char *path)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_mkdir_async(smb2, path,
                            generic_status_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_mkdir_async failed");
		return -1;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -1;
        }

	return cb_data.status;
}

int smb2_lstat(struct smb2_context *smb2, struct smb2fh *fh,
               struct smb2_stat_64 *st)
{
        struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (smb2_lstat_async(smb2, fh, st,
                             generic_status_cb, &cb_data) != 0) {
		smb2_set_error(smb2, "smb2_lstat_async failed");
		return -1;
	}

	if (wait_for_reply(smb2, &cb_data) < 0) {
                return -1;
        }

	return cb_data.status;
}
