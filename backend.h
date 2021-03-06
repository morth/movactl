/*
 * Copyright (c) 2008 Pelle Johansson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BACKEND_H
#define BACKEND_H

#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct backend_output {
	char *data;
	ssize_t len;
	struct timeval throttle;
};

void add_backend_device(const char *str);

void backend_reopen_devices(void);

void backend_listen_fd (const char *dev, int fd);

void backend_listen_all (void);
void backend_close_all (void);

#ifdef __cplusplus
}

#include <functional>
#include <memory>
#include <vector>

class backend_device;
class status;
class status_notify_token;

class backend_ptr
{
	std::unique_ptr<backend_device> bdev;
	friend class backend_device;
	template<class ...Args> backend_ptr(Args&& ...args);

public:
	typedef std::function<void(const std::string &code, const std::string &val)> notify_cb;
	typedef std::function<class status *(backend_ptr&, std::string, std::string, std::string, int)> creator;

	void remove_output(const struct backend_output **inptr);

	bool query_command(const std::string &code);
	bool query_status(const std::string &code);
	int query(const std::string &code, std::string &out_buf);
	void send_command(const std::string &cmd, const std::vector<int32_t> &args);
	void send_status_request(const std::string &code);

	std::unique_ptr<status_notify_token> start_notify(const std::string &code, backend_ptr::notify_cb cb);
};

#endif

#endif /*BACKEND_H*/
