/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef HOPKINS_SOUND_H
#define HOPKINS_SOUND_H

#include "common/scummsys.h"
#include "common/str.h"
#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "audio/mixer.h"

namespace Hopkins {

class VoiceItem {
public:
	int _status;
	int _wavIndex;
	int fieldC;
	int field14;
};

class SwavItem {
public:
	bool _active;
	Audio::RewindableAudioStream *_audioStream;
	Audio::SoundHandle _soundHandle;
	bool freeSample;
};

class MusicItem {
public:
	bool _active;
};

class SoundItem {
public:
	bool _active;
};

#define VOICE_COUNT 3
#define SWAV_COUNT 50
#define SOUND_COUNT 10

class HopkinsEngine;

class SoundManager {
private:
	HopkinsEngine *_vm;

	int VOICE_STAT(int voiceIndex);
	void stopVoice(int voiceIndex);
	void SDL_LVOICE(Common::String filename, size_t filePosition, size_t entryLength);
	void PLAY_VOICE_SDL();
	bool DEL_SAMPLE_SDL(int wavIndex);
	bool SDL_LoadVoice(const Common::String &filename, size_t fileOffset, size_t entryLength, SwavItem &item);
	void LOAD_SAMPLE2_SDL(int wavIndex, const Common::String &filename, bool freeSample);
	void LOAD_NWAV(const Common::String &file, int wavIndex);
	void PLAY_NWAV(int wavIndex);
	void DEL_NWAV(int wavIndex);
	void PLAY_SAMPLE_SDL(int voiceIndex, int wavIndex);

	/**
	 * Checks voices to see if they're finished
	 */
	void checkVoices();

	/**
	 * Creates an audio stream based on a passed raw stream
	 */
	Audio::RewindableAudioStream *makeSoundStream(Common::SeekableReadStream *stream);
public:
	Audio::SoundHandle _musicHandle;
	int SPECIAL_SOUND;
	int _soundVolume;
	int _voiceVolume;
	int _musicVolume;
	bool _soundOffFl;
	bool _musicOffFl;
	bool _voiceOffFl;
	bool _textOffFl;
	bool _soundFl;
	bool VBL_MERDE;
	int SOUND_NUM;
	bool MOD_FLAG;
	int old_music;

	VoiceItem Voice[VOICE_COUNT];
	SwavItem Swav[SWAV_COUNT];
	SoundItem SOUND[SOUND_COUNT];
	MusicItem Music;
public:
	SoundManager();
	~SoundManager();
	void setParent(HopkinsEngine *vm);

	void WSOUND_INIT();
	void VERIF_SOUND();
	void LOAD_ANM_SOUND();
	void playAnim_SOUND(int soundNumber);
	void LOAD_WAV(const Common::String &file, int wavIndex);
	void WSOUND(int soundNumber);
	void WSOUND_OFF();
	void PLAY_MOD(const Common::String &file);
	void loadMusic(const Common::String &file);
	void playMusic();
	void stopMusic();
	void delMusic();


	bool mixVoice(int voiceId, int voiceMode);
	void DEL_SAMPLE(int soundIndex);
	void playSound(const Common::String &file);
	void PLAY_SOUND2(const Common::String &file2);
	void MODSetSampleVolume();
	void MODSetVoiceVolume();
	void MODSetMusicVolume(int volume);
	void loadSample(int wavIndex, const Common::String &file);
	void playSample(int wavIndex, int voiceMode);
	void PLAY_SAMPLE2(int idx);
	void playWav(int wavIndex);

	void syncSoundSettings();
	void updateScummVMSoundSettings();
	void checkSounds();
	Common::String setExtension(const Common::String &str, const Common::String &ext);
};

} // End of namespace Hopkins

#endif /* HOPKINS_SOUND_H */
