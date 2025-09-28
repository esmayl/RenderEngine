#pragma once

#include "FoodNode.h"
#include "GameEnums.h"
#include "Hazard.h"
#include "InstanceData.h"
#include "Objects/PartyParticle.h"
#include "Vector2D.h"

#include <deque>
#include <vector>

namespace Game
{
    struct GameWorldState
    {
        // Camera-independent gameplay targets
        Vector2D flockTarget{};
        Vector2D previousFlockTarget{};
        double flockTransitionTime = 0.0;
        double flockFrozenTime     = 0.0;

        // Colony state
        Vector2D nestPos{ 0.0f, 0.0f };
        std::vector<FoodNode> foodNodes;
        int activeFoodIndex     = -1;
        float minFoodSpacing    = 0.12f;
        float defaultFoodAmount = 100.0f;
        int depositingFoodIndex = -1;
        int maxAnts             = 512;
        int activeAnts          = 64;
        double spawnAccumulator = 0.0;
        float antsPerSecond     = 32.0f;
        int score               = 0;
        float antSpeed          = 0.5f;
        float initialSpeed      = 0.5f;
        float followDistance    = 0.05f;
        double legElapsed       = 0.0;
        double travelTime       = 0.0;
        float scoreCarryAccum   = 0.0f;
        AntMode mode            = AntMode::Idle;

        std::deque<double> pendingSpawns;

        // Config
        int initialAnts      = 10;
        double spawnDelaySec = 0.1;
        bool enableSugar     = true;
        bool enableHazard    = true;

        // Stage system
        GameState gameState     = GameState::Playing;
        int stage               = 1;
        double stageTimeLeft    = 60.0;
        int stageTarget         = 200;
        int stageScore          = 0;
        int combo               = 1;
        double sinceLastDeposit = 9999.0;
        bool endlessMode        = false;
        bool upgradePending     = false;

        // Random slow event (e.g., rain)
        bool slowActive      = false;
        double slowTimeLeft  = 0.0;
        double slowCooldown  = 8.0;
        double slowSinceLast = 0.0;

        // Hazard (repellent)
        Hazard hazard{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.04f, false };
        double hazardTimeLeft  = 0.0;
        double hazardDuration  = 5.0;
        double hazardSinceLast = 0.0;
        double hazardCooldown  = 12.0;

        // Celebration effects
        std::vector<PartyParticle> partyParticles;
        bool partyMode           = false;
        bool stageClearBurstDone = false;
        int konamiIndex          = 0;

        // Temporary power-ups
        bool frenzyActive      = false;
        double frenzyTimeLeft  = 0.0;
        double frenzyCooldown  = 12.0;
        double frenzySinceLast = 12.0;

        // Debug/gameplay toggles
        bool antsEnabled  = true;
        bool showDebugHud = false;

        // Bonus sugar spawner
        double bonusSpawnSince    = 0.0;
        double bonusSpawnInterval = 10.0;

        // CPU-side instance storage mirrored to GPU buffers
        std::vector<InstanceData> instances;
    };
} // namespace Game
