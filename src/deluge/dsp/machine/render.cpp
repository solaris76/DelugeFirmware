#include "dsp/machine/render.h"
#include "util/functions.h"
#include "util/waves.h"
#include <algorithm>
#include <cmath>

namespace deluge::dsp::machine {

namespace {

constexpr float kInv127 = 1.f / 127.f;

inline float u8f(uint8_t v) {
	return static_cast<float>(v) * kInv127;
}

// Drum-layer decay: dial 1–50 covers short/snappy hits; 50–127 stretches into longer tails.
// Returns per-sample exponential coefficient for env *= (1 - coef).
inline float drumDecayCoef(uint8_t v) {
	float x = std::max(1, static_cast<int>(v)) * kInv127;
	// ~4ms at 1, ~80ms at 32, ~180ms at 50, ~1.5s at 127 (approx)
	float tauSamples = 180.f + std::pow(x, 2.2f) * 66000.f;
	return 1.f / tauSamples;
}

// Pitch-env time similarly biased short (kick bend / snare snap)
inline float drumPitchEnvCoef(uint8_t v) {
	float x = std::max(1, static_cast<int>(v)) * kInv127;
	float tauSamples = 90.f + std::pow(x, 2.0f) * 22000.f;
	return 1.f / tauSamples;
}

inline uint32_t nextNoise(uint32_t& state) {
	state = state * 1664525u + 1013904223u;
	return state;
}

inline int32_t noiseSample(uint32_t& state) {
	return static_cast<int32_t>(nextNoise(state) >> 1) - 0x40000000;
}

// Soft morph between sine / triangle / saw / square using waveIndex 0–120
inline int32_t morphWave(uint32_t phase, uint8_t waveIndex, uint8_t phaseDist) {
	float t = std::clamp(waveIndex / 120.f, 0.f, 1.f);
	// Phase distortion: squeeze toward 0 or 1
	float pd = (static_cast<float>(phaseDist) - 50.f) / 50.f; // -1..1
	uint32_t ph = phase;
	if (pd > 0.01f) {
		float x = static_cast<float>(phase) * (1.f / 4294967296.f);
		x = std::pow(x, 1.f + pd * 2.f);
		ph = static_cast<uint32_t>(x * 4294967296.f);
	}
	else if (pd < -0.01f) {
		float x = static_cast<float>(phase) * (1.f / 4294967296.f);
		x = 1.f - std::pow(1.f - x, 1.f - pd * 2.f);
		ph = static_cast<uint32_t>(x * 4294967296.f);
	}

	int32_t sine = getSine(ph);
	int32_t tri;
	{
		uint32_t p = ph >> 1;
		tri = (ph < 0x80000000u) ? static_cast<int32_t>(p) - 0x40000000
		                         : 0x3FFFFFFF - static_cast<int32_t>(p - 0x40000000);
	}
	int32_t saw = static_cast<int32_t>(ph >> 1) - 0x40000000;
	int32_t sqr = getSquare(ph);

	if (t < 0.33f) {
		float u = t / 0.33f;
		return static_cast<int32_t>(sine * (1.f - u) + tri * u);
	}
	if (t < 0.66f) {
		float u = (t - 0.33f) / 0.33f;
		return static_cast<int32_t>(tri * (1.f - u) + saw * u);
	}
	float u = (t - 0.66f) / 0.34f;
	return static_cast<int32_t>(saw * (1.f - u) + sqr * u);
}

inline float envFollow(float& env, float target, float atkCoef, float decCoef) {
	if (target > env) {
		env += (target - env) * atkCoef;
	}
	else {
		env += (target - env) * decCoef;
	}
	return env;
}

// Convert a full-scale sine into a phase offset. index ~0.5 is mild FM; ~2 is bright.
inline uint32_t phaseMod(int32_t sample, float index) {
	if (index <= 0.f) {
		return 0;
	}
	// sample ≈ ±2^30 → scale so index 1.0 ≈ ±1/8 cycle
	return static_cast<uint32_t>(static_cast<int32_t>(sample * (index * 0.125f)));
}

// Digitone-ish carrier mix after operators have already been FM'd.
// op: 0=A, 1=B1, 2=B2, 3=C (audio-rate, post-modulation)
inline int32_t mixFmCarriers(int32_t op[4], uint8_t algo, uint8_t mix) {
	int32_t x = 0;
	int32_t y = 0;
	switch (algo & 7) {
	case 0: // single carrier C
		x = op[3];
		y = op[3];
		break;
	case 1: // C + B1
		x = op[3];
		y = op[1];
		break;
	case 2: // C + A
		x = op[3];
		y = op[0];
		break;
	case 3:
		x = (op[3] >> 1) + (op[1] >> 2);
		y = op[0] >> 1;
		break;
	case 4:
		x = op[3];
		y = (op[1] >> 1) + (op[2] >> 2);
		break;
	case 5:
		x = (op[3] >> 1) + (op[0] >> 2);
		y = (op[1] >> 1) + (op[2] >> 2);
		break;
	case 6:
		x = (op[3] >> 1) + (op[0] >> 1);
		y = op[1] >> 1;
		break;
	default:
		x = (op[3] >> 1) + (op[0] >> 2);
		y = (op[1] >> 1) + (op[2] >> 2);
		break;
	}
	int32_t m = static_cast<int32_t>(mix);
	return ((x * (128 - m)) + (y * m)) >> 7;
}

} // namespace

float ratioFromIndexC(uint8_t idx) {
	static constexpr float kRatios[] = {0.25f, 0.5f, 0.75f, 1.f,  2.f,  3.f,  4.f,  5.f,  6.f, 7.f,
	                                    8.f,   9.f,  10.f,  11.f, 12.f, 13.f, 14.f, 15.f, 16.f};
	idx = std::min<uint8_t>(idx, 18);
	return kRatios[idx];
}

float ratioFromIndexA(uint8_t idx) {
	static constexpr float kRatios[] = {0.25f, 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 1.75f, 2.f,  2.25f, 2.5f, 2.75f, 3.f,
	                                    3.25f, 3.5f, 3.75f, 4.f, 4.25f, 4.5f, 4.75f, 5.f,  5.5f,  6.f,  6.5f,  7.f,
	                                    7.5f,  8.f,  8.5f,  9.f, 9.5f,  10.f, 11.f,  12.f, 13.f,  14.f, 15.f,  16.f};
	idx = std::min<uint8_t>(idx, 35);
	return kRatios[idx];
}

void unpackRatioB(uint8_t dial, float& b1, float& b2) {
	// Approximate Digitone revolving B1/B2 dial
	int v = dial;
	int step = v / 16;
	int fine = v % 16;
	b1 = ratioFromIndexC(std::min(step, 18));
	b2 = ratioFromIndexC(std::min(fine, 18));
}

void renderFmTone(FmTonePatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	if (timeScale < 0.15f) {
		timeScale = 0.15f;
	}
	// Single Ratio dial: C primary; A/B derived for width.
	float rC = ratioFromIndexC(std::min<uint8_t>(patch.ratio, 18));
	float rA = ratioFromIndexA(std::min<uint8_t>(static_cast<uint8_t>(patch.ratio + patch.harmonics / 8), 35));
	float rB1, rB2;
	unpackRatioB(static_cast<uint8_t>((patch.ratio * 7 + patch.detune) & 127), rB1, rB2);

	float det = u8f(patch.detune) * 0.02f;
	rA *= (1.f + det);
	rB2 *= (1.f - det);

	uint32_t incC = static_cast<uint32_t>(phaseIncrement * rC);
	uint32_t incA = static_cast<uint32_t>(phaseIncrement * rA);
	uint32_t incB1 = static_cast<uint32_t>(phaseIncrement * rB1);
	uint32_t incB2 = static_cast<uint32_t>(phaseIncrement * rB2);

	float modLev = 0.15f + u8f(patch.harmonics) * 0.85f;
	float dec = (0.002f + u8f(patch.decay) * 0.1f) * timeScale;
	float fb = u8f(patch.feedback) * 0.7f;
	float baseIndex = 0.15f + u8f(patch.harmonics) * 1.35f;

	if (st.noteOn && st.sampleCount == 0) {
		st.phase[0] = st.phase[1] = st.phase[2] = st.phase[3] = 0;
		st.envA = 0.f;
		st.envB = 0.f;
	}

	int32_t amp = amplitude;
	int32_t feedbackMem = 0;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		float target = st.noteOn ? modLev : 0.f;
		envFollow(st.envA, target, 1.f / (1.f + 8.f), 1.f / (1.f + dec * 4000.f));
		envFollow(st.envB, target * 0.85f, 1.f / (1.f + 10.f), 1.f / (1.f + dec * 4500.f));

		float idxA = baseIndex * st.envA;
		float idxB = baseIndex * st.envB;

		int32_t opA = getSine(st.phase[0]);
		int32_t opB2 = getSine(st.phase[2]);
		st.phase[2] += incB2;
		int32_t opB1 = getSine(st.phase[1] + phaseMod(opB2, idxB));
		st.phase[1] += incB1;

		uint32_t mod = phaseMod(opA, idxA) + phaseMod(opB1, idxB) + phaseMod(feedbackMem, fb);
		int32_t opC = getSine(st.phase[3] + mod);
		st.phase[0] += incA;
		st.phase[3] += incC;
		feedbackMem = opC;

		int32_t op[4] = {opA, opB1, opB2, opC};
		int32_t sample = mixFmCarriers(op, patch.algorithm, patch.mix);

		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 4;
	}
}

