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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include "synth.hpp"

#ifdef _WIN32
#include "midi_source_win32.hpp"
#include "key_sink_win32.hpp"
#include "audio_wasapi.hpp"
#else
#include "midi_source_alsa.hpp"
#include "key_sink_xcb.hpp"
#include "audio_pulse.hpp"
#endif

// 3 octave range for Bard.
constexpr int base_key = 36; // C somewhere on my keyboard.
constexpr int num_octaves = 3;
constexpr int num_keys = num_octaves * 12 + 1; // High C is also included.

static std::vector<uint32_t> initialize_bind_table(const KeySink &key)
{
	std::vector<uint32_t> code_table;
	code_table.reserve(num_keys);

	for (int i = 0; i < num_keys; i++)
	{
		if (i + 1 == num_keys)
			code_table.push_back(key.translate_key(','));
		else if ('a' + i <= 'z')
			code_table.push_back(key.translate_key('a' + i));
		else if (i > ('z' - 'a'))
			code_table.push_back(key.translate_key('0' + i - ('z' - 'a' + 1)));
	}

	return code_table;
}

int main(int argc, char **argv)
{
	const char *client = nullptr;
	if (argc >= 2)
		client = argv[1];

	MIDISource source;
	if (!source.init(client))
		return EXIT_FAILURE;

	KeySink key;
	if (!key.init())
		return EXIT_FAILURE;

	Synth synth;
	AudioBackend pulse(&synth);
	if (!pulse.init(48000.0f, 2))
		return EXIT_FAILURE;

	auto code_table = initialize_bind_table(key);

	pulse.start();
	MIDISource::NoteEvent ev = {};
	int pressed_note_offset = -1;
	while (source.wait_next_note_event(ev))
	{
		int node_offset = ev.note - base_key;
		bool in_range = node_offset >= 0 && node_offset < num_keys;

		if (in_range)
		{
			// Ignore weird double taps.
			if (ev.pressed && pressed_note_offset == node_offset)
				continue;

			if (ev.pressed)
				synth.post_note_on(ev.note);
			else
				synth.post_note_off(ev.note);

			KeySink::Event key_events[2] = {};
			unsigned event_count = 0;

			bool release_held_key = ev.pressed || pressed_note_offset == node_offset;

			// There is no polyphony, so release any pressed key before we can press another one.
			// If we're releasing a key, release only the held key if there's a match.
			if (release_held_key && pressed_note_offset >= 0)
			{
				auto &e = key_events[event_count++];
				e.code = code_table[pressed_note_offset];
				e.press = false;
				synth.post_note_off(pressed_note_offset + base_key);
				pressed_note_offset = -1;
			}

			if (ev.pressed)
			{
				auto &e = key_events[event_count++];
				e.code = code_table[node_offset];
				e.press = true;
				pressed_note_offset = node_offset;
			}

			if (event_count)
				key.dispatch(key_events, event_count);
		}
	}

	if (pressed_note_offset >= 0)
	{
		KeySink::Event key_event = {};
		key_event.code = code_table[pressed_note_offset];
		key.dispatch(&key_event, 1);
	}

	pulse.stop();
}
