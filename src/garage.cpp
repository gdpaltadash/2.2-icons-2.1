#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>

#include <algorithm>
#include <cstring>

#include "icons.hpp"

using namespace geode::prelude;

namespace {
	constexpr int ICON_COLS = 12;
	constexpr int ICON_ROWS = 3;
	constexpr int ICONS_PER_PAGE = ICON_COLS * ICON_ROWS;

	constexpr int COLOR_COLS = 14;
	constexpr int COLOR_ROWS = 4;
	constexpr int COLORS_PER_PAGE = COLOR_COLS * COLOR_ROWS;

	constexpr float CELL = 33.f;

	struct TabInfo {
		IconType type;
		const char* off;
		const char* on;
		float scale; // scale of the icon inside a grid cell
	};

	const TabInfo TABS[7] = {
		{ IconType::Cube,   "gj_iconBtn_off_001.png",   "gj_iconBtn_on_001.png",   0.85f },
		{ IconType::Ship,   "gj_shipBtn_off_001.png",   "gj_shipBtn_on_001.png",   0.55f },
		{ IconType::Ball,   "gj_ballBtn_off_001.png",   "gj_ballBtn_on_001.png",   0.65f },
		{ IconType::Ufo,    "gj_birdBtn_off_001.png",   "gj_birdBtn_on_001.png",   0.6f  },
		{ IconType::Wave,   "gj_dartBtn_off_001.png",   "gj_dartBtn_on_001.png",   0.7f  },
		{ IconType::Robot,  "gj_robotBtn_off_001.png",  "gj_robotBtn_on_001.png",  0.55f },
		{ IconType::Spider, "gj_spiderBtn_off_001.png", "gj_spiderBtn_on_001.png", 0.5f  },
	};

	int equippedId(IconType t) {
		auto gm = GameManager::sharedState();
		switch (t) {
			case IconType::Cube: return gm->getPlayerFrame();
			case IconType::Ship: return gm->getPlayerShip();
			case IconType::Ball: return gm->getPlayerBall();
			case IconType::Ufo: return gm->getPlayerBird();
			case IconType::Wave: return gm->getPlayerDart();
			case IconType::Robot: return gm->getPlayerRobot();
			case IconType::Spider: return gm->getPlayerSpider();
			default: return 1;
		}
	}

	void equip(IconType t, int id) {
		auto gm = GameManager::sharedState();
		switch (t) {
			case IconType::Cube: gm->setPlayerFrame(id); break;
			case IconType::Ship: gm->setPlayerShip(id); break;
			case IconType::Ball: gm->setPlayerBall(id); break;
			case IconType::Ufo: gm->setPlayerBird(id); break;
			case IconType::Wave: gm->setPlayerDart(id); break;
			case IconType::Robot: gm->setPlayerRobot(id); break;
			case IconType::Spider: gm->setPlayerSpider(id); break;
			default: break;
		}
		gm->setPlayerIconType(t);
	}

	// a plain white texture, tinted to draw the color swatches
	CCTexture2D* whiteTexture() {
		static CCTexture2D* tex = nullptr;
		if (!tex) {
			unsigned char px[2 * 2 * 4];
			std::memset(px, 255, sizeof(px));
			tex = new CCTexture2D();
			tex->initWithData(px, kCCTexture2DPixelFormat_RGBA8888, 2, 2, CCSizeMake(2, 2));
		}
		return tex;
	}

	CCSprite* makeSwatch(ccColor3B c, float size) {
		auto s = CCSprite::createWithTexture(whiteTexture());
		s->setScaleX(size / 2.f);
		s->setScaleY(size / 2.f);
		s->setColor(c);
		return s;
	}
}

// New garage in the style of GD 2.2: big preview on the left, tabs for the icon
// kinds, a grid with pages and the 107 color palette.
class Garage22 : public CCLayer {
protected:
	GJGarageLayer* m_owner = nullptr;
	int m_tab = 0;
	int m_page = 0;
	bool m_colorMode = false;
	int m_colorTarget = 1; // 1 = main color, 2 = secondary color

