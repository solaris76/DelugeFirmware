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

// Simple 4-op algorithm routings (Digitone-inspired subset)
// Returns carrier mix into x and y style single output for now
inline int32_t runFmOps(int32_t op[4], uint8_t algo, uint8_t mix) {
	// op: 0=A, 1=B1, 2=B2, 3=C  (as signed samples)
	int32_t x = 0;
	int32_t y = 0;
	switch (algo & 7) {
	case 0: // C <- A, C <- B1<-B2  (branch into C)
		x = op[3];
		y = op[3];
		break;
	case 1: // two stacks: A->C and B2->B1
		x = op[3];
		y = op[1];
		break;
	case 2:
		x = op[3];
		y = op[0];
		break;
	case 3:
		x = op[3] + (op[1] >> 1);
		y = op[0];
		break;
	case 4:
		x = op[3];
		y = op[1] + (op[2] >> 1);
		break;
	case 5:
		x = op[3] + (op[0] >> 1);
		y = op[1] + (op[2] >> 1);
		break;
	case 6:
		x = op[3] + op[0];
		y = op[1];
		break;
	default: // parallel-ish
		x = op[3] + (op[0] >> 1);
		y = op[1] + (op[2] >> 1);
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
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement) {
	float rC = ratioFromIndexC(patch.ratioC);
	float rA = ratioFromIndexA(patch.ratioA);
	float rB1, rB2;
	unpackRatioB(patch.ratioB, rB1, rB2);

	rC *= 1.f + (static_cast<int>(patch.ratioCOffset) - 64) * 0.002f;
	rA *= 1.f + (static_cast<int>(patch.ratioAOffset) - 64) * 0.002f;
	rB1 *= 1.f + (static_cast<int>(patch.ratioB1Offset) - 64) * 0.002f;
	rB2 *= 1.f + (static_cast<int>(patch.ratioB2Offset) - 64) * 0.002f;

	float det = u8f(patch.detune) * 0.02f;
	rA *= (1.f + det);
	rB2 *= (1.f - det);

	uint32_t incC = static_cast<uint32_t>(phaseIncrement * rC);
	uint32_t incA = static_cast<uint32_t>(phaseIncrement * rA);
	uint32_t incB1 = static_cast<uint32_t>(phaseIncrement * rB1);
	uint32_t incB2 = static_cast<uint32_t>(phaseIncrement * rB2);

	float aAtk = 0.002f + u8f(patch.aEnvAtk) * 0.05f;
	float aDec = 0.002f + u8f(patch.aEnvDec) * 0.08f;
	float bAtk = 0.002f + u8f(patch.bEnvAtk) * 0.05f;
	float bDec = 0.002f + u8f(patch.bEnvDec) * 0.08f;
	float aEnd = u8f(patch.aEnvEnd);
	float bEnd = u8f(patch.bEnvEnd);
	float aLev = u8f(patch.aLevel);
	float bLev = u8f(patch.bLevel);
	float fb = u8f(patch.feedback);
	float harm = 0.35f + u8f(patch.harmonics) * 1.4f; // scales FM index / brightness

	if (st.noteOn && st.sampleCount == 0 && patch.phaseReset) {
		st.phase[0] = st.phase[1] = st.phase[2] = st.phase[3] = 0;
		st.envA = 0.f;
		st.envB = 0.f;
	}

	int32_t amp = amplitude;
	int32_t feedbackMem = 0;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		float aTarget = (st.noteOn ? aLev : aEnd);
		float bTarget = (st.noteOn ? bLev : bEnd);
		envFollow(st.envA, aTarget, 1.f / (1.f + aAtk * 4000.f), 1.f / (1.f + aDec * 4000.f));
		envFollow(st.envB, bTarget, 1.f / (1.f + bAtk * 4000.f), 1.f / (1.f + bDec * 4000.f));

		int32_t modA = static_cast<int32_t>(getSine(st.phase[0]) * st.envA * harm);
		int32_t modB2 = getSine(st.phase[2]);
		st.phase[2] += incB2;
		int32_t modB1 =
		    static_cast<int32_t>(getSine(st.phase[1] + static_cast<uint32_t>(modB2 * st.envB * harm)) * st.envB * harm);
		st.phase[1] += incB1;

		int32_t fbAmt = static_cast<int32_t>(feedbackMem * fb);
		int32_t car = getSine(st.phase[3] + static_cast<uint32_t>((modA + modB1 + fbAmt) << 2));
		// Harmonics blend in a little 2nd partial of the carrier
		if (patch.harmonics > 8) {
			int32_t h2 = getSine(st.phase[3] << 1);
			car = (car * (160 - (patch.harmonics >> 1)) + h2 * (patch.harmonics >> 1)) >> 7;
		}
		st.phase[0] += incA;
		st.phase[3] += incC;
		feedbackMem = car;

		int32_t op[4] = {modA, modB1, modB2, car};
		int32_t sample = runFmOps(op, patch.algorithm, patch.mix);

		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 5;
	}
}

