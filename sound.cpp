
#include "game.h"
#include "resource.h"
#include "system.h"
#include "util.h"

enum {
	kFlagPlaying = 1 << 0,
	kFlagPaused  = 1 << 1, // no PCM
	kFlagNoCode  = 1 << 2  // no bytecode
};

static const bool kLimitSounds = false; // limit the number of active playing sounds

// if x < 90, lut[x] ~= x / 2
// if x > 90, lut[x] ~= 45 + (x - 90) * 2
static const uint8_t _volumeRampTable[129] = {
	0x00, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06,
	0x06, 0x07, 0x07, 0x07, 0x08, 0x08, 0x09, 0x09, 0x09, 0x0A, 0x0A, 0x0B, 0x0B, 0x0C, 0x0C, 0x0C,
	0x0D, 0x0D, 0x0E, 0x0E, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13, 0x14,
	0x14, 0x15, 0x15, 0x16, 0x16, 0x17, 0x17, 0x18, 0x18, 0x19, 0x1A, 0x1A, 0x1B, 0x1B, 0x1C, 0x1C,
	0x1D, 0x1D, 0x1E, 0x1F, 0x1F, 0x20, 0x20, 0x21, 0x22, 0x22, 0x23, 0x23, 0x24, 0x25, 0x25, 0x26,
	0x27, 0x27, 0x28, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2F, 0x30, 0x32, 0x33, 0x35, 0x37, 0x38,
	0x3A, 0x3C, 0x3D, 0x3F, 0x41, 0x43, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0x4E, 0x50, 0x52, 0x54, 0x56,
	0x59, 0x5B, 0x5D, 0x5F, 0x62, 0x64, 0x66, 0x69, 0x6B, 0x6E, 0x70, 0x73, 0x76, 0x78, 0x7B, 0x7E,
	0x80
};

static uint32_t sssGroupValue(uint8_t source, uint8_t mask, SssInfo *s) {
	uint32_t value = 0;
	assert(source < 3); // 0,1,2

	// bits 20..24
	value = source;
	// bits 24..32
	value |= mask << 4;

	// bits 16..20
	value <<= 4;
	value |= s->sampleIndex & 15;

	// bits 12..16
	value <<= 4;
	value |= s->concurrencyMask;

	// bits 0..12
	value <<= 12;
	value |= (s->sssBankIndex & 0xFFF);

	return value;
}

static bool compareSssGroup(uint32_t flags_a, uint32_t flags_b) {
	if (0) {
		// the x86 code compares the 3 bit fields.
		// the original code possibly used a structure with bit fields that was optimized by the compiler as a uint32_t
		if (((flags_a >> 20) & 15) == ((flags_b >> 20) & 15)) { // source : 0,1,2 (Andy, monster1, monster2)
			if ((flags_a & 0xFFF) == (flags_b & 0xFFF)) { // bank index
				return (flags_a >> 24) == (flags_b >> 24); // sample index bit 0..31, used as a mask lut[][] |= 1 << bit
			}
		}
		return false;
	}
	// we can instead simply compare masked integers
	return (flags_a & 0xFFF00FFF) == (flags_b & 0xFFF00FFF);
}

// returns the active samples for the table/source/bank
static uint32_t *getSssGroupPtr(Resource *res, int num, uint32_t flags) {
	const int source = (flags >> 20) & 15; // 0,1,2
	assert(source < 3);
	const int bankIndex = flags & 0xFFF;
	assert(bankIndex < res->_sssHdr.banksDataCount);
	switch (num) {
	case 1:
		return &res->_sssGroup1[source][bankIndex];
	case 2:
		return &res->_sssGroup2[source][bankIndex];
	case 3:
		return &res->_sssGroup3[source][bankIndex];
	}
	error("Invalid sssGroup %d", num);
	return 0;
}

void Game::muteSound() {
	MixerLock ml(&_mix);
	_snd_muted = true;
	_mix._mixingQueueSize = 0;
}

void Game::unmuteSound() {
	MixerLock ml(&_mix);
	_snd_muted = false;
	_mix._mixingQueueSize = 0;
}

void Game::resetSound() {
	MixerLock ml(&_mix);
	clearSoundObjects();
}

SssObject *Game::findLowestRankSoundObject() const {
	SssObject *so = 0;
	if (_playingSssObjectsCount >= _playingSssObjectsMax && _sssObjectsList1) {
		so = _sssObjectsList1;
		for (SssObject *current = so->nextPtr; current; current = current->nextPtr) {
			if (so->currentPriority > current->currentPriority) {
				so = current;
			}
		}
	}
	return so;
}

void Game::removeSoundObjectFromList(SssObject *so) {
	debug(kDebug_SOUND, "removeSoundObjectFromList so %p flags 0x%x", so, so->flags);
	so->pcm = 0;
	if ((so->flags & kFlagPlaying) != 0) {

		// remove from linked list1
		SssObject *next = so->nextPtr;
		SssObject *prev = so->prevPtr;
		if (next) {
			next->prevPtr = prev;
		}
		if (prev) {
			prev->nextPtr = next;
		} else {
			assert(so == _sssObjectsList1);
			_sssObjectsList1 = next;
		}
		--_playingSssObjectsCount;

		if (kLimitSounds) {
			if (_lowRankSssObject == so || (_playingSssObjectsCount < _playingSssObjectsMax && _lowRankSssObject)) {
				_lowRankSssObject = findLowestRankSoundObject();
			}
		}
	} else {

		// remove from linked list2
		SssObject *next = so->nextPtr;
		SssObject *prev = so->prevPtr;
		if (next) {
			next->prevPtr = prev;
		}
		if (prev) {
			prev->nextPtr = next;
		} else {
			assert(so == _sssObjectsList2);
			_sssObjectsList2 = next;
		}
	}
}

