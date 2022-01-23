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

#include "key_sink.hpp"
#include <stdlib.h>
#include <stdio.h>

static xcb_keycode_t get_keycode(xcb_key_symbols_t *syms, xcb_keysym_t c)
{
	xcb_keycode_t *codes = xcb_key_symbols_get_keycode(syms, c);

	if (!codes)
	{
		fprintf(stderr, "Failed to query keycode for %u.\n", c);
		exit(EXIT_FAILURE);
	}

	if (codes[0] == XCB_NO_SYMBOL)
	{
		fprintf(stderr, "No symbol found for %u.\n", c);
		exit(EXIT_FAILURE);
	}

	if (codes[1] != XCB_NO_SYMBOL)
	{
		fprintf(stderr, "Unexpected multi-key symbol for %u.\n", c);
		exit(EXIT_FAILURE);
	}

	xcb_keycode_t code = codes[0];
	free(codes);
	return code;
}

bool KeySink::init()
{
	conn = xcb_connect(nullptr, nullptr);
	if (!conn)
	{
		fprintf(stderr, "Unable to connect to X server.\n");
		return false;
	}

	xcb_generic_error_t *err = nullptr;
	xcb_test_get_version_cookie_t cookie = xcb_test_get_version(conn, 2, 1);
	xcb_test_get_version_reply_t *reply = xcb_test_get_version_reply(conn, cookie, &err);

	if (reply)
	{
		printf("Got XTEST version %u.%u.\n", reply->major_version, reply->minor_version);
		free(reply);
	}

	if (err)
	{
		printf("Got error: %d.\n", err->error_code);
		free(err);
		return false;
	}

	syms = xcb_key_symbols_alloc(conn);

	win = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
	return true;
}

xcb_keycode_t KeySink::translate_key(xcb_keysym_t c) const
{
	return get_keycode(syms, c);
}

xcb_keycode_t KeySink::translate_key(SpecialKey key) const
{
	constexpr xcb_keysym_t XK_Shift_L = 0xffe1;
	constexpr xcb_keysym_t XK_Control_L = 0xffe3;

	switch (key)
	{
	case SpecialKey::LeftShift:
		return get_keycode(syms, XK_Shift_L);
	case SpecialKey::LeftControl:
		return get_keycode(syms, XK_Control_L);
	default:
		return 0;
	}
}

void KeySink::dispatch(const Event *events, size_t count)
{
	for (size_t i = 0; i < count; i++)
		xcb_test_fake_input(conn, events[i].press ? XCB_KEY_PRESS : XCB_KEY_RELEASE, events[i].code, 0, win, 0, 0, 0);
	xcb_flush(conn);
}

KeySink::~KeySink()
{
	if (syms)
		xcb_key_symbols_free(syms);
	if (conn)
		xcb_disconnect(conn);
}
