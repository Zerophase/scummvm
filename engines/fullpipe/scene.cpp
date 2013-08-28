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

#include "fullpipe/fullpipe.h"

#include "fullpipe/objects.h"
#include "fullpipe/ngiarchive.h"
#include "fullpipe/statics.h"
#include "fullpipe/messages.h"
#include "fullpipe/gameloader.h"

#include "fullpipe/constants.h"

#include "common/algorithm.h"

namespace Fullpipe {

Scene *FullpipeEngine::accessScene(int sceneId) {
	SceneTag *t = 0;

	for (SceneTagList::iterator s = _gameProject->_sceneTagList->begin(); s != _gameProject->_sceneTagList->end(); ++s) {
		if (s->_sceneId == sceneId) {
			t = &(*s);
			break;
		}
	}

	if (!t)
		return 0;

	if (!t->_scene) {
		t->loadScene();
	}

	return t->_scene;
}

bool SceneTagList::load(MfcArchive &file) {
	debug(5, "SceneTagList::load()");

	int numEntries = file.readUint16LE();

	for (int i = 0; i < numEntries; i++) {
		SceneTag *t = new SceneTag();
		t->load(file);
		push_back(*t);
	}

	return true;
}

SceneTag::SceneTag() {
	_field_4 = 0;
	_scene = 0;
	_tag = 0;
}

bool SceneTag::load(MfcArchive &file) {
	debug(5, "SceneTag::load()");

	_field_4 = 0;
	_scene = 0;

	_sceneId = file.readUint16LE();

	_tag = file.readPascalString();

	debug(6, "sceneId: %d  tag: %s", _sceneId, _tag);

	return true;
}

SceneTag::~SceneTag() {
	free(_tag);
}

void SceneTag::loadScene() {
	char *archname = genFileName(0, _sceneId, "nl");

	Common::Archive *arch = makeNGIArchive(archname);

	char *fname = genFileName(0, _sceneId, "sc");

	Common::SeekableReadStream *file = arch->createReadStreamForMember(fname);

	_scene = new Scene();

	MfcArchive archive(file);

	_scene->load(archive);

	if (_scene->_shadows)
		_scene->_shadows->init();

	delete file;

	g_fullpipe->_currArchive = 0;

	free(fname);
	free(archname);
}

Scene::Scene() {
	_sceneId = 0;
	_field_BC = 0;
	_shadows = 0;
	_soundList = 0;
	_libHandle = 0;
	_sceneName = 0;
}

bool Scene::load(MfcArchive &file) {
	debug(5, "Scene::load()");

	Background::load(file);

	_sceneId = file.readUint16LE();
	
	_sceneName = file.readPascalString();
	debug(0, "scene: <%s> %d", transCyrillic((byte *)_sceneName), _sceneId);

	int count = file.readUint16LE();
	debug(7, "scene.ani: %d", count);

	for (int i = 0; i < count; i++) {
		int aniNum = file.readUint16LE();
		char *aniname = genFileName(0, aniNum, "ani");

		Common::SeekableReadStream *f = g_fullpipe->_currArchive->createReadStreamForMember(aniname);

		StaticANIObject *ani = new StaticANIObject();

		MfcArchive archive(f);

		ani->load(archive);
		ani->_sceneId = _sceneId;

		_staticANIObjectList1.push_back(ani);

		delete f;
		free(aniname);
	}

	count = file.readUint16LE();
	debug(7, "scene.mq: %d", count);

	for (int i = 0; i < count; i++) {
		int qNum = file.readUint16LE();
		char *qname = genFileName(0, qNum, "qu");

		Common::SeekableReadStream *f = g_fullpipe->_currArchive->createReadStreamForMember(qname);
		MfcArchive archive(f);

		archive.readUint16LE(); // Skip 2 bytes

		MessageQueue *mq = new MessageQueue();

		mq->load(archive);

		_messageQueueList.push_back(mq);

		delete f;
		free(qname);
	}

	count = file.readUint16LE();
	debug(7, "scene.fa: %d", count);

	for (int i = 0; i < count; i++) {
		// There are no .FA files
		assert(0);
	}

	_libHandle = g_fullpipe->_currArchive;

	if (_picObjList.size() > 0 && _bgname && strlen(_bgname) > 1) {
		char fname[260];

		strcpy(fname, _bgname);
		strcpy(strrchr(fname, '.') + 1, "col");

		MemoryObject *col = new MemoryObject();
		col->loadFile(fname);

		_palette = col;
	}

	char *shdname = genFileName(0, _sceneId, "shd");

	Shadows *shd = new Shadows();

	if (shd->loadFile(shdname))
		_shadows = shd;

	free(shdname);

	char *slsname = genFileName(0, _sceneId, "sls");

	if (g_fullpipe->_soundEnabled) {
		_soundList = new SoundList();

		if (g_fullpipe->_flgSoundList) {
			char *nlname = genFileName(17, _sceneId, "nl");

			_soundList->loadFile(slsname, nlname);

			free(nlname);
		} else {
			_soundList->loadFile(slsname, 0);
		}
	}

	free(slsname);

	initStaticANIObjects();

	if (file.size() - file.pos() > 0)
		error("Scene::load  (%d bytes left)", file.size() - file.pos());

	return true;
}

void Scene::initStaticANIObjects() {
	for (uint i = 0; i < _staticANIObjectList1.size(); i++)
		((StaticANIObject *)_staticANIObjectList1[i])->initMovements();
}

void Scene::init() {
	_x = 0;
	_y = 0;

	g_fullpipe->_sceneRect.moveTo(0, 0);

	for (uint i = 0; i < _picObjList.size(); i++)
		((PictureObject *)_picObjList[i])->clearFlags();

	for (uint i = 0; i < _staticANIObjectList1.size(); i++)
		((StaticANIObject *)_staticANIObjectList1[i])->clearFlags();

	if (_staticANIObjectList2.size() != _staticANIObjectList1.size()) {
		_staticANIObjectList2.clear();

		for (CPtrList::iterator s = _staticANIObjectList1.begin(); s != _staticANIObjectList1.end(); ++s)
			_staticANIObjectList2.push_back(*s);
	}
}

StaticANIObject *Scene::getAniMan() {
	StaticANIObject *aniMan = getStaticANIObject1ById(ANI_MAN, -1);

	deleteStaticANIObject(aniMan);

	return aniMan;
}

StaticANIObject *Scene::getStaticANIObject1ById(int obj, int a3) {
	for (CPtrList::iterator s = _staticANIObjectList1.begin(); s != _staticANIObjectList1.end(); ++s) {
		StaticANIObject *o = (StaticANIObject *)*s;
		if (o->_id == obj && (a3 == -1 || o->_okeyCode == a3))
			return o;
	}

	return 0;
}

StaticANIObject *Scene::getStaticANIObject1ByName(char *name, int a3) {
	for (uint n = 0; n < _staticANIObjectList1.size(); n++) {
		StaticANIObject *o = (StaticANIObject *)_staticANIObjectList1[n];
		if (!strcmp(o->_objectName, name) && (a3 == -1 || o->_okeyCode == a3))
			return o;
	}

	return 0;
}

void Scene::deleteStaticANIObject(StaticANIObject *obj) {
	for (uint n = 0; n < _staticANIObjectList1.size(); n++)
		if ((StaticANIObject *)_staticANIObjectList1[n] == obj) {
			_staticANIObjectList1.remove_at(n);
			break;
		}

	for (uint n = 0; n < _staticANIObjectList2.size(); n++)
		if ((StaticANIObject *)_staticANIObjectList2[n] == obj) {
			_staticANIObjectList2.remove_at(n);
			break;
		}
}

void Scene::addStaticANIObject(StaticANIObject *obj, bool addList2) {
	if (obj->_okeyCode)
		obj->renumPictures(&_staticANIObjectList1);

	_staticANIObjectList1.push_back(obj);

	if (addList2) {
		if (!obj->_okeyCode)
			obj->clearFlags();

		_staticANIObjectList2.push_back(obj);
	}
}

void Scene::setPictureObjectsFlag4() {
	for (uint i = 0; i < _picObjList.size(); i++) {
		((PictureObject *)_picObjList[i])->_flags |= 4;
	}
}

PictureObject *Scene::getPictureObjectById(int objId, int flags) {
	for (uint i = 0; i < _picObjList.size(); i++) {
		if (((PictureObject *)_picObjList[i])->_id == objId && ((PictureObject *)_picObjList[i])->_okeyCode == flags)
			return (PictureObject *)_picObjList[i];
	}

	return 0;
}

PictureObject *Scene::getPictureObjectByName(const char *objName, int flags) {
	for (uint i = 0; i < _picObjList.size(); i++) {
		if (!strcmp(((PictureObject *)_picObjList[i])->_objectName, objName) && ((PictureObject *)_picObjList[i])->_okeyCode == flags || flags == -1)
			return (PictureObject *)_picObjList[i];
	}

	return 0;
}

void Scene::deletePictureObject(PictureObject *obj) {
	for (uint i = 0; i < _picObjList.size(); i++) {
		if (((PictureObject *)_picObjList[i]) == obj) {
			_picObjList.remove_at(i);
			delete obj;

			return;
		}
	}
}

MessageQueue *Scene::getMessageQueueById(int messageId) {
	for (uint i = 0; i < _messageQueueList.size(); i++)
		if (((MessageQueue *)_messageQueueList[i])->_dataId == messageId)
			return (MessageQueue *)_messageQueueList[i];

	return 0;
}

MessageQueue *Scene::getMessageQueueByName(char *name) {
	for (uint i = 0; i < _messageQueueList.size(); i++)
		if (!strcmp(((MessageQueue *)_messageQueueList[i])->_queueName, name))
			return (MessageQueue *)_messageQueueList[i];

	return 0;
}

void Scene::preloadMovements(CGameVar *var) {
	CGameVar *preload = var->getSubVarByName("PRELOAD");
	if (!preload)
		return;

	for (CGameVar *i = preload->_subVars; i; i = i->_nextVarObj) {
		StaticANIObject *ani = getStaticANIObject1ByName(i->_varName, -1);

		if (ani) {
			CGameVar *subVars = i->_subVars;

			if (subVars) {
				for (;subVars; subVars = subVars->_nextVarObj) {
					Movement *mov = ani->getMovementByName(subVars->_varName);

					if (mov)
						mov->loadPixelData();
				}
			} else {
				ani->loadMovementsPixelData();
			}
		}
    }
}

void Scene::initObjectCursors(const char *varname) {
	CGameVar *cursorsVar = g_fullpipe->getGameLoaderGameVar()->getSubVarByName(varname)->getSubVarByName("CURSORS");

	if (!cursorsVar || !cursorsVar->_subVars)
		return;

	int maxId = 0;
	int minId = 0xffff;

	for (CGameVar *sub = cursorsVar->_subVars; sub; sub = sub->_nextVarObj) {
		GameObject *obj = getPictureObjectByName(sub->_varName, -1);

		if (obj || (obj = getStaticANIObject1ByName(sub->_varName, -1)) != 0) {
          if (obj->_id < minId)
            minId = obj->_id;
          if (obj->_id > maxId)
            maxId = obj->_id;
        }
    }

	g_fullpipe->_minCursorId = minId;
	g_fullpipe->_maxCursorId = maxId;

	g_fullpipe->_objectIdCursors.resize(maxId - minId + 1);

	for (CGameVar *sub = cursorsVar->_subVars; sub; sub = sub->_nextVarObj) {
		GameObject *obj = getPictureObjectByName(sub->_varName, -1);

		if (!obj)
			obj = getStaticANIObject1ByName(sub->_varName, -1);

		PictureObject *pic = getGameLoaderInventory()->getScene()->getPictureObjectByName(sub->_value.stringValue, -1);

		if (obj && pic)
			g_fullpipe->_objectIdCursors[obj->_id - minId] = pic->_id;
	}
}

bool Scene::compareObjPriority(const void *p1, const void *p2) {
	if (((StaticANIObject *)p1)->_priority > ((StaticANIObject *)p2)->_priority)
		return true;

	return false;
}

void Scene::objectList_sortByPriority(CPtrList &list) {
	Common::sort(list.begin(), list.end(), Scene::compareObjPriority);
}

void Scene::draw() {
	debug(0, ">>>>> Scene::draw()");
	updateScrolling();

	drawContent(60000, 0, true);

	objectList_sortByPriority(_staticANIObjectList2);

	for (CPtrList::iterator s = _staticANIObjectList2.begin(); s != _staticANIObjectList2.end(); ++s) {
		((StaticANIObject *)*s)->draw2();
	}

	int priority = -1;
	for (CPtrList::iterator s = _staticANIObjectList2.begin(); s != _staticANIObjectList2.end(); ++s) {
		drawContent(((StaticANIObject *)*s)->_priority, priority, false);
		((StaticANIObject *)*s)->draw();

		priority = ((StaticANIObject *)*s)->_priority;
	}

	drawContent(-1, priority, false);
}

void Scene::updateScrolling() {
	warning("STUB Scene::updateScrolling()");
}

void Scene::updateScrolling2() {
	warning("STUB Scene::updateScrolling2()");
}

StaticANIObject *Scene::getStaticANIObjectAtPos(int x, int y) {
	StaticANIObject *res = 0;

	for (uint i = 0; i < _staticANIObjectList1.size(); i++) {
		StaticANIObject *p = (StaticANIObject *)_staticANIObjectList1[i];
		int pixel;

		if ((p->_field_8 & 1) && (p->_flags & 4) &&
				p->getPixelAtPos(x, y, &pixel) &&
				(!res || res->_priority >= p->_priority))
			res = p;
	}

	return res;
}

PictureObject *Scene::getPictureObjectAtPos(int x, int y) {
	PictureObject *res = 0;

	for (uint i = 0; i < _picObjList.size(); i++) {
		PictureObject *p = (PictureObject *)_picObjList[i];
		if ((p->_field_8 & 1) && (p->_flags & 4) &&
				p->isPixelHitAtPos(x, y) &&
				(!res || res->_priority >= p->_priority))
			res = p;
	}

	return res;
}

int Scene::getPictureObjectIdAtPos(int x, int y) {
	PictureObject *resp = 0;
	int res = 0;

	for (uint i = 0; i < _picObjList.size(); i++) {
		PictureObject *p = (PictureObject *)_picObjList[i];
		if ((p->_field_8 & 1) && (p->_flags & 4) &&
				p->isPixelHitAtPos(x, y) &&
				(!res || resp->_priority >= p->_priority)) {
			resp = p;
			res = p->_id;
		}
	}

	return res;
}

void Scene::update(int counterdiff) {
	debug(0, "Scene::update(%d)", counterdiff);

	for (CPtrList::iterator s = _staticANIObjectList2.begin(); s != _staticANIObjectList2.end(); ++s)
		((StaticANIObject *)*s)->update(counterdiff);
}

void Scene::drawContent(int minPri, int maxPri, bool drawBg) {
	if (!_picObjList.size() && !_bigPictureArray1Count)
		return;

	if (_palette) {
		g_fullpipe->_globalPalette = _palette->_data;
	}

	debug(8, "Scene::drawContent(>%d, <%d, %d)", minPri, maxPri, drawBg);

	if (_picObjList.size() > 2) { // We need to z-sort them
		objectList_sortByPriority(_picObjList);
	}

	if (minPri == -1 && _picObjList.size())
		minPri = ((PictureObject *)_picObjList.back())->_priority - 1;

	if (maxPri == -1)
		maxPri = 60000;

	debug(8, "-> Scene::drawContent(>%d, <%d, %d)", minPri, maxPri, drawBg);

	Common::Point point;

	debug(8, "_bigPict: %d objlist: %d", _bigPictureArray1Count, _picObjList.size());
	if (drawBg && _bigPictureArray1Count && _picObjList.size()) {

		_bigPictureArray[0][0]->getDimensions(&point);

		int width = point.x;
		int height = point.y;

		debug(8, "w: %d h:%d", width, height);

		((PictureObject *)_picObjList[0])->getDimensions(&point);

		debug(8, "w2: %d h2:%d", point.x, point.y);

		int bgStX = g_fullpipe->_sceneRect.left % point.x;

		if (bgStX < 0)
			bgStX += point.x;

		int bgNumX = bgStX / width;
		int bgOffsetX = bgStX % width;

		int bgStY = g_fullpipe->_sceneRect.top % point.y;

		if (bgStY < 0)
			bgStY += point.y;

		int bgNumY = bgStY / height;
		int bgOffsetY = bgStY % height;

		int bgPosX = g_fullpipe->_sceneRect.left - bgOffsetX;

		if (bgPosX < g_fullpipe->_sceneRect.right - 1) {
			int v24 = height * bgNumY;
			int v51 = height * bgNumY;
			while (1) {
				int v25 = bgNumY;
				for (int y = g_fullpipe->_sceneRect.top - bgOffsetY; y < g_fullpipe->_sceneRect.bottom - 1;) {
					BigPicture *v27 = _bigPictureArray[bgNumX][v25];
					v27->draw(bgPosX, y, 0, 0);
					y += v27->getDimensions(&point)->y;
					v25++;

					if (v25 >= _bigPictureArray2Count) {
						if (!((PictureObject *)_picObjList[0])->_flags & 0x20)
							break;
						v25 = 0;
					}
				}
				_bigPictureArray[bgNumX][0]->getDimensions(&point);
				int v32 = point.x + bgPosX;
				bgPosX += point.x;
				bgNumX++;

				if (bgNumX >= _bigPictureArray1Count) {
					if (!(((PictureObject *)_picObjList[0])->_flags & 0x2))
						break;
					bgNumX = 0;
				}
				if (v32 >= g_fullpipe->_sceneRect.right - 1)
					break;
				v24 = v51;
			}
		}
    }


	for (uint i = 1; i < _picObjList.size(); i++) {
		PictureObject *obj = (PictureObject *)_picObjList[i];

		debug(8, "pri: %d", obj->_priority);
		if (obj->_priority < minPri || obj->_priority >= maxPri)
			continue;

		int objX = obj->_ox;
		int objY = obj->_oy;

		debug(8, "obj: %d %d", objX, objY);

		obj->getDimensions(&point);

		int width = point.x;
		int height = point.y;

		if (obj->_flags & 8) {
			while (objX > g_fullpipe->_sceneRect.right) {
				objX -= width;
				obj->setOXY(objX, objY);
			}
			for (int j = width + objX; width + objX < g_fullpipe->_sceneRect.left; j = width + objX) {
				objX = j;
				obj->setOXY(j, objY);
			}
		}

		if (obj->_flags & 0x10) {
			while (objY > g_fullpipe->_sceneRect.bottom) {
				objY -= height;
				obj->setOXY(objX, objY);
			}
			for (int j = objY + height; objY + height < g_fullpipe->_sceneRect.top; j = objY + height) {
				objY = j;
				obj->setOXY(objX, j);
			}
		}
		if (obj->_flags & 4)
			obj->draw();

		if (obj->_flags & 2) {
			if (objX > g_fullpipe->_sceneRect.left) {
				obj->setOXY(objX - width, objY);
				obj->draw();
				obj->setOXY(objX, objY);
			}
			if (width + objX < g_fullpipe->_sceneRect.right) {
				obj->setOXY(width + objX, objY);
				obj->draw();
				obj->setOXY(objX, objY);
			}
		}

		if (obj->_flags & 0x20) {
			if (objY > g_fullpipe->_sceneRect.top) {
				obj->setOXY(objX, objY - height);
				obj->draw();
				obj->setOXY(objX, objY);
			}
			if (height + objY < g_fullpipe->_sceneRect.bottom) {
				obj->setOXY(objX, height + objY);
				obj->draw();
				obj->setOXY(objX, objY);
			}
		}
	}
}

} // End of namespace Fullpipe
