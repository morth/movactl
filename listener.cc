/*
 * Copyright (c) 2013 Per Johansson
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

#include <string>

#include <boost/asio.hpp>

#include <signal.h>
#include <sys/wait.h>

#include "spawn.hh"
#include "smart_fd.hh"
#include "bos.hh"

#include "osx_system_idle.h"

#define STEREO_LINE ":stereo"
#define TV_LINE ":tv"

using boost::asio::io_service;
using boost::asio::posix::stream_descriptor;
using boost::asio::ip::tcp;

void reap(pid_t pid)
{
	pid_t r;
	int status;

	do
		r = waitpid(pid, &status, 0);
	while (r == -1 && errno == EINTR);

	if (r == -1)
		throw std::system_error(errno, std::system_category(), "waitpid");
}

smart_fd spawn_movactl(pid_t *pid = NULL)
{
	smart_pipe iopipe;
	extern char **environ;
        static const char *const args[] = { "movactl", STEREO_LINE, "listen", "volume", "power", "audio_source", NULL};

	spawn::file_actions actions;

	actions.add_stdout(iopipe.write);
	actions.addclose(iopipe.write);

	int err = posix_spawn(pid, "/usr/local/bin/movactl", actions, NULL, (char**)args, environ);

	if (err)
		throw std::system_error(err, std::system_category(), "posix_spawn");

	return std::move(iopipe.read);
}

smart_fd spawn_phystatus(const char *idx, pid_t *pid = NULL)
{
	smart_pipe iopipe;
	extern char **environ;
	const char *const args[] = { "mii-tool", "-w", "-p", idx, "eth1", NULL };
	spawn::file_actions actions;

	actions.add_stdout(iopipe.write);
	actions.addclose(iopipe.write);

	int err = posix_spawn(pid, "/usr/sbin/mii-tool", actions, NULL, (char**)args, environ);

	if (err)
		throw std::system_error(err, std::system_category(), "posix_spawn");

	return std::move(iopipe.read);
}

void movactl_send(const std::vector<std::string> &args)
{
	std::vector<const char *> argv;
	pid_t pid;

	std::cout << "--movactl";
	argv.push_back("movactl");
	argv.push_back("--");
	for (auto &a : args) {
		std::cout << " " << a;
		argv.push_back(a.c_str());
	}
	argv.push_back(NULL);
	std::cout << "\n";

	int err = posix_spawn(&pid, "/usr/local/bin/movactl", NULL, NULL, (char**)&argv[0], NULL);

	if (err)
		throw std::system_error(err, std::system_category(), "posix_spawn");

	reap(pid);
}

constexpr unsigned int fnv1a_hash(const char *str)
{
	return *str ? (fnv1a_hash(str + 1) ^ *str) * 16777619U : 2166136261U;
}

std::string itos(int v)
{
	std::ostringstream ss;

	ss << v;
	return ss.str();
}

void power_on(const std::string &stereo, const std::string &tv)
{
	time_t now = time(NULL);
	struct tm tm = {0};

	localtime_r(&now, &tm);

	if (tm.tm_hour >= 1 && tm.tm_hour < 7)
		return;

	movactl_send({STEREO_LINE, "power", "on"});
	movactl_send({TV_LINE, "power", "on"});
	movactl_send({STEREO_LINE, "source", "select", stereo});
	movactl_send({TV_LINE, "source", "select", tv});
}

static std::string
default_volume()
{
	time_t now = time(NULL);
	struct tm tm = {0};

	localtime_r(&now, &tm);

	if (tm.tm_hour >= 7 && tm.tm_hour < 20)
		return "-37";
	return "-47";
}

void switch_from(const std::string &from, const std::map<std::string, std::string> &values)
{
	auto it = values.find("audio_source");
	if (it == values.end())
	{
		std::cerr << "switch_from(" << from << "): No audio source\n";
		return;
	}
	if (it->second != from)
	{
		std::cerr << "switch_from(" << from << "): audio_source == " << it->second << "\n";
		return;
	}

	it = values.find("playstation");
	if (it != values.end() && it->second == "really_up")
	{
		movactl_send({STEREO_LINE, "source", "select", "vcr1"});
		movactl_send({TV_LINE, "source", "select", "hdmi3"});
		return;
	}

	it = values.find("old_psx");
	if (it != values.end() && it->second == "up")
	{
		movactl_send({STEREO_LINE, "source", "select", "aux1"});
		movactl_send({TV_LINE, "source", "select", "hdmi1"});
		return;
	}

	it = values.find("wii");
	if (it != values.end() && it->second == "on")
	{
		movactl_send({STEREO_LINE, "source", "select", "dss"});
		movactl_send({TV_LINE, "source", "select", "hdmi1"});
		return;
	}

	it = values.find("tv");
	if (it != values.end() && it->second == "on")
	{
		movactl_send({STEREO_LINE, "source", "select", "tv"});
		movactl_send({TV_LINE, "source", "select", "hdmi1"});
		return;
	}

#ifdef IDLE
	struct timespec idle;
	if (IDLE(&idle) == 0 && idle.tv_sec <= 600)
	{
		movactl_send({STEREO_LINE, "source", "select", "dvd"});
		movactl_send({TV_LINE, "source", "select", "hdmi1"});
		return;
	}
#endif

	movactl_send({STEREO_LINE, "power", "off"});
}

/* Ugly to use global, but will have to do for now. */
static std::unique_ptr<tcp::socket> playstation_check_socket;
static void check_playstation_really_up(io_service &io);

