#include <alsa/asoundlib.h>
#include <stdio.h>

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
			printf("Note on! %d (vel = %d)\n", ev->data.note.note, ev->data.note.velocity);
		}
		else if (ev->type == SND_SEQ_EVENT_NOTEOFF || (ev->type == SND_SEQ_EVENT_NOTEON && ev->data.note.velocity == 0))
		{
			printf("Note off! %d\n", ev->data.note.note);
		}

		snd_seq_free_event(ev);
	}

	snd_seq_close(seq);
}
