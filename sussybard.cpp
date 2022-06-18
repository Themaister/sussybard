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
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include "synth.hpp"
#include "cli_parser.hpp"
#include "midi_source_udp.hpp"
#include "udp_sink.hpp"

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
constexpr int num_octaves = 3;
constexpr int num_keys = num_octaves * 12 + 1; // High C is also included.

static std::vector<uint32_t> initialize_bind_table(const KeySink *key)
{
	std::vector<uint32_t> code_table;

	if (key)
	{
		code_table.reserve(num_keys);

		for (int i = 0; i < num_keys; i++)
		{
			if (i + 1 == num_keys)
				code_table.push_back(key->translate_key(','));
			else if ('a' + i <= 'z')
				code_table.push_back(key->translate_key('a' + i));
			else if (i > ('z' - 'a'))
				code_table.push_back(key->translate_key('0' + i - ('z' - 'a' + 1)));
		}
	}
	else
		code_table.resize(num_keys, 0);

	return code_table;
}

struct Arguments
{
	std::string client;
	std::string udp_sink;
	bool key_sink = false;
	bool udp_source = false;
	int midi_transpose = 0;
	int synth_transpose = 12;
	int base_key = 36;
};

static std::unique_ptr<MIDISource> create_midi_source(const Arguments &args)
{
	std::unique_ptr<MIDISource> source;
	if (args.udp_source)
	{
		source = std::make_unique<MIDISourceUDP>();
	}
	else
	{
#ifdef _WIN32
		source = std::make_unique<MIDISourceMM>();
#else
		source = std::make_unique<MIDISourceALSA>();
#endif
	}

	if (!source->init(args.client.c_str()))
		return {};

	return source;
}

static void print_help()
{
	fprintf(stderr, "sussybard\n"
	                "\t[--midi-source <MIDI device name>]\n"
	                "\t[--udp-source <port>]\n"
	                "\t[--key-sink]\n"
	                "\t[--udp-sink <addr:port>]\n"
	                "\t[--midi-transpose <semitones (default = 0)>]\n"
	                "\t[--synth-transpose <semitones (default = 12)>]\n"
	                "\t[--base-key <MIDI key which maps to lowest C on Bard instrument> (default = 36 / C2)]\n"
	                "\t[--help]\n");
}

int main(int argc, char **argv)
{
	Arguments args;
	Util::CLICallbacks cbs;

	cbs.add("--midi-source", [&](Util::CLIParser &parser) { args.client = parser.next_string(); });
	cbs.add("--udp-source", [&](Util::CLIParser &parser) { args.client = parser.next_string(); args.udp_source = true; });
	cbs.add("--key-sink", [&](Util::CLIParser &) { args.key_sink = true; });
	cbs.add("--udp-sink", [&](Util::CLIParser &parser) { args.udp_sink = parser.next_string(); });
	cbs.add("--midi-transpose", [&](Util::CLIParser &parser) { args.midi_transpose = parser.next_int(); });
	cbs.add("--synth-transpose", [&](Util::CLIParser &parser) { args.synth_transpose = parser.next_int(); });
	cbs.add("--base-key", [&](Util::CLIParser &parser) { args.base_key = parser.next_int(); });
	cbs.add("--help", [&](Util::CLIParser &parser) { parser.end(); });

	Util::CLIParser parser(std::move(cbs), argc - 1, argv + 1);
	if (!parser.parse())
	{
		print_help();
		return EXIT_FAILURE;
	}
	else if (parser.is_ended_state())
	{
		print_help();
		return EXIT_SUCCESS;
	}

	auto source = create_midi_source(args);
	if (!source)
		return EXIT_FAILURE;

	std::unique_ptr<KeySink> key;
	std::unique_ptr<UDPSink> udp_sink;

	if (args.key_sink)
	{
		key = std::make_unique<KeySink>();
		if (!key->init())
			return EXIT_FAILURE;
	}

	if (!args.udp_sink.empty())
	{
		udp_sink = std::make_unique<UDPSink>();
		if (!udp_sink->init(args.udp_sink.c_str()))
			return EXIT_FAILURE;
	}

	auto code_table = initialize_bind_table(key.get());

	Synth synth;
	AudioBackend pulse(&synth);
	if (!pulse.init(48000.0f, 2))
		return EXIT_FAILURE;

	pulse.start();
	MIDISource::NoteEvent ev = {};
	int pressed_note_offset = -1;
	while (source->wait_next_note_event(ev))
	{
		ev.note += args.midi_transpose;

		if (udp_sink && !udp_sink->send(ev.note, ev.pressed))
			break;

		int node_offset = ev.note - args.base_key;
		bool in_range = node_offset >= 0 && node_offset < num_keys;

		if (in_range)
		{
			// Ignore weird double taps.
			if (ev.pressed && pressed_note_offset == node_offset)
				continue;

			if (ev.pressed)
				synth.post_note_on(ev.note + args.synth_transpose);
			else
				synth.post_note_off(ev.note + args.synth_transpose);

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
				synth.post_note_off(pressed_note_offset + args.base_key + args.synth_transpose);
				pressed_note_offset = -1;
			}

			if (ev.pressed)
			{
				auto &e = key_events[event_count++];
				e.code = code_table[node_offset];
				e.press = true;
				pressed_note_offset = node_offset;
			}

			if (key && event_count)
				key->dispatch(key_events, event_count);
		}
	}

	if (key && pressed_note_offset >= 0)
	{
		KeySink::Event key_event = {};
		key_event.code = code_table[pressed_note_offset];
		key->dispatch(&key_event, 1);
	}

	pulse.stop();
}