void Game::updateSoundObject(SssObject *so) {
	_sssUpdatedObjectsTable[so->num] = true;
	_sssObjectsChanged = so->filter->changed;
	if ((so->flags & kFlagNoCode) == 0) {
		if (so->pcm == 0) {
			if (so->codeDataStage1) {
				so->codeDataStage1 = executeSssCode(so, so->codeDataStage1);
			}
			if (so->pcm == 0) {
				return;
			}
			if (so->codeDataStage4) {
				so->codeDataStage4 = executeSssCode(so, so->codeDataStage4);
			}
			if (so->pcm == 0) {
				return;
			}
		} else {
			if (so->codeDataStage1) {
				so->codeDataStage1 = executeSssCode(so, so->codeDataStage1);
			}
			if (so->pcm == 0) {
				return;
			}
			if (so->panningPtr) {
				const int panning = getSoundObjectPanning(so);
				if (panning != so->panning) {
					so->panning = panning;
					_sssObjectsChanged = true;
				}
			} else {
				if (so->codeDataStage2) {
					so->codeDataStage2 = executeSssCode(so, so->codeDataStage2);
				}
			}
			if (so->pcm == 0) {
				return;
			}
			if (so->codeDataStage3) {
				so->codeDataStage3 = executeSssCode(so, so->codeDataStage3);
			}
			if (so->pcm == 0) {
				return;
			}
			if (so->codeDataStage4) {
				so->codeDataStage4 = executeSssCode(so, so->codeDataStage4);
			}
			if (so->pcm == 0) {
				return;
			}
			if (_sssObjectsChanged && (so->flags & kFlagPlaying) != 0) {
				setSoundObjectPanning(so);
			}
		}
	} else if ((so->flags & kFlagPlaying) != 0) {
		if (so->panningPtr) {
			const int panning = getSoundObjectPanning(so);
			if (panning != so->panning) {
				so->panning = panning;
				_sssObjectsChanged = true;
				setSoundObjectPanning(so);
			}
		} else if (_sssObjectsChanged) {
			setSoundObjectPanning(so);
		}
	}
	if (so->pcmFramesCount != 0) {
		--so->pcmFramesCount;
		if ((so->flags & kFlagPaused) == 0) {
			++so->currentPcmFrame;
		}
	} else {
		const uint32_t flags = so->flags0;
		removeSoundObjectFromList(so);
		if (so->nextSoundBank != -1) {
			LvlObject *tmp = _currentSoundLvlObject;
			_currentSoundLvlObject = so->lvlObject;
			createSoundObject(so->nextSoundBank, so->nextSoundSample, flags);
			_currentSoundLvlObject = tmp;
			return;
		}
		updateSssGroup2(flags);
	}
}

void Game::sssOp12_removeSounds2(int num, uint8_t source, uint8_t sampleIndex) {
	assert(source < 3);
	assert(num < _res->_sssHdr.banksDataCount);
	assert(sampleIndex < 32);
	const uint32_t mask = (1 << sampleIndex);
	_res->_sssGroup1[source][num] &= ~mask;
	for (SssObject *so = _sssObjectsList1; so; so = so->nextPtr) {
		if (so->bankIndex == num && ((so->flags1 >> 20) & 15) == source && (so->flags1 >> 24) == sampleIndex) {
			so->codeDataStage3 = 0;
			if (so->codeDataStage4 == 0) {
				removeSoundObjectFromList(so);
			}
			so->nextSoundBank = -1;
			so->delayCounter = -2;
		}
	}
	for (SssObject *so = _sssObjectsList2; so; so = so->nextPtr) {
		if (so->bankIndex == num && ((so->flags1 >> 20) & 15) == source && (so->flags1 >> 24) == sampleIndex) {
			so->codeDataStage3 = 0;
			if (so->codeDataStage4 == 0) {
				removeSoundObjectFromList(so);
			}
			so->nextSoundBank = -1;
			so->delayCounter = -2;
		}
	}
	while (_sssObjectsCount > 0 && _sssObjectsTable[_sssObjectsCount - 1].pcm == 0) {
		--_sssObjectsCount;
	}
}

void Game::sssOp16_resumeSound(SssObject *so) {
	debug(kDebug_SOUND, "sssOp16_resumeSound so %p flags 0x%x", so, so->flags);
	if ((so->flags & kFlagPaused) != 0) {
		SssObject *next = so->nextPtr;
		SssObject *prev = so->prevPtr;
		SssPcm *pcm = so->pcm;
		so->pcm = 0;
		if (next) {
			next->prevPtr = prev;
		}
		if (prev) {
			prev->nextPtr = next;
		} else {
			assert(so == _sssObjectsList2);
			_sssObjectsList2 = next;
		}
		so->pcm = pcm;
		so->flags &= ~kFlagPaused;
		prependSoundObjectToList(so);
	}
}

