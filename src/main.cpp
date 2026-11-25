#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/SimplePlayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "icons.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Colors: the 107 colors of GD 2.2 (GD 2.1 only knows 42)
// ---------------------------------------------------------------------------
class $modify(GameManager) {
	ccColor3B colorForIdx(int idx) {
		if (idx >= 0 && idx < icons22::COLOR_COUNT) return icons22::color(idx);
		return GameManager::colorForIdx(idx);
	}

	// everything is unlocked
	bool isColorUnlocked(int id, bool type) {
		if (id >= 0 && id < icons22::COLOR_COUNT) return true;
		return GameManager::isColorUnlocked(id, type);
	}

	bool isIconUnlocked(int id, IconType type) {
		if (icons22::validIcon(type, id)) return true;
		return GameManager::isIconUnlocked(id, type);
	}
};

// ---------------------------------------------------------------------------
// Icons: the sprite frames of an icon are loaded the first time it is needed
// ---------------------------------------------------------------------------
class $modify(SimplePlayer) {
	bool init(int iconID) {
		icons22::ensure(IconType::Cube, iconID);
		return SimplePlayer::init(iconID);
	}

	void updatePlayerFrame(int iconID, IconType type) {
		icons22::ensure(type, iconID);
		SimplePlayer::updatePlayerFrame(iconID, type);
	}
};

class $modify(PlayerObject) {
	void updatePlayerFrame(int id) {
		icons22::ensure(IconType::Cube, id);
		PlayerObject::updatePlayerFrame(id);
	}

	void updatePlayerBirdFrame(int id) {
		icons22::ensure(IconType::Ufo, id);
		PlayerObject::updatePlayerBirdFrame(id);
	}

	void updatePlayerDartFrame(int id) {
		icons22::ensure(IconType::Wave, id);
		PlayerObject::updatePlayerDartFrame(id);
	}

	void updatePlayerRobotFrame(int id) {
		icons22::ensure(IconType::Robot, id);
		PlayerObject::updatePlayerRobotFrame(id);
	}
};

// ship, ball and spider have no hook above: everything equipped is loaded
// before a level starts and when the menu opens
class $modify(PlayLayer) {
	bool init(GJGameLevel* level) {
		icons22::ensureEquipped();
		return PlayLayer::init(level);
	}
};

class $modify(MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		icons22::ensureEquipped();
		return true;
	}
};
