/* Copyright (c) 2017-2022 Hans-Kristian Arntzen
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

#include <pulse/pulseaudio.h>
#include <atomic>
#include <vector>
#include "fmsynth.h"

// Hacked and stripped down version of Granite's Pulse backend.

class BackendCallback
{
public:
	virtual ~BackendCallback() = default;
	virtual void mix_samples(float * const *channels, size_t num_frames) noexcept = 0;

	virtual void set_backend_parameters(float sample_rate, unsigned channels, size_t max_num_frames) = 0;
	virtual void on_backend_stop() = 0;
	virtual void on_backend_start() = 0;
	virtual void set_latency_usec(uint32_t usec) = 0;
};

class Synth : public BackendCallback
{
public:
	~Synth();

	// FF XIV Bard doesn't have velocity or anything fancy, keep it simple.
	// We just need performance guiding.
	void post_note_on(int note);
	void post_note_off(int note);

	void mix_samples(float * const *channels, size_t num_frames) noexcept override;
	void set_backend_parameters(float sample_rate, unsigned channels, size_t max_num_frames) override;
	void on_backend_stop() override;
	void on_backend_start() override;
	void set_latency_usec(uint32_t usec) override;

private:
	fmsynth_t *fm = nullptr;
	enum { RingSize = 4096 };
	std::vector<uint32_t> ring;
	std::atomic_uint32_t atomic_write_count;
	uint32_t read_count = 0;
	uint32_t write_count = 0;
};

struct Pulse
{
public:
	explicit Pulse(BackendCallback *callback_);
	~Pulse();

	bool init(float sample_rate_, unsigned channels_);
	bool start();
	bool stop();

	float get_sample_rate() const
	{
		return sample_rate;
	}

	unsigned get_num_channels() const
	{
		return channels;
	}

	enum { MaxChannels = 2 };

	BackendCallback *callback;
	float sample_rate = 0.0f;
	unsigned channels = 0;

	pa_threaded_mainloop *mainloop = nullptr;
	pa_context *context = nullptr;
	pa_stream *stream = nullptr;
	size_t buffer_frames = 0;
	int success = -1;
	bool has_success = false;
	bool is_active = false;

	void update_buffer_attr(const pa_buffer_attr &attr) noexcept;
	size_t to_frames(size_t size) const noexcept;
};