void Game::sssOp17_pauseSound(SssObject *so) {
	debug(kDebug_SOUND, "sssOp17_pauseSound so %p flags 0x%x", so, so->flags);
	if ((so->flags & kFlagPaused) == 0) {
		SssPcm *pcm = so->pcm;
		SssObject *prev = so->prevPtr;
		SssObject *next = so->nextPtr;
		so->pcm = 0;
		if ((so->flags & kFlagPlaying) != 0) {
			if (next) {
				next->prevPtr = prev;
			}
			if (prev) {
				prev->nextPtr = next;
			} else {
				assert(so == _sssObjectsList1);
				_sssObjectsList1 = next;
			}
			--_playingSssObjectsCount;

			if (kLimitSounds) {
				if (so == _lowRankSssObject || (_playingSssObjectsCount < _playingSssObjectsMax && _lowRankSssObject)) {
					_lowRankSssObject = findLowestRankSoundObject();
				}
			}
		} else {
			if (next) {
				next->prevPtr = prev;
			}
			if (prev) {
				prev->nextPtr = next;
			} else {
				assert(so == _sssObjectsList2);
				_sssObjectsList2 = next;
			}
		}
		so->pcm = pcm;
		so->flags = (so->flags & ~kFlagPlaying) | kFlagPaused;
		prependSoundObjectToList(so);
	}
}

void Game::sssOp4_removeSounds(uint32_t flags) {
	const uint32_t mask = 1 << (flags >> 24);
	*getSssGroupPtr(_res, 1, flags) &= ~mask;
	for (SssObject *so = _sssObjectsList1; so; so = so->nextPtr) {
		if (((so->flags1 ^ flags) & 0xFFFF0FFF) == 0) { // (a & m) == (b & m)
			so->codeDataStage3 = 0;
			if (so->codeDataStage4 == 0) {
				removeSoundObjectFromList(so);
			}
			so->nextSoundBank = -1;
			so->delayCounter = -2;
		}
	}
	for (SssObject *so = _sssObjectsList2; so; so = so->nextPtr) {
		if (((so->flags1 ^ flags) & 0xFFFF0FFF) == 0) {
			so->codeDataStage3 = 0;
			if (so->codeDataStage4 == 0) {
				removeSoundObjectFromList(so);
			}
			so->nextSoundBank = -1;
			so->delayCounter = -2;
		}
	}
	while (_sssObjectsCount > 0 && _sssObjectsTable[_sssObjectsCount - 1].pcm == 0) {
		--_sssObjectsCount;
	}
}

