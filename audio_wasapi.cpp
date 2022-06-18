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

#include "audio_wasapi.hpp"
#include "dsp.hpp"
#include <algorithm>

static const size_t MAX_NUM_FRAMES = 256;

// Doesn't link properly on MinGW.
const static GUID _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
	0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

WASAPI::WASAPI(BackendCallback *callback_)
	: callback(callback_)
{
	audio_event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
}

WASAPI::~WASAPI()
{
	stop();
	if (format)
		CoTaskMemFree(format);
	if (pRenderClient)
		pRenderClient->Release();
	if (pAudioClient)
		pAudioClient->Release();
	if (pDevice)
		pDevice->Release();
	if (pEnumerator)
		pEnumerator->Release();
	if (audio_event)
		CloseHandle(audio_event);
}

static REFERENCE_TIME seconds_to_reference_time(double t)
{
	return REFERENCE_TIME(t * 10000000.0 + 0.5);
}

bool WASAPI::init(float, unsigned channels_)
{
	if (FAILED(CoInitialize(nullptr)))
		return false;

	if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
	                            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
	                            reinterpret_cast<void **>(&pEnumerator))))
	{
		return false;
	}

	if (FAILED(pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice)))
		return false;

	if (FAILED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
	                             reinterpret_cast<void **>(&pAudioClient))))
	{
		return false;
	}

	if (FAILED(pAudioClient->GetMixFormat(&format)))
		return false;

	if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
		return false;

	auto *ex = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(format);
	if (ex->SubFormat != _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT || format->wBitsPerSample != 32)
		return false;

	format->nChannels = WORD(channels_);
	channels = channels_;
	sample_rate = float(format->nSamplesPerSec);

	const double target_latency = 0.020;
	auto reference_time = seconds_to_reference_time(target_latency);

	if (FAILED(pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
	                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
	                                    reference_time, 0, format, nullptr)))
	{
		return false;
	}

	if (FAILED(pAudioClient->SetEventHandle(audio_event)))
		return false;

	if (FAILED(pAudioClient->GetBufferSize(&buffer_frames)))
		return false;

	if (FAILED(pAudioClient->GetService(__uuidof(IAudioRenderClient),
	                                    reinterpret_cast<void **>(&pRenderClient))))
	{
		return false;
	}

	if (callback)
		callback->set_backend_parameters(get_sample_rate(), get_num_channels(), MAX_NUM_FRAMES);

	return true;
}

bool WASAPI::start()
{
	if (is_active)
		return false;
	is_active = true;
	dead = false;

	if (callback)
	{
		callback->on_backend_start();
		thr = std::thread(&WASAPI::thread_runner, this);
	}
	else
	{
		if (!kick_start())
			return false;
	}

	return true;
}

bool WASAPI::stop()
{
	if (!is_active)
		return false;
	is_active = false;

	if (thr.joinable())
	{
		dead.store(true, std::memory_order_relaxed);
		SetEvent(audio_event);
		thr.join();
	}

	if (callback)
		callback->on_backend_stop();
	return true;
}

bool WASAPI::kick_start() noexcept
{
	BYTE *interleaved = nullptr;
	if (FAILED(pRenderClient->GetBuffer(buffer_frames, &interleaved)))
		return false;

	if (FAILED(pRenderClient->ReleaseBuffer(buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT)))
		return false;

	if (FAILED(pAudioClient->Start()))
		return false;

	return true;
}

bool WASAPI::get_write_avail(UINT32 &avail) noexcept
{
	UINT32 padding;
	if (FAILED(pAudioClient->GetCurrentPadding(&padding)))
		return false;

	avail = buffer_frames - padding;
	return true;
}

bool WASAPI::get_write_avail_blocking(UINT32 &avail) noexcept
{
	if (!get_write_avail(avail))
		return false;

	while (!dead.load(std::memory_order_relaxed) && avail == 0)
	{
		DWORD ret = WaitForSingleObject(audio_event, INFINITE);

		if (ret == WAIT_OBJECT_0)
		{
			if (!get_write_avail(avail))
				return false;
		}
		else if (ret == WAIT_TIMEOUT)
			return false;
		else
			return false;
	}

	return avail != 0;
}

void WASAPI::thread_runner() noexcept
{
	DWORD task_index = 0;
	HANDLE audio_task = AvSetMmThreadCharacteristicsA("Pro Audio", &task_index);

	if (!kick_start())
	{
		if (audio_task)
			AvRevertMmThreadCharacteristics(audio_task);
		return;
	}

	float mix_channels[MaxChannels][MAX_NUM_FRAMES];
	float *mix_channel_ptr[MaxChannels];
	for (unsigned i = 0; i < format->nChannels; i++)
		mix_channel_ptr[i] = mix_channels[i];

	while (!dead.load(std::memory_order_relaxed))
	{
		UINT32 write_avail = 0;
		if (!get_write_avail_blocking(write_avail))
			break;

		float *interleaved = nullptr;
		if (FAILED(pRenderClient->GetBuffer(write_avail, reinterpret_cast<BYTE **>(&interleaved))))
			break;

		UINT32 to_release = write_avail;

		while (write_avail != 0)
		{
			size_t to_write = std::min<size_t>(write_avail, MAX_NUM_FRAMES);
			callback->mix_samples(mix_channel_ptr, to_write);
			write_avail -= to_write;

			DSP::interleave_stereo_f32(interleaved, mix_channels[0], mix_channels[1], to_write);
			interleaved += to_write * format->nChannels;
		}

		if (FAILED(pRenderClient->ReleaseBuffer(to_release, 0)))
			break;
	}

	if (audio_task)
		AvRevertMmThreadCharacteristics(audio_task);

	if (FAILED(pAudioClient->Stop()))
		return;

	if (FAILED(pAudioClient->Reset()))
		return;
}