void renderFmDrum(FmDrumPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	if (timeScale < 0.15f) {
		timeScale = 0.15f;
	}
	float tuneMul = 0.05f + u8f(patch.tune) * 1.7f;
	uint32_t baseInc = static_cast<uint32_t>(phaseIncrement * tuneMul);

	float sweepAmt = u8f(patch.sweep);
	float sweepDepth = sweepAmt * 3.0f;
	uint8_t sweepTime = static_cast<uint8_t>(12 + (1.f - sweepAmt) * 50.f + sweepAmt * 70.f);
	float bodyDecCoef = drumDecayCoef(patch.decay) / timeScale;
	float noiseDecCoef = drumDecayCoef(static_cast<uint8_t>(8 + patch.noise / 2)) / timeScale;
	float sweepCoef = drumPitchEnvCoef(sweepTime) / timeScale;

	float modAmt = u8f(patch.mod);
	float modA = modAmt * 1.1f;
	float modB = modAmt * 0.75f;
	float rA = 0.6f + modAmt * 5.5f;
	float rB = 1.2f + modAmt * 4.0f;
	// Algo nudges ratio flavour
	rA *= 1.f + (patch.algorithm % 3) * 0.15f;
	rB *= 1.f + ((patch.algorithm / 2) % 3) * 0.12f;

	float fold = u8f(patch.fold);
	float fb = fold * 0.35f;
	float noiseAmt = u8f(patch.noise);

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		st.envA = 1.f;
		st.envB = 1.f;
		st.phase[3] = 0;
		st.clickSamplesLeft = 10 + static_cast<uint32_t>(noiseAmt * 50.f);
	}

	int32_t amp = amplitude;
	int32_t feedbackMem = 0;
	float modDec = drumDecayCoef(static_cast<uint8_t>(20 + patch.decay / 2)) / timeScale;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		st.bodyEnv *= (1.f - bodyDecCoef);
		st.noiseEnv *= (1.f - noiseDecCoef);
		st.envA *= (1.f - modDec);
		st.envB *= (1.f - modDec * 1.15f);
		st.pitchEnv *= (1.f - sweepCoef);

		float pitchMul = 1.f + sweepDepth * st.pitchEnv;
		uint32_t inc = static_cast<uint32_t>(baseInc * pitchMul);
		uint32_t incA = static_cast<uint32_t>(inc * rA);
		uint32_t incB = static_cast<uint32_t>(inc * rB);

		int32_t mA = getSine(st.phase[0]);
		int32_t mB = getSine(st.phase[1]);
		st.phase[0] += incA;
		st.phase[1] += incB;

		uint32_t mod = phaseMod(mA, modA * st.envA) + phaseMod(mB, modB * st.envB) + phaseMod(feedbackMem, fb);
		int32_t body = getSine(st.phase[3] + mod);
		st.phase[3] += inc;
		feedbackMem = body;
		if (fold > 0.01f) {
			float f = body * (1.f / 2147483648.f);
			f = std::tanh(f * (1.f + fold * 7.f));
			body = static_cast<int32_t>(f * 2147483647.f);
		}
		body = static_cast<int32_t>(body * st.bodyEnv);

		int32_t noise = noiseSample(st.noiseState);
		noise = static_cast<int32_t>(noise * st.noiseEnv * noiseAmt * 0.45f);

		int32_t click = 0;
		if (st.clickSamplesLeft > 0) {
			click = noiseSample(st.noiseState) >> 3;
			click = static_cast<int32_t>(click * noiseAmt);
			st.clickSamplesLeft--;
		}

		int32_t sample = body + noise + click;
		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 4;
	}
}

