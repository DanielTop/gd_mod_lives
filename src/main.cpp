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
        CCLabelBMFont* livesLabel = nullptr;
        float safeY = 0.f;       // last known safe Y position
        float groundY = 0.f;     // ground level Y
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

    // Track safe position every frame
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto player = m_player1;
        if (!player || player->m_isDead) return;

        float y = player->getPositionY();
        float groundLevel = m_groundLayer ? m_groundLayer->getPositionY() + 105.f : 105.f;

        // Save ground reference
        m_fields->groundY = groundLevel;

        // Consider position "safe" when player is on ground or
        // within reasonable screen bounds
        if (player->m_isOnGround) {
            m_fields->safeY = y;
            m_fields->hasSafePos = true;
        }
    }

    void stabilizePlayer(PlayerObject* player) {
        if (!player) return;

        // Reset vertical velocity to stop flying off
        player->m_yVelocity = 0.0;

        // Clamp Y position to screen bounds
        float minY = m_fields->groundY > 0 ? m_fields->groundY : 105.f;
        float maxY = CCDirector::sharedDirector()->getWinSize().height * 3.f;
        float curY = player->getPositionY();

        if (curY < minY || curY > maxY) {
            // Teleport to last safe Y, or ground level as fallback
            float targetY = m_fields->hasSafePos ? m_fields->safeY : minY;
            player->setPositionY(targetY);
        }

        // Reset rotation momentum
        player->setRotation(0.f);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        m_fields->lives = getMaxLives();
        m_fields->invincible = false;
        m_fields->hasSafePos = false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
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

        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Always let the anticheat spike through — blocking it
        // breaks slope physics
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

        // During invincibility, ignore hits but still stabilize
        if (m_fields->invincible) {
            stabilizePlayer(player);
            return;
        }

        // Still have lives — survive
        if (m_fields->lives > 1) {
            m_fields->lives--;
            updateLabel();

            m_fields->invincible = true;

            // Stabilize player position and velocity
            stabilizePlayer(player);

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
        m_fields->hasSafePos = false;
        PlayLayer::resetLevel();
        updateLabel();
    }

    void onQuit() {
        m_fields->livesLabel = nullptr;
        PlayLayer::onQuit();
    }
};
