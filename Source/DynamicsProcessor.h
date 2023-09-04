#pragma once
#include <array>
#include "Lowpass.h"

namespace dsp
{
	static constexpr float MinDb = -240.f;

	inline float dbToGain(float db) noexcept
	{
		return std::pow(10.f, db / 20.f);
	}

	inline float gainToDb(float gain) noexcept
	{
		return 20.f * std::log10(gain);
	}

	struct LevelDetector
	{
		LevelDetector() :
			buffer(),
			lp(MinDb),
			sampleRate(1.f),
			atkX(0.f),
			rlsX(0.f),
			env(MinDb)
		{
		}

		void prepare(float _sampleRate, int blockSize) noexcept
		{
			sampleRate = _sampleRate;
			buffer.resize(blockSize);
		}

		void operator()(const float* smpls,
			float atkMs, float rlsMs,
			int numSamples) noexcept
		{
			atkX = lp.getXFromMs(atkMs, sampleRate);
			rlsX = lp.getXFromMs(rlsMs, sampleRate);

			for (auto s = 0; s < numSamples; ++s)
				buffer[s] = synthesize(smpls[s]);
		}

		const float* data() const noexcept
		{
			return buffer.data();
		}

	protected:
		std::vector<float> buffer;
		LowpassF lp;
		float sampleRate, atkX, rlsX, env;

		float synthesize(float smpl) noexcept
		{
			const auto rect = std::abs(smpl);
			const auto rectDb = rect == 0.f ? MinDb : gainToDb(rect);

			if (env < rectDb)
				lp.setX(atkX);
			else
				lp.setX(rlsX);

			env = lp(rectDb);

			return env;
		}
	};

	namespace transferFunc
	{
		inline float downwardsExpander(float x, float threshold, float ratio, float knee) noexcept
		{
			const auto k2 = knee * .5f;
			const auto t0 = threshold - k2;

			if(x < t0)
				return ratio * (x - threshold) + threshold;

			const auto t1 = threshold + k2;

			if(x > t1)
				return x;

			auto x0 = (x - t1);
			x0 *= x0;
			const auto m = -(ratio - 1.f) / (2.f * knee);

			return x + m * x0;
		}

		inline float computeGainDb(float transferCurve, float level) noexcept
		{
			return transferCurve - level;
		}

		inline float computeGainDbDownwardsExpander(float level, float threshold, float ratio, float knee) noexcept
		{
			const auto tc = downwardsExpander(level, threshold, ratio, knee);
			return computeGainDb(tc, level);
		}
	}

	struct DynamicsProcessorMono
	{
		DynamicsProcessorMono() :
			lvlDetector()
		{}

		void prepare(float sampleRate, int blockSize)
		{
			lvlDetector.prepare(sampleRate, blockSize);
		}

		void operator()(float* smpls,
			float thresholdDb, float ratioDb, float kneeDb,
			float attackMs, float releaseMs,
			int numSamples) noexcept
		{
			lvlDetector(smpls, attackMs, releaseMs, numSamples);
			auto lvlData = lvlDetector.data();

			for (auto s = 0; s < numSamples; ++s)
			{
				const auto dry = smpls[s];
				const auto lvl = lvlData[s];
				const auto gainDownwardsExpanderDb = transferFunc::computeGainDbDownwardsExpander(lvl, thresholdDb, ratioDb, kneeDb);
				const auto gain = dbToGain(gainDownwardsExpanderDb);
				const auto wet = dry * gain;
				smpls[s] = wet;
			}
		}

		LevelDetector lvlDetector;
	};

	struct DynamicsProcessor
	{
		DynamicsProcessor() :
			dyns()
		{}

		void prepare(float sampleRate, int blockSize)
		{
			for(auto& dyn : dyns)
				dyn.prepare(sampleRate, blockSize);
		}

		void operator()(float* const* samples,
			float thresholdDb, float ratioDb, float kneeDb,
			float attackMs, float releaseMs, float makeupDb,
			int numChannels, int numSamples) noexcept
		{
			for(auto ch = 0; ch < numChannels; ++ch)
				dyns[ch](samples[ch], thresholdDb, ratioDb, kneeDb, attackMs, releaseMs, numSamples);

			const auto makeUp = dbToGain(makeupDb);
			for(auto ch = 0; ch < numChannels; ++ch)
				for(auto s = 0; s < numSamples; ++s)
					samples[ch][s] *= makeUp;
		}

		std::array<DynamicsProcessorMono, 2> dyns;
	};
}

/*

Downwards Expander:
https://www.desmos.com/calculator/rsolqzzufh

Upwards Expander:
https://www.desmos.com/calculator/wcly80brku

Downwards Compressor:
https://www.desmos.com/calculator/2gmfwaeyu9

Upwards Compressor:
https://www.desmos.com/calculator/lvzrunltb6

*/