void renderWaveTone(WaveTonePatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                    uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	if (timeScale < 0.15f) {
		timeScale = 0.15f;
	}
	float off1 = 1.f + (static_cast<int>(patch.osc1LinOffset) - 64) * 0.0015f;
	float off2 = 1.f + (static_cast<int>(patch.osc2LinOffset) - 64) * 0.0015f;
	uint32_t inc1 = static_cast<uint32_t>(phaseIncrement * off1);
	uint32_t inc2 = static_cast<uint32_t>(phaseIncrement * off2);
	if (patch.oscDrift) {
		inc2 += patch.oscDrift << 8;
	}

	if (st.sampleCount == 0 && patch.phaseReset == 1) {
		st.phase[0] = st.phase[1] = 0;
	}
	else if (st.sampleCount == 0 && patch.phaseReset == 2) {
		st.phase[0] = nextNoise(st.noiseState);
		st.phase[1] = nextNoise(st.noiseState);
	}

	float nDec = (0.002f + u8f(patch.noiseDec) * 0.1f) / timeScale;
	if (st.sampleCount == 0) {
		st.noiseEnv = 1.f;
	}

	int32_t amp = amplitude;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		int32_t o1 = morphWave(st.phase[0], patch.osc1Wave, patch.osc1PhaseDist);
		int32_t o2 = morphWave(st.phase[1], patch.osc2Wave, patch.osc2PhaseDist);
		st.phase[0] += inc1;

		if (patch.oscMod == 3) { // hard sync
			st.phase[1] += inc2;
			if (st.phase[0] < inc1) {
				st.phase[1] = 0;
			}
		}
		else {
			st.phase[1] += inc2;
		}

		int32_t mix;
		if (patch.oscMod == 1 || patch.oscMod == 2) {
			mix = multiply_32x32_rshift32(o1, o2) << 1;
			if (patch.oscMod == 1) {
				mix = (mix >> 1) + (o1 >> 1);
			}
			mix = static_cast<int32_t>((static_cast<int64_t>(mix) * (patch.osc1Level + patch.osc2Level)) / 254);
		}
		else {
			mix = static_cast<int32_t>(
			    (static_cast<int64_t>(o1) * patch.osc1Level + static_cast<int64_t>(o2) * patch.osc2Level) / 254);
		}

		if (patch.noiseLevel) {
			st.noiseEnv *= (1.f - nDec * 0.15f);
			int32_t n = noiseSample(st.noiseState);
			if (patch.noiseType == 1) {
				n = getSine(st.phase[2]);
				st.phase[2] += inc1 + (patch.noiseCharacter << 12);
			}
			else if (patch.noiseType == 2) {
				if ((st.sampleCount & 0x3F) == 0) {
					st.phase[2] = static_cast<uint32_t>(n);
				}
				n = static_cast<int32_t>(st.phase[2]) - 0x40000000;
			}
			else if (patch.noiseCharacter > 20) {
				if ((nextNoise(st.noiseState) & 0xFF) < patch.noiseCharacter) {
					n = 0;
				}
			}
			n = static_cast<int32_t>(n * st.noiseEnv * u8f(patch.noiseLevel) * 0.3f);
			mix += n;
		}

		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(mix, amp) << 4;
	}
}

