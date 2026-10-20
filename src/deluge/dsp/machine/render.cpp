#include "dsp/machine/render.h"
#include "util/functions.h"
#include "util/waves.h"
#include <algorithm>
#include <cmath>

namespace deluge::dsp::machine {

namespace {

constexpr float kInv127 = 1.f / 127.f;
constexpr float kInvInt32 = 1.f / 2147483648.f;
constexpr float kScaleInt32 = 2147483647.f;
constexpr float kPercLevel = 0.34f;
constexpr float kSkinLevel = 0.42f;

inline float u8f(uint8_t v) {
	return static_cast<float>(v) * kInv127;
}

inline float clampTimeScale(float timeScale) {
	return timeScale < 0.15f ? 0.15f : timeScale;
}

// Cheap soft clip (no tanh) — same family as Resonator Wire.
inline float softClip(float x) {
	if (x > 1.f) {
		return 1.f - 1.f / (x + 1.f);
	}
	if (x < -1.f) {
		return -1.f + 1.f / (-x + 1.f);
	}
	return x;
}

// Multi-reflect fold then softClip. fold 0..1.
inline float waveFold(float f, float fold) {
	if (fold <= 0.01f) {
		return f;
	}
	f *= 1.f + fold * 5.5f;
	int stages = 1 + static_cast<int>(fold * 3.f);
	for (int stage = 0; stage < stages; stage++) {
		if (f > 1.f) {
			f = 2.f - f;
		}
		else if (f < -1.f) {
			f = -2.f - f;
		}
	}
	return softClip(f);
}

// Drum-layer decay: dial 1–50 short/snappy; 50–127 longer tails.
// Returns per-sample exponential coefficient for env *= (1 - coef).
inline float drumDecayCoef(uint8_t v) {
	float x = std::max(1, static_cast<int>(v)) * kInv127;
	// ~pow(x, 2.2) via x^2 * (0.2 + 0.8x) — no libm pow
	float x2 = x * x;
	float shaped = x2 * (0.2f + 0.8f * x);
	float tauSamples = 180.f + shaped * 66000.f;
	return 1.f / tauSamples;
}

inline float drumPitchEnvCoef(uint8_t v) {
	float x = std::max(1, static_cast<int>(v)) * kInv127;
	float tauSamples = 90.f + (x * x) * 22000.f;
	return 1.f / tauSamples;
}

inline uint32_t nextNoise(uint32_t& state) {
	state = state * 1664525u + 1013904223u;
	return state;
}

inline int32_t noiseSample(uint32_t& state) {
	return static_cast<int32_t>(nextNoise(state) >> 1) - 0x40000000;
}

// High-pass-ish noise for metallic air (Perc).
inline int32_t hpNoise(uint32_t& noiseState, uint32_t& prevStore) {
	int32_t noise = noiseSample(noiseState);
	int32_t prev = static_cast<int32_t>(prevStore);
	int32_t hp = noise - prev;
	prevStore = static_cast<uint32_t>(noise);
	return static_cast<int32_t>(noise * 0.25f + hp * 0.75f);
}

inline int32_t triangleFromPhase(uint32_t ph) {
	uint32_t p = ph >> 1;
	return (ph < 0x80000000u) ? static_cast<int32_t>(p) - 0x40000000
	                          : 0x3FFFFFFF - static_cast<int32_t>(p - 0x40000000);
}

// Soft morph sine → tri → saw → square. Skips pow when phaseDist ≈ 50.
inline int32_t morphWaveFast(uint32_t phase, uint8_t waveIndex, uint8_t phaseDist) {
	float t = std::clamp(waveIndex / 120.f, 0.f, 1.f);
	uint32_t ph = phase;

	int pdDelta = static_cast<int>(phaseDist) - 50;
	if (pdDelta > 1 || pdDelta < -1) {
		float pd = static_cast<float>(pdDelta) * 0.02f; // -1..1
		float x = static_cast<float>(phase) * (1.f / 4294967296.f);
		if (pd > 0.f) {
			float x3 = x * x * x;
			x = x * (1.f - pd) + x3 * pd;
			ph = static_cast<uint32_t>(std::clamp(x, 0.f, 0.999999f) * 4294967296.f);
		}
		else {
			float y = 1.f - x;
			float y3 = y * y * y;
			float u = -pd;
			y = y * (1.f - u) + y3 * u;
			x = 1.f - y;
			ph = static_cast<uint32_t>(std::clamp(x, 0.f, 0.999999f) * 4294967296.f);
		}
	}

	int32_t sine = getSine(ph);
	int32_t tri = triangleFromPhase(ph);
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

// Convert a full-scale sine into a phase offset. index ~0.5 mild FM; ~2 bright.
inline uint32_t phaseMod(int32_t sample, float index) {
	if (index <= 0.f) {
		return 0;
	}
	return static_cast<uint32_t>(static_cast<int32_t>(sample * (index * 0.125f)));
}

inline void accumulateSample(int32_t* dest, int32_t sample, int32_t& amp, int32_t amplitudeIncrement) {
	amp += amplitudeIncrement;
	*dest += multiply_32x32_rshift32(sample, amp) << 4;
}

inline int32_t floatToSample(float f) {
	return static_cast<int32_t>(f * kScaleInt32);
}

// Cap how fast a drum body can die (~2.7ms) so short Decay still fades cleanly.
inline float clampBodyDecCoef(float coef) {
	constexpr float kMax = 1.f / 120.f;
	return coef > kMax ? kMax : coef;
}

// Noise burst that fades out — avoids a hard cut click when the window ends.
inline int32_t windowedNoiseBurst(uint32_t& noiseState, uint32_t& samplesLeft, float totalLen, float amount,
                                  int shift) {
	if (samplesLeft == 0 || totalLen < 1.f) {
		return 0;
	}
	float win = static_cast<float>(samplesLeft) / totalLen;
	win *= win; // ease-out
	int32_t n = noiseSample(noiseState) >> shift;
	samplesLeft--;
	return static_cast<int32_t>(n * amount * win);
}

// ~1.5ms ease-in so hard onsets don't click into delay/reverb.
constexpr float kOnsetAttackSamples = 64.f;

inline float onsetAttackGain(uint32_t sampleCount) {
	if (sampleCount >= static_cast<uint32_t>(kOnsetAttackSamples)) {
		return 1.f;
	}
	float atk = static_cast<float>(sampleCount) / kOnsetAttackSamples;
	return atk * atk;
}

inline int32_t withOnsetAttack(int32_t mix, uint32_t sampleCount) {
	float g = onsetAttackGain(sampleCount);
	return g >= 1.f ? mix : static_cast<int32_t>(mix * g);
}

inline float withOnsetAttackF(float f, uint32_t sampleCount) {
	return f * onsetAttackGain(sampleCount);
}

} // namespace

void renderFmDrum(FmDrumPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                  uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	timeScale = clampTimeScale(timeScale);
	// Tune owns pitch only — algo must not shift the fundamental.
	float tuneMul = 0.08f + u8f(patch.tune) * 1.1f;
	uint32_t baseInc = static_cast<uint32_t>(phaseIncrement * tuneMul);

	float sweepAmt = u8f(patch.sweep);
	float sweepDepth = sweepAmt * 2.6f;
	uint8_t sweepTime = static_cast<uint8_t>(12 + (1.f - sweepAmt) * 50.f + sweepAmt * 70.f);
	float bodyDecCoef = clampBodyDecCoef(drumDecayCoef(patch.decay) / timeScale);
	float noiseDecCoef = clampBodyDecCoef(drumDecayCoef(static_cast<uint8_t>(8 + patch.noise / 2)) / timeScale);
	float sweepCoef = drumPitchEnvCoef(sweepTime) / timeScale;

	float modAmt = u8f(patch.mod);
	float modA = modAmt * 1.05f;
	float modB = modAmt * 0.7f;
	float rA = 1.5f + modAmt * 3.5f;
	float rB = 2.2f + modAmt * 2.8f;

	float fold = u8f(patch.fold);
	float fb = fold * 0.35f;
	float noiseAmt = u8f(patch.noise);

	uint8_t algo = patch.algorithm % 7;
	float noiseBias = 1.f;
	float clickBias = 1.f;
	float modSkew = 1.f;
	switch (algo) {
	case 0:
		noiseBias = 0.55f;
		clickBias = 0.8f;
		break;
	case 1:
		noiseBias = 0.7f;
		clickBias = 1.4f;
		modSkew = 1.25f;
		break;
	case 2:
		noiseBias = 1.35f;
		clickBias = 1.1f;
		break;
	case 3:
		noiseBias = 0.4f;
		clickBias = 1.2f;
		modSkew = 0.75f;
		break;
	case 4:
		noiseBias = 0.9f;
		modSkew = 1.45f;
		rB *= 1.35f;
		break;
	case 5:
		noiseBias = 1.5f;
		clickBias = 0.6f;
		break;
	default:
		noiseBias = 1.1f;
		clickBias = 1.0f;
		modSkew = 1.15f;
		break;
	}
	modA *= modSkew;
	modB *= (2.f - modSkew * 0.5f);

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		st.envA = 1.f;
		st.envB = 1.f;
		st.phase[3] = 0;
		st.clickSamplesLeft = 8 + static_cast<uint32_t>(noiseAmt * 36.f * clickBias);
	}

