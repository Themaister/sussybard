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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <vector>
#include "fmsynth.h"

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

class Synth final : public BackendCallback
{
public:
	~Synth() override;

	// FF XIV Bard doesn't have velocity or anything fancy, keep it simple.
	// We just need performance guiding.
	void post_note_on(int channel, int note);
	void post_note_off(int channel, int note);

	void mix_samples(float * const *channels, size_t num_frames) noexcept override;
	void set_backend_parameters(float sample_rate, unsigned channels, size_t max_num_frames) override;
	void on_backend_stop() override;
	void on_backend_start() override;
	void set_latency_usec(uint32_t usec) override;

private:
	enum { RingSize = 4096, NumSplits = 2 };
	fmsynth_t *fms[NumSplits] = {};
	std::vector<uint32_t> ring;
	std::atomic_uint32_t atomic_write_count;
	uint32_t read_count = 0;
	uint32_t write_count = 0;
};
