/* ScummVM - Scumm Interpreter
 * Copyright (C) 2001  Ludvig Strigeus
 * Copyright (C) 2001-2004 The ScummVM project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 *
 */

#include "stdafx.h"
#include "common/file.h"
#include "common/util.h"

#include "sound/mixer.h"
#include "sound/rate.h"
#include "sound/audiostream.h"
#include "sound/flac.h"
#include "sound/mp3.h"
#include "sound/vorbis.h"


#pragma mark -
#pragma mark --- Channel classes ---
#pragma mark -


/**
 * Channels used by the sound mixer.
 */
class Channel {
private:
	SoundMixer *_mixer;
	PlayingSoundHandle *_handle;
	bool _autofreeStream;
	const bool _isMusic;
	byte _volume;
	int8 _balance;
	bool _paused;
	int _id;
	uint32 _samplesConsumed;
	uint32 _samplesDecoded;
	uint32 _mixerTimeStamp;

protected:
	RateConverter *_converter;
	AudioStream *_input;

public:

	Channel(SoundMixer *mixer, PlayingSoundHandle *handle, bool isMusic, int id = -1);
	Channel(SoundMixer *mixer, PlayingSoundHandle *handle, AudioStream *input, bool autofreeStream, bool isMusic, bool reverseStereo = false, int id = -1);
	virtual ~Channel();

	void mix(int16 *data, uint len);

	bool isFinished() const {
		return _input->endOfStream();
	}
	bool isMusicChannel() const {
		return _isMusic;
	}
	void pause(bool paused) {
		_paused = paused;
	}
	bool isPaused() {
		return _paused;
	}
	void setVolume(const byte volume) {
		_volume = volume;
	}
	void setBalance(const int8 balance) {
		_balance = balance;
	}
	int getId() const {
		return _id;
	}
	uint32 getElapsedTime();
};

class ChannelStream : public Channel {
public:
	ChannelStream(SoundMixer *mixer, PlayingSoundHandle *handle, uint rate, byte flags, uint32 buffer_size);
	void append(void *sound, uint32 size);

	void finish();
};


#pragma mark -
#pragma mark --- Procedural stream ---
#pragma mark -


class ProcInputStream : public AudioStream {
public:
	typedef void (*InputProc)(void *refCon, int16 *data, uint len);

private:
	const int _rate;
	const bool _isStereo;
	InputProc _proc;
	void *_refCon;

public:
	ProcInputStream(int rate, bool stereo, InputProc proc, void *refCon)
		: _rate(rate), _isStereo(stereo), _proc(proc), _refCon(refCon) { }
	int readBuffer(int16 *buffer, const int numSamples) {
		memset(buffer, 0, 2 * numSamples);	// FIXME
		(_proc)(_refCon, buffer, _isStereo ? (numSamples / 2) : numSamples);
		return numSamples;
	}
	bool isStereo() const { return _isStereo; }
	bool endOfData() const { return false; }
	
	int getRate() const { return _rate; }
};


#pragma mark -
#pragma mark --- SoundMixer ---
#pragma mark -


SoundMixer::SoundMixer() {
	_syst = OSystem::instance();
	_mutex = _syst->createMutex();

	_premixChannel = 0;
	int i = 0;

	_globalVolume = 0;
	_musicVolume = 0;

	_paused = false;
	
	for (i = 0; i != NUM_CHANNELS; i++)
		_channels[i] = 0;

	_mixerReady = _syst->setSoundCallback(mixCallback, this);
	_outputRate = (uint)_syst->getOutputSampleRate();

	if (_outputRate == 0)
		error("OSystem returned invalid sample rate");

	debug(1, "Output sample rate: %d Hz", _outputRate);
}

SoundMixer::~SoundMixer() {
	_syst->clearSoundCallback();
	stopAll();

	delete _premixChannel;
	_premixChannel = 0;

	_syst->deleteMutex(_mutex);
}

bool SoundMixer::isPaused() {
	return _paused;
}

void SoundMixer::setupPremix(PremixProc *proc, void *param) {
	Common::StackLock lock(_mutex);

	delete _premixChannel;
	_premixChannel = 0;
	
	if (proc == 0)
		return;

	// Create an input stream
	AudioStream *input = new ProcInputStream(_outputRate, true, proc, param);

	// Create the channel
	_premixChannel = new Channel(this, 0, input, true, false);
}

void SoundMixer::setupPremix(AudioStream *stream) {
	Common::StackLock lock(_mutex);

	delete _premixChannel;
	_premixChannel = 0;
	
	if (stream == 0)
		return;

	// Create the channel
	_premixChannel = new Channel(this, 0, stream, false, false);
}

