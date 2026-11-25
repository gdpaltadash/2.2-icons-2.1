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
// In GD 2.113 colorForIdx is really a static function: the caller does not
// pass `this`, it pushes (ccColor3B* out, int idx) and the function returns with
// `ret 8`. The binding declares it as a member function, so a normal $modify
// hook breaks the stack/registers (crash in MenuGameLayer). Instead the start of
// the function is replaced with a jmp to this __stdcall function, which has the
// exact same layout.
static ccColor3B* __stdcall colorForIdx22(ccColor3B* out, int idx) {
	*out = icons22::color(idx); // ids outside 0..106 give white, like the original
	return out;
}

$on_mod(Loaded) {
	// SaiModPack has its own color palette: the 2.2 colors can be turned off
	if (!Mod::get()->getSettingValue<bool>("colors-22")) return;
	uintptr_t target = geode::base::get() + 0xc8d10; // GameManager::colorForIdx
	auto func = reinterpret_cast<uintptr_t>(&colorForIdx22);
	int32_t rel = static_cast<int32_t>(func - (target + 5));
	ByteVector bytes = {
		0xE9,
		static_cast<uint8_t>(rel & 0xff),
		static_cast<uint8_t>((rel >> 8) & 0xff),
		static_cast<uint8_t>((rel >> 16) & 0xff),
		static_cast<uint8_t>((rel >> 24) & 0xff),
	};
	auto res = Mod::get()->patch(reinterpret_cast<void*>(target), bytes);
	if (!res) log::error("colorForIdx patch failed: {}", res.unwrapErr());
}

class $modify(GameManager) {
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
		// every 2.2 icon goes into the sprite frame cache once, so the garage of
		// SaiModPack (which counts player_%02d_001.png etc.) shows all of them
		icons22::preloadAll();
		return true;
	}
};
