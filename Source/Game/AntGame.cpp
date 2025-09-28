#include "AntGame.h"

#include "InstancedRendererEngine2D.h"
#include "Utilities.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>

using namespace Game;

AntGame::AntGame( InstancedRendererEngine2D& renderer ) : renderer_( renderer )
{
}

void AntGame::initialize()
{
    renderer_.SetGame( this );
    loadSettings();

    // Prewarm instance storage with a reasonable capacity
    state_.instances.clear();
    state_.instances.reserve( 1024 );
    const int initialCount = 1024;
    for ( int i = 0; i < initialCount; ++i )
    {
        InstanceData instance{};
        instance.posX       = state_.nestPos.x;
        instance.posY       = state_.nestPos.y;
        instance.goalX      = state_.nestPos.x;
        instance.goalY      = state_.nestPos.y;
        instance.laneOffset = 0.1f;
        instance.speedScale = 1.0f;
        instance.color = DirectX::XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f );
        instance.movementState = 0;
        instance.sourceIndex   = -1;
        instance.holdTimer     = 0.0f;
        state_.instances.emplace_back( instance );
    }

    renderer_.InitializeSimulationBuffers( state_.instances );

    resetGame();
    startStage( 1 );
}

void AntGame::update( double dt )
{
    updateGameLogic( dt );
}

void AntGame::handleEvent( UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch ( msg )
    {
    case WM_KEYUP:
        if ( wParam == 'R' )
        {
            resetGame();
            startStage( 1 );
        }
        else if ( wParam == 'N' )
        {
            advanceStage();
        }
        else if ( wParam == 'E' )
        {
            toggleEndless( true );
        }
        else if ( wParam == '1' || wParam == VK_NUMPAD1 )
        {
            applyUpgrade( 1 );
        }
        else if ( wParam == '2' || wParam == VK_NUMPAD2 )
        {
            applyUpgrade( 2 );
        }
        else if ( wParam == '3' || wParam == VK_NUMPAD3 )
        {
            applyUpgrade( 3 );
        }

        // Konami Code tracking
        {
            static const WPARAM seq[] = { VK_UP,    VK_UP,   VK_DOWN,  VK_DOWN, VK_LEFT,
                                          VK_RIGHT, VK_LEFT, VK_RIGHT, 'B',     'A' };
            constexpr int seqLen      = sizeof( seq ) / sizeof( seq[0] );
            if ( wParam == seq[state_.konamiIndex] )
            {
                state_.konamiIndex++;
                if ( state_.konamiIndex >= seqLen )
                {
                    state_.konamiIndex = 0;
                    state_.partyMode   = !state_.partyMode;
                    triggerConfettiBurst( renderer_.GetScreenWidth() / 2, renderer_.GetScreenHeight() / 2, 160 );
                }
            }
            else
            {
                state_.konamiIndex = ( wParam == seq[0] ) ? 1 : 0;
            }
        }

        if ( wParam == 'F' )
        {
            if ( !state_.frenzyActive && state_.frenzySinceLast >= state_.frenzyCooldown )
            {
                state_.frenzyActive    = true;
                state_.frenzyTimeLeft  = 4.0;
                state_.frenzySinceLast = 0.0;
            }
        }

        if ( wParam == VK_F1 || wParam == 'H' )
        {
            state_.showDebugHud = !state_.showDebugHud;
        }
        break;
    case WM_LBUTTONUP:
    {
        const int x = GET_X_LPARAM( lParam );
        const int y = GET_Y_LPARAM( lParam );
        if ( state_.partyMode )
        {
            triggerConfettiBurst( x, y, 90 );
        }
        int idx = findNearestFoodScreen( x, y, 24.0f );
        if ( idx >= 0 )
        {
            if ( idx == state_.activeFoodIndex )
            {
                setActiveFoodByIndex( -1 );
            }
            else
            {
                setActiveFoodByIndex( idx );
            }
        }
        break;
    }
    default:
        break;
    }
}

void AntGame::onResize( int width, int height )
{
    renderer_.OnResize( width, height );
}

void AntGame::toggleEndless( bool enabled )
{
    state_.endlessMode = enabled;
}

void AntGame::applyUpgrade( int option )
{
    if ( state_.gameState != GameState::StageClear )
        return;

    switch ( option )
    {
    case 1:
        state_.antSpeed *= 1.15f;
        break;
    case 2:
        state_.antsPerSecond *= 1.25f;
        state_.pendingSpawns.clear();
        state_.spawnAccumulator = 0.0;
        rebuildDepartureStagger();
        break;
    case 3:
        state_.maxAnts += 128;
        break;
    default:
        break;
    }

    advanceStage();
}

