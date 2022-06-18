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

#include "midi_source_win32.hpp"
#include <stdio.h>

void MIDISourceMM::list_midi_ports()
{
	auto num_devices = midiInGetNumDevs();
	MIDIINCAPS caps;

	for (unsigned i = 0; i < num_devices; i++)
	{
		if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
			printf("MIDI device %u: %s.\n", i, caps.szPname);
		else
			fprintf(stderr, "Failed to query MIDI IN caps for device %u.\n", i);
	}
}

MIDISourceMM::~MIDISourceMM()
{
	if (handle)
	{
		if (midiInStop(handle) != MMSYSERR_NOERROR)
			fprintf(stderr, "Failed to stop MIDI.\n");
		midiInClose(handle);
	}
}

void MIDISourceMM::key_on(int note)
{
	std::lock_guard<std::mutex> holder{lock};
	note_queue.push({ note, true });
	cond.notify_one();
}

void MIDISourceMM::key_off(int note)
{
	std::lock_guard<std::mutex> holder{lock};
	note_queue.push({ note, false });
	cond.notify_one();
}

bool MIDISourceMM::wait_next_note_event(NoteEvent &event)
{
	std::unique_lock<std::mutex> holder{lock};
	cond.wait(holder, [this]() {
		return !note_queue.empty();
	});
	event = note_queue.front();
	note_queue.pop();
	return true;
}

static void CALLBACK midi_callback(HMIDIIN, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dw_param1, DWORD_PTR)
{
	auto *source = reinterpret_cast<MIDISourceMM *>(dwInstance);
	if (wMsg == MM_MIM_DATA)
	{
		auto code = uint8_t(dw_param1 >> 0);
		auto note = uint8_t(dw_param1 >> 8);
		auto vel = uint8_t(dw_param1 >> 16);

		bool note_on = (code & 0xf0) == 0x90;
		bool note_off = (code & 0xf0) == 0x80;

		if (note_on && vel > 0)
			source->key_on(note);
		else if (note_off || (note_on && vel == 0))
			source->key_off(note);
	}
}

bool MIDISourceMM::init(const char *client)
{
	list_midi_ports();

	if (!client || *client == '\0')
	{
		fprintf(stderr, "No client selected.\n");
		return false;
	}

	auto num_devices = midiInGetNumDevs();
	if (!num_devices)
	{
		fprintf(stderr, "No MIDI input devices found.\n");
		return false;
	}

	unsigned deviceID = UINT_MAX;

	for (unsigned i = 0; i < num_devices; i++)
	{
		MIDIINCAPS caps;
		if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
		    strcmp(caps.szPname, client) == 0)
		{
			deviceID = i;
			break;
		}
	}

	if (deviceID == UINT_MAX)
	{
		fprintf(stderr, "Did not find MIDI device with name %s.\n", client);
		return false;
	}

	if (midiInOpen(&handle, deviceID, reinterpret_cast<DWORD_PTR>(midi_callback),
	               reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION) !=
	    MMSYSERR_NOERROR)
	{
		fprintf(stderr, "Failed to open MIDI device.\n");
		return false;
	}

	if (midiInStart(handle) != MMSYSERR_NOERROR)
		fprintf(stderr, "Failed to start MIDI.\n");

	return true;
}