	SimplePlayer* m_preview = nullptr;
	CCNode* m_previewHolder = nullptr;
	CCMenu* m_gridMenu = nullptr;
	CCSprite* m_marker = nullptr;
	CCLabelBMFont* m_pageLabel = nullptr;
	CCMenu* m_mainMenu = nullptr;
	CCSprite* m_tabOn[7] = {};
	CCSprite* m_tabOff[7] = {};
	CCSprite* m_glowOn = nullptr;
	CCSprite* m_glowOff = nullptr;
	CCSprite* m_swatch1 = nullptr;
	CCSprite* m_swatch2 = nullptr;
	CCSprite* m_targetMark = nullptr;
	CCLabelBMFont* m_modeLabel = nullptr;
	CCSize m_win;
	CCPoint m_gridOrigin; // center of the first cell (top left)
	float m_panelL = 0.f;
	float m_panelBottom = 0.f;
	float m_previewX = 0.f;

public:
	static Garage22* create(GJGarageLayer* owner) {
		auto ret = new Garage22();
		if (ret && ret->init(owner)) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}

	bool init(GJGarageLayer* owner) {
		if (!CCLayer::init()) return false;
		m_owner = owner;
		m_win = CCDirector::sharedDirector()->getWinSize();

		auto gm = GameManager::sharedState();
		auto lastType = gm->getPlayerIconType();
		for (int i = 0; i < 7; ++i) {
			if (TABS[i].type == lastType) m_tab = i;
		}

		// background
		auto bg = CCLayerColor::create(ccc4(31, 87, 196, 255), m_win.width, m_win.height);
		this->addChild(bg, -2);

		float gridW = ICON_COLS * CELL;
		float panelL = m_win.width - gridW - 18.f;
		m_panelL = panelL;
		m_panelBottom = m_win.height * 0.17f;
		auto panel = CCLayerColor::create(ccc4(0, 0, 0, 110), gridW + 12.f, ICON_ROWS * CELL + 12.f);
		panel->setPosition(ccp(panelL - 6.f, m_win.height * 0.17f - 6.f));
		this->addChild(panel, -1);

		m_gridOrigin = ccp(panelL + CELL / 2.f, m_win.height * 0.17f + (ICON_ROWS - 0.5f) * CELL);

		m_mainMenu = CCMenu::create();
		m_mainMenu->setPosition(ccp(0, 0));
		this->addChild(m_mainMenu, 5);

		// back button (original onBack of the garage)
		{
			auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
			auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, m_owner, menu_selector(GJGarageLayer::onBack));
			btn->setPosition(ccp(25.f, m_win.height - 25.f));
			m_mainMenu->addChild(btn);
		}

		// tabs
		float tabY = m_win.height * 0.17f + ICON_ROWS * CELL + 34.f;
		float tabX0 = panelL + 18.f;
		for (int i = 0; i < 7; ++i) {
			auto holder = CCNode::create();
			holder->setContentSize(CCSizeMake(36.f, 36.f));
			auto off = CCSprite::createWithSpriteFrameName(TABS[i].off);
			auto on = CCSprite::createWithSpriteFrameName(TABS[i].on);
			off->setPosition(ccp(18.f, 18.f));
			on->setPosition(ccp(18.f, 18.f));
			holder->addChild(off);
			holder->addChild(on);
			m_tabOff[i] = off;
			m_tabOn[i] = on;
			auto btn = CCMenuItemSpriteExtra::create(holder, nullptr, this, menu_selector(Garage22::onTab));
			btn->setTag(i);
			btn->setPosition(ccp(tabX0 + i * 40.f, tabY));
			m_mainMenu->addChild(btn);
		}

		// colors / icons switch
		{
			m_modeLabel = CCLabelBMFont::create("Colors", "goldFont.fnt");
			m_modeLabel->setScale(0.7f);
			auto btn = CCMenuItemSpriteExtra::create(m_modeLabel, nullptr, this, menu_selector(Garage22::onMode));
			btn->setPosition(ccp(panelL + gridW - 36.f, tabY));
			m_mainMenu->addChild(btn);
		}

