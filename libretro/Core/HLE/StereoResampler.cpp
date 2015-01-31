// Copyright (c) 2015- PPSSPP Project and Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

// Adapted from Dolphin.

#include <string.h>

#include "base/logging.h"
#include "Common/ChunkFile.h"
#include "Common/MathUtil.h"
#include "Common/Atomics.h"
#include "Core/HW/StereoResampler.h"
#include "Globals.h"

#ifdef _M_SSE
#include <emmintrin.h>
#endif

StereoResampler::StereoResampler()
	: m_dma_mixer(this, 44100)
{ }

inline void ClampBufferToS16(s16 *out, const s32 *in, size_t size) {
#ifdef _M_SSE
	// Size will always be 16-byte aligned as the hwBlockSize is.
	while (size >= 8) {
		__m128i in1 = _mm_loadu_si128((__m128i *)in);
		__m128i in2 = _mm_loadu_si128((__m128i *)(in + 4));
		__m128i packed = _mm_packs_epi32(in1, in2);
		_mm_storeu_si128((__m128i *)out, packed);
		out += 8;
		in += 8;
		size -= 8;
	}
	for (size_t i = 0; i < size; i++) {
		out[i] = clamp_s16(in[i]);
	}
#else
	for (size_t i = 0; i < size; i++) {
		out[i] = clamp_s16(in[i]);
	}
#endif
}

void StereoResampler::MixerFifo::Clear() {
	memset(m_buffer, 0, sizeof(m_buffer));
}

// Executed from sound stream thread
unsigned int StereoResampler::MixerFifo::Mix(short* samples, unsigned int numSamples, bool consider_framelimit, int sample_rate) {
	unsigned int currentSample = 0;

	// Cache access in non-volatile variable
	// This is the only function changing the read value, so it's safe to
	// cache it locally although it's written here.
	// The writing pointer will be modified outside, but it will only increase,
	// so we will just ignore new written data while interpolating.
	// Without this cache, the compiler wouldn't be allowed to optimize the
	// interpolation loop.
	u32 indexR = Common::AtomicLoad(m_indexR);
	u32 indexW = Common::AtomicLoad(m_indexW);

	// We force on the audio resampler if the output sample rate doesn't match the input.
   for (; currentSample < numSamples * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2) {
      u32 indexR2 = indexR + 2; //next sample
      s16 l1 = m_buffer[indexR & INDEX_MASK]; //current
      s16 r1 = m_buffer[(indexR + 1) & INDEX_MASK]; //current
      samples[currentSample] = l1;
      samples[currentSample + 1] = r1;
      indexR += 2;
   }
   aid_sample_rate_ = sample_rate;

	int realSamples = currentSample;

	if (currentSample < numSamples * 2)
		underrunCount_++;

	// Padding with the last value to reduce clicking
	short s[2];
	s[0] = clamp_s16(m_buffer[(indexR - 1) & INDEX_MASK]);
	s[1] = clamp_s16(m_buffer[(indexR - 2) & INDEX_MASK]);
	for (; currentSample < numSamples * 2; currentSample += 2) {
		samples[currentSample] = s[0];
		samples[currentSample + 1] = s[1];
	}

   // Flush cached variable
   Common::AtomicStore(m_indexR, indexR);

	//if (realSamples != numSamples * 2) {
	//	ILOG("Underrun! %i / %i", realSamples / 2, numSamples);
	//}
	lastBufSize_ = (m_indexW - m_indexR) & INDEX_MASK;

	return realSamples / 2;
}

unsigned int StereoResampler::Mix(short* samples, unsigned int num_samples, bool consider_framelimit, int sample_rate) {
	if (!samples)
		return 0;

	return m_dma_mixer.Mix(samples, num_samples, consider_framelimit, sample_rate);
}

void StereoResampler::MixerFifo::PushSamples(const s32 *samples, unsigned int num_samples) {
	// Cache access in non-volatile variable
	// indexR isn't allowed to cache in the audio throttling loop as it
	// needs to get updates to not deadlock.
	u32 indexW = Common::AtomicLoad(m_indexW);

	// Check if we have enough free space
	// indexW == m_indexR results in empty buffer, so indexR must always be smaller than indexW
	if (num_samples * 2 + ((indexW - Common::AtomicLoad(m_indexR)) & INDEX_MASK) >= MAX_SAMPLES * 2)
		return;

	// AyuanX: Actual re-sampling work has been moved to sound thread
	// to alleviate the workload on main thread
	// and we simply store raw data here to make fast mem copy
	int over_bytes = num_samples * 4 - (MAX_SAMPLES * 2 - (indexW & INDEX_MASK)) * sizeof(short);
	if (over_bytes > 0) {
		ClampBufferToS16(&m_buffer[indexW & INDEX_MASK], samples, (num_samples * 4 - over_bytes) / 2);
		ClampBufferToS16(&m_buffer[0], samples + (num_samples * 4 - over_bytes) / sizeof(short), over_bytes / 2);
	} else {
		ClampBufferToS16(&m_buffer[indexW & INDEX_MASK], samples, num_samples * 2);
	}

	Common::AtomicAdd(m_indexW, num_samples * 2);
}

void StereoResampler::PushSamples(const int *samples, unsigned int num_samples) {
	m_dma_mixer.PushSamples(samples, num_samples);
}

void StereoResampler::MixerFifo::SetInputSampleRate(unsigned int rate) {
	m_input_sample_rate = rate;
}

void StereoResampler::DoState(PointerWrap &p) {
	auto s = p.Section("resampler", 1);
	if (!s)
		return;
}

void StereoResampler::MixerFifo::GetAudioDebugStats(AudioDebugStats *stats) {
}

void StereoResampler::GetAudioDebugStats(AudioDebugStats *stats) {
}
