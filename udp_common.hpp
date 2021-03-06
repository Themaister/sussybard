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

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
static inline bool init_socket_api()
{
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
		return false;

	return true;
}
#else
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
using SOCKET = int;
static constexpr SOCKET INVALID_SOCKET = -1;
static inline bool init_socket_api()
{
	return true;
}

static inline void closesocket(int fd)
{
	close(fd);
}
#endif
