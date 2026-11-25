#include "icons.hpp"


#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_set>

using namespace geode::prelude;

namespace icons22 {
	static const ccColor3B PALETTE[COLOR_COUNT] = {
	{125,255,0}, {0,255,0}, {0,255,125}, {0,255,255}, {0,125,255}, {0,0,255},
	{125,0,255}, {255,0,255}, {255,0,125}, {255,0,0}, {255,125,0}, {255,255,0},
	{255,255,255}, {185,0,255}, {255,185,0}, {0,0,0}, {0,200,255}, {175,175,175},
	{90,90,90}, {255,125,125}, {0,175,75}, {0,125,125}, {0,75,175}, {75,0,175},
	{125,0,125}, {175,0,75}, {175,75,0}, {125,125,0}, {75,175,0}, {255,75,0},
	{150,50,0}, {150,100,0}, {100,150,0}, {0,150,100}, {0,100,150}, {100,0,150},
	{150,0,100}, {150,0,0}, {0,150,0}, {0,0,150}, {125,255,175}, {125,125,255},
	{255,250,127}, {250,127,255}, {0,255,192}, {80,50,14}, {205,165,118}, {182,128,255},
	{255,58,58}, {77,77,143}, {0,10,76}, {253,212,206}, {190,181,255}, {112,0,0},
	{82,2,0}, {56,1,6}, {128,79,79}, {122,53,53}, {81,36,36}, {163,98,70},
	{117,73,54}, {86,53,40}, {255,185,114}, {255,160,64}, {102,49,30}, {91,39,0},
	{71,32,0}, {167,123,77}, {109,83,57}, {81,62,42}, {255,255,192}, {253,224,160},
	{192,255,160}, {177,255,109}, {192,255,224}, {148,255,228}, {67,161,138}, {49,109,95},
	{38,84,73}, {0,96,0}, {0,64,0}, {0,96,96}, {0,64,64}, {160,255,255},
	{1,7,112}, {0,73,109}, {0,50,76}, {0,38,56}, {80,128,173}, {51,83,117},
	{35,60,86}, {224,224,224}, {61,6,140}, {55,8,96}, {64,64,64}, {111,73,164},
	{84,54,127}, {66,42,99}, {252,181,255}, {175,87,175}, {130,67,130}, {94,49,94},
	{128,128,128}, {102,3,62}, {71,1,52}, {210,255,50}, {118,189,255}
	};

	ccColor3B color(int idx) {
		if (idx < 0 || idx >= COLOR_COUNT) return { 255, 255, 255 };
		return PALETTE[idx];
	}

	int maxId(IconType type) {
		switch (type) {
			case IconType::Cube: return 485;
			case IconType::Ship: return 169;
			case IconType::Ball: return 118;
			case IconType::Ufo: return 149;
			case IconType::Wave: return 96;
			case IconType::Robot: return 68;
			case IconType::Spider: return 69;
			default: return 0;
		}
	}

	bool validIcon(IconType type, int id) {
		return id >= 1 && id <= maxId(type);
	}

	// file name (without extension) of the sheet of one icon, inside Resources/icons
	static std::string sheetName(IconType type, int id) {
		char buf[64];
		switch (type) {
			case IconType::Cube: std::snprintf(buf, sizeof(buf), "player_%02d", id); break;
			case IconType::Ship: std::snprintf(buf, sizeof(buf), "ship_%02d", id); break;
			case IconType::Ball: std::snprintf(buf, sizeof(buf), "player_ball_%02d", id); break;
			case IconType::Ufo: std::snprintf(buf, sizeof(buf), "bird_%02d", id); break;
			case IconType::Wave: std::snprintf(buf, sizeof(buf), "dart_%02d", id); break;
			case IconType::Robot: std::snprintf(buf, sizeof(buf), "robot_%02d", id); break;
			case IconType::Spider: std::snprintf(buf, sizeof(buf), "spider_%02d", id); break;
			default: return "";
		}
		return buf;
	}

	// name of one frame that always exists in the sheet
	static std::string probeFrame(IconType type, int id) {
		char buf[64];
		switch (type) {
			case IconType::Cube: std::snprintf(buf, sizeof(buf), "player_%02d_001.png", id); break;
			case IconType::Ship: std::snprintf(buf, sizeof(buf), "ship_%02d_001.png", id); break;
			case IconType::Ball: std::snprintf(buf, sizeof(buf), "player_ball_%02d_001.png", id); break;
			case IconType::Ufo: std::snprintf(buf, sizeof(buf), "bird_%02d_001.png", id); break;
			case IconType::Wave: std::snprintf(buf, sizeof(buf), "dart_%02d_001.png", id); break;
			case IconType::Robot: std::snprintf(buf, sizeof(buf), "robot_%02d_01_001.png", id); break;
			case IconType::Spider: std::snprintf(buf, sizeof(buf), "spider_%02d_01_001.png", id); break;
			default: return "";
		}
		return buf;
	}