void SoundMixer::newStream(PlayingSoundHandle *handle, uint rate, byte flags, uint32 buffer_size, byte volume, int8 balance) {
	Common::StackLock lock(_mutex);

	Channel *chan = new ChannelStream(this, handle, rate, flags, buffer_size);
	chan->setVolume(volume);
	chan->setBalance(balance);
	insertChannel(handle, chan);
}

void SoundMixer::appendStream(PlayingSoundHandle handle, void *sound, uint32 size) {
	Common::StackLock lock(_mutex);
	
	if (!handle.isActive())
		return;

	int index = handle.getIndex();

	if ((index < 0) || (index >= NUM_CHANNELS)) {
		warning("soundMixer::appendStream has invalid index %d", index);
		return;
	}

	ChannelStream *chan;
#if !defined(_WIN32_WCE) && !defined(__PALM_OS__)
	chan = dynamic_cast<ChannelStream *>(_channels[index]);
#else
	chan = (ChannelStream*)_channels[index];
#endif
	if (!chan) {
		error("Trying to append to nonexistant stream : %d", index);
	} else {
		chan->append(sound, size);
	}
}

void SoundMixer::endStream(PlayingSoundHandle handle) {
	Common::StackLock lock(_mutex);

	// Simply ignore stop requests for handles of sounds that already terminated
	if (!handle.isActive())
		return;

	int index = handle.getIndex();

	if ((index < 0) || (index >= NUM_CHANNELS)) {
		warning("soundMixer::endStream has invalid index %d", index);
		return;
	}

	ChannelStream *chan;
#if !defined(_WIN32_WCE) && !defined(__PALM_OS__)
	chan = dynamic_cast<ChannelStream *>(_channels[index]);
#else
	chan = (ChannelStream*)_channels[index];
#endif
	if (!chan) {
		error("Trying to end a nonexistant streamer : %d", index);
	} else {
		chan->finish();
	}
}

void SoundMixer::insertChannel(PlayingSoundHandle *handle, Channel *chan) {

	int index = -1;
	for (int i = 0; i != NUM_CHANNELS; i++) {
		if (_channels[i] == 0) {
			index = i;
			break;
		}
	}
	if(index == -1) {
		warning("SoundMixer::out of mixer slots");
		delete chan;
		return;
	}

	_channels[index] = chan;
	if (handle)
		handle->setIndex(index);
}

void SoundMixer::playRaw(PlayingSoundHandle *handle, void *sound, uint32 size, uint rate, byte flags, int id, byte volume, int8 balance, uint32 loopStart, uint32 loopEnd) {
	Common::StackLock lock(_mutex);

	// Prevent duplicate sounds
	if (id != -1) {
		for (int i = 0; i != NUM_CHANNELS; i++)
			if (_channels[i] != 0 && _channels[i]->getId() == id) {
				if ((flags & SoundMixer::FLAG_AUTOFREE) != 0)
					free(sound);
				return;
			}
	}

	// Create the input stream
	AudioStream *input;
	if (flags & SoundMixer::FLAG_LOOP) {
		if (loopEnd == 0) {
			input = makeLinearInputStream(rate, flags, (byte *)sound, size, 0, size);
		} else {
			assert(loopStart < loopEnd && loopEnd <= size);
			input = makeLinearInputStream(rate, flags, (byte *)sound, size, loopStart, loopEnd - loopStart);
		}
	} else {
		input = makeLinearInputStream(rate, flags, (byte *)sound, size, 0, 0);
	}

	// Create the channel
	Channel *chan = new Channel(this, handle, input, true, false, (flags & SoundMixer::FLAG_REVERSE_STEREO) != 0, id);
	chan->setVolume(volume);
	chan->setBalance(balance);
	insertChannel(handle, chan);
}

void SoundMixer::playInputStream(PlayingSoundHandle *handle, AudioStream *input, bool isMusic, byte volume, int8 balance, int id, bool autofreeStream) {
	Common::StackLock lock(_mutex);

	if (input == 0) {
		warning("input stream is 0");
		return;
	}

	// Prevent duplicate sounds
	if (id != -1) {
		for (int i = 0; i != NUM_CHANNELS; i++)
			if (_channels[i] != 0 && _channels[i]->getId() == id) {
				if (autofreeStream)
					delete input;
				return;
			}
	}

	// Create the channel
	Channel *chan = new Channel(this, handle, input, autofreeStream, isMusic, false, id);
	chan->setVolume(volume);
	chan->setBalance(balance);
	insertChannel(handle, chan);
}