	int32_t amp = amplitude;
	int32_t feedbackMem = 0;
	float modDec = drumDecayCoef(static_cast<uint8_t>(20 + patch.decay / 2)) / timeScale;
	bool doFold = fold > 0.01f;
	float clickTotal = 8.f + noiseAmt * 36.f * clickBias;

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
		int32_t body = morphWaveFast(st.phase[3] + mod, patch.wave, 40);
		st.phase[3] += inc;
		feedbackMem = body;
		if (doFold) {
			body = floatToSample(waveFold(body * kInvInt32, fold));
		}
		body = static_cast<int32_t>(body * st.bodyEnv);

		int32_t noise = noiseSample(st.noiseState);
		noise = static_cast<int32_t>(noise * st.noiseEnv * noiseAmt * 0.45f * noiseBias);

		int32_t click =
		    windowedNoiseBurst(st.noiseState, st.clickSamplesLeft, clickTotal, noiseAmt * clickBias * 0.85f, 3);

		accumulateSample(&dest[i], withOnsetAttack(body + noise + click, st.sampleCount), amp, amplitudeIncrement);
	}
}

void renderWaveTone(WaveTonePatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                    uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	timeScale = clampTimeScale(timeScale);
	float pitchMul = 0.5f + u8f(patch.pitch); // 64 ≈ unity
	float off1 = 1.f + (static_cast<int>(patch.osc1LinOffset) - 64) * 0.0015f;
	float off2 = 1.f + (static_cast<int>(patch.osc2LinOffset) - 64) * 0.0015f;
	uint32_t inc1 = static_cast<uint32_t>(phaseIncrement * off1 * pitchMul);
	uint32_t inc2 = static_cast<uint32_t>(phaseIncrement * off2 * pitchMul);
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
	bool hardSync = patch.oscMod == 3;
	bool ringMod = patch.oscMod == 1 || patch.oscMod == 2;
	float noiseLevelF = patch.noiseLevel ? u8f(patch.noiseLevel) * 0.3f : 0.f;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		int32_t o1 = morphWaveFast(st.phase[0], patch.osc1Wave, patch.osc1PhaseDist);
		int32_t o2 = morphWaveFast(st.phase[1], patch.osc2Wave, patch.osc2PhaseDist);
		st.phase[0] += inc1;

		if (hardSync) {
			st.phase[1] += inc2;
			if (st.phase[0] < inc1) {
				st.phase[1] = 0;
			}
		}
		else {
			st.phase[1] += inc2;
		}

		int32_t mix;
		if (ringMod) {
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
			n = static_cast<int32_t>(n * st.noiseEnv * noiseLevelF);
			mix += n;
		}

		accumulateSample(&dest[i], mix, amp, amplitudeIncrement);
	}
}

