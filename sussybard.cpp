#include <alsa/asoundlib.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <xcb/xtest.h>
#include <xcb/xcb_keysyms.h>
#include <stdlib.h>
#include <vector>

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

static void list_midi_ports(snd_seq_t *seq)
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

int main(int argc, char **argv)
{
	const char *client = nullptr;
	if (argc >= 2)
		client = argv[1];

	snd_seq_t *seq = nullptr;
	int ret;
	if ((ret = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0)) < 0)
	{
		fprintf(stderr, "Failed to open. (ret = %d)\n", ret);
		return EXIT_FAILURE;
	}

	snd_seq_set_client_name(seq, "Sussybard");
	list_midi_ports(seq);

	if (!client)
	{
		fprintf(stderr, "No client provided.\n");
		return EXIT_SUCCESS;
	}

	int in_port = snd_seq_create_simple_port(seq, "listen:in",
	                                         SND_SEQ_PORT_CAP_WRITE |
	                                         SND_SEQ_PORT_CAP_SUBS_WRITE,
	                                         SND_SEQ_PORT_TYPE_APPLICATION);

	if (in_port < 0)
	{
		fprintf(stderr, "Failed to open port, %d.\n", in_port);
		return EXIT_FAILURE;
	}

	snd_seq_addr_t addr;
	ret = snd_seq_parse_address(seq, &addr, client);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to parse address.\n");
		return EXIT_FAILURE;
	}

	printf("Found port %d:%d for client.\n", addr.client, addr.port);

	ret = snd_seq_connect_from(seq, in_port, addr.client, addr.port);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to connect.\n");
		return EXIT_FAILURE;
	}

	xcb_connection_t *conn = xcb_connect(nullptr, nullptr);
	if (!conn)
	{
		fprintf(stderr, "Unable to connect to X server.\n");
		return EXIT_FAILURE;
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
	}

	xcb_window_t win = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;

	// 3 octave range for Bard.
	constexpr int base_key = 36; // C somewhere on my keyboard.
	constexpr int num_octaves = 3;
	constexpr int num_keys = num_octaves * 12 + 1; // High C is also included.

	xcb_key_symbols_t *syms = xcb_key_symbols_alloc(conn);
	struct Event
	{
		xcb_keycode_t code;
		uint8_t type;
	};
	std::vector<std::vector<Event>> code_tables(num_keys);
	for (int octave = 0; octave < num_octaves; octave++)
	{
		int base = octave * 12;
		constexpr xcb_keysym_t XK_Shift_L = 0xffe1;
		constexpr xcb_keysym_t XK_Control_L = 0xffe3;

		if (octave == 0)
		{
			xcb_keycode_t down = get_keycode(syms, XK_Control_L);
			for (int j = 0; j < 12; j++)
				code_tables[base + j].push_back({ down, XCB_KEY_PRESS });
		}
		else if (octave == 2)
		{
			xcb_keycode_t up = get_keycode(syms, XK_Shift_L);
			for (int j = 0; j < 13; j++)
				code_tables[base + j].push_back({ up, XCB_KEY_PRESS });
		}

		const auto add_key_press_release = [&](int i, xcb_keysym_t c) {
			xcb_keycode_t code = get_keycode(syms, c);
			code_tables[base + i].push_back({ code, XCB_KEY_PRESS });
			code_tables[base + i].push_back({ code, XCB_KEY_RELEASE });
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
			add_key_press_release(12, 'i'); // B

		if (octave == 0)
		{
			xcb_keycode_t down = get_keycode(syms, XK_Control_L);
			for (int j = 0; j < 12; j++)
				code_tables[base + j].push_back({ down, XCB_KEY_RELEASE });
		}
		else if (octave == 2)
		{
			xcb_keycode_t up = get_keycode(syms, XK_Shift_L);
			for (int j = 0; j < 13; j++)
				code_tables[base + j].push_back({ up, XCB_KEY_RELEASE });
		}
	}

	snd_seq_event_t *ev = nullptr;
	for (;;)
	{
		ret = snd_seq_event_input(seq, &ev);
		if (ret < 0)
		{
			fprintf(stderr, "Getting MIDI event failed. (ret = %d)\n", ret);
			break;
		}

		if (ev->type == SND_SEQ_EVENT_NOTEON && ev->data.note.velocity > 0)
		{
			//printf("Note on! %d (vel = %d)\n", ev->data.note.note, ev->data.note.velocity);
			int node_offset = int(ev->data.note.note) - base_key;
			if (node_offset >= 0 && node_offset < num_keys)
			{
				auto &chain = code_tables[node_offset];
				for (auto &input : chain)
					xcb_test_fake_input(conn, input.type, input.code, 0, win, 0, 0, 0);
			}
			xcb_flush(conn);
		}
#if 0
		// We cannot sustain notes in FF, so don't bother for now.
		else if (ev->type == SND_SEQ_EVENT_NOTEOFF || (ev->type == SND_SEQ_EVENT_NOTEON && ev->data.note.velocity == 0))
		{
			printf("Note off! %d\n", ev->data.note.note);
		}
#endif

		snd_seq_free_event(ev);
	}

	snd_seq_close(seq);
	xcb_key_symbols_free(syms);
	xcb_disconnect(conn);
}
