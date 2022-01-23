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
#include <vector>
#include "fmsynth.h"
#include "midi_source.hpp"
#include "key_sink.hpp"

// 3 octave range for Bard.
constexpr int base_key = 36; // C somewhere on my keyboard.
constexpr int num_octaves = 3;
constexpr int num_keys = num_octaves * 12 + 1; // High C is also included.

static std::vector<std::vector<KeySink::Event>> initialize_bind_table(const KeySink &key)
{
	std::vector<std::vector<KeySink::Event>> code_tables(num_keys);

	for (int octave = 0; octave < num_octaves; octave++)
	{
		int base = octave * 12;

		if (octave == 0)
		{
			auto down = key.translate_key(KeySink::SpecialKey::LeftControl);
			for (int j = 0; j < 12; j++)
				code_tables[base + j].push_back({ down, true });
		}
		else if (octave == 2)
		{
			xcb_keycode_t up = key.translate_key(KeySink::SpecialKey::LeftShift);
			for (int j = 0; j < 13; j++)
				code_tables[base + j].push_back({ up, true });
		}

		const auto add_key_press_release = [&](int i, char c) {
			xcb_keycode_t code = key.translate_key(c);
			code_tables[base + i].push_back({ code, true });
			code_tables[base + i].push_back({ code, false });
		};

		add_key_press_release(0, 'q'); // C
		add_key_press_release(1, '2'); // C#
		add_key_press_release(2, 'w'); // D
		add_key_press_release(3, '3'); // D#
		add_key_press_release(4, 'e'); // E
		add_key_press_release(5, 'r'); // F
		add_key_press_release(6, '5'); // F#
		add_key_press_release(7, 't'); // G
		add_key_press_release(8, '6'); // G#
		add_key_press_release(9, 'y'); // A
		add_key_press_release(10, '7'); // A#
		add_key_press_release(11, 'u'); // B
		if (octave == 2)
			add_key_press_release(12, 'i'); // C + 1

		if (octave == 0)
		{
			auto down = key.translate_key(KeySink::SpecialKey::LeftControl);
			for (int j = 0; j < 12; j++)
				code_tables[base + j].push_back({ down, false });
		}
		else if (octave == 2)
		{
			xcb_keycode_t up = key.translate_key(KeySink::SpecialKey::LeftShift);
			for (int j = 0; j < 13; j++)
				code_tables[base + j].push_back({ up, false });
		}
	}

	return code_tables;
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

	auto code_tables = initialize_bind_table(key);

	MIDISource::NoteEvent ev = {};
	while (source.wait_next_note_event(ev))
	{
		if (!ev.pressed)
			continue;

		//printf("Note on! %d (vel = %d)\n", ev->data.note.note, ev->data.note.velocity);
		int node_offset = ev.note - base_key;
		if (node_offset >= 0 && node_offset < num_keys)
		{
			auto &chain = code_tables[node_offset];
			key.dispatch(chain.data(), chain.size());
		}
	}
}
