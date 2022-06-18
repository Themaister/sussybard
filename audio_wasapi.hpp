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

#include <thread>
#include <atomic>
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include "synth.hpp"

// Hacked and stripped down version of Granite's WASAPI backend.

struct WASAPI
{
public:
	explicit WASAPI(BackendCallback *callback_);
	~WASAPI();

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

	void thread_runner() noexcept;

	std::thread thr;
	std::atomic<bool> dead;

	IMMDeviceEnumerator *pEnumerator = nullptr;
	IMMDevice *pDevice = nullptr;
	IAudioClient *pAudioClient = nullptr;
	IAudioRenderClient *pRenderClient = nullptr;
	UINT32 buffer_frames = 0;
	WAVEFORMATEX *format = nullptr;
	bool is_active = false;
	HANDLE audio_event = nullptr;

	bool kick_start() noexcept;

	bool get_write_avail(UINT32 &avail) noexcept;
	bool get_write_avail_blocking(UINT32 &avail) noexcept;
};

using AudioBackend = WASAPI;