int AntGame::findNearestFoodScreen( int x, int y, float maxPixelRadius ) const
{
    if ( state_.foodNodes.empty() )
        return -1;

    const auto mouse     = renderer_.ScreenToWorld( x, y );
    const float maxDist2 = maxPixelRadius * maxPixelRadius;
    int bestIndex        = -1;
    float bestDist2      = maxDist2;

    for ( size_t i = 0; i < state_.foodNodes.size(); ++i )
    {
        const auto& node = state_.foodNodes[i];
        const float dx   = mouse.x - node.pos.x;
        const float dy   = mouse.y - node.pos.y;
        const float d2   = dx * dx + dy * dy;
        if ( d2 < bestDist2 )
        {
            bestDist2 = d2;
            bestIndex = static_cast<int>( i );
        }
    }

    return bestIndex;
}

int AntGame::getActiveFoodIndex() const
{
    return state_.activeFoodIndex;
}

void AntGame::setActiveFoodByIndex( int index )
{
    if ( index < -1 || index >= static_cast<int>( state_.foodNodes.size() ) )
        return;
    state_.activeFoodIndex = index;
    rebuildDepartureStagger();
}

void AntGame::loadSettings()
{
    const char* candidates[] = { "settings.ini", "../settings.ini", "../../settings.ini" };
    std::ifstream f;
    for ( const char* path : candidates )
    {
        f.open( path );
        if ( f.is_open() )
            break;
    }
    if ( !f.is_open() )
        return;

    std::string line;
    auto trim = []( std::string& s )
    {
        size_t a = s.find_first_not_of( " \t\r\n" );
        size_t b = s.find_last_not_of( " \t\r\n" );
        if ( a == std::string::npos )
        {
            s.clear();
        }
        else
        {
            s = s.substr( a, b - a + 1 );
        }
    };

    while ( std::getline( f, line ) )
    {
        if ( line.empty() || line[0] == '#' || line[0] == ';' ||
             ( line.size() > 1 && line[0] == '/' && line[1] == '/' ) )
            continue;
        auto pos = line.find( '=' );
        if ( pos == std::string::npos )
            continue;
        std::string key = line.substr( 0, pos );
        std::string val = line.substr( pos + 1 );
        trim( key );
        trim( val );
        try
        {
            if ( key == "initialAnts" )
            {
                state_.initialAnts = max( 0, std::stoi( val ) );
            }
            else if ( key == "antsPerSecond" )
            {
                state_.antsPerSecond = max( 0.0f, std::stof( val ) );
            }
            else if ( key == "spawnDelaySec" )
            {
                state_.spawnDelaySec = max( 0.0, std::stod( val ) );
            }
            else if ( key == "defaultFoodAmount" )
            {
                state_.defaultFoodAmount = max( 0.0f, std::stof( val ) );
            }
            else if ( key == "minFoodSpacing" )
            {
                state_.minFoodSpacing = max( 0.0f, std::stof( val ) );
            }
            else if ( key == "initialSpeed" )
            {
                state_.initialSpeed = max( 0.0f, std::stof( val ) );
                state_.antSpeed     = state_.initialSpeed;
            }
        }
        catch ( ... )
        {
        }
    }

    state_.maxAnts = state_.initialAnts;
}

void AntGame::resetGame()
{
    state_.score            = 0;
    state_.stageScore       = 0;
    state_.stage            = 1;
    state_.gameState        = GameState::Playing;
    state_.combo            = 1;
    state_.sinceLastDeposit = 9999.0;
    state_.slowActive       = false;
    state_.slowSinceLast    = 0.0;
    state_.slowTimeLeft     = 0.0;
    state_.foodNodes.clear();
    state_.activeFoodIndex  = -1;
    state_.activeAnts       = 0;
    state_.spawnAccumulator = 0.0;
    state_.pendingSpawns.clear();
}

void AntGame::resetAnts()
{
    double sendInterval = max( 0.02, 1.0 / max( 0.01, (double)state_.antsPerSecond ) );

    for ( int i = 0; i < (int)state_.instances.size(); ++i )
    {
        InstanceData& it = state_.instances[i];
        it.posX          = state_.nestPos.x;
        it.posY          = state_.nestPos.y;
        it.directionX    = 0.0f;
        it.directionY    = 0.0f;
        it.goalX         = state_.nestPos.x;
        it.goalY         = state_.nestPos.y;
        it.movementState = 1;
        it.holdTimer     = ( i < state_.activeAnts ) ? (float)( sendInterval * i ) : 9999.0f;
    }

    renderer_.UploadInstanceBuffer( state_.instances );
    state_.legElapsed = 0.0;
    state_.travelTime = 0.0;
}