void renderPerc(PercPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	if (timeScale < 0.15f) {
		timeScale = 0.15f;
	}
	auto role = static_cast<PercRole>(patch.role);

	float color = u8f(patch.color);
	float pitchMul = 0.55f + u8f(patch.pitch) * 1.45f;
	float bodyDecCoef = drumDecayCoef(patch.decay) / timeScale;
	float noiseDecCoef = drumDecayCoef(static_cast<uint8_t>(10 + patch.noise / 2)) / timeScale;
	float crunch = u8f(patch.crunch);
	float drive = 1.f + crunch * 4.5f;
	float noiseAmt = u8f(patch.noise);

	float ratios[6] = {1.f,
	                   1.34f + color * 0.3f,
	                   1.72f + color * 0.45f,
	                   2.05f + color * 0.55f,
	                   2.43f + color * 0.7f,
	                   2.61f + color * 0.85f};
	float noiseBias = 0.35f;
	float bodyBias = 1.f;
	uint8_t waveIdx = static_cast<uint8_t>(70 + color * 50.f);
	float fmIndex = 0.f;
	float beatAmt = color * 0.008f;
	bool useXor = false;
	bool useGrains = false;
	int activePartials = 4;

	switch (role) {
	case PercRole::Metal:
		activePartials = 6;
		noiseBias = 0.25f;
		break;
	case PercRole::Bell:
		activePartials = 2;
		ratios[1] = 1.48f + color * 0.2f;
		noiseBias = 0.08f;
		bodyBias = 1.15f;
		break;
	case PercRole::Hat808:
		activePartials = 6;
		noiseBias = 0.9f;
		bodyBias = 0.8f;
		break;
	case PercRole::FM:
		activePartials = 2;
		fmIndex = 0.8f + color * 3.5f;
		noiseBias = 0.2f;
		waveIdx = 110;
		break;
	case PercRole::XOR:
		activePartials = 4;
		useXor = true;
		noiseBias = 0.3f;
		break;
	case PercRole::Grains:
		useGrains = true;
		activePartials = 0;
		noiseBias = 1.4f;
		bodyBias = 0.f;
		break;
	default:
		break;
	}

	uint32_t baseInc = static_cast<uint32_t>(static_cast<float>(phaseIncrement) * pitchMul);
	float pEnvAmt = 0.3f + color * 0.9f;
	float pitchEnvCoef = drumPitchEnvCoef(static_cast<uint8_t>(12 + patch.decay / 4)) / timeScale;

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		st.envA = 0.f;
		for (auto& p : st.phase) {
			p = 0;
		}
		st.phase[1] = 0x20000000u;
		st.clickSamplesLeft = 6 + static_cast<uint32_t>(crunch * 60.f);
	}

	int32_t amp = amplitude;
	uint32_t grainPeriod = 40 + static_cast<uint32_t>((1.f - u8f(patch.pitch)) * 280.f);
	uint32_t grainLen = 20 + static_cast<uint32_t>(color * 90.f);

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		st.bodyEnv *= (1.f - bodyDecCoef);
		st.noiseEnv *= (1.f - noiseDecCoef);
		st.pitchEnv *= (1.f - pitchEnvCoef);

		float pitchScale = 1.f + pEnvAmt * st.pitchEnv;
		int32_t body = 0;

		if (useGrains) {
			if ((st.sampleCount % grainPeriod) == 1) {
				st.envA = 1.f;
			}
			st.envA *= (1.f - (1.f / static_cast<float>(std::max<uint32_t>(8, grainLen))));
		}
		else if (role == PercRole::FM) {
			uint32_t inc0 = static_cast<uint32_t>(static_cast<float>(baseInc) * pitchScale);
			uint32_t inc1 = static_cast<uint32_t>(static_cast<float>(baseInc) * ratios[1] * pitchScale);
			int32_t mod = getSquare(st.phase[1]);
			st.phase[1] += inc1;
			body = getSquare(st.phase[0] + phaseMod(mod, fmIndex * st.bodyEnv));
			st.phase[0] += inc0;
		}
		else if (useXor) {
			int32_t x = 0;
			for (int p = 0; p < activePartials; p++) {
				float det = (p == 1) ? (1.f + beatAmt) : 1.f;
				uint32_t inc = static_cast<uint32_t>(static_cast<float>(baseInc) * ratios[p] * pitchScale * det);
				int32_t s = getSquare(st.phase[p]);
				st.phase[p] += inc;
				x ^= (s >> 31);
			}
			body = x ? 0x3FFFFFFF : static_cast<int32_t>(0xC0000000);
		}
		else {
			for (int p = 0; p < activePartials; p++) {
				float det = (p == 1) ? (1.f + beatAmt) : 1.f;
				uint32_t inc = static_cast<uint32_t>(static_cast<float>(baseInc) * ratios[p] * pitchScale * det);
				int32_t s = (color > 0.55f) ? getSquare(st.phase[p]) : morphWave(st.phase[p], waveIdx, 50);
				st.phase[p] += inc;
				body += static_cast<int32_t>(s * (1.f / static_cast<float>(p + 1)));
			}
		}

		body = static_cast<int32_t>(body * st.bodyEnv * bodyBias);

		int32_t noise = noiseSample(st.noiseState);
		// Always HP-ish for metallic air
		int32_t prev = static_cast<int32_t>(st.phase[5]);
		int32_t hp = noise - prev;
		st.phase[5] = static_cast<uint32_t>(noise);
		noise = static_cast<int32_t>(noise * 0.25f + hp * 0.75f);
		float noiseGate = useGrains ? st.envA : st.noiseEnv;
		noise = static_cast<int32_t>(noise * noiseGate * noiseAmt * 0.55f * noiseBias);

		int32_t click = 0;
		if (st.clickSamplesLeft > 0) {
			click = noiseSample(st.noiseState) >> 3;
			click = static_cast<int32_t>(click * crunch);
			st.clickSamplesLeft--;
		}

		float f = (body + noise + click) * (1.f / 2147483648.f);
		f = std::tanh(f * drive);
		int32_t sample = static_cast<int32_t>(f * 2147483647.f);

		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 4;
	}
}