void renderPerc(PercPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	timeScale = clampTimeScale(timeScale);
	auto role = static_cast<PercRole>(patch.role);

	float color = u8f(patch.color);
	float pitchMul = 0.55f + u8f(patch.pitch) * 1.45f;
	float bodyDecCoef = clampBodyDecCoef(drumDecayCoef(patch.decay) / timeScale);
	float noiseDecCoef = clampBodyDecCoef(drumDecayCoef(static_cast<uint8_t>(10 + patch.noise / 2)) / timeScale);
	float crunch = u8f(patch.crunch);
	float drive = 1.f + crunch * 2.4f;
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
	bool useSquarePartials = color > 0.55f;
	int activePartials = 4;

	switch (role) {
	case PercRole::Metal:
		activePartials = 4; // was 6 — big CPU save, still metallic
		noiseBias = 0.25f;
		break;
	case PercRole::Bell:
		activePartials = 2;
		ratios[1] = 1.48f + color * 0.2f;
		noiseBias = 0.08f;
		bodyBias = 1.15f;
		break;
	case PercRole::Hat808:
		activePartials = 4;
		noiseBias = 0.9f;
		bodyBias = 0.8f;
		useSquarePartials = true;
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

	// Precompute base increments (pitchScale applied in-loop).
	uint32_t baseIncs[6]{};
	float partialW[6]{};
	for (int p = 0; p < activePartials; p++) {
		baseIncs[p] = static_cast<uint32_t>(static_cast<float>(baseInc) * ratios[p]);
		partialW[p] = 1.f / static_cast<float>(p + 1);
	}
	if (activePartials > 1) {
		baseIncs[1] = static_cast<uint32_t>(static_cast<float>(baseIncs[1]) * (1.f + beatAmt));
	}

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		st.envA = 0.f;
		for (auto& p : st.phase) {
			p = 0;
		}
		st.phase[1] = 0x20000000u;
		float clickLen = 6.f + crunch * 40.f;
		st.envB = clickLen; // remember length for windowed burst
		st.clickSamplesLeft = static_cast<uint32_t>(clickLen);
	}

	int32_t amp = amplitude;
	uint32_t grainPeriod = 40 + static_cast<uint32_t>((1.f - u8f(patch.pitch)) * 280.f);
	uint32_t grainLen = 20 + static_cast<uint32_t>(color * 90.f);
	float grainDec = 1.f / static_cast<float>(std::max<uint32_t>(8, grainLen));
	float noiseScale = noiseAmt * 0.55f * noiseBias;
	float clickTotal = st.envB > 1.f ? st.envB : 1.f;

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
			st.envA *= (1.f - grainDec);
		}
		else if (role == PercRole::FM) {
			uint32_t inc0 = static_cast<uint32_t>(static_cast<float>(baseInc) * pitchScale);
			uint32_t inc1 = static_cast<uint32_t>(static_cast<float>(baseIncs[1]) * pitchScale);
			int32_t mod = getSquare(st.phase[1]);
			st.phase[1] += inc1;
			body = getSquare(st.phase[0] + phaseMod(mod, fmIndex * st.bodyEnv));
			st.phase[0] += inc0;
		}
		else if (useXor) {
			int32_t x = 0;
			for (int p = 0; p < activePartials; p++) {
				uint32_t inc = static_cast<uint32_t>(static_cast<float>(baseIncs[p]) * pitchScale);
				int32_t s = getSquare(st.phase[p]);
				st.phase[p] += inc;
				x ^= (s >> 31);
			}
			body = x ? 0x3FFFFFFF : static_cast<int32_t>(0xC0000000);
		}
		else {
			for (int p = 0; p < activePartials; p++) {
				uint32_t inc = static_cast<uint32_t>(static_cast<float>(baseIncs[p]) * pitchScale);
				int32_t s;
				if (useSquarePartials) {
					s = getSquare(st.phase[p]);
				}
				else if (color < 0.35f) {
					s = getSine(st.phase[p]);
				}
				else {
					s = morphWaveFast(st.phase[p], waveIdx, 50);
				}
				st.phase[p] += inc;
				body += static_cast<int32_t>(s * partialW[p]);
			}
		}

		body = static_cast<int32_t>(body * st.bodyEnv * bodyBias);

		int32_t noise = hpNoise(st.noiseState, st.phase[5]);
		float noiseGate = useGrains ? st.envA : st.noiseEnv;
		noise = static_cast<int32_t>(noise * noiseGate * noiseScale);

		int32_t click = windowedNoiseBurst(st.noiseState, st.clickSamplesLeft, clickTotal, crunch * 0.85f, 3);

		float f = softClip((body + noise + click) * kInvInt32 * drive) * kPercLevel;
		accumulateSample(&dest[i], floatToSample(withOnsetAttackF(f, st.sampleCount)), amp, amplitudeIncrement);
	}
}