void SoundMixer::mix(int16 *buf, uint len) {
	Common::StackLock lock(_mutex);

	//  zero the buf
	memset(buf, 0, 2 * len * sizeof(int16));

	if (!_paused) {
		if (_premixChannel)
			_premixChannel->mix(buf, len);

		// now mix all channels
		for (int i = 0; i != NUM_CHANNELS; i++)
			if (_channels[i]) {
				if (_channels[i]->isFinished()) {
					delete _channels[i];
					_channels[i] = 0;
				} else if (!_channels[i]->isPaused())
					_channels[i]->mix(buf, len);
			}
	}
}

void SoundMixer::mixCallback(void *s, byte *samples, int len) {
	assert(s);
	assert(samples);
	// Len is the number of bytes in the buffer; we divide it by
	// four to get the number of samples (stereo 16 bit).
	((SoundMixer *)s)->mix((int16 *)samples, len >> 2);
}

void SoundMixer::stopAll() {
	Common::StackLock lock(_mutex);
	for (int i = 0; i != NUM_CHANNELS; i++)
		if (_channels[i] != 0) {
			delete _channels[i];
			_channels[i] = 0;
		}
}

void SoundMixer::stopID(int id) {
	Common::StackLock lock(_mutex);
	for (int i = 0; i != NUM_CHANNELS; i++) {
		if (_channels[i] != 0 && _channels[i]->getId() == id) {
			delete _channels[i];
			_channels[i] = 0;
		}
	}
}

void SoundMixer::stopHandle(PlayingSoundHandle handle) {
	Common::StackLock lock(_mutex);

	// Simply ignore stop requests for handles of sounds that already terminated
	if (!handle.isActive())
		return;

	int index = handle.getIndex();

	if ((index < 0) || (index >= NUM_CHANNELS)) {
		warning("soundMixer::stopHandle has invalid index %d", index);
		return;
	}

	if (_channels[index]) {
		delete _channels[index];
		_channels[index] = 0;
	}
}

void SoundMixer::setChannelVolume(PlayingSoundHandle handle, byte volume) {
	Common::StackLock lock(_mutex);

	if (!handle.isActive())
		return;

	int index = handle.getIndex();

	if ((index < 0) || (index >= NUM_CHANNELS)) {
		warning("soundMixer::setChannelVolume has invalid index %d", index);
		return;
	}

	if (_channels[index])
		_channels[index]->setVolume(volume);
}

void SoundMixer::setChannelBalance(PlayingSoundHandle handle, int8 balance) {
	Common::StackLock lock(_mutex);

	if (!handle.isActive())
		return;

	int index = handle.getIndex();

	if ((index < 0) || (index >= NUM_CHANNELS)) {
		warning("soundMixer::setChannelBalance has invalid index %d", index);
		return;
	}

	if (_channels[index])
		_channels[index]->setBalance(balance);
}

uint32 SoundMixer::getSoundElapsedTime(PlayingSoundHandle handle) {
	Common::StackLock lock(_mutex);

	if (!handle.isActive())
		return 0;

	int index = handle.getIndex();

	if ((index < 0) || (index >= NUM_CHANNELS)) {
		warning("soundMixer::getSoundElapsedTime has invalid index %d", index);
		return 0;
	}

	if (_channels[index])
		return _channels[index]->getElapsedTime();

	warning("soundMixer::getSoundElapsedTime has no channel object for index %d", index);
	return 0;
}

void SoundMixer::pauseAll(bool paused) {
	_paused = paused;
}

void SoundMixer::pauseID(int id, bool paused) {
	Common::StackLock lock(_mutex);
	for (int i = 0; i != NUM_CHANNELS; i++) {
		if (_channels[i] != 0 && _channels[i]->getId() == id) {
			_channels[i]->pause(paused);
			return;
		}
	}
}

void SoundMixer::pauseHandle(PlayingSoundHandle handle, bool paused) {
	Common::StackLock lock(_mutex);

	// Simply ignore pause/unpause requests for handles of sound that alreayd terminated
	if (!handle.isActive())
		return;

	int index = handle.getIndex();

	if ((index < 0) || (index >= NUM_CHANNELS)) {
		warning("soundMixer::pauseHandle has invalid index %d", index);
		return;
	}

	if (_channels[index])
		_channels[index]->pause(paused);
}

bool SoundMixer::isSoundIDActive(int id) {
	Common::StackLock lock(_mutex);
	for (int i = 0; i != NUM_CHANNELS; i++)
		if (_channels[i] && _channels[i]->getId() == id)
			return true;
	return false;
}

bool SoundMixer::hasActiveSFXChannel() {
	// FIXME/TODO: We need to distinguish between SFX and music channels
	Common::StackLock lock(_mutex);
	for (int i = 0; i != NUM_CHANNELS; i++)
		if (_channels[i] && !_channels[i]->isMusicChannel())
			return true;
	return false;
}

void SoundMixer::setVolume(int volume) {
	// Check range
	if (volume > 256)
		volume = 256;
	else if (volume < 0)
		volume = 0;

	_globalVolume = volume;
}