void AntGame::startStage( int number )
{
    state_.stage            = number;
    state_.stageTarget      = 150 + ( state_.stage - 1 ) * 120;
    state_.stageTimeLeft    = 60.0 + ( state_.stage - 1 ) * 10.0;
    state_.antsPerSecond    = 20.0f + state_.stage * 10.0f;
    state_.stageScore       = 0;
    state_.combo            = 1;
    state_.sinceLastDeposit = 9999.0;
    state_.slowActive       = false;
    state_.slowSinceLast    = 0.0;
    state_.slowTimeLeft     = 0.0;
    state_.foodNodes.clear();

    int count = min( 3 + state_.stage, 10 );
    spawnRandomFood( count );
    state_.activeFoodIndex = -1;
    state_.mode            = AntMode::Idle;

    state_.activeAnts       = min( state_.maxAnts, state_.initialAnts );
    state_.spawnAccumulator = 0.0;
    state_.pendingSpawns.clear();

    resetAnts();
    state_.mode = ( state_.activeFoodIndex >= 0 ) ? AntMode::ToFood : AntMode::Idle;
}

void AntGame::advanceStage()
{
    state_.stageClearBurstDone = false;
    startStage( state_.stage + 1 );
    state_.gameState      = GameState::Playing;
    state_.upgradePending = false;
}

void AntGame::restartGame()
{
    resetGame();
    startStage( 1 );
}

void AntGame::updateGameLogic( double dt )
{
    updateHazard( dt );

    if ( state_.gameState == GameState::GameOver )
        return;
    if ( state_.gameState == GameState::StageClear )
    {
        state_.mode = AntMode::Idle;
        updateParty( dt );
        return;
    }

    state_.stageTimeLeft = max( 0.0, state_.stageTimeLeft - dt );
    updateEvents( dt );
    updatePendingSpawns( dt );
    updateStageProgress();
    updateParty( dt );

    if ( state_.mode == AntMode::Idle )
        return;

    state_.legElapsed += dt;
    state_.sinceLastDeposit += dt;

    if ( state_.antSpeed <= 0.0f )
        return;
}

void AntGame::updateHazard( double dt )
{
    if ( !state_.hazard.active )
        return;

    state_.hazard.pos.x += state_.hazard.vel.x * static_cast<float>( dt );
    state_.hazard.pos.y += state_.hazard.vel.y * static_cast<float>( dt );

    if ( state_.hazard.pos.x > 1.0f - state_.hazard.radius )
    {
        state_.hazard.pos.x = 1.0f - state_.hazard.radius;
        state_.hazard.vel.x *= -1.0f;
    }
    if ( state_.hazard.pos.x < -1.0f + state_.hazard.radius )
    {
        state_.hazard.pos.x = -1.0f + state_.hazard.radius;
        state_.hazard.vel.x *= -1.0f;
    }
    if ( state_.hazard.pos.y > 1.0f - state_.hazard.radius )
    {
        state_.hazard.pos.y = 1.0f - state_.hazard.radius;
        state_.hazard.vel.y *= -1.0f;
    }
    if ( state_.hazard.pos.y < -1.0f + state_.hazard.radius )
    {
        state_.hazard.pos.y = -1.0f + state_.hazard.radius;
        state_.hazard.vel.y *= -1.0f;
    }
}

void AntGame::updateEvents( double dt )
{
    state_.slowSinceLast += dt;
    state_.frenzySinceLast += dt;
}

void AntGame::updatePendingSpawns( double dt )
{
    if ( state_.mode == AntMode::ToFood && state_.activeFoodIndex >= 0 )
    {
        int capacity = state_.maxAnts - ( state_.activeAnts + static_cast<int>( state_.pendingSpawns.size() ) );
        if ( capacity > 0 && state_.pendingSpawns.empty() )
        {
            double interval = max( 0.01, state_.spawnDelaySec );
            state_.pendingSpawns.push_back( interval );
        }
    }

    if ( state_.pendingSpawns.empty() )
        return;

    for ( double& t : state_.pendingSpawns )
        t -= dt;

    if ( state_.pendingSpawns.front() > 0.0 || state_.activeAnts >= state_.maxAnts )
        return;

    state_.pendingSpawns.pop_front();
    int slot = state_.activeAnts;
    state_.activeAnts++;

    InstanceData init = {};
    init.posX         = state_.nestPos.x;
    init.posY         = state_.nestPos.y;
    init.directionX   = 0.0f;
    init.directionY   = 0.0f;
    init.goalX        = state_.nestPos.x;
    init.goalY        = state_.nestPos.y;
    init.laneOffset   = 0.03f;
    init.speedScale   = 1.15f;
    init.color = DirectX::XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f );
    init.movementState = 1;
    init.sourceIndex   = -1;
    init.holdTimer     = static_cast<float>( max( 0.01, state_.spawnDelaySec ) );

    if ( slot >= static_cast<int>( state_.instances.size() ) )
    {
        state_.instances.push_back( init );
        renderer_.ResizeInstanceStorage( state_.instances );
    }
    else
    {
        state_.instances[slot] = init;
        renderer_.UploadInstanceSlot( slot, init );
    }
}

