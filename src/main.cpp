#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

int getMaxLives() {
    return Mod::get()->getSettingValue<int64_t>("lives-count");
}

float getInvincibilityTime() {
    return static_cast<float>(Mod::get()->getSettingValue<double>("invincibility-time"));
}

bool isProtectAll() {
    return Mod::get()->getSettingValue<std::string>("protection-mode") == "all";
}

float getCheckpointInterval() {
    return static_cast<float>(Mod::get()->getSettingValue<double>("checkpoint-interval"));
}

// ========== Game mode definitions ==========

struct GameMode {
    const char* name;
    const char* settingKey;
    int id; // internal index for applyMode
};

static const GameMode ALL_MODES[] = {
    {"CUBE",   "mode-cube",   0},
    {"SHIP",   "mode-ship",   1},
    {"BALL",   "mode-ball",   2},
    {"UFO",    "mode-ufo",    3},
    {"WAVE",   "mode-wave",   4},
    {"ROBOT",  "mode-robot",  5},
    {"SPIDER", "mode-spider", 6},
    {"SWING",  "mode-swing",  7},
};
static constexpr int TOTAL_MODES = 8;

// Build list of enabled modes from settings
std::vector<int> getEnabledModes() {
    std::vector<int> modes;
    for (int i = 0; i < TOTAL_MODES; i++) {
        if (Mod::get()->getSettingValue<bool>(ALL_MODES[i].settingKey)) {
            modes.push_back(i);
        }
    }
    if (modes.empty()) modes.push_back(0); // at least cube
    return modes;
}

// ========== Rescue ==========

struct SafePosition {
    CCPoint pos = {0.f, 0.f};
    bool valid = false;
    bool isGround = false; // was player on ground when saved?
};

static SafePosition g_safePos;

void doRescuePlayer(PlayLayer* pl) {
    if (!g_safePos.valid) return;

    auto player = pl->m_player1;
    if (!player || player->m_isDead) return;

    // Both required: direct field + virtual method (cocos2d node tree update)
    player->m_position = g_safePos.pos;
    player->setPosition(g_safePos.pos);
    player->setRotation(0.f);
    player->m_yVelocity = 0.0;
    player->m_fallSpeed = 0.0;
    player->m_wasTeleported = true;

    auto indicator = pl->getChildByTag(9997);
    if (indicator) {
        auto label = static_cast<CCLabelBMFont*>(indicator);
        label->setString("RESCUE!");
        label->setOpacity(255);
        label->stopAllActions();
        label->runAction(CCSequence::create(
            CCDelayTime::create(1.5f),
            CCFadeOut::create(0.5f),
            nullptr
        ));
    }
}

// ========== Game mode cycling ==========

void disableAllModes(PlayerObject* player) {
    player->toggleFlyMode(false, true);
    player->toggleRollMode(false, true);
    player->toggleBirdMode(false, true);
    player->toggleDartMode(false, true);
    player->toggleRobotMode(false, true);
    player->toggleSpiderMode(false, true);
    player->toggleSwingMode(false, true);
}

void applyMode(PlayerObject* player, int modeIndex) {
    disableAllModes(player);
    switch (modeIndex) {
        case 1: player->toggleFlyMode(true, true); break;    // Ship
        case 2: player->toggleRollMode(true, true); break;   // Ball
        case 3: player->toggleBirdMode(true, true); break;   // UFO
        case 4: player->toggleDartMode(true, true); break;   // Wave
        case 5: player->toggleRobotMode(true, true); break;  // Robot
        case 6: player->toggleSpiderMode(true, true); break; // Spider
        case 7: player->toggleSwingMode(true, true); break;  // Swing
        default: break; // 0 = Cube
    }
}

void doCycleGameMode(PlayLayer* pl) {
    auto player = pl->m_player1;
    if (!player || player->m_isDead) return;

    auto enabledModes = getEnabledModes();
    if (enabledModes.size() <= 1) return; // nothing to cycle

    // Get current position in cycle
    auto stateNode = pl->getChildByTag(9996);
    int currentModeIdx = stateNode ? static_cast<int>(stateNode->getPositionX()) : 0;

    // Find current mode in enabled list, advance to next
    int posInList = 0;
    for (int i = 0; i < (int)enabledModes.size(); i++) {
        if (enabledModes[i] == currentModeIdx) {
            posInList = i;
            break;
        }
    }
    int nextPos = (posInList + 1) % enabledModes.size();
    int nextModeIdx = enabledModes[nextPos];

    // Dummy portal for proper state transition
    auto dummyObj = TeleportPortalObject::create("edit_eGameRotBtn_001.png", true);
    dummyObj->m_cameraIsFreeMode = true;

    applyMode(player, nextModeIdx);
    pl->playerWillSwitchMode(player, dummyObj);

    if (stateNode) stateNode->setPositionX(static_cast<float>(nextModeIdx));

    player->m_yVelocity = 0.0;

    // Show indicator
    auto indicator = pl->getChildByTag(9997);
    if (indicator) {
        auto label = static_cast<CCLabelBMFont*>(indicator);
        label->setString(fmt::format("[M] {}", ALL_MODES[nextModeIdx].name).c_str());
        label->setOpacity(255);
        label->stopAllActions();
        label->runAction(CCSequence::create(
            CCDelayTime::create(1.5f),
            CCFadeOut::create(0.5f),
            nullptr
        ));
    }
}