void renderSkin(SkinPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	timeScale = clampTimeScale(timeScale);
	auto mode = static_cast<SkinMode>(patch.mode);

	float pitchMul = 0.08f + u8f(patch.pitch) * 1.55f;
	uint32_t baseInc = static_cast<uint32_t>(static_cast<float>(phaseIncrement) * pitchMul);

	float harm = u8f(patch.harm);
	float spread = harm * 0.85f;
	float morph = u8f(patch.morph);
	float fold = u8f(patch.fold);
	uint8_t waveIdx = static_cast<uint8_t>(morph * 120.f);
	bool useSineOnly = morph < 0.08f;

	int nOsc = 1 + static_cast<int>(harm * 5.f + 0.5f);
	if (nOsc > 6) {
		nOsc = 6;
	}

	float bodyDecCoef = clampBodyDecCoef(drumDecayCoef(patch.decay) / timeScale);
	float atk = 1.f - u8f(patch.decay) * 0.55f;
	float pitchEnvCoef = drumPitchEnvCoef(static_cast<uint8_t>(14 + atk * 40.f)) / timeScale;
	bool usePitchEnv = (mode == SkinMode::Liquid) || (mode == SkinMode::Metal && harm > 0.45f);
	float pitchEnvAmt = (mode == SkinMode::Liquid) ? (1.3f + harm * 1.5f) : (0.45f + spread * 0.9f);

	float decayF = u8f(patch.decay);
	bool isMetal = mode == SkinMode::Metal;
	float noiseAmt = u8f(patch.noise);
	// Transient pop + hiss both scale with Noise dial (0 = clean body).
	float popAmt = noiseAmt * (isMetal ? (0.14f + (1.f - decayF) * 0.16f) : (0.22f + (1.f - decayF) * 0.28f));
	float noiseScale = noiseAmt * (isMetal ? (0.35f + harm * 0.4f) : 0.45f);
	bool doFold = fold > 0.01f;

	// Block-constant ratios + osc decay multipliers (pitchScale applied in-loop).
	float spreadRatios[6];
	float oscDecMul[6];
	float modIdx1 = 0.6f + spread * 1.8f;
	float modIdx0 = 0.8f + harm * 2.2f;
	for (int o = 0; o < 6; o++) {
		float harmN = static_cast<float>(o + 1);
		float inharm = 1.f + static_cast<float>(o) * (0.35f + spread * 1.1f);
		spreadRatios[o] = harmN * (1.f - spread) + inharm * spread;
		oscDecMul[o] = 1.f + static_cast<float>(o) * (0.35f - harm * 0.25f);
	}

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
		float popLen = isMetal ? (5.f + popAmt * 22.f) : (8.f + popAmt * 36.f);
		st.envB = popLen;
		st.clickSamplesLeft = static_cast<uint32_t>(popLen);
	}

	int32_t amp = amplitude;
	float noiseDecCoef = bodyDecCoef * 1.25f;
	float popTotal = st.envB > 1.f ? st.envB : 1.f;

	auto sampleWave = [&](uint32_t phase) -> int32_t {
		if (useSineOnly) {
			return getSine(phase);
		}
		return morphWaveFast(phase, waveIdx, 50);
	};

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		st.bodyEnv *= (1.f - bodyDecCoef);
		st.noiseEnv *= (1.f - noiseDecCoef);
		st.pitchEnv *= (1.f - pitchEnvCoef);

		float pitchScale = usePitchEnv ? (1.f + pitchEnvAmt * st.pitchEnv) : 1.f;

		for (int o = 0; o < 6; o++) {
			st.oscEnv[o] *= (1.f - bodyDecCoef * oscDecMul[o]);
		}

		int32_t mixed = 0;
		if (isMetal) {
			auto chain = [&](int c0, int c1, int c2) -> int32_t {
				uint32_t inc2 = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[c2] * pitchScale);
				uint32_t inc1 = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[c1] * pitchScale);
				uint32_t inc0 = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[c0] * pitchScale);
				int32_t m2 = sampleWave(st.phase[c2]);
				st.phase[c2] += inc2;
				int32_t m1 = sampleWave(st.phase[c1] + phaseMod(m2, modIdx1));
				st.phase[c1] += inc1;
				int32_t c = sampleWave(st.phase[c0] + phaseMod(m1, modIdx0));
				st.phase[c0] += inc0;
				return static_cast<int32_t>(c * st.oscEnv[c0] + m1 * st.oscEnv[c1] * 0.35f);
			};
			mixed = chain(0, 1, 2) + chain(3, 4, 5);
		}
		else {
			for (int o = 0; o < nOsc; o++) {
				uint32_t inc = static_cast<uint32_t>(static_cast<float>(baseInc) * spreadRatios[o] * pitchScale);
				int32_t s = sampleWave(st.phase[o]);
				st.phase[o] += inc;
				mixed += static_cast<int32_t>(s * st.oscEnv[o]);
			}
		}

		mixed = static_cast<int32_t>(mixed * st.bodyEnv);

		int32_t noise = noiseSample(st.noiseState);
		noise = static_cast<int32_t>(noise * st.noiseEnv * noiseScale);

		int32_t pop = windowedNoiseBurst(st.noiseState, st.clickSamplesLeft, popTotal, popAmt, 2);

		float f = (mixed + noise + pop) * kInvInt32;
		if (doFold) {
			// Ease fold with the body so short hits don't fold a dying spike into a click.
			float foldAmt = fold * std::min(1.f, st.bodyEnv * 2.5f);
			f = waveFold(f, foldAmt);
		}
		f *= kSkinLevel;

		accumulateSample(&dest[i], floatToSample(withOnsetAttackF(f, st.sampleCount)), amp, amplitudeIncrement);
	}
}

