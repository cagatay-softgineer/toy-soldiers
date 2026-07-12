#include "app/audio.h"

#include "sokol_audio.h"

#include <cmath>
#include <cstring>
#include <mutex>

// Tiny additive synth: fixed voice pool + a step sequencer for music loops.
// The stream callback runs on the audio thread — state is guarded by a mutex
// (critical sections are tiny; fine for a jam title).

namespace toy {

namespace {

constexpr int kMaxVoices = 32;
constexpr float kPi = 3.14159265358979f;

enum class Wave : uint8_t { Square, Triangle, Sine, Noise };

struct Voice {
	bool active = false;
	bool isMusic = false;
	Wave wave = Wave::Square;
	float freq = 440.0f;
	float freqSlide = 0.0f; // Hz per second
	float phase = 0.0f;
	float t = 0.0f;      // seconds since start (negative = delayed start)
	float dur = 0.1f;    // seconds of sustain (excluding release)
	float attack = 0.004f;
	float release = 0.05f;
	float gain = 0.25f;
	uint32_t noiseState = 0x1234567u;
};

struct MusicState {
	MusicTrack track = MusicTrack::None;
	int step = 0;
	float stepTimer = 0.0f;
	bool stingDone = false;
};

struct AudioState {
	std::mutex lock;
	Voice voices[kMaxVoices];
	MusicState music;
	float masterVol = 1.0f;
	float sfxVol = 0.8f;
	float musicVol = 0.5f;
	float duck = 1.0f;       // smoothed toward duckTarget in the callback (#199)
	float duckTarget = 1.0f;
	bool ready = false;
};

AudioState g_audio;

float sampleWave(Voice& v)
{
	switch (v.wave) {
	case Wave::Square:
		return v.phase < 0.5f ? 0.6f : -0.6f;
	case Wave::Triangle:
		return v.phase < 0.5f ? (v.phase * 4.0f - 1.0f) : (3.0f - v.phase * 4.0f);
	case Wave::Sine:
		return std::sin(v.phase * 2.0f * kPi);
	case Wave::Noise: {
		uint32_t x = v.noiseState;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		v.noiseState = x;
		return (static_cast<float>(x & 0xFFFF) / 32768.0f) - 1.0f;
	}
	}
	return 0.0f;
}

float envelope(const Voice& v)
{
	if (v.t < 0.0f) {
		return 0.0f; // delayed start
	}
	if (v.t < v.attack) {
		return v.t / v.attack;
	}
	if (v.t < v.attack + v.dur) {
		return 1.0f;
	}
	const float rel = v.t - v.attack - v.dur;
	if (rel < v.release) {
		return 1.0f - rel / v.release;
	}
	return -1.0f; // done
}

// Spawn a voice (caller holds lock).
void spawn(Wave wave, float freq, float dur, float gain, float delay = 0.0f, float slide = 0.0f, bool music = false,
		   float release = 0.05f)
{
	for (Voice& v : g_audio.voices) {
		if (!v.active) {
			v = Voice{};
			v.active = true;
			v.isMusic = music;
			v.wave = wave;
			v.freq = freq;
			v.freqSlide = slide;
			v.dur = dur;
			v.gain = gain;
			v.t = -delay;
			v.release = release;
			return;
		}
	}
}

// --- music sequencer (runs inside the callback, lock held) ---

// Pentatonic-ish toy scale (C major pentatonic around C4).
constexpr float kScale[] = { 261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 523.25f };

void musicTick(float stepSeconds)
{
	MusicState& m = g_audio.music;
	if (m.track == MusicTrack::None) {
		return;
	}
	m.stepTimer -= stepSeconds;
	if (m.stepTimer > 0.0f) {
		return;
	}

	switch (m.track) {
	case MusicTrack::Menu: {
		// Slow, sparse lullaby: one soft note every ~1.2s.
		static const int pat[] = { 0, 2, 4, 2, 3, 1, 2, 0 };
		const float f = kScale[pat[m.step % 8]];
		spawn(Wave::Sine, f, 0.5f, 0.10f, 0.0f, 0.0f, true, 0.45f);
		spawn(Wave::Sine, f * 0.5f, 0.7f, 0.05f, 0.0f, 0.0f, true, 0.5f);
		m.stepTimer = 1.2f;
		++m.step;
		break;
	}
	case MusicTrack::Match: {
		// Light march: triangle plucks every 0.45s, low pulse on beat.
		static const int pat[] = { 0, 3, 2, 4, 1, 3, 2, 5 };
		const float f = kScale[pat[m.step % 8]];
		spawn(Wave::Triangle, f, 0.10f, 0.09f, 0.0f, 0.0f, true, 0.12f);
		if (m.step % 4 == 0) {
			spawn(Wave::Square, kScale[0] * 0.25f, 0.08f, 0.06f, 0.0f, 0.0f, true, 0.1f);
		}
		m.stepTimer = 0.45f;
		++m.step;
		break;
	}
	case MusicTrack::ResultsSting: {
		// One ascending arpeggio, then silence.
		if (!m.stingDone) {
			m.stingDone = true;
			for (int i = 0; i < 4; ++i) {
				spawn(Wave::Triangle, kScale[i <= 2 ? i * 2 : 5], 0.12f, 0.14f, 0.14f * static_cast<float>(i), 0.0f,
					  true, 0.2f);
			}
		}
		m.stepTimer = 10.0f;
		break;
	}
	default:
		break;
	}
}

void streamCb(float* buffer, int numFrames, int numChannels)
{
	std::lock_guard<std::mutex> guard(g_audio.lock);
	const float dt = 1.0f / static_cast<float>(saudio_sample_rate());
	musicTick(static_cast<float>(numFrames) * dt);

	for (int f = 0; f < numFrames; ++f) {
		// #199: ~50ms exponential glide toward the duck target — no clicks on focus change.
		g_audio.duck += (g_audio.duckTarget - g_audio.duck) * 0.0005f;
		float s = 0.0f;
		for (Voice& v : g_audio.voices) {
			if (!v.active) {
				continue;
			}
			v.t += dt;
			const float env = envelope(v);
			if (env < 0.0f) {
				v.active = false;
				continue;
			}
			if (v.t >= 0.0f) {
				v.freq += v.freqSlide * dt;
				if (v.freq < 20.0f) {
					v.freq = 20.0f;
				}
				v.phase += v.freq * dt;
				while (v.phase >= 1.0f) {
					v.phase -= 1.0f;
				}
				const float vol = v.gain * env * (v.isMusic ? g_audio.musicVol : g_audio.sfxVol);
				s += sampleWave(v) * vol;
			}
		}
		s *= g_audio.masterVol * g_audio.duck;
		// Soft clip.
		if (s > 1.0f) {
			s = 1.0f;
		}
		if (s < -1.0f) {
			s = -1.0f;
		}
		for (int c = 0; c < numChannels; ++c) {
			buffer[f * numChannels + c] = s;
		}
	}
}

} // namespace

bool audioInit()
{
	if (g_audio.ready) {
		return true;
	}
	saudio_desc desc = {};
	desc.stream_cb = streamCb;
	desc.num_channels = 1;
	saudio_setup(&desc);
	g_audio.ready = saudio_isvalid();
	return g_audio.ready;
}

void audioShutdown()
{
	if (!g_audio.ready) {
		return;
	}
	saudio_shutdown();
	g_audio.ready = false;
}

bool audioReady()
{
	return g_audio.ready;
}

void audioSetVolumes(float master, float sfx, float music)
{
	std::lock_guard<std::mutex> guard(g_audio.lock);
	auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
	g_audio.masterVol = clamp01(master);
	g_audio.sfxVol = clamp01(sfx);
	g_audio.musicVol = clamp01(music);
}

void audioSetDucked(bool ducked)
{
	std::lock_guard<std::mutex> guard(g_audio.lock);
	g_audio.duckTarget = ducked ? 0.15f : 1.0f; // #199
}

void sfxPlay(Sfx s)
{
	if (!g_audio.ready) {
		return;
	}
	std::lock_guard<std::mutex> guard(g_audio.lock);
	switch (s) {
	case Sfx::CardPlay:
		spawn(Wave::Square, 520.0f, 0.05f, 0.16f, 0.0f, 1400.0f);
		break;
	case Sfx::Damage:
		spawn(Wave::Noise, 400.0f, 0.06f, 0.20f);
		spawn(Wave::Square, 140.0f, 0.07f, 0.16f, 0.0f, -300.0f);
		break;
	case Sfx::BigDamage:
		spawn(Wave::Noise, 300.0f, 0.12f, 0.26f);
		spawn(Wave::Square, 110.0f, 0.14f, 0.22f, 0.0f, -220.0f);
		spawn(Wave::Square, 55.0f, 0.16f, 0.18f, 0.02f);
		break;
	case Sfx::Shield:
		spawn(Wave::Sine, 320.0f, 0.10f, 0.16f, 0.0f, 2200.0f);
		break;
	case Sfx::Draw:
		spawn(Wave::Square, 880.0f, 0.015f, 0.10f);
		break;
	case Sfx::TowerDestroy:
		spawn(Wave::Noise, 250.0f, 0.30f, 0.28f, 0.0f, 0.0f, false, 0.25f);
		spawn(Wave::Square, 220.0f, 0.30f, 0.18f, 0.0f, -320.0f, false, 0.2f);
		spawn(Wave::Square, 165.0f, 0.25f, 0.14f, 0.12f, -240.0f, false, 0.2f);
		break;
	case Sfx::UiClick:
		spawn(Wave::Square, 1200.0f, 0.012f, 0.07f);
		break;
	case Sfx::IllegalBuzz:
		spawn(Wave::Square, 110.0f, 0.12f, 0.16f);
		spawn(Wave::Square, 104.0f, 0.12f, 0.12f);
		break;
	case Sfx::YourTurn:
		spawn(Wave::Sine, 660.0f, 0.07f, 0.14f);
		spawn(Wave::Sine, 880.0f, 0.10f, 0.14f, 0.09f);
		break;
	case Sfx::Win:
		for (int i = 0; i < 5; ++i) {
			spawn(Wave::Triangle, kScale[i] * (i == 4 ? 2.0f : 1.0f), 0.10f, 0.16f, 0.11f * static_cast<float>(i));
		}
		break;
	}
}

void sfxEventStinger(EventKind kind)
{
	if (!g_audio.ready) {
		return;
	}
	std::lock_guard<std::mutex> guard(g_audio.lock);
	switch (kind) {
	case EventKind::Sandstorm:
		spawn(Wave::Noise, 200.0f, 0.35f, 0.14f, 0.0f, 0.0f, false, 0.3f);
		spawn(Wave::Sine, 220.0f, 0.2f, 0.08f, 0.0f, -80.0f);
		break;
	case EventKind::Rain:
		for (int i = 0; i < 5; ++i) {
			spawn(Wave::Sine, 900.0f + 120.0f * static_cast<float>(i % 3), 0.02f, 0.07f,
				  0.06f * static_cast<float>(i));
		}
		break;
	case EventKind::Flood:
		spawn(Wave::Sine, 180.0f, 0.3f, 0.16f, 0.0f, 120.0f);
		spawn(Wave::Noise, 150.0f, 0.2f, 0.10f, 0.1f);
		break;
	case EventKind::Cat:
		// "Meow-ish": sine slide up then down.
		spawn(Wave::Sine, 520.0f, 0.10f, 0.15f, 0.0f, 900.0f);
		spawn(Wave::Sine, 760.0f, 0.12f, 0.13f, 0.12f, -700.0f);
		break;
	case EventKind::Dog:
		// "Woof": two low bursts.
		spawn(Wave::Square, 130.0f, 0.06f, 0.20f, 0.0f, -180.0f);
		spawn(Wave::Square, 120.0f, 0.07f, 0.20f, 0.14f, -160.0f);
		break;
	case EventKind::Blackout:
		spawn(Wave::Sine, 440.0f, 0.18f, 0.13f, 0.0f, -320.0f);
		break;
	case EventKind::Ants:
		for (int i = 0; i < 6; ++i) {
			spawn(Wave::Square, 1400.0f + 200.0f * static_cast<float>(i % 2), 0.015f, 0.05f,
				  0.04f * static_cast<float>(i));
		}
		break;
	default:
		break;
	}
}

void musicPlay(MusicTrack t)
{
	if (!g_audio.ready) {
		return;
	}
	std::lock_guard<std::mutex> guard(g_audio.lock);
	if (g_audio.music.track == t) {
		return;
	}
	// Fade out old music voices quickly.
	for (Voice& v : g_audio.voices) {
		if (v.active && v.isMusic) {
			v.dur = 0.0f;
			if (v.t < v.attack) {
				v.t = v.attack;
			}
		}
	}
	g_audio.music.track = t;
	g_audio.music.step = 0;
	g_audio.music.stepTimer = 0.0f;
	g_audio.music.stingDone = false;
}

} // namespace toy
