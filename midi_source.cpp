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

#include "midi_source.hpp"

bool MIDISource::wait_next_note_event(NoteEvent &event)
{
	snd_seq_event_t *ev = nullptr;
	bool got_event = false;

	while (!got_event)
	{
		int ret = snd_seq_event_input(seq, &ev);
		if (ret < 0)
		{
			fprintf(stderr, "Getting MIDI event failed. (ret = %d)\n", ret);
			break;
		}

		if (ev->type == SND_SEQ_EVENT_NOTEON && ev->data.note.velocity > 0)
		{
			event.note = ev->data.note.note;
			event.pressed = true;
			got_event = true;
		}
		else if (ev->type == SND_SEQ_EVENT_NOTEOFF || (ev->type == SND_SEQ_EVENT_NOTEON && ev->data.note.velocity == 0))
		{
			event.note = ev->data.note.note;
			event.pressed = false;
			got_event = true;
		}

		snd_seq_free_event(ev);
	}

	return got_event;
}

MIDISource::~MIDISource()
{
	if (seq)
		snd_seq_close(seq);
}

bool MIDISource::init(const char *client)
{
	int ret;
	if ((ret = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0)) < 0)
	{
		fprintf(stderr, "Failed to open. (ret = %d)\n", ret);
		return false;
	}

	snd_seq_set_client_name(seq, "Sussybard");
	list_midi_ports();

	if (!client)
	{
		fprintf(stderr, "No client provided ...\n");
		return false;
	}

	int in_port = snd_seq_create_simple_port(seq, "listen:in",
	                                         SND_SEQ_PORT_CAP_WRITE |
	                                         SND_SEQ_PORT_CAP_SUBS_WRITE,
	                                         SND_SEQ_PORT_TYPE_APPLICATION);

	if (in_port < 0)
	{
		fprintf(stderr, "Failed to open port, %d.\n", in_port);
		return false;
	}

	snd_seq_addr_t addr;
	ret = snd_seq_parse_address(seq, &addr, client);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to parse address.\n");
		return false;
	}

	printf("Found port %d:%d for client.\n", addr.client, addr.port);

	ret = snd_seq_connect_from(seq, in_port, addr.client, addr.port);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to connect.\n");
		return false;
	}

	return true;
}

void MIDISource::list_midi_ports()
{
	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0)
	{
		int client = snd_seq_client_info_get_client(cinfo);
		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0)
		{
			if ((snd_seq_port_info_get_type(pinfo) & SND_SEQ_PORT_TYPE_MIDI_GENERIC) == 0)
				continue;

			unsigned caps = snd_seq_port_info_get_capability(pinfo);
			const unsigned required = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
			if ((caps & required) != required)
				continue;

			printf("Client: [%s] (%d:%d) [%s].\n",
			       snd_seq_client_info_get_name(cinfo),
			       snd_seq_port_info_get_client(pinfo),
			       snd_seq_port_info_get_port(pinfo),
			       snd_seq_port_info_get_name(pinfo));
		}
	}
}