void renderResonator(ResonatorPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                     uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	timeScale = clampTimeScale(timeScale);
	auto model = static_cast<ResonatorModel>(patch.model);

	float pitchMul = 0.5f + u8f(patch.pitch); // 64 ≈ unity
	phaseIncrement = static_cast<uint32_t>(static_cast<float>(phaseIncrement) * pitchMul);

	float structure = u8f(patch.structure);
	float bright = u8f(patch.brightness);
	float damp = u8f(patch.damping);
	float pos = u8f(patch.position);
	float excite = u8f(patch.excite);

	constexpr int kPartials = 4;
	uint32_t incs[kPartials];
	float decays[kPartials];
	float amps[kPartials];

	float stretch = 1.f;
	float stiff = (structure - 0.5f) * 0.28f;
	float qLoss = 0.62f + bright * 0.32f;
	float decay0 = 1.f - (0.00015f + (1.f - damp) * (1.f - damp) * 0.008f) / timeScale;

	for (int i = 0; i < kPartials; i++) {
		float n = static_cast<float>(i + 1);
		float ratio;
		if (model == ResonatorModel::Wire) {
			static constexpr float kWire[4] = {1.f, 2.28f, 3.65f, 5.05f};
			ratio = kWire[i] * (1.f + structure * 0.35f * static_cast<float>(i) * 0.25f);
		}
		else {
			ratio = n * stretch;
			stretch += stiff;
			stiff *= (stiff < 0.f) ? 0.93f : 0.98f;
		}
		incs[i] = static_cast<uint32_t>(static_cast<float>(phaseIncrement) * ratio);
		decays[i] = decay0;
		for (int k = 0; k < i; k++) {
			decays[i] *= qLoss;
		}
		if (decays[i] < 0.995f) {
			decays[i] = 0.995f;
		}
		if (decays[i] > 0.99995f) {
			decays[i] = 0.99995f;
		}
		float w = 1.f - static_cast<float>(i) * (0.18f - bright * 0.1f);
		float posW = 1.f - std::abs(pos - (0.15f + 0.2f * static_cast<float>(i))) * 1.4f;
		if (posW < 0.15f) {
			posW = 0.15f;
		}
		amps[i] = w * posW * (0.45f + bright * 0.55f);
	}

	if (st.sampleCount == 0) {
		for (int i = 0; i < kPartials; i++) {
			st.phase[i] = static_cast<uint32_t>(i * 0x1A2B3C4Du);
			st.oscEnv[i] = amps[i];
		}
		for (auto& c : st.combBuf) {
			c = 0.f;
		}
		st.combPos = 0;
		float f0 = static_cast<float>(phaseIncrement) * (1.f / 4294967296.f);
		uint32_t delay = static_cast<uint32_t>(std::clamp(1.f / std::max(f0, 0.002f), 12.f, 127.f));
		st.combLen = static_cast<uint16_t>(delay);
		st.clickSamplesLeft = 12 + static_cast<uint32_t>(excite * 50.f);
		st.bodyEnv = 1.f;
	}

	float bodyDec = (0.00008f + (1.f - damp) * 0.0015f) / timeScale;
	float combFb = 0.9f + damp * 0.09f;
	float brightLp = 0.35f + bright * 0.55f;
	float wireDrive = 1.f + structure * 2.2f;
	int32_t amp = amplitude;
	bool useComb = (model != ResonatorModel::Modal);
	bool isWire = model == ResonatorModel::Wire;
	bool isStrings = model == ResonatorModel::Strings;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		st.bodyEnv *= (1.f - bodyDec);

		float exc = 0.f;
		if (st.clickSamplesLeft > 0) {
			exc = (static_cast<float>(noiseSample(st.noiseState)) * kInvInt32) * excite
			      * (static_cast<float>(st.clickSamplesLeft) * (1.f / 64.f));
			st.clickSamplesLeft--;
		}

		float out = 0.f;

		if (!useComb) {
			for (int p = 0; p < kPartials; p++) {
				out += static_cast<float>(getSine(st.phase[p])) * kInvInt32 * st.oscEnv[p];
				st.phase[p] += incs[p];
				st.oscEnv[p] *= decays[p];
			}
			out += exc * 0.35f;
			out *= 1.6f;
		}
		else {
			uint16_t len = st.combLen;
			if (len < 8) {
				len = 8;
			}
			if (len > 127) {
				len = 127;
			}
			uint16_t readPos = static_cast<uint16_t>((st.combPos + 128 - len) & 127);
			uint16_t read2 = static_cast<uint16_t>((readPos + 1) & 127);
			float delayed = st.combBuf[readPos];
			float lp = delayed * (1.f - brightLp) + st.combBuf[read2] * brightLp;
			if (isWire) {
				lp = softClip(lp * wireDrive);
			}
			float y = exc + lp * combFb;
			st.combBuf[st.combPos] = y;
			st.combPos = static_cast<uint16_t>((st.combPos + 1) & 127);
			out = y;

			if (isStrings) {
				for (int p = 0; p < 2; p++) {
					out += static_cast<float>(getSine(st.phase[p])) * kInvInt32 * st.oscEnv[p] * 0.22f;
					st.phase[p] += incs[p];
					st.oscEnv[p] *= decays[p];
				}
				out *= 0.85f;
			}
			else {
				out *= 0.42f;
			}
		}

		out *= st.bodyEnv;
		if (out > 1.f) {
			out = 1.f;
		}
		else if (out < -1.f) {
			out = -1.f;
		}

		accumulateSample(&dest[i], floatToSample(withOnsetAttackF(out, st.sampleCount)), amp, amplitudeIncrement);
	}
}