void renderSkin(SkinPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	if (timeScale < 0.15f) {
		timeScale = 0.15f;
	}
	auto mode = static_cast<SkinMode>(patch.mode);

	// Pitch dial covers bass→treble register
	float pitchMul = 0.08f + u8f(patch.pitch) * 1.55f;
	uint32_t baseInc = static_cast<uint32_t>(static_cast<float>(phaseIncrement) * pitchMul);

	float harm = u8f(patch.harm);
	float spread = harm * 0.85f; // spread rides with harm
	float morph = u8f(patch.morph);
	float fold = u8f(patch.fold);
	uint8_t waveIdx = static_cast<uint8_t>(morph * 120.f);

	int nOsc = 1 + static_cast<int>(harm * 5.f + 0.5f);
	if (nOsc > 6) {
		nOsc = 6;
	}

	float bodyDecCoef = drumDecayCoef(patch.decay) / timeScale;
	// Attack character derived from decay: short decay → snappier noise pop
	float atk = 1.f - u8f(patch.decay) * 0.55f;
	float pitchEnvCoef = drumPitchEnvCoef(static_cast<uint8_t>(14 + atk * 40.f)) / timeScale;
	bool usePitchEnv = (mode == SkinMode::Liquid) || (mode == SkinMode::Metal && harm > 0.45f);
	float pitchEnvAmt = (mode == SkinMode::Liquid) ? (1.3f + harm * 1.5f) : (0.45f + spread * 0.9f);

	float noiseAmt = (mode == SkinMode::Metal) ? (0.15f + harm * 0.25f) : (0.05f + (1.f - u8f(patch.decay)) * 0.35f);
	float popAmt = 0.35f + (1.f - u8f(patch.decay)) * 0.5f;

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		for (int o = 0; o < 6; o++) {
			st.phase[o] = static_cast<uint32_t>(o * 0x11111111u);
			float idx = static_cast<float>(o) / 5.f;
			float level = (1.f - idx * 0.75f) * (0.35f + harm * 0.65f);
			if (o >= nOsc) {
				level = 0.f;
			}
			st.oscEnv[o] = level;
		}
		st.clickSamplesLeft = static_cast<uint32_t>(8 + popAmt * 45.f);
	}

	int32_t amp = amplitude;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		st.bodyEnv *= (1.f - bodyDecCoef);
		st.noiseEnv *= (1.f - bodyDecCoef * 1.25f);
		st.pitchEnv *= (1.f - pitchEnvCoef);

		float pitchScale = usePitchEnv ? (1.f + pitchEnvAmt * st.pitchEnv) : 1.f;

		float spreadRatios[6];
		for (int o = 0; o < 6; o++) {
			float harmN = static_cast<float>(o + 1);
			float inharm = 1.f + static_cast<float>(o) * (0.35f + spread * 1.1f);
			spreadRatios[o] = harmN * (1.f - spread) + inharm * spread;
			float decMul = 1.f + static_cast<float>(o) * (0.35f - harm * 0.25f);
			st.oscEnv[o] *= (1.f - bodyDecCoef * decMul);
		}

		int32_t mixed = 0;
		if (mode == SkinMode::Metal) {
			auto chain = [&](int c0, int c1, int c2) -> int32_t {
				uint32_t inc2 = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[c2] * pitchScale);
				uint32_t inc1 = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[c1] * pitchScale);
				uint32_t inc0 = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[c0] * pitchScale);
				int32_t m2 = morphWave(st.phase[c2], waveIdx, 50);
				st.phase[c2] += inc2;
				int32_t m1 = morphWave(st.phase[c1] + phaseMod(m2, 0.6f + spread * 1.8f), waveIdx, 50);
				st.phase[c1] += inc1;
				int32_t c = morphWave(st.phase[c0] + phaseMod(m1, 0.8f + harm * 2.2f), waveIdx, 50);
				st.phase[c0] += inc0;
				return static_cast<int32_t>(c * st.oscEnv[c0] + m1 * st.oscEnv[c1] * 0.35f);
			};
			mixed = chain(0, 1, 2) + chain(3, 4, 5);
		}
		else {
			for (int o = 0; o < nOsc; o++) {
				uint32_t inc = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[o] * pitchScale);
				int32_t s = morphWave(st.phase[o], waveIdx, 50);
				st.phase[o] += inc;
				mixed += static_cast<int32_t>(s * st.oscEnv[o]);
			}
		}

		mixed = static_cast<int32_t>(mixed * st.bodyEnv);

		int32_t noise = noiseSample(st.noiseState);
		noise = static_cast<int32_t>(noise * st.noiseEnv * noiseAmt * 0.45f);

		int32_t pop = 0;
		if (st.clickSamplesLeft > 0) {
			pop = noiseSample(st.noiseState) >> 2;
			pop = static_cast<int32_t>(pop * popAmt);
			st.clickSamplesLeft--;
		}

		float f = (mixed + noise + pop) * (1.f / 2147483648.f);
		if (fold > 0.01f) {
			float gain = 1.f + fold * 8.f;
			f *= gain;
			for (int stage = 0; stage < 1 + static_cast<int>(fold * 3.f); stage++) {
				if (f > 1.f) {
					f = 2.f - f;
				}
				else if (f < -1.f) {
					f = -2.f - f;
				}
			}
			f = std::tanh(f);
		}

		int32_t sample = static_cast<int32_t>(f * 2147483647.f);
		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 4;
	}
}