const uint8_t *Game::executeSssCode(SssObject *so, const uint8_t *code, bool tempSssObject) {
	while (1) {
		debug(kDebug_SOUND, "executeSssCode() code %d", *code);
		switch (*code) {
		case 0:
			return 0;
		case 2: // add_sound
			if (so->delayCounter >= -1) {
				LvlObject *tmp = _currentSoundLvlObject;
				_currentSoundLvlObject = so->lvlObject;
				createSoundObject(READ_LE_UINT16(code + 2), (int8_t)code[1], so->flags0);
				_currentSoundLvlObject = tmp;
			}
			code += 4;
			if (so->pcm == 0) {
				return code;
			}
			break;
		case 4: { // remove_sound
				const uint8_t sampleIndex = code[1];
				const uint16_t bankIndex = READ_LE_UINT16(code + 2);
				uint32_t flags = (so->flags0 & 0xFFF0F000);
				flags |= ((sampleIndex & 0xF) << 16) | (bankIndex & 0xFFF);
				sssOp4_removeSounds(flags);
				code += 4;
			}
			break;
		case 5: { // seek_forward
				const int32_t frame = READ_LE_UINT32(code + 4);
				if (so->currentPcmFrame < frame) {
					so->currentPcmFrame = frame;
					if (so->pcm) {
						const int16_t *ptr = so->pcm->ptr;
						if (ptr) {
							so->currentPcmPtr = ptr + READ_LE_UINT32(code + 8) / sizeof(int16_t);
						}
					}
				}
				code += 12;
			}
			break;
		case 6: { // repeat_jge
				--so->repeatCounter;
				if (so->repeatCounter < 0) {
					code += 8;
				} else {
					const int32_t offset = READ_LE_UINT32(code + 4);
					code -= offset;
				}
			}
			break;
		case 8: { // seek_backward_delay
				const int32_t frame = READ_LE_UINT32(code + 12);
				if (so->currentPcmFrame > frame) {
					--so->pauseCounter;
					if (so->pauseCounter < 0) {
						code += 16;
						break;
					}
					so->currentPcmFrame = READ_LE_UINT32(code + 8);
					if (so->pcm) {
						const int16_t *ptr = so->pcm->ptr;
						if (ptr) {
							so->currentPcmPtr = ptr + READ_LE_UINT32(code + 4) / sizeof(int16_t);
						}
					}
				}
				return code;
			}
			break;
		case 9: { // modulate_panning
				so->panningModulateCurrent += so->panningModulateDelta;
				const int panning = (so->panningModulateCurrent + 0x8000) >> 16;
				if (panning != so->panning) {
					so->panning = panning;
					_sssObjectsChanged = true;
				}
				--so->panningModulateSteps;
				if (so->panningModulateSteps >= 0) {
					return code;
				}
				code += 4;
			}
			break;
		case 10: { // modulate_volume
				if (so->volumeModulateSteps >= 0) {
					so->volumeModulateCurrent += so->volumeModulateDelta;
					const int volume = (so->volumeModulateCurrent + 0x8000) >> 16;
					if (volume != so->volume) {
						so->volume = volume;
						_sssObjectsChanged = true;
					}
					--so->volumeModulateSteps;
					if (so->volumeModulateSteps >= 0) {
						return code;
					}
				}
				code += 4;
			}
			break;
		case 11: { // set_volume
				if (so->volume != code[1]) {
					so->volume = code[1];
					_sssObjectsChanged = true;
				}
				code += 4;
			}
			break;
		case 12: { // remove_sounds2
				uint32_t va =  so->flags1 >> 24;
				uint32_t vd = (so->flags1 >> 20) & 0xF;
				uint16_t vc = READ_LE_UINT16(code + 2);
				sssOp12_removeSounds2(vc, vd, va);
				code += 4;
			}
			break;
		case 13: { // init_volume_modulation
				const int count = READ_LE_UINT32(code + 4);
				so->volumeModulateSteps = count - 1;
				const int16_t value = READ_LE_UINT16(code + 2);
				if (value == -1) {
					so->volumeModulateCurrent = so->volume << 16;
				} else {
					assert(value >= 0);
					so->volumeModulateCurrent = value << 16;
					so->volume = value;
					_sssObjectsChanged = true;
				}
				so->volumeModulateDelta = ((code[1] << 16) - so->volumeModulateCurrent) / count;
				return code + 8;
			}
			break;
		case 14: { // init_panning_modulation
				const int count = READ_LE_UINT32(code + 8);
				so->panningModulateSteps = count - 1;
				const int16_t value = READ_LE_UINT16(code + 2);
				if (value == -1) {
					so->panningModulateCurrent = so->panning << 16;
				} else {
					assert(value >= 0);
					so->panningModulateCurrent = value << 16;
					so->panning = value;
					_sssObjectsChanged = true;
				}
				so->panningModulateDelta = ((code[1] << 16) - so->panningModulateCurrent) / count;
				return code + 12;
			}
			break;
		case 16: { // resume_sound
				if (tempSssObject) {
					// 'tempSssObject' is allocated on the stack, it must not be added to the linked list
					warning("Invalid call to .sss opcode 16 with temporary SssObject");
					return 0;
				}
				--so->pauseCounter;
				if (so->pauseCounter >= 0) {
					return code;
				}
				sssOp16_resumeSound(so);
				code += 4;
				if (so->pcm == 0) {
					return code;
				}
				_sssObjectsChanged = true;
			}
			break;
		case 17: { // pause_sound
				if (tempSssObject) {
					// 'tempSssObject' is allocated on the stack, it must not be added to the linked list
					warning("Invalid call to .sss opcode 17 with temporary SssObject");
					return 0;
				}
				sssOp17_pauseSound(so);
				so->pauseCounter = READ_LE_UINT32(code + 4);
				return code + 8;
			}
			break;
		case 18: { // decrement_repeat_counter
				if (so->repeatCounter < 0) {
					so->repeatCounter = READ_LE_UINT32(code + 4);
				}
				code += 8;
			}
			break;
		case 19: { // set_panning
				if (so->panning != code[1]) {
					so->panning = code[1];
					_sssObjectsChanged = true;
				}
				code += 4;
			}
			break;
		case 20: { // set_pause_counter
				so->pauseCounter = READ_LE_UINT16(code + 2);
				code += 4;
			}
			break;
		case 21: { // decrement_delay_counter
				--so->delayCounter;
				if (so->delayCounter >= 0) {
					return code;
				}
				code += 4;
			}
			break;
		case 22: { // set_delay_counter
				so->delayCounter = READ_LE_UINT32(code + 4);
				return code + 8;
			}
			break;
		case 23: { // decrement_volume_modulate_steps
				--so->volumeModulateSteps;
				if (so->volumeModulateSteps >= 0) {
					return code;
				}
				code += 4;
			}
			break;
		case 24: { // set_volume_modulate_steps
				so->volumeModulateSteps = READ_LE_UINT32(code + 4);
				return code + 8;
			}
			break;
		case 25: { // decrement_panning_modulate_steps
				--so->panningModulateSteps;
				if (so->panningModulateSteps >= 0) {
					return code;
				}
				code += 4;
			}
			break;
		case 26: { // set_panning_modulate_steps
				so->panningModulateSteps = READ_LE_UINT32(code + 4);
				return code + 8;
			}
			break;
		case 27: { // seek_backward
				const int32_t frame = READ_LE_UINT32(code + 12);
				if (so->currentPcmFrame > frame) {
					so->currentPcmFrame = READ_LE_UINT32(code + 8);
					if (so->pcm) {
						const int16_t *ptr = so->pcm->ptr;
						if (ptr) {
							so->currentPcmPtr = ptr + READ_LE_UINT32(code + 4) / sizeof(int16_t);
						}
					}
				}
				return code;
			}
			break;
		case 28: { // jump
				const int32_t offset = READ_LE_UINT32(code + 4);
				code -= offset;
			}
			break;
		case 29: // end
			so->pcmFramesCount = 0;
			return 0;
		default:
			error("Invalid .sss opcode %d", *code);
			break;
		}
	}
	return code;
}

