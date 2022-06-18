/* Copyright (c) 2022 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "midi_source_udp.hpp"
#include <stdint.h>
#include <stdlib.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#endif

bool MIDISourceUDP::init(const char *client)
{
	if (!client || *client == '\0')
		return false;
	auto port = uint16_t(strtoul(client, nullptr, 0));

#ifdef _WIN32
	{
		WSADATA wsa_data;
		if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
			return false;
	}
#endif

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == INVALID_SOCKET)
		return false;

	sockaddr_in local = {};
	local.sin_port = htons(port);
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;

	const int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
	               reinterpret_cast<const char *>(&one), sizeof(one)) < 0)
		return false;

	if (bind(fd, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) < 0)
		return false;

	return true;
}

bool MIDISourceUDP::wait_next_note_event(NoteEvent &event)
{
	uint8_t buf[1024];

	int res;
	do
	{
		res = int(recvfrom(fd, reinterpret_cast<char *>(buf), sizeof(buf) - 1, 0, nullptr, nullptr));
		if (res < 0)
			return false;
	} while (res == 0);

	// Most basic protocol that ever existed :)

	event.pressed = (buf[0] & 0x80) != 0;
	event.note = buf[0] & 0x7f;
	return true;
}

MIDISourceUDP::~MIDISourceUDP()
{
	if (fd != INVALID_SOCKET)
	{
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif
	}
}