void update(io_service &io, const std::string &line)
{
	std::string stat, value;
	static std::map<std::string, std::string> values;

	std::cout << "update: " << line << "\n";

	std::istringstream(line) >> stat >> value;

	if (value == "negotiating")
		return;

	auto elem = values.insert(make_pair(stat, value));
	if (!elem.second && elem.first->second == value)
	{
		std::cerr << "Not updating " << elem.first->first << ", " << value << " == " << elem.first->second << "\n";
		return;
	}
	elem.first->second = value;

	switch(fnv1a_hash(stat.c_str()))
	{
	case fnv1a_hash("volume"):
		movactl_send({TV_LINE, "volume", "value", itos(atoi(value.c_str()) + 72)});
		break;
	case fnv1a_hash("power"):
		if (value == "off")
			movactl_send({TV_LINE, "power", value});
		break;
	case fnv1a_hash("audio_source"):
		switch(fnv1a_hash(value.c_str()))
		{
		case fnv1a_hash("tv"):
			movactl_send({STEREO_LINE, "volume", "value", default_volume()});
			break;
		case fnv1a_hash("dvd"):
		case fnv1a_hash("vcr1"):
			movactl_send({STEREO_LINE, "volume", "value", default_volume()});
			break;
		case fnv1a_hash("dss"):
			movactl_send({STEREO_LINE, "volume", "value", default_volume()});
			break;
		}
		break;
	case fnv1a_hash("playstation"):
		if (value == "up")
			check_playstation_really_up(io);
		else if (value == "really_up")
			power_on("vcr1", "hdmi3");
		else
		{
			playstation_check_socket.reset();
			switch_from("vcr1", values);
		}
		break;
	case fnv1a_hash("old_psx"):
		if (value == "up")
		{
			power_on("aux1", "hdmi1");
			movactl_send({STEREO_LINE, "source", "select", "dss"});
			movactl_send({STEREO_LINE, "source", "select", "aux1"});
		}
		else
			switch_from("aux1", values);
		break;
	case fnv1a_hash("tv"):
		if (value == "on")
			power_on("tv", "hdmi1");
		else
			switch_from("tv", values);
		break;
	case fnv1a_hash("wii"):
		if (value == "on")
			power_on("dss", "hdmi1");
		else
			switch_from("dss", values);
		break;
	case fnv1a_hash("airplay"):
		if (value == "on")
			power_on("dvd", "hdmi2");
		else
			switch_from("dvd", values);
		break;
	}
}

static void
check_playstation_really_up_handler(int cntr, const boost::system::error_code& error)
{
	std::cout << "check_playstation_really_up_handler: " << error << "\n";

	if (!error) {
		update(playstation_check_socket->get_io_service(), "playstation really_up");
		return;
	}

	static const boost::system::error_code econnrefused(ECONNREFUSED, boost::system::system_category());
	static const boost::system::error_code ehostunreach(EHOSTUNREACH, boost::system::system_category());
	static const boost::system::error_code ehostdown(EHOSTDOWN, boost::system::system_category());
	if (error != econnrefused && error != ehostunreach && error != ehostdown)
		return;

	if (cntr < 15) {
		sleep(2); /* Blocking ftw */
		playstation_check_socket->close();
		tcp::endpoint endpoint(boost::asio::ip::address::from_string("192.168.2.185"), 9295);
		playstation_check_socket->async_connect(endpoint, std::bind(check_playstation_really_up_handler, cntr + 1, std::placeholders::_1));
	}
}

static void
check_playstation_really_up(io_service &io)
{
	playstation_check_socket.reset(new tcp::socket(io));
	tcp::endpoint endpoint(boost::asio::ip::address::from_string("192.168.2.185"), 9295);
	playstation_check_socket->async_connect(endpoint, std::bind(check_playstation_really_up_handler, 0, std::placeholders::_1));
}

template <typename T>
class input
{
public:
	T object;
	boost::asio::streambuf buffer;
	std::istream stream;
	bool active;
	const std::string ewhat;
	std::function<void(io_service&, const std::string&)> update_fn;

	template <typename ...Args>
	input(std::string ewhat, std::function<void(io_service&, const std::string&)> update_fn, Args &&...args)
		: object(args...), stream(&buffer), active(false), ewhat(ewhat), update_fn(update_fn)
	{
	}

	virtual void activate()
	{
		if (!active)
		{
			boost::asio::async_read_until(object, buffer, '\n',
					std::bind(&input<T>::readcb, this,
						std::placeholders::_1,
						std::placeholders::_2));
			active = true;
		}
	}

