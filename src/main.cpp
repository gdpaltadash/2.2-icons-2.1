#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/SimplePlayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <Geode/modify/CCSprite.hpp>

#include "icons.hpp"

#include <Windows.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cstddef>

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
// GD 2.1 draws the players through CCSpriteBatchNodes (m_batchNodePlayer, ...)
// that draw everything with one texture (GJ_GameSheet02). Every 2.2 icon has its
// own texture, so inside those batches it comes out broken.
//
// The players stay in their batches (the game, Mega Hack's hitboxes etc. expect
// that); only the drawing of batches that mix textures is replaced: every sprite is drawn
// on its own with its own texture, using the quad the batch already computed.
namespace {
	// batches that contain at least one sprite on another texture (players,
	// ghost trail...). They are drawn sprite by sprite. Drawing a normal batch
	// this way is still correct, only slower, so stale entries are harmless.
	std::unordered_set<CCSpriteBatchNode*> g_mixed;

	// texture of the frame each such sprite shows. A batched sprite ignores
	// setTexture(), so getTexture() can be the batch texture.
	std::unordered_map<CCSprite*, CCTexture2D*> g_frameTex;

	void markMixed(CCNode* node, CCSpriteBatchNode* batch) {
		auto s = dynamic_cast<CCSprite*>(node);
		if (!s) return;
		if (s->getTexture() && s->getTexture() != batch->getTexture()) g_mixed.insert(batch);
		if (auto kids = s->getChildren()) {
			for (unsigned i = 0; i < kids->count(); ++i) {
				markMixed(static_cast<CCNode*>(kids->objectAtIndex(i)), batch);
			}
		}
	}

	CCTexture2D* textureOf(CCSprite* s) {
		auto it = g_frameTex.find(s);
		if (it != g_frameTex.end()) return it->second;
		return s->getTexture();
	}

	// same math as CCSprite::setTextureCoords, but for the sprite's own texture
	void fixTexCoords(CCSprite* s, CCTexture2D* tex, ccV3F_C4B_T2F_Quad& q) {
		float W = static_cast<float>(tex->getPixelsWide());
		float H = static_cast<float>(tex->getPixelsHigh());
		if (W <= 0 || H <= 0) return;
		CCRect r = CC_RECT_POINTS_TO_PIXELS(s->getTextureRect());
		float left, right, top, bottom;
		if (s->isTextureRectRotated()) {
			left = r.origin.x / W;
			right = (r.origin.x + r.size.height) / W;
			top = r.origin.y / H;
			bottom = (r.origin.y + r.size.width) / H;
			if (s->isFlipX()) std::swap(top, bottom);
			if (s->isFlipY()) std::swap(left, right);
			q.bl.texCoords = { left, top };
			q.br.texCoords = { left, bottom };
			q.tl.texCoords = { right, top };
			q.tr.texCoords = { right, bottom };
		} else {
			left = r.origin.x / W;
			right = (r.origin.x + r.size.width) / W;
			top = r.origin.y / H;
			bottom = (r.origin.y + r.size.height) / H;
			if (s->isFlipX()) std::swap(left, right);
			if (s->isFlipY()) std::swap(top, bottom);
			q.bl.texCoords = { left, bottom };
			q.br.texCoords = { right, bottom };
			q.tl.texCoords = { left, top };
			q.tr.texCoords = { right, top };
		}
	}

	bool emptyQuad(ccV3F_C4B_T2F_Quad const& q) {
		return q.tl.vertices.x == 0 && q.tl.vertices.y == 0
			&& q.br.vertices.x == 0 && q.br.vertices.y == 0
			&& q.tr.vertices.x == 0 && q.bl.vertices.x == 0;
	}
}

class $modify(CCSprite) {
	void setDisplayFrame(CCSpriteFrame* frame) {
		CCSprite::setDisplayFrame(frame);
		// a batched sprite whose frame is on another texture than the batch:
		// remember the real texture (frames are set while the player is built,
		// before we know which batches are the player ones, so no check here)
		auto batch = this->getBatchNode();
		if (frame && batch && frame->getTexture() != batch->getTexture()) {
			g_frameTex[this] = frame->getTexture();
			g_mixed.insert(batch);
		} else if (!g_frameTex.empty()) {
			g_frameTex.erase(this);
		}
	}
};

class $modify(CCSpriteBatchNode) {
	void addChild(CCNode* child, int z, int tag) {
		CCSpriteBatchNode::addChild(child, z, tag);
		if (child) markMixed(child, this);
	}

	void draw() {
		if (!g_mixed.count(this)) {
			CCSpriteBatchNode::draw();
			return;
		}
		auto kids = this->getChildren();
		if (!kids || kids->count() == 0) return;

		// what the original draw does first: compute every quad (batch space)
		for (unsigned i = 0; i < kids->count(); ++i) {
			static_cast<CCSprite*>(kids->objectAtIndex(i))->updateTransform();
		}

		auto shader = this->getShaderProgram();
		if (!shader) return;
		shader->use();
		shader->setUniformsForBuiltins();
		ccBlendFunc blend = this->getBlendFunc();
		ccGLBlendFunc(blend.src, blend.dst);
		ccGLEnableVertexAttribs(kCCVertexAttribFlag_PosColorTex);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		auto list = this->getDescendants();
		if (!list) return;
		for (unsigned i = 0; i < list->count(); ++i) {
			auto s = dynamic_cast<CCSprite*>(list->objectAtIndex(i));
			if (!s) continue;
			ccV3F_C4B_T2F_Quad q = s->getQuad();
			if (emptyQuad(q)) continue; // hidden sprite
			auto tex = textureOf(s);
			if (!tex) continue;
			fixTexCoords(s, tex, q);
			ccGLBindTexture2D(tex->getName());

			constexpr int stride = sizeof(ccV3F_C4B_T2F);
			auto base = reinterpret_cast<char*>(&q);
			glVertexAttribPointer(kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, stride, base + offsetof(ccV3F_C4B_T2F, vertices));
			glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, stride, base + offsetof(ccV3F_C4B_T2F, texCoords));
			glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, base + offsetof(ccV3F_C4B_T2F, colors));
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
	}
};

class $modify(PlayLayer) {
	bool init(GJGameLevel* level) {
		icons22::ensureEquipped();
		return PlayLayer::init(level);
	}
};

class $modify(LevelEditorLayer) {
	bool init(GJGameLevel* level) {
		icons22::ensureEquipped();
		return LevelEditorLayer::init(level);
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