void renderResonator(ResonatorPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                     uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	if (timeScale < 0.15f) {
		timeScale = 0.15f;
	}
	auto model = static_cast<ResonatorModel>(patch.model);

	float structure = u8f(patch.structure);
	float bright = u8f(patch.brightness);
	float damp = u8f(patch.damping);
	float pos = u8f(patch.position);
	float excite = u8f(patch.excite);

	// Note pitch → fundamental in normalized freq (~0..0.5)
	float f0 = static_cast<float>(phaseIncrement) * (1.f / 4294967296.f);
	f0 = std::clamp(f0, 0.0008f, 0.45f);

	constexpr int kModes = 12;
	float freqs[kModes];
	float qs[kModes];
	float amps[kModes];

	// Structure: 0 ≈ harmonic, 1 ≈ stiff / inharmonic stretch
	float stretch = 1.f;
	float stiff = (structure - 0.5f) * 0.35f;
	for (int i = 0; i < kModes; i++) {
		float n = static_cast<float>(i + 1);
		if (model == ResonatorModel::Modal) {
			freqs[i] = f0 * n * stretch;
			stretch += stiff;
			if (stiff < 0.f) {
				stiff *= 0.93f;
			}
			else {
				stiff *= 0.98f;
			}
		}
		else if (model == ResonatorModel::Strings) {
			// Slightly detuned sympathetic stack
			freqs[i] = f0 * (1.f + i * (0.5f + structure * 0.08f)) * (1.f + (i & 1) * structure * 0.004f);
		}
		else {
			// Wire: odd-ish inharmonic partials
			freqs[i] = f0 * std::pow(n, 1.0f + structure * 0.55f);
		}
		freqs[i] = std::clamp(freqs[i], 0.0005f, 0.48f);

		// Damping high → long ring; Brightness keeps highs alive
		float qBase = 8.f + damp * damp * 420.f;
		float qLoss = 0.55f + bright * 0.4f;
		qs[i] = qBase * std::pow(qLoss, static_cast<float>(i));
		qs[i] = std::clamp(qs[i], 2.f, 800.f);

		// Position: cosine spatial weighting (strike location)
		float ang = 3.14159265f * pos * n;
		amps[i] = std::cos(ang);
		amps[i] *= amps[i]; // energy
		amps[i] *= (1.f - static_cast<float>(i) / static_cast<float>(kModes)) * (0.35f + bright * 0.65f);
	}

	if (st.sampleCount == 0) {
		for (auto& z : st.resZ1) {
			z = 0.f;
		}
		for (auto& z : st.resZ2) {
			z = 0.f;
		}
		for (auto& c : st.combBuf) {
			c = 0.f;
		}
		st.combPos = 0;
		st.clickSamplesLeft = 20 + static_cast<uint32_t>(excite * 80.f);
		st.noiseEnv = 1.f;
		st.bodyEnv = 1.f;
	}

	// Body envelope so it dies with damping even if Q is high
	float bodyDec = drumDecayCoef(static_cast<uint8_t>(20 + damp * 100.f)) / timeScale;
	int32_t amp = amplitude;

	// Comb delay for Strings / Wire
	uint32_t delaySamples = static_cast<uint32_t>(std::clamp(1.f / std::max(f0, 0.001f), 8.f, 255.f));
	float combFb = 0.88f + damp * 0.11f;
	float dispersion = structure * 0.35f;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		st.bodyEnv *= (1.f - bodyDec * 0.35f);

		float exc = 0.f;
		if (st.clickSamplesLeft > 0) {
			exc = (static_cast<float>(noiseSample(st.noiseState)) * (1.f / 2147483648.f)) * excite;
			// Soften into a short burst
			exc *= static_cast<float>(st.clickSamplesLeft) / 100.f;
			st.clickSamplesLeft--;
		}

		float out = 0.f;

		if (model == ResonatorModel::Modal) {
			// Parallel band-pass resonators (simplified SVF-ish one-pole complex)
			for (int m = 0; m < kModes; m++) {
				float w = 2.f * 3.14159265f * freqs[m];
				float r = std::exp(-w / (2.f * qs[m]));
				float a = 2.f * r * std::cos(w);
				// y[n] = x + a*y[n-1] - r^2*y[n-2]
				float y = exc * amps[m] + a * st.resZ1[m] - (r * r) * st.resZ2[m];
				st.resZ2[m] = st.resZ1[m];
				st.resZ1[m] = y;
				out += y;
			}
			out *= 0.45f;
		}
		else {
			// Comb / KS family
			uint16_t readPos = static_cast<uint16_t>((st.combPos + 256 - delaySamples) & 255);
			float delayed = st.combBuf[readPos];
			// Mild averaging = lowpass damping; brightness opens it
			uint16_t read2 = static_cast<uint16_t>((readPos + 1) & 255);
			float lp = delayed * (1.f - bright * 0.55f) + st.combBuf[read2] * (bright * 0.55f);
			if (model == ResonatorModel::Wire) {
				// Soft clip in the loop (dispersion / nonlinearity)
				lp = std::tanh(lp * (1.f + dispersion * 3.f));
			}
			float y = exc + lp * combFb;
			st.combBuf[st.combPos] = y;
			st.combPos = static_cast<uint16_t>((st.combPos + 1) & 255);
			out = y;

			// Blend a couple of modal partials for Strings body
			if (model == ResonatorModel::Strings) {
				for (int m = 0; m < 4; m++) {
					float w = 2.f * 3.14159265f * freqs[m];
					float r = std::exp(-w / (2.f * qs[m]));
					float a = 2.f * r * std::cos(w);
					float ym = exc * amps[m] * 0.35f + a * st.resZ1[m] - (r * r) * st.resZ2[m];
					st.resZ2[m] = st.resZ1[m];
					st.resZ1[m] = ym;
					out += ym * 0.25f;
				}
			}
		}

		out *= st.bodyEnv;
		out = std::tanh(out * 1.4f);
		int32_t sample = static_cast<int32_t>(out * 2147483647.f);

		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 4;
	}
}

} // namespace deluge::dsp::machine