	virtual void handle(const boost::system::error_code &error, std::size_t bytes)
	{
		if (error)
			throw boost::system::system_error(error, ewhat);

		std::string line;
		while (std::getline(stream, line).good())
			update_fn(object.get_io_service(), line);
		stream.clear();

		activate();
	}

	void readcb(const boost::system::error_code &error, std::size_t bytes)
	{
		active = false;
		handle(error, bytes);
	}
};

class tcp_input : public input<tcp::socket>
{
public:
	bool connected;
	tcp::resolver resolver;
	tcp::resolver::iterator iterator;
	boost::asio::deadline_timer timer;
	tcp::resolver::query query;

	tcp_input(std::string ewhat, std::function<void(io_service&, const std::string&)> update_fn, io_service &io,
			tcp::resolver::query query)
		: input(std::move(ewhat), std::move(update_fn), io), connected(false), resolver(io), timer(io),
		query(std::move(query))
	{
	}

	virtual void activate()
	{
		if (!connected)
			return;
		input<tcp::socket>::activate();
	}

	void connect_done(const boost::system::error_code &error)
	{
		if (error)
		{
			connect_next(error);
			return;
		}
		connected = true;
		object.set_option(boost::asio::socket_base::keep_alive(true));
		activate();
	}

	void connect_next(const boost::system::error_code &error)
	{
		if (error)
			std::cerr << "warn: tcp_input::connect_next: " << error << "\n";
		if (iterator == tcp::resolver::iterator())
		{
			timer.expires_from_now(boost::posix_time::seconds(3));
			timer.async_wait(std::bind(&tcp_input::connect, this));
			return;
		}

		object.async_connect(*iterator++, std::bind(&tcp_input::connect_done, this, std::placeholders::_1));
	}

	void connect()
	{
		boost::system::error_code ec;
		object.cancel(ec);
		iterator = resolver.resolve(query, ec);

		connect_next(ec);
	}

	virtual void handle(const boost::system::error_code &error, std::size_t bytes)
	{
		if (error)
			std::cerr << "warn: tpc_input::handle: " << error << "\n";
		if (!bytes) {
			object.close();
			connected = false;
			connect();
			return;
		}

		input<tcp::socket>::handle(error, bytes);
	}
};

class phy_input
{
public:
	std::string phy_idx;
	boost::asio::deadline_timer timer;
	std::function<void(io_service&, const std::string&)> real_update_fn;
	pid_t pid;
	smart_fd fd;
	input<stream_descriptor> sinput;

	phy_input(std::string ewhat, std::function<void(io_service&, const std::string&)> update_fn, io_service &io,
			std::string idx)
		: phy_idx(std::move(idx)),
		timer(io),
		real_update_fn(std::move(update_fn)),
		fd(spawn_phystatus(phy_idx.c_str(), &pid)),
		sinput(std::move(ewhat), std::bind(&phy_input::phy_update, this, std::placeholders::_1, std::placeholders::_2), io, fd)
	{
		fd.release();
	}

	~phy_input()
	{
		kill(pid, SIGTERM);
		reap(pid);
	}

	void phy_update_timeout(const std::string &line, const boost::system::error_code &error)
	{
		if (error)
		{
			std::cerr << "phy_update_timeout: skipping " << line << " (" << error << ")\n";
			return;
		}
		std::string v;

		if (line.find("no link") == std::string::npos)
			v = "up";
		else
			v = "down";
		real_update_fn(timer.get_io_service(), sinput.ewhat + " " + v);
	}

	void phy_update(io_service &io, const std::string &line)
	{
		timer.expires_from_now(boost::posix_time::seconds(3));
		timer.async_wait(std::bind(&phy_input::phy_update_timeout, this, std::string(line), std::placeholders::_1));
	}

	void activate(void)
	{
		sinput.activate();
	}
};

int
main()
{
	bos();

	tcp::resolver::query apquery("vardagsrum", "7120");
	try
	{
		while (1)
		{
			pid_t movapid;
			smart_fd movafd = spawn_movactl(&movapid);

			io_service io;
			input<stream_descriptor> movainput("movactl", update, io, movafd);
			movafd.release();

			phy_input psxinput("playstation", update, io, "1");
			phy_input psx_old_input("old_psx", update, io, "2");

			input<boost::asio::serial_port> wiitvinput("wii/tv", update, io, "/dev/ttyACM0");
			wiitvinput.object.set_option(boost::asio::serial_port_base::baud_rate(9600));

			tcp_input apinput("airplay", update, io, apquery);
			apinput.connect();

			try
			{
				while (1)
				{
					movainput.activate();
					psxinput.activate();
					psx_old_input.activate();
					wiitvinput.activate();
					apinput.activate();

					io.run();
					io.reset();
				}
			}
			catch (boost::system::system_error &e)
			{
				std::cerr << "err: " << e.what() << "\n";
			}

			kill(movapid, SIGTERM);
			reap(movapid);
		}
	}
	catch (std::exception &e)
	{
		std::cerr << "exception: " << e.what() << "\n";
	}
	return 1;
}

