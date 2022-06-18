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

#include "udp_sink.hpp"
#include <string.h>
#include <stdint.h>
#include <string>

UDPSink::~UDPSink()
{
	if (fd != INVALID_SOCKET)
		closesocket(fd);
}

bool UDPSink::init(const char *server)
{
	const char *port_delim = strrchr(server, ':');
	addrinfo *lookup_addr;
	int ret;

	if (!port_delim)
		return false;

	if (!init_socket_api())
		return false;

	std::string hostname{server, port_delim};

	port_delim++;
	if ((ret = getaddrinfo(hostname.c_str(), port_delim, nullptr, &lookup_addr)) != 0)
		return false;

	const addrinfo *iter = lookup_addr;
	while (iter)
	{
		if (iter->ai_family == AF_INET && iter->ai_addrlen == sizeof(addr))
		{
			memcpy(&addr, iter->ai_addr, sizeof(addr));
			break;
		}

		iter = iter->ai_next;
	}

	freeaddrinfo(lookup_addr);

	if (!iter)
		return false;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == INVALID_SOCKET)
		return false;

	return true;
}

bool UDPSink::send(int note, bool pressed)
{
	uint8_t msg = uint8_t(note) | (pressed ? 0x80 : 0);
	int ret = int(sendto(fd, reinterpret_cast<const char *>(&msg), 1, 0,
	                     reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)));
	return ret > 0;
}