SssObject *Game::addSoundObject(SssPcm *pcm, int priority, uint32_t flags_a, uint32_t flags_b) {
	int minIndex = -1;
	int minPriority = -1;
	for (int i = 0; i < kMaxSssObjects; ++i) {
		if (!_sssObjectsTable[i].pcm) {
			minPriority = 0;
			minIndex = i;
			break;
		}
		if (_sssObjectsTable[i].currentPriority < minPriority) {
			minPriority = _sssObjectsTable[i].currentPriority;
			minIndex = i;
		}
	}
	if (minIndex == -1) {
		return 0;
	}
	assert(minIndex != -1);
	SssObject *so = &_sssObjectsTable[minIndex];
	if (so->pcm && minPriority >= priority) {
		return 0;
	}
	if (so->pcm) {
		removeSoundObjectFromList(so);
	}
	so->flags1 = flags_a;
	so->currentPriority = priority;
	so->pcm = pcm;
	so->volume = kDefaultSoundVolume;
	so->panning = kDefaultSoundPanning;
	so->stereo = (pcm->flags & 1) != 0;
	so->nextSoundBank = -1;
	so->currentPcmFrame = 0;
	so->flags = 0;
	so->pcmFramesCount = pcm->strideCount;
	so->currentPcmPtr = pcm->ptr;
	if (!so->currentPcmPtr) {
		so->flags |= kFlagPaused;
	}
	so->flags0 = flags_b;
	prependSoundObjectToList(so);
	return so->pcm ? so : 0;
}

void Game::prependSoundObjectToList(SssObject *so) {
	if (!so->pcm || !so->pcm->ptr) {
		so->flags = (so->flags & ~kFlagPlaying) | kFlagPaused;
	}
	debug(kDebug_SOUND, "prependSoundObjectToList so %p flags 0x%x", so, so->flags);
	if (so->flags & kFlagPaused) {
		debug(kDebug_SOUND, "Adding so %p to list2 flags 0x%x", so, so->flags);
		so->prevPtr = 0;
		so->nextPtr = _sssObjectsList2;
		if (_sssObjectsList2) {
			_sssObjectsList2->prevPtr = so;
		}
		_sssObjectsList2 = so;
	} else {
		debug(kDebug_SOUND, "Adding so %p to list1 flags 0x%x", so, so->flags);
		SssObject *stopSo = so; // vf
		if (so->pcm && so->pcm->ptr) {
			if (kLimitSounds && _playingSssObjectsCount >= _playingSssObjectsMax) {
				if (so->currentPriority > _lowRankSssObject->currentPriority) {

					stopSo = _lowRankSssObject;
					SssObject *next = _lowRankSssObject->nextPtr; // vd
					SssObject *prev = _lowRankSssObject->prevPtr; // vc

					so->nextPtr = next;
					so->prevPtr = prev;

					if (next) {
						next->prevPtr = so;
					}
					if (prev) {
						prev->nextPtr = so;
					} else {
						assert(so == _sssObjectsList1);
						_sssObjectsList1 = so;
					}
					_lowRankSssObject = findLowestRankSoundObject();
				}
			} else {
				stopSo = 0;

				++_playingSssObjectsCount;
				so->nextPtr = _sssObjectsList1;
				so->prevPtr = 0;
				if (_sssObjectsList1) {
					_sssObjectsList1->prevPtr = so;
				}
				_sssObjectsList1 = so;
				if (kLimitSounds) {
					if (_playingSssObjectsCount < _playingSssObjectsMax) {
						_lowRankSssObject = 0;
					} else if (!_lowRankSssObject) {
						_lowRankSssObject = findLowestRankSoundObject();
					} else if (so->currentPriority < _lowRankSssObject->currentPriority) {
						_lowRankSssObject = so;
					}
				}
			}
			so->flags |= kFlagPlaying;
		}
		if (stopSo) {
			stopSo->flags &= ~kFlagPlaying;
			stopSo->pcm = 0;
			updateSssGroup2(stopSo->flags0);
		}
	}
	if (so->num >= _sssObjectsCount) {
		_sssObjectsCount = so->num + 1;
	}
}

void Game::updateSssGroup2(uint32_t flags) {
	const uint32_t mask = 1 << (flags >> 24);
	uint32_t *sssGroupPtr = getSssGroupPtr(_res, 2, flags);
	if ((*sssGroupPtr & mask) != 0) {
		for (SssObject *so = _sssObjectsList1; so; so = so->nextPtr) {
			if (compareSssGroup(so->flags0, flags)) {
				return;
			}
		}
		for (SssObject *so = _sssObjectsList2; so; so = so->nextPtr) {
			if (compareSssGroup(so->flags0, flags)) {
				return;
			}
		}
		*sssGroupPtr &= ~mask;
	}
}

