#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/UILayer.hpp>

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

// Free functions that operate on PlayLayer directly
void doRescuePlayer(PlayLayer* pl) {
    auto player = pl->m_player1;
    if (!player || player->m_isDead) return;

    player->m_yVelocity = 0.0;
    player->setRotation(0.f);

    // Find safe Y from our stored tag, or use ground level
    float targetY = 105.f;
    auto safeNode = pl->getChildByTag(9998);
    if (safeNode) {
        targetY = safeNode->getPositionY();
    }
    player->setPositionY(targetY);

    // Show indicator
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

void doToggleFlyMode(PlayLayer* pl) {
    auto player = pl->m_player1;
    if (!player || player->m_isDead) return;

    // Check current fly state via tag node
    auto stateNode = pl->getChildByTag(9996);
    bool isFlying = stateNode && stateNode->getPositionX() > 0;

    if (!isFlying) {
        player->toggleFlyMode(true, false);
        if (stateNode) stateNode->setPositionX(1.f);
    } else {
        player->toggleFlyMode(false, false);
        if (stateNode) stateNode->setPositionX(0.f);
    }

    player->m_yVelocity = 0.0;

    // Show indicator
    auto indicator = pl->getChildByTag(9997);
    if (indicator) {
        auto label = static_cast<CCLabelBMFont*>(indicator);
        label->setString(!isFlying ? "[M] FLY" : "[M] CUBE");
        label->setOpacity(255);
        label->stopAllActions();
        label->runAction(CCSequence::create(
            CCDelayTime::create(1.5f),
            CCFadeOut::create(0.5f),
            nullptr
        ));
    }
}

// ========== UILayer hook for keybinds ==========

class $modify(LivesUILayer, UILayer) {
    void keyDown(cocos2d::enumKeyCodes key, double timestamp) {
        auto pl = PlayLayer::get();
        if (pl) {
            if (key == cocos2d::enumKeyCodes::KEY_B) {
                doRescuePlayer(pl);
                return;
            }
            if (key == cocos2d::enumKeyCodes::KEY_M) {
                doToggleFlyMode(pl);
                return;
            }
        }
        UILayer::keyDown(key, timestamp);
    }
};

// ========== PlayLayer hook for lives system ==========

class $modify(LivesPlayLayer, PlayLayer) {

    struct Fields {
        int lives = 0;
        bool invincible = false;
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

    // Track safe position
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto player = m_player1;
        if (!player || player->m_isDead) return;

        if (player->m_isOnGround) {
            // Store safe Y in a hidden node
            auto safeNode = this->getChildByTag(9998);
            if (safeNode) {
                safeNode->setPositionY(player->getPositionY());
            }
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        m_fields->lives = getMaxLives();
        m_fields->invincible = false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Lives counter (top right) - tag 9999
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

        // Mode indicator (center top) - tag 9997
        auto modeLabel = CCLabelBMFont::create("", "bigFont.fnt");
        modeLabel->setScale(0.5f);
        modeLabel->setOpacity(0);
        modeLabel->setPosition({winSize.width / 2, winSize.height - 25});
        modeLabel->setZOrder(1000);
        modeLabel->setColor({255, 255, 255});
        modeLabel->setTag(9997);
        this->addChild(modeLabel);

        // Hidden node for safe Y position - tag 9998
        auto safeNode = CCNode::create();
        safeNode->setTag(9998);
        safeNode->setPositionY(105.f);
        this->addChild(safeNode);

        // Hidden node for fly mode state - tag 9996
        auto stateNode = CCNode::create();
        stateNode->setTag(9996);
        stateNode->setPositionX(0.f); // 0 = cube, 1 = fly
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

        auto stateNode = this->getChildByTag(9996);
        if (stateNode) stateNode->setPositionX(0.f);

        PlayLayer::resetLevel();
        updateLabel();
    }

    void onQuit() {
        PlayLayer::onQuit();
    }
};
