#pragma once

#include "GameWorldState.h"

#include <Windows.h>

class InstancedRendererEngine2D;

namespace Game
{
    class AntGame
    {
      public:
        explicit AntGame( InstancedRendererEngine2D& renderer );

        void initialize();
        void update( double dt );

        void handleEvent( UINT msg, WPARAM wParam, LPARAM lParam );
        void onResize( int width, int height );

        void toggleEndless( bool enabled );
        void applyUpgrade( int option );
        void advanceStage();
        void restartGame();

        int findNearestFoodScreen( int x, int y, float maxPixelRadius ) const;
        int getActiveFoodIndex() const;
        void setActiveFoodByIndex( int index );

        GameWorldState& state()
        {
            return state_;
        }
        const GameWorldState& state() const
        {
            return state_;
        }

      private:
        void loadSettings();
        void resetGame();
        void resetAnts();
        void startStage( int number );
        void updateGameLogic( double dt );
        void updateHazard( double dt );
        void updateEvents( double dt );
        void updatePendingSpawns( double dt );
        void updateStageProgress();
        void rebuildDepartureStagger();
        void spawnRandomFood( int count );
        void spawnFoodAtScreen( int x, int y, float amount );
        void setFlockTarget( int x, int y );
        void setFood( int x, int y, float amount );
        void setNest( int x, int y );
        void triggerConfettiBurst( int x, int y, int count );
        void updateParty( double dt );

        InstancedRendererEngine2D& renderer_;
        GameWorldState state_;
    };
} // namespace Game
