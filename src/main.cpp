#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/SimplePlayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>

#include "icons.hpp"

#include <Windows.h>
#include <vector>

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

// SaiModPack.dll lives in the "extensions" folder, which Geode does not load
// (only the old standalone Mega Hack did). Load it here if nobody else did.
static void loadSaiModPack() {
	if (!Mod::get()->getSettingValue<bool>("load-sai")) return;
	if (GetModuleHandleW(L"SaiModPack.dll")) {
		log::info("SaiModPack is already loaded");
		return;
	}
	auto path = dirs::getGameDir() / "extensions" / "SaiModPack.dll";
	HMODULE h = LoadLibraryW(path.wstring().c_str());
	if (h) log::info("Loaded {}", path.string());
	else log::error("Could not load {} (error {})", path.string(), GetLastError());
}

$on_mod(Loaded) {
	loadSaiModPack();

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

// ---------------------------------------------------------------------------
// In-game drawing
// ---------------------------------------------------------------------------
// GD 2.1 draws the players through CCSpriteBatchNodes that use one texture
// (GJ_GameSheet02). Every 2.2 icon has its own texture, so inside those batches
// it is drawn with the wrong texture -> broken icons. The children of the
// player batches are moved into plain nodes at the same place / z order, where
// each sprite is drawn with its own texture.
static void setBlendRecursive(CCNode* node, ccBlendFunc const& blend) {
	if (auto sprite = dynamic_cast<CCSprite*>(node)) sprite->setBlendFunc(blend);
	if (auto kids = node->getChildren()) {
		for (unsigned i = 0; i < kids->count(); ++i) {
			setBlendRecursive(static_cast<CCNode*>(kids->objectAtIndex(i)), blend);
		}
	}
}

static void unbatch(CCNode* node) {
	auto batch = dynamic_cast<CCSpriteBatchNode*>(node);
	if (!batch || !batch->getParent()) return;
	if (!batch->getChildren() || batch->getChildrenCount() == 0) return;

	auto parent = batch->getParent();
	auto holder = CCNode::create();
	holder->setPosition(batch->getPosition());
	holder->setScale(batch->getScale());
	holder->setAnchorPoint(batch->getAnchorPoint());
	holder->setVisible(batch->isVisible());
	parent->addChild(holder, batch->getZOrder());

	ccBlendFunc blend = batch->getBlendFunc();
	bool additive = blend.dst == GL_ONE;

	// CCArray::createWithArray / copy() deep-copies (calls copy() on every node,
	// which nodes do not support): take a plain list of the children instead
	std::vector<CCNode*> kids;
	auto arr = batch->getChildren();
	for (unsigned i = 0; i < arr->count(); ++i) {
		auto child = static_cast<CCNode*>(arr->objectAtIndex(i));
		if (!child) continue;
		child->retain();
		kids.push_back(child);
	}
	for (auto child : kids) {
		int z = child->getZOrder();
		int tag = child->getTag();
		batch->removeChild(child, false); // also clears the batch from the sprite and its children
		holder->addChild(child, z, tag);
		if (additive) setBlendRecursive(child, blend);
		child->release();
	}
}

static void unbatchPlayers(GJBaseGameLayer* layer) {
	CCNode* targets[5] = {
		layer->m_batchNodePlayer,
		layer->m_batchNodeAddPlayer,
		layer->m_batchNodePlayerGlow,
		layer->m_player1 ? layer->m_player1->getParent() : nullptr,
		layer->m_player2 ? layer->m_player2->getParent() : nullptr,
	};
	for (int i = 0; i < 5; ++i) {
		bool seen = false;
		for (int j = 0; j < i; ++j) if (targets[j] == targets[i]) seen = true;
		if (!seen) unbatch(targets[i]);
	}
}

class $modify(PlayLayer) {
	bool init(GJGameLevel* level) {
		icons22::ensureEquipped();
		if (!PlayLayer::init(level)) return false;
		unbatchPlayers(this);
		return true;
	}
};

class $modify(LevelEditorLayer) {
	bool init(GJGameLevel* level) {
		icons22::ensureEquipped();
		if (!LevelEditorLayer::init(level)) return false;
		unbatchPlayers(this);
		return true;
	}
};

class $modify(MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		// every 2.2 icon goes into the sprite frame cache once, so the garage of
		// SaiModPack (which counts player_%02d_001.png etc.) shows all of them
		icons22::preloadAll();
		icons22::syncSaiCounters();
		return true;
	}
};