void renderFmDrum(FmDrumPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement) {
	float tuneMul = 0.25f + u8f(patch.tune) * 3.5f;
	uint32_t baseInc = static_cast<uint32_t>(phaseIncrement * tuneMul);

	float sweepDepth = u8f(patch.sweepDepth) * 4.f;
	float bodyHold = u8f(patch.bodyHold) * 0.02f; // short holds only
	float noiseHold = u8f(patch.noiseHold) * 0.015f;
	float bodyDecCoef = drumDecayCoef(patch.bodyDecay);
	float noiseDecCoef = drumDecayCoef(patch.noiseDecay);
	float modDecA = drumDecayCoef(patch.decayA);
	float modDecB = drumDecayCoef(patch.decayB);
	float sweepCoef = drumPitchEnvCoef(patch.sweepTime);
	float modA = u8f(patch.modA);
	float modB = u8f(patch.modB);
	float rA = 0.5f + u8f(patch.ratioA) * 8.f;
	float rB = 0.5f + u8f(patch.ratioB) * 8.f;
	float fold = u8f(patch.waveFold);
	float fb = u8f(patch.feedback);

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		st.envA = 1.f;
		st.envB = 1.f;
		if (patch.opCPhase < 91) {
			st.phase[3] = static_cast<uint32_t>((patch.opCPhase / 90.f) * 0x40000000u);
		}
		st.clickSamplesLeft = 32 + (patch.drumTransient >> 1);
	}

	float holdSamples = bodyHold * 44100.f;
	float noiseHoldSamples = noiseHold * 44100.f;
	int32_t amp = amplitude;
	int32_t feedbackMem = 0;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;

		if (st.sampleCount > holdSamples) {
			st.bodyEnv *= (1.f - bodyDecCoef);
		}
		if (st.sampleCount > noiseHoldSamples) {
			st.noiseEnv *= (1.f - noiseDecCoef);
		}
		st.envA *= (1.f - modDecA);
		st.envB *= (1.f - modDecB);
		st.pitchEnv *= (1.f - sweepCoef);

		float pitchMul = 1.f + sweepDepth * st.pitchEnv;
		uint32_t inc = static_cast<uint32_t>(baseInc * pitchMul);
		uint32_t incA = static_cast<uint32_t>(inc * rA);
		uint32_t incB = static_cast<uint32_t>(inc * rB);

		int32_t mA = static_cast<int32_t>(getSine(st.phase[0]) * st.envA * modA);
		int32_t mB = static_cast<int32_t>(getSine(st.phase[1]) * st.envB * modB);
		st.phase[0] += incA;
		st.phase[1] += incB;

		int32_t fbAmt = static_cast<int32_t>(feedbackMem * fb);
		int32_t body = getSine(st.phase[3] + static_cast<uint32_t>((mA + mB + fbAmt) << 3));
		st.phase[3] += inc;
		feedbackMem = body;
		if (fold > 0.01f) {
			float f = body * (1.f / 2147483648.f);
			f = std::tanh(f * (1.f + fold * 8.f));
			body = static_cast<int32_t>(f * 2147483647.f);
		}
		body = static_cast<int32_t>(body * st.bodyEnv * u8f(patch.bodyLevel));

		int32_t noise = noiseSample(st.noiseState);
		// crude band via mix with differentiated noise
		int32_t n2 = noiseSample(st.noiseState);
		noise = (noise + static_cast<int32_t>((n2 - noise) * (u8f(patch.noiseWidth)))) >> 1;
		noise = static_cast<int32_t>(noise * st.noiseEnv * u8f(patch.noiseLevel) * 0.5f);
		if (patch.noiseRingMod) {
			noise = multiply_32x32_rshift32(noise, body) << 2;
		}

		int32_t click = 0;
		if (st.clickSamplesLeft > 0) {
			click = noiseSample(st.noiseState) >> 2;
			click = static_cast<int32_t>(click * u8f(patch.transientLevel));
			st.clickSamplesLeft--;
		}

		int32_t sample = body + noise + click;
		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 4;
	}
}