		// page arrows
		{
			auto l = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
			l->setScale(0.7f);
			auto bl = CCMenuItemSpriteExtra::create(l, nullptr, this, menu_selector(Garage22::onPrev));
			bl->setPosition(ccp(panelL - 14.f, m_gridOrigin.y - CELL));
			m_mainMenu->addChild(bl);

			auto r = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
			r->setScale(0.7f);
			r->setFlipX(true);
			auto br = CCMenuItemSpriteExtra::create(r, nullptr, this, menu_selector(Garage22::onNext));
			br->setPosition(ccp(panelL + gridW + 14.f, m_gridOrigin.y - CELL));
			m_mainMenu->addChild(br);
		}

		m_pageLabel = CCLabelBMFont::create("1/1", "goldFont.fnt");
		m_pageLabel->setScale(0.55f);
		m_pageLabel->setPosition(ccp(panelL + gridW / 2.f, m_win.height * 0.17f - 20.f));
		this->addChild(m_pageLabel, 5);

		// preview (left side)
		float px = panelL / 2.f - 4.f;
		m_previewX = px;
		m_previewHolder = CCNode::create();
		m_previewHolder->setPosition(ccp(px, m_win.height * 0.64f));
		this->addChild(m_previewHolder, 5);

		// the two colors
		float cy = m_win.height * 0.38f;
		m_swatch1 = makeSwatch(icons22::color(gm->getPlayerColor()), 26.f);
		m_swatch2 = makeSwatch(icons22::color(gm->getPlayerColor2()), 26.f);
		{
			auto h1 = CCNode::create();
			h1->setContentSize(CCSizeMake(30.f, 30.f));
			m_swatch1->setPosition(ccp(15.f, 15.f));
			h1->addChild(m_swatch1);
			auto b1 = CCMenuItemSpriteExtra::create(h1, nullptr, this, menu_selector(Garage22::onTarget));
			b1->setTag(1);
			b1->setPosition(ccp(px - 24.f, cy));
			m_mainMenu->addChild(b1);

			auto h2 = CCNode::create();
			h2->setContentSize(CCSizeMake(30.f, 30.f));
			m_swatch2->setPosition(ccp(15.f, 15.f));
			h2->addChild(m_swatch2);
			auto b2 = CCMenuItemSpriteExtra::create(h2, nullptr, this, menu_selector(Garage22::onTarget));
			b2->setTag(2);
			b2->setPosition(ccp(px + 24.f, cy));
			m_mainMenu->addChild(b2);

			m_targetMark = CCSprite::createWithSpriteFrameName("GJ_select_001.png");
			m_targetMark->setScale(0.55f);
			this->addChild(m_targetMark, 6);
		}

		// glow toggle
		{
			auto holder = CCNode::create();
			holder->setContentSize(CCSizeMake(28.f, 28.f));
			m_glowOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
			m_glowOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
			m_glowOff->setPosition(ccp(14.f, 14.f));
			m_glowOn->setPosition(ccp(14.f, 14.f));
			holder->addChild(m_glowOff);
			holder->addChild(m_glowOn);
			auto btn = CCMenuItemSpriteExtra::create(holder, nullptr, this, menu_selector(Garage22::onGlow));
			btn->setPosition(ccp(px - 26.f, cy - 38.f));
			m_mainMenu->addChild(btn);

			auto lbl = CCLabelBMFont::create("Glow", "bigFont.fnt");
			lbl->setScale(0.5f);
			lbl->setAnchorPoint(ccp(0.f, 0.5f));
			lbl->setPosition(ccp(px - 8.f, cy - 38.f));
			this->addChild(lbl, 5);
		}