void SoundMixer::setMusicVolume(int volume) {
	// Check range
	if (volume > 256)
		volume = 256;
	else if (volume < 0)
		volume = 0;

	_musicVolume = volume;
}


#pragma mark -
#pragma mark --- Channel implementations ---
#pragma mark -


Channel::Channel(SoundMixer *mixer, PlayingSoundHandle *handle, bool isMusic, int id)
	: _mixer(mixer), _handle(handle), _autofreeStream(true), _isMusic(isMusic),
	  _volume(255), _balance(0), _paused(false), _id(id), _samplesConsumed(0),
	  _samplesDecoded(0), _mixerTimeStamp(0), _converter(0), _input(0) {
	assert(mixer);
}

Channel::Channel(SoundMixer *mixer, PlayingSoundHandle *handle, AudioStream *input,
				bool autofreeStream, bool isMusic, bool reverseStereo, int id)
	: _mixer(mixer), _handle(handle), _autofreeStream(autofreeStream), _isMusic(isMusic),
	  _volume(255), _balance(0), _paused(false), _id(id), _samplesConsumed(0),
	  _samplesDecoded(0), _mixerTimeStamp(0), _converter(0), _input(input) {
	assert(mixer);
	assert(input);

	// Get a rate converter instance
	_converter = makeRateConverter(_input->getRate(), mixer->getOutputRate(), _input->isStereo(), reverseStereo);
}

Channel::~Channel() {
	delete _converter;
	if (_autofreeStream)
		delete _input;
	if (_handle)
		_handle->resetIndex();
}

/* len indicates the number of sample *pairs*. So a value of
   10 means that the buffer contains twice 10 sample, each
   16 bits, for a total of 40 bytes.
 */
void Channel::mix(int16 *data, uint len) {
	assert(_input);

	if (_input->endOfData()) {
		// TODO: call drain method
	} else {
		assert(_converter);

		// From the channel balance/volume and the global volume, we compute
		// the effective volume for the left and right channel. Note the
		// slightly odd divisor: the 255 reflects the fact that the maximal
		// value for _volume is 255, while the 127 is there because the
		// balance value ranges from -127 to 127.  The mixer (music/sound)
		// volume is in the range 0 - 256.
		// Hence, the vol_l/vol_r values will be in that range, too
		
		int vol = (isMusicChannel() ? _mixer->getMusicVolume() : _mixer->getVolume()) * _volume;
		st_volume_t vol_l, vol_r;

		if (_balance == 0) {
			vol_l = vol / 255;
			vol_r = vol / 255;
		} else if (_balance < 0) {
			vol_l = vol / 255;
			vol_r = ((127 + _balance) * vol) / (255 * 127);
		} else {
			vol_l = ((127 - _balance) * vol) / (255 * 127);
			vol_r = vol / 255;
		}

		_samplesConsumed = _samplesDecoded;
		_mixerTimeStamp = g_system->getMillis();

		_converter->flow(*_input, data, len, vol_l, vol_r);

		_samplesDecoded += len;
	}
}

uint32 Channel::getElapsedTime() {
	if (_mixerTimeStamp == 0)
		return 0;

	// Convert the number of samples into a time duration. To avoid
	// overflow, this has to be done in a somewhat non-obvious way.

	uint rate = _mixer->getOutputRate();

	uint32 seconds = _samplesConsumed / rate;
	uint32 milliseconds = (1000 * (_samplesConsumed % rate)) / rate;

	uint32 delta = g_system->getMillis() - _mixerTimeStamp;

	// In theory it would seem like a good idea to limit the approximation
	// so that it never exceeds the theoretical upper bound set by
	// _samplesDecoded. Meanwhile, back in the real world, doing so makes
	// the Broken Sword cutscenes noticeably jerkier. I guess the mixer
	// isn't invoked at the regular intervals that I first imagined.

	// FIXME: This won't work very well if the sound is paused.
	return 1000 * seconds + milliseconds + delta;
}

ChannelStream::ChannelStream(SoundMixer *mixer, PlayingSoundHandle *handle,
							uint rate, byte flags, uint32 buffer_size)
	: Channel(mixer, handle, false) {
	// Create the input stream
	_input = makeAppendableAudioStream(rate, flags, buffer_size);

	// Get a rate converter instance
	_converter = makeRateConverter(_input->getRate(), mixer->getOutputRate(), _input->isStereo(), (flags & SoundMixer::FLAG_REVERSE_STEREO) != 0);
}

void ChannelStream::finish() {
	((AppendableAudioStream *)_input)->finish();
}

void ChannelStream::append(void *data, uint32 len) {
	((AppendableAudioStream *)_input)->append((const byte *)data, len);
}
