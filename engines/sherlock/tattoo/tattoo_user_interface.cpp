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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "sherlock/tattoo/tattoo_user_interface.h"
#include "sherlock/tattoo/tattoo_scene.h"
#include "sherlock/tattoo/tattoo.h"

namespace Sherlock {

namespace Tattoo {

TattooUserInterface::TattooUserInterface(SherlockEngine *vm): UserInterface(vm) {
	_menuBuffer = nullptr;
	_invMenuBuffer = nullptr;
	_tagBuffer = nullptr;
	_invGraphic = nullptr;
	_scrollSize = _scrollSpeed = 0;
	_drawMenu = false;
	_bgFound = _oldBgFound = -1;
	_bgShape = nullptr;
	_personFound = false;
	_lockoutTimer = 0;
	_fileMode = SAVEMODE_NONE;
	_exitZone = -1;
	_scriptZone = -1;
	_arrowZone = _oldArrowZone = -1;
	_activeObj = -1;
}

void TattooUserInterface::initScrollVars() {
	_scrollSize = 0;
	_currentScroll.x = _currentScroll.y = 0;
	_targetScroll.x = _targetScroll.y = 0;
}

void TattooUserInterface::handleInput() {
	TattooEngine &vm = *(TattooEngine *)_vm;
	Events &events = *_vm->_events;
	TattooScene &scene = *(TattooScene *)_vm->_scene;
	Common::Point mousePos = events.mousePos();

	events.pollEventsAndWait();
	_keyState.keycode = Common::KEYCODE_INVALID;

	// Check the mouse positioning
	if (events.isCursorVisible())
		_bgFound = scene.findBgShape(mousePos);
	_personFound = _bgFound >= 1000;
	_bgShape = (_bgFound != -1 && _bgFound < 1000) ? &scene._bgShapes[_bgFound] : nullptr;

	if (_lockoutTimer)
		--_lockoutTimer;
	
	// Key handling
	if (events.kbHit()) {
		_keyState = events.getKey();

		if (_keyState.keycode == Common::KEYCODE_s && vm._allowFastMode)
			vm._fastMode = !vm._fastMode;

		else if (_keyState.keycode == Common::KEYCODE_ESCAPE && vm._runningProlog && !_lockoutTimer) {
			vm.setFlags(-76);
			vm.setFlags(396);
			scene._goToScene = STARTING_GAME_SCENE;
		}
	}

	if (!events.isCursorVisible())
		_keyState.keycode = Common::KEYCODE_INVALID;

	// Handle input depending on what mode we're in
	switch (_menuMode) {
	case STD_MODE:
		doStandardControl();
		break;
	case LOOK_MODE:
		doLookControl();
		break;
	case FILES_MODE:
		doFileControl();
		break;
	case INV_MODE:
		doInventoryControl();
		break;
	case VERB_MODE:
		doVerbControl();
		break;
	case TALK_MODE:
		doTalkControl();
		break;
	case MESSAGE_MODE:
		doMessageControl();
		break;
	case LAB_MODE:
		doLabControl();
		break;
	default:
		break;
	}
}

void TattooUserInterface::drawInterface(int bufferNum) {
	TattooScene &scene = *(TattooScene *)_vm->_scene;
	Screen &screen = *_vm->_screen;
	TattooEngine &vm = *(TattooEngine *)_vm;
	
	if (_invMenuBuffer != nullptr) {
		Common::Rect r = _invMenuBounds;
		r.grow(-3);
		r.translate(-_currentScroll.x, 0);
		_grayAreas.clear();
		_grayAreas.push_back(r);

		drawGrayAreas();
		screen._backBuffer1.transBlitFrom(*_invMenuBuffer, Common::Point(_invMenuBounds.left, _invMenuBounds.top));
	}

	if (_menuBuffer != nullptr) {
		Common::Rect r = _menuBounds;
		r.grow(-3);
		r.translate(-_currentScroll.x, 0);
		_grayAreas.clear();
		_grayAreas.push_back(r);

		drawGrayAreas();
		screen._backBuffer1.transBlitFrom(*_menuBuffer, Common::Point(_invMenuBounds.left, _invMenuBounds.top));
	}

	// See if we need to draw a Text Tag floating with the cursor
	if (_tagBuffer != nullptr)
		screen._backBuffer1.transBlitFrom(*_tagBuffer, Common::Point(_tagBounds.left, _tagBounds.top));

	// See if we need to draw an Inventory Item Graphic floating with the cursor
	if (_invGraphic != nullptr)
		screen._backBuffer1.transBlitFrom(*_invGraphic, Common::Point(_invGraphicBounds.left, _invGraphicBounds.top));

	if (vm._creditsActive)
		vm.drawCredits();

	// Bring the widgets to the screen
	if (scene._mask != nullptr)
		screen._flushScreen = true;

	if (screen._flushScreen)
		screen.blockMove(_currentScroll);

	// If there are UI widgets open, slam the areas they were drawn on to the physical screen
	if (_menuBuffer != nullptr)
		screen.slamArea(_menuBounds.left - _currentScroll.x, _menuBounds.top, _menuBounds.width(), _menuBounds.height());
	
	if (_invMenuBuffer != nullptr)
		screen.slamArea(_invMenuBounds.left - _currentScroll.x, _invMenuBounds.top, _invMenuBounds.width(), _invMenuBounds.height());

	// If therea re widgets being cleared, then restore that area of the screen
	if (_oldMenuBounds.right) {
		screen.slamArea(_oldMenuBounds.left - _currentScroll.x, _oldMenuBounds.top, _oldMenuBounds.width(), _oldMenuBounds.height());
		_oldMenuBounds.left = _oldMenuBounds.top = _oldMenuBounds.right = _oldMenuBounds.bottom = 0;
	}
	
	if (_oldInvMenuBounds.left) {
		screen.slamArea(_oldInvMenuBounds.left - _currentScroll.x, _oldInvMenuBounds.top, _oldInvMenuBounds.width(), _oldInvMenuBounds.height());
		_oldInvMenuBounds.left = _oldInvMenuBounds.top = _oldInvMenuBounds.right = _oldInvMenuBounds.bottom = 0;
	}

	// See if  need to clear any tags
	if (_oldTagBounds.right) {
		screen.slamArea(_oldTagBounds.left - _currentScroll.x, _oldTagBounds.top, _oldTagBounds.width(), _oldTagBounds.height());

		// If there's no tag actually being displayed, then reset bounds so we don't keep restoring the area
		if (_tagBuffer == nullptr) {
			_tagBounds.left = _tagBounds.top = _tagBounds.right = _tagBounds.bottom = 0;
			_oldTagBounds.left = _oldTagBounds.top = _oldTagBounds.right = _oldTagBounds.bottom = 0;
		}
	}
	if (_tagBuffer != nullptr)
		screen.slamArea(_tagBounds.left - _currentScroll.x, _tagBounds.top, _tagBounds.width(), _tagBounds.height());

	// See if we need to flush areas assocaited with the inventory graphic
	if (_oldInvGraphicBounds.right) {
		screen.slamArea(_oldInvGraphicBounds.left - _currentScroll.x, _oldInvGraphicBounds.top,
			_oldInvGraphicBounds.width(), _oldInvGraphicBounds.height());

		// If there's no graphic actually being displayed, then reset bounds so we don't keep restoring the area
		if (_invGraphic == nullptr) {
			_invGraphicBounds.left = _invGraphicBounds.top = _invGraphicBounds.right = _invGraphicBounds.bottom = 0;
			_oldInvGraphicBounds.left = _oldInvGraphicBounds.top = _oldInvGraphicBounds.right = _oldInvGraphicBounds.bottom = 0;
		}
	}
	if (_invGraphic != nullptr)
		screen.slamArea(_invGraphicBounds.left - _currentScroll.x, _invGraphicBounds.top, _invGraphicBounds.width(), _invGraphicBounds.height());
}

void TattooUserInterface::doBgAnimRestoreUI() {
	TattooScene &scene = *((TattooScene *)_vm->_scene);
	Screen &screen = *_vm->_screen;

	// If _oldMenuBounds was set, then either a new menu has been opened or the current menu has been closed.
	// Either way, we need to restore the area where the menu was displayed
	if (_oldMenuBounds.width() > 0)
		screen._backBuffer1.blitFrom(screen._backBuffer2, Common::Point(_oldMenuBounds.left, _oldMenuBounds.top),
			_oldMenuBounds);

	if (_oldInvMenuBounds.width() > 0)
		screen._backBuffer1.blitFrom(screen._backBuffer2, Common::Point(_oldInvMenuBounds.left, _oldInvMenuBounds.top),
			_oldInvMenuBounds);

	if (_menuBuffer != nullptr)
		screen._backBuffer1.blitFrom(screen._backBuffer2, Common::Point(_menuBounds.left, _menuBounds.top), _menuBounds);
	if (_invMenuBuffer != nullptr)
		screen._backBuffer1.blitFrom(screen._backBuffer2, Common::Point(_invMenuBounds.left, _invMenuBounds.top), _invMenuBounds);

	// If there is a Text Tag being display, restore the area underneath it
	if (_oldTagBounds.width() > 0)
		screen._backBuffer1.blitFrom(screen._backBuffer2, Common::Point(_oldTagBounds.left, _oldTagBounds.top), 
			_oldTagBounds);

	// If there is an Inventory being shown, restore the graphics underneath it
	if (_oldInvGraphicBounds.width() > 0)
		screen._backBuffer1.blitFrom(screen._backBuffer2, Common::Point(_oldInvGraphicBounds.left, _oldInvGraphicBounds.top), 
			_oldInvGraphicBounds);

	// If a canimation is active, restore the graphics underneath it
	if (scene._activeCAnim._imageFrame != nullptr)
		screen.restoreBackground(scene._activeCAnim._oldBounds);

	// If a canimation just ended, remove it's graphics from the backbuffer
	if (scene._activeCAnim._removeBounds.width() > 0)
		screen.restoreBackground(scene._activeCAnim._removeBounds);
}

void TattooUserInterface::doScroll() {
	Screen &screen = *_vm->_screen;
	int oldScroll = _currentScroll.x;

	// If we're already at the target scroll position, nothing needs to be done
	if (_targetScroll.x == _currentScroll.x)
		return;

	screen._flushScreen = true;
	if (_targetScroll.x > _currentScroll.x) {
		_currentScroll.x += _scrollSpeed;
		if (_currentScroll.x > _targetScroll.x)
			_currentScroll.x = _targetScroll.x;
	} else if (_targetScroll.x < _currentScroll.x) {
		_currentScroll.x -= _scrollSpeed;
		if (_currentScroll.x < _targetScroll.x)
			_currentScroll.x = _targetScroll.x;
	}

	if (_menuBuffer != nullptr)
		_menuBounds.translate(_currentScroll.x - oldScroll, 0);
	if (_invMenuBuffer != nullptr)
		_invMenuBounds.translate(_currentScroll.x - oldScroll, 0);
}

void TattooUserInterface::drawGrayAreas() {
	// TODO
}

void TattooUserInterface::doStandardControl() {
	TattooEngine &vm = *(TattooEngine *)_vm;
	Events &events = *_vm->_events;
	People &people = *_vm->_people;
	TattooScene &scene = *(TattooScene *)_vm->_scene;
	Talk &talk = *_vm->_talk;
	Common::Point mousePos = events.mousePos();
	bool noDesc = false;

	// Don't do any input processing whilst the prolog is running
	if (vm._runningProlog)
		return;

	// Display the names of any Objects the cursor is pointing at
	displayObjectNames();

	switch (_keyState.keycode) {
	case Common::KEYCODE_F5:
		// Save game
		turnTextOff();
		_fileMode = SAVEMODE_SAVE;
		_menuBounds = Common::Rect(0, 0, 0, 0);
		initFileMenu();
		return;

	case Common::KEYCODE_F7:
		// Load game
		turnTextOff();
		_fileMode = SAVEMODE_LOAD;
		_menuBounds = Common::Rect(0, 0, 0, 0);
		initFileMenu();
		return;

	case Common::KEYCODE_F1:
		// Display journal
		if (vm.readFlags(76)) {
			turnTextOff();
			doJournal();
			
			// See if we're in a Lab Table Room
			_menuMode = (scene._labTableScene) ? LAB_MODE : STD_MODE;
			return;
		}
		break;

	case Common::KEYCODE_TAB:
	case Common::KEYCODE_F3:
		// Display inventory
		turnTextOff();
		doInventory(2);
		return;

	case Common::KEYCODE_F4:
		// Display options
		turnTextOff();
		doControls();
		return;

	case Common::KEYCODE_F10:
		// Quit menu
		turnTextOff();
		_menuBounds = Common::Rect(-1, -1, -1, -1);
		doQuitMenu();
		return;

	default:
		break;
	}

	// See if a mouse button was released
	if (events._released || events._rightReleased) {
		// See if the mouse was released in an exit (Arrow) zone. Unless it's also pointing at an object
		// within the zone, in which case the object gets precedence
		_exitZone = -1;
		if (_arrowZone != -1 && events._released)
			_exitZone = _arrowZone;

		// Turn any Text display off
		if (_arrowZone == -1 || events._rightReleased)
			turnTextOff();

		if (_personFound) {
			if (people[_bgFound - 1000]._description.empty() || people[_bgFound - 1000]._description.hasPrefix(" "))
				noDesc = true;
		} else if (_bgFound != -1) {
			if (people[_bgFound - 1000]._description.empty() || people[_bgFound - 1000]._description.hasPrefix(" "))
				noDesc = true;
		} else {
			noDesc = true;
		}

		if (events._rightReleased) {
			// Show the verbs menu for the highlighted object
			activateVerbMenu(!noDesc);
		} else if (_personFound || (_bgFound < 1000 && _bgShape->_aType == PERSON)) {
			// The object found is a person (the default for people is TALK)
			talk.talk(_bgFound);
			_activeObj = -1;
		} else if (!noDesc) {
			// Either call the code to Look at it's Examine Field or call the Exit animation
			// if the object is an exit, specified by the first four characters of the name being "EXIT"
			Common::String name = _personFound ? people[_bgFound - 1000]._name : _bgShape->_name;
			if (name.hasPrefix("EXIT")) {
				lookAtObject();
			} else {
				// Run the Exit animation and set which scene to go to next
				for (int idx = 0; idx < 6; ++idx) {
					if (!_bgShape->_use[idx]._verb.compareToIgnoreCase("Open")) {
						checkAction(_bgShape->_use[idx], _bgFound);
						_activeObj = -1;
					}
				}
			}
		} else {
			// See if there are any Script Zones where they clicked
			if (scene.checkForZones(mousePos, _scriptZone) != 0) {
				// Mouse click in a script zone
				events._pressed = events._released = false;
			} else if (scene.checkForZones(mousePos, NOWALK_ZONE) != 0) {
				events._pressed = events._released = false;
			} else {
				// Walk to where the mouse was clicked
				people._walkDest = Common::Point(mousePos.x + _currentScroll.x, mousePos.y);
				people[HOLMES].goAllTheWay();
			}
		}
	}
}

void TattooUserInterface::doLookControl() {
	warning("TODO: ui control (look)");
}

void TattooUserInterface::doFileControl() {
	warning("TODO: ui control (file)");
}

void TattooUserInterface::doInventoryControl() {
	warning("TODO: ui control (inventory)");
}

void TattooUserInterface::doVerbControl() {
	warning("TODO: ui control (verb)");
}

void TattooUserInterface::doTalkControl() {
	warning("TODO: ui control (talk)");
}

void TattooUserInterface::doMessageControl() {
	warning("TODO: ui control (message)");
}

void TattooUserInterface::doLabControl() {
	warning("TODO: ui control (lab)");
}

void TattooUserInterface::displayObjectNames() {
	// TODO
}

void TattooUserInterface::initFileMenu() {
	// TODO
}

void TattooUserInterface::turnTextOff() {
	// TODO
}

void TattooUserInterface::doJournal() {
	// TODO
}

void TattooUserInterface::doInventory(int mode) {
	// TODO
}

void TattooUserInterface::doControls() {
	// TODO
}

void TattooUserInterface::doQuitMenu() {
	// TODO
}

void TattooUserInterface::activateVerbMenu(bool objectsOn) {
	// TODO
}

void TattooUserInterface::lookAtObject() {
	// TODO
}

} // End of namespace Tattoo

} // End of namespace Sherlock
