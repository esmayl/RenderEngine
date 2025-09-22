//
// Created by esmayl on 22/09/2025.
//

#ifndef RENDERENGINE_PARTYPARTICLES_H
#define RENDERENGINE_PARTYPARTICLES_H


// --- Fun elements state ---
struct PartyParticle
{
    float x, y;     // pixels
    float vx, vy;   // pixels/sec
    float life;     // remaining seconds
    float ttl;      // total lifetime seconds
    uint32_t color; // IM_COL32 RGBA
    int shape;      // 0: rect, 1: tri, 2: circle
    float size;     // pixels
    float rot;      // radians
    float rotVel;   // radians/sec
};


#endif //RENDERENGINE_PARTYPARTICLES_H