		this->refreshTabs();
		this->refreshPreview();
		this->refreshGrid();
		return true;
	}

	// ---- state helpers ----------------------------------------------------
	IconType tabType() const { return TABS[m_tab].type; }

	int pageCount() const {
		int total = m_colorMode ? icons22::COLOR_COUNT : icons22::maxId(tabType());
		int per = m_colorMode ? COLORS_PER_PAGE : ICONS_PER_PAGE;
		return std::max(1, (total + per - 1) / per);
	}

	void refreshTabs() {
		for (int i = 0; i < 7; ++i) {
			bool sel = (i == m_tab) && !m_colorMode;
			m_tabOn[i]->setVisible(sel);
			m_tabOff[i]->setVisible(!sel);
		}
	}

	void refreshPreview() {
		auto gm = GameManager::sharedState();
		auto type = tabType();
		int id = equippedId(type);

		if (m_preview) {
			m_preview->removeFromParent();
			m_preview = nullptr;
		}
		icons22::ensure(type, id);
		m_preview = SimplePlayer::create(type == IconType::Cube ? id : 1);
		if (type != IconType::Cube) m_preview->updatePlayerFrame(id, type);
		m_preview->setScale(type == IconType::Cube ? 2.4f : 1.5f);
		m_preview->setColor(icons22::color(gm->getPlayerColor()));
		m_preview->setSecondColor(icons22::color(gm->getPlayerColor2()));
		m_preview->setGlowOutline(gm->getPlayerGlow());
		m_previewHolder->addChild(m_preview);

		m_swatch1->setColor(icons22::color(gm->getPlayerColor()));
		m_swatch2->setColor(icons22::color(gm->getPlayerColor2()));
		m_glowOn->setVisible(gm->getPlayerGlow());
		m_glowOff->setVisible(!gm->getPlayerGlow());

		m_targetMark->setPosition(ccp(m_previewX + (m_colorTarget == 1 ? -24.f : 24.f), m_win.height * 0.38f));
	}

	CCPoint cellPos(int index, int cols) const {
		int col = index % cols;
		int row = index / cols;
		return ccp(m_gridOrigin.x + col * CELL, m_gridOrigin.y - row * CELL);
	}

	void refreshGrid() {
		if (m_gridMenu) {
			// this can run inside the callback of a button of the menu: keep it
			// alive until the end of the frame
			m_gridMenu->retain();
			m_gridMenu->removeFromParent();
			m_gridMenu->autorelease();
			m_gridMenu = nullptr;
			m_marker = nullptr;
		}
		m_page = std::clamp(m_page, 0, pageCount() - 1);

		m_gridMenu = CCMenu::create();
		m_gridMenu->setPosition(ccp(0, 0));
		this->addChild(m_gridMenu, 4);

		auto gm = GameManager::sharedState();
		m_marker = CCSprite::createWithSpriteFrameName("GJ_select_001.png");
		m_marker->setScale(0.62f);
		m_marker->setVisible(false);
		this->m_gridMenu->addChild(m_marker, 10);

		if (m_colorMode) {
			int sel = m_colorTarget == 1 ? gm->getPlayerColor() : gm->getPlayerColor2();
			for (int i = 0; i < COLORS_PER_PAGE; ++i) {
				int idx = m_page * COLORS_PER_PAGE + i;
				if (idx >= icons22::COLOR_COUNT) break;
				float cw = (ICON_COLS * CELL) / COLOR_COLS;
				float rh = (ICON_ROWS * CELL) / COLOR_ROWS;
				auto holder = CCNode::create();
				holder->setContentSize(CCSizeMake(cw - 2.f, rh - 2.f));
				auto sw = makeSwatch(icons22::color(idx), std::min(cw, rh) - 5.f);
				sw->setPosition(ccp((cw - 2.f) / 2.f, (rh - 2.f) / 2.f));
				holder->addChild(sw);
				auto btn = CCMenuItemSpriteExtra::create(holder, nullptr, this, menu_selector(Garage22::onColor));
				btn->setTag(idx);
				CCPoint pos = ccp(
					m_panelL + cw * ((i % COLOR_COLS) + 0.5f),
					m_panelBottom + ICON_ROWS * CELL - rh * ((i / COLOR_COLS) + 0.5f)
				);
				btn->setPosition(pos);
				m_gridMenu->addChild(btn);
				if (idx == sel) {
					m_marker->setPosition(pos);
					m_marker->setVisible(true);
				}
			}
		} else {
			auto type = tabType();
			int sel = equippedId(type);
			int maxId = icons22::maxId(type);
			for (int i = 0; i < ICONS_PER_PAGE; ++i) {
				int id = m_page * ICONS_PER_PAGE + i + 1;
				if (id > maxId) break;
				icons22::ensure(type, id);

				auto holder = CCNode::create();
				holder->setContentSize(CCSizeMake(CELL - 3.f, CELL - 3.f));
				auto sp = SimplePlayer::create(type == IconType::Cube ? id : 1);
				if (type != IconType::Cube) sp->updatePlayerFrame(id, type);
				sp->setScale(TABS[m_tab].scale);
				sp->setColor(icons22::color(gm->getPlayerColor()));
				sp->setSecondColor(icons22::color(gm->getPlayerColor2()));
				sp->setGlowOutline(gm->getPlayerGlow());
				sp->setPosition(ccp((CELL - 3.f) / 2.f, (CELL - 3.f) / 2.f));
				holder->addChild(sp);

				auto btn = CCMenuItemSpriteExtra::create(holder, nullptr, this, menu_selector(Garage22::onIcon));
				btn->setTag(id);
				auto pos = cellPos(i, ICON_COLS);
				btn->setPosition(pos);
				m_gridMenu->addChild(btn);
				if (id == sel) {
					m_marker->setPosition(pos);
					m_marker->setVisible(true);
				}
			}
		}

		m_pageLabel->setString(CCString::createWithFormat("%d/%d", m_page + 1, pageCount())->getCString());
		m_modeLabel->setString(m_colorMode ? "Icons" : "Colors");
		this->refreshTabs();
	}

	// ---- callbacks -----------------------------------------------------------
	void onTab(CCObject* sender) {
		m_tab = static_cast<CCNode*>(sender)->getTag();
		m_colorMode = false;
		m_page = 0;
		// open the page where the equipped icon is
		int sel = equippedId(tabType());
		if (sel > 0) m_page = (sel - 1) / ICONS_PER_PAGE;
		GameManager::sharedState()->setPlayerIconType(tabType());
		this->refreshPreview();
		this->refreshGrid();
	}

	void onMode(CCObject*) {
		m_colorMode = !m_colorMode;
		m_page = 0;
		this->refreshGrid();
	}

	void onPrev(CCObject*) {
		m_page = (m_page + pageCount() - 1) % pageCount();
		this->refreshGrid();
	}

	void onNext(CCObject*) {
		m_page = (m_page + 1) % pageCount();
		this->refreshGrid();
	}

	void onIcon(CCObject* sender) {
		int id = static_cast<CCNode*>(sender)->getTag();
		equip(tabType(), id);
		this->refreshPreview();
		// move the marker without rebuilding the whole grid
		int i = (id - 1) % ICONS_PER_PAGE;
		if (m_marker) {
			m_marker->setPosition(cellPos(i, ICON_COLS));
			m_marker->setVisible(true);
		}
	}

	void onTarget(CCObject* sender) {
		m_colorTarget = static_cast<CCNode*>(sender)->getTag();
		m_colorMode = true;
		m_page = 0;
		this->refreshPreview();
		this->refreshGrid();
	}

	void onColor(CCObject* sender) {
		int idx = static_cast<CCNode*>(sender)->getTag();
		auto gm = GameManager::sharedState();
		if (m_colorTarget == 1) gm->setPlayerColor(idx);
		else gm->setPlayerColor2(idx);
		this->refreshPreview();
		this->refreshGrid();
	}

	void onGlow(CCObject*) {
		auto gm = GameManager::sharedState();
		gm->setPlayerGlow(!gm->getPlayerGlow());
		this->refreshPreview();
		this->refreshGrid();
	}
};

class $modify(GJGarageLayer) {
	bool init() {
		// the whole 2.1 garage is replaced
		if (!CCLayer::init()) return false;

		icons22::ensureEquipped();

		auto ui = Garage22::create(this);
		if (!ui) return false;
		this->addChild(ui);
		this->setKeypadEnabled(true);
		return true;
	}
};
