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

#include "key_sink_win32.hpp"

bool KeySink::init()
{
	return true;
}

KeySink::~KeySink()
{
}

void KeySink::dispatch(const Event *events, size_t count)
{
	input_buffer.clear();
	input_buffer.reserve(count);

	for (size_t i = 0; i < count; i++)
	{
		INPUT ip = {};
		ip.type = INPUT_KEYBOARD;
		ip.ki.dwFlags = events[i].press ? 0 : KEYEVENTF_KEYUP;
		ip.ki.wVk = events[i].code;
		input_buffer.push_back(ip);
	}

	SendInput(UINT(count), input_buffer.data(), sizeof(INPUT));
}

uint32_t KeySink::translate_key(char key) const
{
	if (isalpha(key))
		return uint32_t(toupper(key));
	else if (key == ',')
		return VK_OEM_COMMA;
	else
		return uint32_t(key);
}

uint32_t KeySink::translate_key(SpecialKey key) const
{
	switch (key)
	{
	case SpecialKey::LeftControl:
		return VK_LCONTROL;
	case SpecialKey::LeftShift:
		return VK_LSHIFT;
	default:
		return 0;
	}
}
