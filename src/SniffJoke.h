/*
 *   SniffJoke is a software able to confuse the Internet traffic analysis,
 *   developed with the aim to improve digital privacy in communications and
 *   to show and test some securiy weakness in traffic analysis software.
 *   
 *   Copyright (C) 2010 vecna <vecna@delirandom.net>
 *                      evilaliv3 <giovanni.pellerano@evilaliv3.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SJ_SNIFFJOKE_H
#define SJ_SNIFFJOKE_H

#include "Utils.h"
#include "UserConf.h"
#include "Process.h"
#include "NetIO.h"

#include <csignal>
#include <cstdio>
#include <memory>

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

using namespace std;

class SniffJoke {
public:
	bool alive;
	SniffJoke(struct sj_cmdline_opts &);
	~SniffJoke();
	void run();

private:
	sj_cmdline_opts &opts;
	UserConf userconf;
	Process proc;
	auto_ptr<NetIO> mitm;
	auto_ptr<HackPool> hack_pool;
	auto_ptr<TCPTrack> conntrack;

	/* after detach:
	 *     service_pid in the root process [the pid of the user process]
	 *                 in the user process [0]
	 * 
	 * this is used as discriminator for the process controller of the pidfile
	 * inside the distructor ~SniffJoke()
	 */
	pid_t service_pid;
	
	int admin_socket;

	void debug_setup(FILE *) const;
	void debug_cleanup();
	void client();
	void client_cleanup();
	void server();
	void server_root_cleanup();
	void server_user_cleanup();
	void kill_child();
	int udp_admin_socket(char [MEDIUMBUF], unsigned short);
	void handle_admin_socket(int admin_socket);
	int recv_command(int sock, char *databuf, int bufsize, struct sockaddr *from, FILE *error_flow, const char *usermsg);	
	void send_command(const char *cmdstring, char [MEDIUMBUF], unsigned short);
	bool parse_port_weight(char *weightstr, Strength *Value);
};

#endif /* SJ_SNIFFJOKE_H */