#pragma once
#include <Geode/Geode.hpp>

namespace icons22 {
	constexpr int COLOR_COUNT = 107; // ids 0..106, same table as GD 2.2

	// highest icon id of every kind in GD 2.2 (swing is intentionally left out)
	int maxId(IconType type);

	// make sure the 2.2 sprite frames of an icon are in the sprite frame cache
	void ensure(IconType type, int id);

	// load the sheets of every 2.2 icon (so garages that count frames see them)
	void preloadAll();

	// ensure every icon the player currently has equipped
	void ensureEquipped();

	cocos2d::ccColor3B color(int idx);

	// ids are valid for the 2.2 art (1..maxId)
	bool validIcon(IconType type, int id);
}
