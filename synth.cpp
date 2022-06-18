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

#include "synth.hpp"
#include <string.h>

Synth::~Synth()
{
	if (fm)
		fmsynth_free(fm);
}

void Synth::set_backend_parameters(float sample_rate, unsigned, size_t)
{
	fm = fmsynth_new(sample_rate, 64);
	// TODO: Set some nice default parameters.
}

void Synth::mix_samples(float *const *channels, size_t num_frames) noexcept
{
	uint32_t target = atomic_write_count.load(std::memory_order_acquire);
	constexpr int transpose = 12;

	for (; read_count < target; read_count++)
	{
		uint32_t note = ring[read_count % RingSize];
		if (note & 0x80000000u)
			fmsynth_note_on(fm, uint8_t(note + transpose), 255);
		else
			fmsynth_note_off(fm, uint8_t(note + transpose));
	}

	memset(channels[0], 0, num_frames * sizeof(float));
	memset(channels[1], 0, num_frames * sizeof(float));
	fmsynth_render(fm, channels[0], channels[1], num_frames);
}

void Synth::post_note_on(int note)
{
	ring[(write_count++) % RingSize] = note | 0x80000000u;
	atomic_write_count.store(write_count, std::memory_order_release);
}

void Synth::post_note_off(int note)
{
	ring[(write_count++) % RingSize] = note;
	atomic_write_count.store(write_count, std::memory_order_release);
}

void Synth::on_backend_stop()
{
}

void Synth::set_latency_usec(uint32_t)
{
}

void Synth::on_backend_start()
{
	write_count = 0;
	read_count = 0;
	atomic_write_count = 0;
	ring.resize(RingSize);

	if (fm)
	{
		fmsynth_reset(fm);
		fmsynth_set_global_parameter(fm, FMSYNTH_GLOBAL_PARAM_VOLUME, 0.1f);

		fmsynth_set_parameter(fm, FMSYNTH_PARAM_DELAY0, 0, 0.01f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_DELAY1, 0, 1.0f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_DELAY2, 0, 1.0f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_RELEASE_TIME, 0, 1.5f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_ENVELOPE_TARGET0, 0, 1.0f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_ENVELOPE_TARGET1, 0, 0.2f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_ENVELOPE_TARGET2, 0, 0.03f);

		for (unsigned i = 1; i < 2; i++)
		{
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_DELAY0, i, 0.005f);
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_DELAY1, i, i == 1 ? 0.25f : 0.23f);
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_DELAY2, i, i == 1 ? 0.25f : 0.15f);
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_RELEASE_TIME, i, i == 1 ? 0.85f : 0.5f);
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_ENVELOPE_TARGET0, i, 1.0f);
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_ENVELOPE_TARGET1, i, 0.2f);
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_ENVELOPE_TARGET2, i, 0.10f);
		}

		for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
		{
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_ENABLE, i, i < 3 ? 1.0f : 0.0f);
			fmsynth_set_parameter(fm, FMSYNTH_PARAM_CARRIERS, i, i == 0 ? 1.0f : 0.0f);
		}

		fmsynth_set_parameter(fm, FMSYNTH_PARAM_FREQ_MOD, 1, 1.0f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_KEYBOARD_SCALING_HIGH_FACTOR, 1, -0.5f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_KEYBOARD_SCALING_LOW_FACTOR, 1, -0.5f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_MOD_TO_CARRIERS0 + 1, 0, 0.8f);
		fmsynth_set_parameter(fm, FMSYNTH_GLOBAL_PARAM_VOLUME, 1, 1.0f);

		fmsynth_set_parameter(fm, FMSYNTH_PARAM_FREQ_MOD, 2, 12.00f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_KEYBOARD_SCALING_HIGH_FACTOR, 2, -1.0f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_KEYBOARD_SCALING_LOW_FACTOR, 2, -1.0f);
		fmsynth_set_parameter(fm, FMSYNTH_PARAM_MOD_TO_CARRIERS0 + 2, 1, 0.5f);
		fmsynth_set_parameter(fm, FMSYNTH_GLOBAL_PARAM_VOLUME, 2, 0.6f);
	}
}