// ========== Keybind listeners ==========

$execute {
    listenForKeybindSettingPresses("rescue-key",
        [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
            if (down && !repeat) {
                if (auto pl = PlayLayer::get()) {
                    doRescuePlayer(pl);
                }
            }
        });

    listenForKeybindSettingPresses("fly-toggle-key",
        [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
            if (down && !repeat) {
                if (auto pl = PlayLayer::get()) {
                    doCycleGameMode(pl);
                }
            }
        });
}

// ========== PlayLayer hook ==========

class $modify(LivesPlayLayer, PlayLayer) {

    struct Fields {
        int lives = 0;
        bool invincible = false;
        float checkpointTimer = 0.f;
    };

    void updateLabel() {
        auto label = static_cast<CCLabelBMFont*>(this->getChildByTag(9999));
        if (!label) return;

        label->setString(fmt::format("{}", m_fields->lives).c_str());

        int max = getMaxLives();
        float ratio = static_cast<float>(m_fields->lives) / max;

        if (ratio > 0.5f) {
            label->setColor({100, 255, 100});
        } else if (ratio > 0.2f) {
            label->setColor({255, 255, 100});
        } else {
            label->setColor({255, 80, 80});
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto player = m_player1;
        if (!player || player->m_isDead) return;

        // Save safe position at configurable interval
        // Prefer ground positions; if never on ground (ship/wave levels), accept any
        m_fields->checkpointTimer += dt;
        float interval = getCheckpointInterval();
        if (m_fields->checkpointTimer >= interval) {
            m_fields->checkpointTimer = 0.f;
            bool onGround = player->m_isOnGround;
            // Update if: not yet valid, or current is better (ground > air), or same type
            if (!g_safePos.valid || onGround || !g_safePos.isGround) {
                g_safePos.pos = player->m_position;
                g_safePos.valid = true;
                g_safePos.isGround = onGround;
            }
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        m_fields->lives = getMaxLives();
        m_fields->invincible = false;
        m_fields->checkpointTimer = 0.f;
        g_safePos = {};

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Lives counter - tag 9999
        auto label = CCLabelBMFont::create(
            fmt::format("{}", m_fields->lives).c_str(),
            "bigFont.fnt"
        );
        label->setScale(0.4f);
        label->setOpacity(200);
        label->setAnchorPoint({1.f, 1.f});
        label->setPosition({winSize.width - 10, winSize.height - 10});
        label->setZOrder(1000);
        label->setColor({100, 255, 100});
        label->setTag(9999);
        this->addChild(label);

        // Mode indicator - tag 9997
        auto modeLabel = CCLabelBMFont::create("", "bigFont.fnt");
        modeLabel->setScale(0.5f);
        modeLabel->setOpacity(0);
        modeLabel->setPosition({winSize.width / 2, winSize.height - 25});
        modeLabel->setZOrder(1000);
        modeLabel->setColor({255, 255, 255});
        modeLabel->setTag(9997);
        this->addChild(modeLabel);

        // Mode state tracker - tag 9996
        auto stateNode = CCNode::create();
        stateNode->setTag(9996);
        stateNode->setPositionX(0.f);
        this->addChild(stateNode);

        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (obj == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        bool shouldProtect = false;

        if (isProtectAll()) {
            shouldProtect = true;
        } else {
            if (obj) {
                auto type = obj->getType();
                shouldProtect = (type == GameObjectType::Hazard ||
                                 type == GameObjectType::AnimatedHazard);
            }
        }

        if (!shouldProtect) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        if (m_fields->invincible) return;

        if (m_fields->lives > 1) {
            m_fields->lives--;
            updateLabel();

            m_fields->invincible = true;

            float invTime = getInvincibilityTime();

            if (player) {
                player->stopActionByTag(42);
                auto blink = CCBlink::create(invTime, 8);
                blink->setTag(42);
                player->runAction(blink);
            }

            auto livesLabel = this->getChildByTag(9999);
            if (livesLabel) {
                livesLabel->runAction(CCSequence::create(
                    CCScaleTo::create(0.1f, 0.55f),
                    CCScaleTo::create(0.15f, 0.4f),
                    nullptr
                ));
            }

            this->stopActionByTag(43);
            auto seq = CCSequence::create(
                CCDelayTime::create(invTime),
                CCCallFunc::create(this, callfunc_selector(LivesPlayLayer::endInvincibility)),
                nullptr
            );
            seq->setTag(43);
            this->runAction(seq);

            return;
        }

        m_fields->invincible = false;
        m_fields->lives = getMaxLives();
        PlayLayer::destroyPlayer(player, obj);
    }

    void endInvincibility() {
        m_fields->invincible = false;
    }

    void resetLevel() {
        m_fields->lives = getMaxLives();
        m_fields->invincible = false;
        m_fields->checkpointTimer = 0.f;
        g_safePos = {};

        auto stateNode = this->getChildByTag(9996);
        if (stateNode) stateNode->setPositionX(0.f);

        PlayLayer::resetLevel();
        updateLabel();
    }

    void onQuit() {
        PlayLayer::onQuit();
    }
};