void renderSyOsc(SyOscPatch const& patch, MachineVoiceState& st, int32_t* dest, int32_t numSamples,
                 uint32_t phaseIncrement, int32_t amplitude, int32_t amplitudeIncrement, float timeScale) {
	timeScale = clampTimeScale(timeScale);
	auto mode = static_cast<SyOscMode>(patch.mode % static_cast<uint8_t>(SyOscMode::COUNT));

	float tuneMul = 0.12f + u8f(patch.pitch) * 1.15f;
	uint32_t baseInc = static_cast<uint32_t>(static_cast<float>(phaseIncrement) * tuneMul);

	float sweepAmt = u8f(patch.sweep);
	float sweepDepth = sweepAmt * (mode == SyOscMode::Sweep ? 3.2f : 2.2f);
	uint8_t sweepTime = static_cast<uint8_t>(10 + (1.f - sweepAmt) * 45.f + sweepAmt * 80.f);
	float bodyDecCoef = clampBodyDecCoef(drumDecayCoef(patch.decay) / timeScale);
	float noiseDecCoef = clampBodyDecCoef(drumDecayCoef(static_cast<uint8_t>(8 + patch.noise / 2)) / timeScale);
	float sweepCoef = drumPitchEnvCoef(sweepTime) / timeScale;

	float ratio = 0.5f + u8f(patch.ratio) * 3.5f;
	float color = u8f(patch.color);
	float noiseAmt = u8f(patch.noise);
	// Color = brightness: more LP when low (same idea as Skin fold easing)
	float lpAmt = 1.f - color * 0.85f; // 1 = dark, ~0.15 = bright

	float mixA = 0.45f;
	float mixB = 0.45f;
	float noiseBias = 0.3f;
	float fmIdx = 0.f;
	float popAmt = 0.22f + noiseAmt * 0.25f;
	bool hardSync = false;
	bool doRing = false;
	bool doFm = false;
	bool softWave = false; // sine A — Dual / quieter tops

	switch (mode) {
	case SyOscMode::Dual:
		mixA = 0.42f;
		mixB = 0.38f;
		noiseBias = 0.18f;
		softWave = true;
		popAmt = 0.16f + noiseAmt * 0.15f;
		break;
	case SyOscMode::Sync:
		hardSync = true;
		mixA = 0.18f;
		mixB = 0.72f;
		noiseBias = 0.22f;
		popAmt = 0.14f + noiseAmt * 0.12f; // sync edges already transient-y
		break;
	case SyOscMode::FM:
		doFm = true;
		fmIdx = 0.5f + u8f(patch.ratio) * 2.4f;
		mixA = 0.12f;
		mixB = 0.78f;
		noiseBias = 0.12f;
		popAmt = 0.14f + noiseAmt * 0.12f;
		break;
	case SyOscMode::Ring:
		doRing = true;
		mixA = 0.f;
		mixB = 0.f;
		noiseBias = 0.25f;
		popAmt = 0.18f + noiseAmt * 0.15f;
		break;
	case SyOscMode::Noise:
		mixA = 0.28f;
		mixB = 0.15f;
		noiseBias = 0.95f; // was 1.35 — keep kit levels in line
		popAmt = 0.2f + noiseAmt * 0.2f;
		break;
	case SyOscMode::Sweep:
		hardSync = true;
		mixA = 0.15f;
		mixB = 0.75f;
		noiseBias = 0.18f;
		popAmt = 0.12f + noiseAmt * 0.1f;
		break;
	default:
		break;
	}

	if (st.sampleCount == 0) {
		st.bodyEnv = 1.f;
		st.noiseEnv = 1.f;
		st.pitchEnv = 1.f;
		st.phase[0] = 0;
		st.phase[1] = 0x20000000u;
		st.combBuf[0] = 0.f; // LP state
		float clickLen = hardSync ? (4.f + popAmt * 16.f) : (6.f + popAmt * 24.f);
		st.envB = clickLen;
		st.clickSamplesLeft = static_cast<uint32_t>(clickLen);
	}

	int32_t amp = amplitude;
	float clickTotal = st.envB > 1.f ? st.envB : 1.f;
	float noiseScale = noiseAmt * 0.45f * noiseBias;
	// Match Perc kit level — dual squares + sync run hot otherwise.
	constexpr float kSyLevel = 0.34f;

	for (int32_t i = 0; i < numSamples; i++) {
		st.sampleCount++;
		st.bodyEnv *= (1.f - bodyDecCoef);
		st.noiseEnv *= (1.f - noiseDecCoef);
		st.pitchEnv *= (1.f - sweepCoef);

		float pitchScale = 1.f + sweepDepth * st.pitchEnv;
		uint32_t incA = static_cast<uint32_t>(static_cast<float>(baseInc) * pitchScale);
		uint32_t incB = static_cast<uint32_t>(static_cast<float>(baseInc) * ratio * pitchScale);

		uint32_t prevA = st.phase[0];
		int32_t oA = softWave ? getSine(st.phase[0]) : getSquare(st.phase[0]);
		st.phase[0] += incA;

		if (hardSync && st.phase[0] < prevA) {
			st.phase[1] = 0;
		}

		int32_t oB;
		if (doFm) {
			oB = getSquare(st.phase[1] + phaseMod(oA, fmIdx * st.bodyEnv));
			st.phase[1] += incB;
		}
		else {
			oB = getSquare(st.phase[1]);
			st.phase[1] += incB;
		}

		int32_t body;
		if (doRing) {
			body = multiply_32x32_rshift32(oA, oB) << 1;
		}
		else {
			body = static_cast<int32_t>(oA * mixA + oB * mixB);
		}
		body = static_cast<int32_t>(body * st.bodyEnv);

		int32_t noise = noiseSample(st.noiseState);
		noise = static_cast<int32_t>(noise * st.noiseEnv * noiseScale);
		int32_t click = windowedNoiseBurst(st.noiseState, st.clickSamplesLeft, clickTotal, popAmt, 3);

		float f = (body + noise + click) * kInvInt32;
		// 1-pole LP; ease with body so short Decay doesn't leave a filtered spike
		float& lp = st.combBuf[0];
		float coef = 0.12f + (1.f - lpAmt) * 0.82f;
		lp += coef * (f - lp);
		f = lp * lpAmt + f * (1.f - lpAmt);
		f = softClip(f) * kSyLevel;

		accumulateSample(&dest[i], floatToSample(withOnsetAttackF(f, st.sampleCount)), amp, amplitudeIncrement);
	}
}

} // namespace deluge::dsp::machine
