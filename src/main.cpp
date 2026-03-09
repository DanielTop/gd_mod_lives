#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

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

class $modify(LivesPlayLayer, PlayLayer) {

    struct Fields {
        int lives = 0;
        bool invincible = false;
        bool flyMode = false;
        CCLabelBMFont* livesLabel = nullptr;
        CCLabelBMFont* modeLabel = nullptr;
        float safeY = 0.f;
        bool hasSafePos = false;
    };

    void updateLabel() {
        auto label = m_fields->livesLabel;
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

    void showModeIndicator(const char* text) {
        auto label = m_fields->modeLabel;
        if (!label) return;

        label->setString(text);
        label->setOpacity(255);
        label->stopAllActions();
        label->runAction(CCSequence::create(
            CCDelayTime::create(1.5f),
            CCFadeOut::create(0.5f),
            nullptr
        ));
    }

    void rescuePlayer() {
        auto player = m_player1;
        if (!player || player->m_isDead) return;

        // Reset velocity
        player->m_yVelocity = 0.0;

        // Teleport to safe Y or ground level
        float groundLevel = 105.f;
        float targetY = m_fields->hasSafePos ? m_fields->safeY : groundLevel;
        player->setPositionY(targetY);

        // Reset rotation
        player->setRotation(0.f);

        // Visual feedback — flash white
        player->stopActionByTag(44);
        auto flash = CCSequence::create(
            CCTintTo::create(0.05f, 255, 255, 255),
            CCTintTo::create(0.2f, 255, 255, 255),
            nullptr
        );
        flash->setTag(44);
        player->runAction(flash);

        showModeIndicator("RESCUE!");
    }

    void toggleFlyMode() {
        auto player = m_player1;
        if (!player || player->m_isDead) return;

        m_fields->flyMode = !m_fields->flyMode;

        if (m_fields->flyMode) {
            player->toggleFlyMode(true, false);
            showModeIndicator("[M] FLY");
        } else {
            player->toggleFlyMode(false, false);
            showModeIndicator("[M] CUBE");
        }

        // Reset velocity on mode switch to avoid weird physics
        player->m_yVelocity = 0.0;
    }

    // Track safe position
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto player = m_player1;
        if (!player || player->m_isDead) return;

        if (player->m_isOnGround) {
            m_fields->safeY = player->getPositionY();
            m_fields->hasSafePos = true;
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        m_fields->lives = getMaxLives();
        m_fields->invincible = false;
        m_fields->flyMode = false;
        m_fields->hasSafePos = false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Lives counter (top right)
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
        m_fields->livesLabel = label;

        // Mode indicator (center top, fades out)
        auto modeLabel = CCLabelBMFont::create("", "bigFont.fnt");
        modeLabel->setScale(0.5f);
        modeLabel->setOpacity(0);
        modeLabel->setPosition({winSize.width / 2, winSize.height - 25});
        modeLabel->setZOrder(1000);
        modeLabel->setColor({255, 255, 255});
        this->addChild(modeLabel);
        m_fields->modeLabel = modeLabel;

        return true;
    }

    void keyDown(cocos2d::enumKeyCodes key, double timestamp) {
        // B = rescue / teleport to safe position
        if (key == cocos2d::enumKeyCodes::KEY_B) {
            rescuePlayer();
            return;
        }

        // M = toggle fly/cube mode
        if (key == cocos2d::enumKeyCodes::KEY_M) {
            toggleFlyMode();
            return;
        }

        PlayLayer::keyDown(key, timestamp);
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Always let the anticheat spike through
        if (obj == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        // Determine if this death should be intercepted
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

        // During invincibility, ignore hits
        if (m_fields->invincible) return;

        // Still have lives — survive
        if (m_fields->lives > 1) {
            m_fields->lives--;
            updateLabel();

            m_fields->invincible = true;

            float invTime = getInvincibilityTime();

            // Blink player
            if (player) {
                player->stopActionByTag(42);
                auto blink = CCBlink::create(invTime, 8);
                blink->setTag(42);
                player->runAction(blink);
            }

            // Pulse label
            if (m_fields->livesLabel) {
                m_fields->livesLabel->runAction(CCSequence::create(
                    CCScaleTo::create(0.1f, 0.55f),
                    CCScaleTo::create(0.15f, 0.4f),
                    nullptr
                ));
            }

            // End invincibility after delay
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

        // Out of lives — real death
        m_fields->livesLabel = nullptr;
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
        m_fields->flyMode = false;
        m_fields->hasSafePos = false;
        PlayLayer::resetLevel();
        updateLabel();
    }

    void onQuit() {
        m_fields->livesLabel = nullptr;
        m_fields->modeLabel = nullptr;
        PlayLayer::onQuit();
    }
};