	// parses every number of strings like "{{36,2},{30,30}}" or "{0,0}"
	static int parseNumbers(const char* s, float* out, int max) {
		int n = 0;
		const char* p = s;
		while (*p && n < max) {
			if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.') {
				char* end = nullptr;
				float v = std::strtof(p, &end);
				if (end == p) { ++p; continue; }
				out[n++] = v;
				p = end;
			} else {
				++p;
			}
		}
		return n;
	}

	static const char* strOf(CCDictionary* d, const char* key) {
		auto o = d->objectForKey(key);
		auto s = o ? dynamic_cast<CCString*>(o) : nullptr;
		return s ? s->getCString() : nullptr;
	}

	// GD 2.1 (cocos2d-x 2.x) can only read plist formats 0-2. The 2.2 icon
	// sheets are format 3, so the frames are read here and added by hand.
	static bool loadSheet(std::string const& name) {
		auto fu = CCFileUtils::sharedFileUtils();
		std::string relPlist = "icons/" + name + ".plist";
		std::string relPng = "icons/" + name + ".png";

		std::string plist = fu->fullPathForFilename(relPlist.c_str(), false).c_str();
		if (plist.empty() || plist == relPlist) return false; // not found
		{
			std::ifstream f(plist, std::ios::binary);
			if (!f.good()) return false;
		}

		auto dict = CCDictionary::createWithContentsOfFile(plist.c_str());
		if (!dict) return false;
		auto frames = dynamic_cast<CCDictionary*>(dict->objectForKey("frames"));
		if (!frames) return false;

		auto tex = CCTextureCache::sharedTextureCache()->addImage(relPng.c_str(), false);
		if (!tex) return false;

		auto cache = CCSpriteFrameCache::sharedSpriteFrameCache();
		CCDictElement* el = nullptr;
		CCDICT_FOREACH(frames, el) {
			auto fd = dynamic_cast<CCDictionary*>(el->getObject());
			if (!fd) continue;

			float rect[4] = { 0, 0, 0, 0 };
			float size[2] = { 0, 0 };
			float offset[2] = { 0, 0 };
			float source[2] = { 0, 0 };

			auto sRect = strOf(fd, "textureRect");
			auto sSize = strOf(fd, "spriteSize");
			auto sOffset = strOf(fd, "spriteOffset");
			auto sSource = strOf(fd, "spriteSourceSize");
			if (!sRect || !sSize || !sSource) continue;
			if (parseNumbers(sRect, rect, 4) < 4) continue;
			if (parseNumbers(sSize, size, 2) < 2) continue;
			if (parseNumbers(sSource, source, 2) < 2) continue;
			if (sOffset) parseNumbers(sOffset, offset, 2);

			bool rotated = false;
			if (auto sRot = strOf(fd, "textureRotated")) {
				rotated = std::string(sRot) == "true" || std::string(sRot) == "1";
			}

			auto frame = CCSpriteFrame::createWithTexture(
				tex,
				CCRectMake(rect[0], rect[1], size[0], size[1]),
				rotated,
				CCPointMake(offset[0], offset[1]),
				CCSizeMake(source[0], source[1])
			);
			if (!frame) continue;
			cache->addSpriteFrame(frame, el->getStrKey());
		}
		return true;
	}

	void ensure(IconType type, int id) {
		if (!validIcon(type, id)) return;

		static std::unordered_set<int> loaded;
		static std::unordered_set<int> failed; // sheets that are not on disk: do not ask again
		int key = static_cast<int>(type) * 1000 + id;
		if (failed.count(key)) return;

		auto cache = CCSpriteFrameCache::sharedSpriteFrameCache();
		bool hasFrame = cache->spriteFrameByName(probeFrame(type, id).c_str()) != nullptr;
		if (hasFrame && loaded.count(key)) return;

		if (loadSheet(sheetName(type, id))) {
			loaded.insert(key);
		} else {
			failed.insert(key);
			if (hasFrame) return; // the 2.1 art stays
			static bool warned = false;
			if (!warned) {
				warned = true;
				log::warn(
					"Icon sheet {} not found. Copy the 'icons' folder of GD 2.2 into the Resources folder of GD 2.1.",
					sheetName(type, id)
				);
			}
		}
	}

	void ensureEquipped() {
		auto gm = GameManager::sharedState();
		ensure(IconType::Cube, gm->getPlayerFrame());
		ensure(IconType::Ship, gm->getPlayerShip());
		ensure(IconType::Ball, gm->getPlayerBall());
		ensure(IconType::Ufo, gm->getPlayerBird());
		ensure(IconType::Wave, gm->getPlayerDart());
		ensure(IconType::Robot, gm->getPlayerRobot());
		ensure(IconType::Spider, gm->getPlayerSpider());
	}
}