SssObject *Game::createSoundObject(int bankIndex, int sampleIndex, uint32_t flags) {
	debug(kDebug_SOUND, "createSoundObject bank %d sample %d c 0x%x", bankIndex, sampleIndex, flags);
	SssObject *ret = 0;
	SssBank *bank = &_res->_sssBanksData[bankIndex];
	if (sampleIndex < 0) {
		if ((bank->flags & 1) != 0) {
			if (bank->count <= 0) {
				return 0;
			}
			const int firstSampleIndex = bank->firstSampleIndex;
			assert(firstSampleIndex >= 0 && firstSampleIndex < _res->_sssHdr.samplesDataCount);
			SssSample *sample = &_res->_sssSamplesData[firstSampleIndex];
			int framesCount = 0;
			for (int i = 0; i < bank->count; ++i) {
				if (sample->pcm != 0xFFFF) {
					SssObject *so = startSoundObject(bankIndex, i, flags);
					if (so && so->pcmFramesCount >= framesCount) {
						framesCount = so->pcmFramesCount;
						ret = so;
					}
				}
				++sample;
			}
		} else {
			uint32_t mask = 1 << (_rnd.update() & 31);
			SssUnk6 *s6 = &_res->_sssDataUnk6[bankIndex];
			if ((s6->mask & mask) == 0) {
				if (mask > s6->mask) {
					do {
						mask >>= 1;
					} while ((s6->mask & mask) == 0);
				} else {
					do {
						mask <<= 1;
					} while ((s6->mask & mask) == 0);
				}
			}
			int b = 0;
			for (; b < bank->count; ++b) {
				if ((s6->unk0[b] & mask) != 0) {
					break;
				}
			}
			if ((bank->flags & 2) != 0) {
				s6->mask &= ~s6->unk0[b];
				if (s6->mask == 0 && bank->count > 0) {
					for (int i = 0; i < bank->count; ++i) {
						s6->mask |= s6->unk0[i];
					}
				}
			}
			ret = startSoundObject(bankIndex, b, flags);
		}
		if (ret && (bank->flags & 4) != 0) {
			ret->nextSoundBank = bankIndex;
			ret->nextSoundSample = -1;
		}
	} else {
		ret = startSoundObject(bankIndex, sampleIndex, flags);
		if (ret && (bank->flags & 4) != 0) {
			ret->nextSoundBank = bankIndex;
			ret->nextSoundSample = sampleIndex;
		}
	}
	return ret;
}

SssObject *Game::startSoundObject(int bankIndex, int sampleIndex, uint32_t flags) {
	debug(kDebug_SOUND, "startSoundObject bank %d sample %d flags 0x%x", bankIndex, sampleIndex, flags);

	SssBank *bank = &_res->_sssBanksData[bankIndex];
	const int sampleNum = bank->firstSampleIndex + sampleIndex;
	debug(kDebug_SOUND, "startSoundObject sample %d", sampleNum);
	assert(sampleNum >= 0 && sampleNum < _res->_sssHdr.samplesDataCount);
	SssSample *sample = &_res->_sssSamplesData[sampleNum];

	// original preloads PCM when changing screens
	SssPcm *pcm = &_res->_sssPcmTable[sample->pcm];
	if (!pcm->ptr && !_res->_isPsx) {
		_res->loadSssPcm(_res->_sssFile, pcm);
	}

	if (sample->framesCount != 0) {
		SssFilter *filter = &_res->_sssFilters[bank->sssFilter];
		const int priority = CLIP(filter->priorityCurrent + sample->initPriority, 0, 7);
		uint32_t flags1 = flags & 0xFFF0F000;
		flags1 |= ((sampleIndex & 0xF) << 16) | (bankIndex & 0xFFF);
		SssObject *so = addSoundObject(pcm, priority, flags1, flags);
		if (so) {
			if (sample->codeOffset1 == kNone && sample->codeOffset2 == kNone && sample->codeOffset3 == kNone && sample->codeOffset4 == kNone) {
				so->flags |= kFlagNoCode;
			}
			so->codeDataStage1 = (sample->codeOffset1 == kNone) ? 0 : _res->_sssCodeData + sample->codeOffset1;
			so->codeDataStage2 = (sample->codeOffset2 == kNone) ? 0 : _res->_sssCodeData + sample->codeOffset2;
			so->codeDataStage3 = (sample->codeOffset3 == kNone) ? 0 : _res->_sssCodeData + sample->codeOffset3;
			so->codeDataStage4 = (sample->codeOffset4 == kNone) ? 0 : _res->_sssCodeData + sample->codeOffset4;
			so->lvlObject = _currentSoundLvlObject;
			so->repeatCounter = -1;
			so->pauseCounter = -1;
			so->delayCounter = -1;
			so->volumeModulateSteps = -100;
			so->panningModulateSteps = -1;
			so->volumeModulateDelta = 0;
			so->panningModulateDelta = 0;
			so->flags0 = flags;
			so->pcmFramesCount = sample->framesCount;
			so->bankIndex = bankIndex;
			so->priority = sample->initPriority;
			so->filter = filter;
			so->volume = sample->initVolume;
			so->panning = sample->initPanning;
			if (sample->initPanning == -1) {
				if (_currentSoundLvlObject) {
					_currentSoundLvlObject->sssObject = so;
					so->panningPtr = &_snd_masterPanning;
					so->panning = getSoundObjectPanning(so);
				} else {
					so->panningPtr = 0;
					so->panning = kDefaultSoundPanning;
				}
			} else {
				so->panningPtr = 0;
			}
			setSoundObjectPanning(so);
			if (so->pcm) {
				updateSoundObject(so);
				return so;
			}
		}
		return 0;
	}

	SssObject tmpObj;
	memset(&tmpObj, 0, sizeof(tmpObj));
	tmpObj.flags0 = flags;
	tmpObj.flags1 = flags;
	tmpObj.bankIndex = bankIndex;
	tmpObj.repeatCounter = -1;
	tmpObj.pauseCounter = -1;
	tmpObj.lvlObject = _currentSoundLvlObject;
	tmpObj.panningPtr = 0;
	debug(kDebug_SOUND, "startSoundObject dpcm %d", sample->pcm);
	tmpObj.pcm = pcm;
	if (sample->codeOffset1 != kNone) {
		const uint8_t *code = _res->_sssCodeData + sample->codeOffset1;
		executeSssCode(&tmpObj, code, true);
	}
	updateSssGroup2(flags);
	return 0;
}