void AntGame::updateStageProgress()
{
    if ( state_.stageScore >= state_.stageTarget )
    {
        resetAnts();
        state_.gameState = GameState::StageClear;
    }
    else if ( state_.stageTimeLeft <= 0.0 )
    {
        if ( state_.endlessMode )
        {
            advanceStage();
        }
        else
        {
            state_.gameState = GameState::GameOver;
        }
    }
}

void AntGame::rebuildDepartureStagger()
{
    double sendInterval = max( 0.02, 1.0 / max( 0.01, (double)state_.antsPerSecond ) );
    for ( int i = 0; i < state_.activeAnts && i < (int)state_.instances.size(); ++i )
    {
        InstanceData& it = state_.instances[i];
        if ( it.movementState == 1 )
        {
            it.holdTimer = static_cast<float>( sendInterval * i );
        }
    }
    renderer_.UploadInstanceBuffer( state_.instances );
}

void AntGame::spawnRandomFood( int count )
{
    for ( int i = 0; i < count; ++i )
    {
        float x = 0.5f;
        float y = 0.5f;
        spawnFoodAtScreen( static_cast<int>( ( x * 0.5f + 0.5f ) * renderer_.GetScreenWidth() ),
                           static_cast<int>( ( -y * 0.5f + 0.5f ) * renderer_.GetScreenHeight() ),
                           state_.defaultFoodAmount );
    }
}

void AntGame::spawnFoodAtScreen( int x, int y, float amount )
{
    Vector2D p = renderer_.ScreenToWorld( x, y );
    state_.foodNodes.emplace_back( FoodNode{ p, amount } );
}

void AntGame::setFlockTarget( int x, int y )
{
    state_.flockTarget         = renderer_.ScreenToWorld( x, y );
    state_.previousFlockTarget = state_.flockTarget;
}

void AntGame::setFood( int x, int y, float amount )
{
    spawnFoodAtScreen( x, y, amount );
}

void AntGame::setNest( int x, int y )
{
    state_.nestPos = renderer_.ScreenToWorld( x, y );
    resetAnts();
}

void AntGame::triggerConfettiBurst( int x, int y, int count )
{
    for ( int i = 0; i < count; ++i )
    {
        PartyParticle p{};
        p.x       = static_cast<float>( x );
        p.y       = static_cast<float>( y );
        // float ang = RandomGenerator::Generate( 0.0f, 6.2831853f );
        // float spd = RandomGenerator::Generate( 120.0f, 420.0f );
        // p.vx      = std::cos( ang ) * spd;
        // p.vy      = std::sin( ang ) * spd - RandomGenerator::Generate( 80.0f, 160.0f );
        // p.ttl     = RandomGenerator::Generate( 1.0f, 2.0f );
        p.life    = p.ttl;
        // int r     = static_cast<int>( RandomGenerator::Generate( 80.0f, 255.0f ) );
        // int g     = static_cast<int>( RandomGenerator::Generate( 80.0f, 255.0f ) );
        // int b     = static_cast<int>( RandomGenerator::Generate( 80.0f, 255.0f ) );
        int a     = 230;
        // p.color   = IM_COL32( r, g, b, a );
        // p.shape   = static_cast<int>( RandomGenerator::Generate( 0.0f, 2.99f ) );
        // p.size    = RandomGenerator::Generate( 4.0f, 10.0f );
        // p.rot     = RandomGenerator::Generate( 0.0f, 6.2831853f );
        // p.rotVel  = RandomGenerator::Generate( -6.0f, 6.0f );
        state_.partyParticles.emplace_back( p );
    }
}

void AntGame::updateParty( double dt )
{
    if ( state_.partyParticles.empty() )
        return;

    for ( auto& p : state_.partyParticles )
    {
        p.life -= static_cast<float>( dt );
        if ( p.life < 0.0f )
            p.life = 0.0f;
        float t = ( p.ttl > 0.0f ) ? ( 1.0f - p.life / p.ttl ) : 1.0f;
        p.y += p.vy * static_cast<float>( dt );
        p.x += p.vx * static_cast<float>( dt );
        p.vy += 140.0f * static_cast<float>( dt );
        p.rot += p.rotVel * static_cast<float>( dt );
        int alpha = static_cast<int>( 255.0f * max( 0.0f, 1.0f - t ) );
        p.color   = ( p.color & 0x00FFFFFF ) | ( alpha << 24 );
    }

    state_.partyParticles.erase( std::remove_if( state_.partyParticles.begin(), state_.partyParticles.end(), []( const PartyParticle& p ) { return p.life <= 0.0f; } ), state_.partyParticles.end() );
}