void renderWaveTone(WaveTonePatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                    uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement) {
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

	float nDec = 0.002f + u8f(patch.noiseDec) * 0.1f;
	if (st.sampleCount == 0) {
		st.noiseEnv = 1.f;
	}

	int32_t amp = amplitude;
	int32_t lev1 = (patch.osc1Level * 65536) / 127;
	int32_t lev2 = (patch.osc2Level * 65536) / 127;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		int32_t o1 = morphWave(st.phase[0], patch.osc1Wave, patch.osc1PhaseDist);
		int32_t o2 = morphWave(st.phase[1], patch.osc2Wave, patch.osc2PhaseDist);
		st.phase[0] += inc1;

		if (patch.oscMod == 3) { // hard sync
			uint32_t prev = st.phase[1];
			st.phase[1] += inc2;
			if (st.phase[0] < inc1) { // wrapped
				st.phase[1] = 0;
			}
			(void)prev;
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
		}
		else {
			mix = multiply_32x32_rshift32(o1, lev1) + multiply_32x32_rshift32(o2, lev2);
		}

		if (patch.noiseLevel) {
			st.noiseEnv *= (1.f - nDec * 0.15f);
			int32_t n = noiseSample(st.noiseState);
			// noiseType: 0 grain (sparse), 1 tuned (pitched), 2 s&w
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
				// grainier: zero out some samples
				if ((nextNoise(st.noiseState) & 0xFF) < patch.noiseCharacter) {
					n = 0;
				}
			}
			n = static_cast<int32_t>(n * st.noiseEnv * u8f(patch.noiseLevel) * 0.35f);
			mix += n;
		}

		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(mix, amp) << 4;
	}
}

void renderPerc(PercPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement) {
	float pitchMul = 0.25f + u8f(patch.pitch) * 3.f;
	uint32_t baseInc = static_cast<uint32_t>(phaseIncrement * pitchMul);
	float pEnvAmt = u8f(patch.pitchEnv) * 3.f;
	float pitchEnvCoef = drumPitchEnvCoef(patch.pitchEnvTime);
	float bodyDecCoef = drumDecayCoef(patch.bodyDecay);
	float noiseDecCoef = drumDecayCoef(patch.noiseDecay);
	float drive = 1.f + u8f(patch.drive) * 6.f;

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		st.phase[0] = 0;
		st.clickSamplesLeft = 16 + (patch.click >> 2);
	}

	float holdSamples = u8f(patch.hold) * 0.02f * 44100.f; // short hold range
	int32_t amp = amplitude;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		if (st.sampleCount > holdSamples) {
			st.bodyEnv *= (1.f - bodyDecCoef);
			st.noiseEnv *= (1.f - noiseDecCoef);
		}
		st.pitchEnv *= (1.f - pitchEnvCoef);

		uint32_t inc = static_cast<uint32_t>(baseInc * (1.f + pEnvAmt * st.pitchEnv));
		int32_t body = getSine(st.phase[0]);
		// color blends in some triangle
		int32_t tri = morphWave(st.phase[0], 40, 50);
		body = (body * (128 - patch.color) + tri * patch.color) >> 7;
		st.phase[0] += inc;
		body = static_cast<int32_t>(body * st.bodyEnv * (0.3f + u8f(patch.tone) * 0.7f));

		int32_t noise = noiseSample(st.noiseState);
		if (patch.noiseFilterType == 1) {
			noise = noise - (static_cast<int32_t>(st.phase[2]) >> 1);
			st.phase[2] = static_cast<uint32_t>(noise);
		}
		noise = static_cast<int32_t>(noise * st.noiseEnv * u8f(patch.noiseLevel) * 0.45f);

		int32_t click = 0;
		if (st.clickSamplesLeft > 0) {
			click = noiseSample(st.noiseState) >> 3;
			click = static_cast<int32_t>(click * u8f(patch.click));
			st.clickSamplesLeft--;
		}

		float f = (body + noise + click) * (1.f / 2147483648.f);
		f = std::tanh(f * drive);
		int32_t sample = static_cast<int32_t>(f * 2147483647.f);

		amp += amplitudeIncrement;
		dest[i] += multiply_32x32_rshift32(sample, amp) << 4;
	}
}

} // namespace deluge::dsp::machine