void Game::playSoundObject(SssInfo *s, int source, int mask) {
	debug(kDebug_SOUND, "playSoundObject num %d lut 0x%x mask 0x%x", s->sssBankIndex, source, mask);
	if (_sssDisabled) {
		return;
	}
	const int filterIndex = _res->_sssBanksData[s->sssBankIndex].sssFilter;
	SssFilter *filter = &_res->_sssFilters[filterIndex];
	bool found = false;
	for (int i = 0; i < _sssObjectsCount; ++i) {
		SssObject *so = &_sssObjectsTable[i];
		if (so->pcm != 0 && so->filter == filter) {
			found = true;
			break;
		}
	}
	int va, vc;
	va = s->targetVolume << 16;
	vc = filter->volumeCurrent;
	if (vc != va) {
		if (!found) {
			filter->volumeCurrent = va;
		} else {
			filter->volumeSteps = 4;
			filter->changed = true;
			filter->volumeDelta = (va - vc) / 4;
		}
	}
	va = s->targetPanning << 16;
	vc = filter->panningCurrent;
	if (vc != va) {
		if (!found) {
			filter->panningCurrent = va;
		} else {
			filter->panningSteps = 4;
			filter->changed = true;
			filter->panningDelta = (va - vc) / 4;
		}
	}
	va = s->targetPriority;
	vc = filter->priorityCurrent;
	if (vc != va) {
		filter->priorityCurrent = va;
		for (int i = 0; i < _sssObjectsCount; ++i) {
			SssObject *so = &_sssObjectsTable[i];
			if (so->pcm != 0 && so->filter == filter) {
				so->currentPriority = CLIP(va + so->priority, 0, 7);
				setLowPrioritySoundObject(so);
			}
		}
	}
	const uint32_t ve = sssGroupValue(source, mask, s);
	const uint8_t _al = s->concurrencyMask;
	if (_al & 2) {
		const uint32_t mask = 1 << (ve >> 24);
		uint32_t *sssGroupPtr3 = getSssGroupPtr(_res, 3, ve);
		*sssGroupPtr3 |= mask;
		uint32_t *sssGroupPtr2 = getSssGroupPtr(_res, 2, ve);
		if (*sssGroupPtr2 & mask) {
			return;
		}
		*sssGroupPtr2 |= mask;
	} else if (_al & 1) {
		const uint32_t mask = 1 << (ve >> 24);
		uint32_t *sssGroupPtr1 = getSssGroupPtr(_res, 1, ve);
		if (*sssGroupPtr1 & mask) {
			return;
		}
		*sssGroupPtr1 |= mask;
	} else if (_al & 4) {
		for (SssObject *so = _sssObjectsList1; so; so = so->nextPtr) {
			if (compareSssGroup(so->flags0, ve)) {
				return;
			}
		}
		for (SssObject *so = _sssObjectsList2; so; so = so->nextPtr) {
			if (compareSssGroup(so->flags0, ve)) {
				return;
			}
		}
	}
	createSoundObject(s->sssBankIndex, s->sampleIndex, ve);
}

void Game::clearSoundObjects() {
	memset(_sssObjectsTable, 0, sizeof(_sssObjectsTable));
	_sssObjectsList1 = 0;
	_sssObjectsList2 = 0;
	_lowRankSssObject = 0;
	for (int i = 0; i < kMaxSssObjects; ++i) {
		_sssObjectsTable[i].num = i;
	}
	_sssObjectsCount = 0;
	_playingSssObjectsCount = 0;
	_mix._mixingQueueSize = 0;
	if (_res->_sssHdr.infosDataCount != 0) {
		const int size = _res->_sssHdr.banksDataCount * sizeof(uint32_t);
		for (int i = 0; i < 3; ++i) {
			memset(_res->_sssGroup1[i], 0, size);
			memset(_res->_sssGroup2[i], 0, size);
			memset(_res->_sssGroup3[i], 0, size);
		}
	}
	_res->resetSssFilters();
}

void Game::setLowPrioritySoundObject(SssObject *so) {
	if ((so->flags & kFlagPaused) == 0) {
		if (kLimitSounds) {
			_lowRankSssObject = findLowestRankSoundObject();
		}
	}
}

int Game::getSoundObjectPanning(SssObject *so) const {
	const LvlObject *obj = so->lvlObject;
	if (obj) {
		switch (obj->type) {
		case 8:
		case 2:
		case 0:
			if (obj->screenNum == _currentLeftScreen) {
				return -1;
			}
			if (obj->screenNum == _currentRightScreen) {
				return 129;
			}
			if (obj->screenNum == _currentScreen || (_currentLevel == kLvl_lar2 && obj->spriteNum == 27) || (_currentLevel == kLvl_isld && obj->spriteNum == 26)) {
				const int dist = (obj->xPos + obj->width / 2) / 2;
				return CLIP(dist, 0, 128);
			}
			// fall-through
		default:
			return -2;
		}
	}
	return so->panning;
}

void Game::setSoundObjectPanning(SssObject *so) {
	if ((so->flags & kFlagPaused) == 0 && so->volume != 0 && _snd_masterVolume != 0) {
		int volume = ((so->filter->volumeCurrent >> 16) * so->volume) >> 7;
		int panning = 0;
		if (so->panningPtr) {
			int priority = CLIP(so->priority + so->filter->priorityCurrent, 0, 7);
			if (so->panning == -2) {
				volume = 0;
				panning = kDefaultSoundPanning;
				priority = 0;
			} else {
				panning = CLIP(so->panning, 0, 128);
				volume >>= 2;
				priority /= 2;
			}
			if (so->currentPriority != priority) {
				so->currentPriority = priority;
				if (kLimitSounds) {
					_lowRankSssObject = findLowestRankSoundObject();
				}
			}
		} else {
			panning = CLIP(so->panning + (so->filter->panningCurrent >> 16), 0, 128);
		}
		if (so->pcm == 0) {
			return;
		}
		if (volume < 0 || volume >= (int)ARRAYSIZE(_volumeRampTable)) {
			warning("Out of bounds volume %d (filter %d volume %d)", volume, (so->filter->volumeCurrent >> 16), so->volume);
			so->panL = 0;
			so->panR = 0;
			return;
		}
		int vd = _volumeRampTable[volume]; // 0..128
		int va = vd << 7; // 0..16384
		switch (panning) {
		case 0: // full left
			so->panL = va;
			so->panR = 0;
			so->panType = 2;
			break;
		case 64: // center
			va /= 2;
			so->panL = va;
			so->panR = va;
			so->panType = 3;
			break;
		case 128: // full right
			so->panL = 0;
			so->panR = va;
			so->panType = 1;
			break;
		default:
			vd *= panning;
			so->panR = vd;
			so->panL = va - vd;
			so->panType = 4;
			break;
		}
		so->panR = (so->panR * _snd_masterVolume + 64) >> 7;
		so->panL = (so->panL * _snd_masterVolume + 64) >> 7;
	} else {
		so->panL = 0;
		so->panR = 0;
		so->panType = 0;
	}
}

void Game::expireSoundObjects(uint32_t flags) {
	const uint32_t mask = 1 << (flags >> 24);
	*getSssGroupPtr(_res, 1, flags) &= ~mask;
	*getSssGroupPtr(_res, 2, flags) &= ~mask;
	for (SssObject *so = _sssObjectsList1; so; so = so->nextPtr) {
		if (((so->flags0 ^ flags) & 0xFFFF0FFF) == 0) {
			so->codeDataStage3 = 0;
			if (so->codeDataStage4 == 0) {
				removeSoundObjectFromList(so);
			}
			so->nextSoundBank = -1;
			so->delayCounter = -2;
		}
	}
	for (SssObject *so = _sssObjectsList2; so; so = so->nextPtr) {
		if (((so->flags0 ^ flags) & 0xFFFF0FFF) == 0) {
			so->codeDataStage3 = 0;
			if (so->codeDataStage4 == 0) {
				removeSoundObjectFromList(so);
			}
			so->nextSoundBank = -1;
			so->delayCounter = -2;
		}
	}
	while (_sssObjectsCount > 0 && _sssObjectsTable[_sssObjectsCount - 1].pcm == 0) {
		--_sssObjectsCount;
	}
}

void Game::mixSoundObjects17640(bool flag) {
	for (int i = 0; i < _res->_sssHdr.filtersDataCount; ++i) {
		SssFilter *filter = &_res->_sssFilters[i];
		filter->changed = false;
		if (filter->volumeSteps != 0) {
			--filter->volumeSteps;
			filter->volumeCurrent += filter->volumeDelta;
			filter->changed = true;
		}

		if (filter->panningSteps != 0) {
			--filter->panningSteps;
			filter->panningCurrent += filter->panningDelta;
			filter->changed = true;
		}
	}
	for (int i = 0; i < _sssObjectsCount; ++i) {
		SssObject *so = &_sssObjectsTable[i];
		if (so->pcm && !_sssUpdatedObjectsTable[i]) {
			if (flag) {
				const uint32_t flags = so->flags0;
				const uint32_t mask = 1 << (flags >> 24);
				if ((*getSssGroupPtr(_res, 2, flags) & mask) != 0 && (*getSssGroupPtr(_res, 3, flags) & mask) == 0) {
					expireSoundObjects(flags);
				}
			}
			if (so->pcm) {
				updateSoundObject(so);
			}
		}
	}
	memset(_sssUpdatedObjectsTable, 0, sizeof(_sssUpdatedObjectsTable));
	if (flag) {
		_res->clearSssGroup3();
	}
	queueSoundObjectsPcmStride();
}

void Game::queueSoundObjectsPcmStride() {
	_mix._mixingQueueSize = 0;
	for (SssObject *so = _sssObjectsList1; so; so = so->nextPtr) {
		const SssPcm *pcm = so->pcm;
		if (pcm != 0) {
			const int16_t *ptr = pcm->ptr;
			if (!ptr) {
				continue;
			}
			if (so->currentPcmPtr < ptr) {
				continue;
			}
			const uint32_t pcmSize = _res->getSssPcmSize(pcm) / sizeof(int16_t);
			const int16_t *end = ptr + pcmSize;
			if (so->currentPcmPtr >= end) {
				continue;
			}
			if (so->panL == 0 && so->panR == 0) {
				continue;
			}
			_mix.queue(so->currentPcmPtr, end, so->panType, so->panL, so->panR, so->stereo);
			const int strideSize = pcmSize / pcm->strideCount;
			assert(strideSize == 1764 || strideSize == 3528 || strideSize == 1792); // words
			so->currentPcmPtr += strideSize;
		}
	}
}
