#pragma once
#include <random>
#include <algorithm>
#include <cstdint>
#include <vector>

// Simple visual dice animation (pseudo-3D) used for quick feedback.
// Lightweight: draws a rotating square with the face number via ImGui draw list.

class DiceVisual {
public:
    static DiceVisual& Instance();

    // start a roll animation for a die with `sides` and eventual face `face`.
    void StartRoll(int sides, int face);
    // start multiple dice visual; `faces` contains each die's final face (will be displayed when settled)
    void StartRollFaces(int sides, const std::vector<int>& faces);

    // per-frame update (dt in seconds)
    void Update(float dt);

    // render overlay (uses ImGui drawlist)
    void Render();
    // Cancel any running animation / clear visual state
    void Cancel();

private:
    DiceVisual() = default;

    int m_sides = 6;
    int m_targetFace = 1;
    int m_currentFace = 1;
    bool m_active = false;
    float m_timer = 0.0f;
    float m_duration = 0.9f;
    float m_scale = 1.0f;

    // rotation angles
    float m_angleX = 0.0f;
    float m_angleY = 0.0f;

    // random generator for interim faces
    std::mt19937 m_rng{ std::random_device{}() };
    float m_faceFlipTimer = 0.0f;
    // legacy single-axis spin for older implementation
    float m_spin = 0.0f;
    // --- lightweight physical state (screen-space pixels) ---
    float m_posX = 0.0f;         // screen x
    float m_posY = 0.0f;         // screen y
    float m_velX = 0.0f;         // px/sec
    float m_velY = 0.0f;         // px/sec
    float m_angle = 0.0f;        // radians
    float m_angVel = 0.0f;       // rad/sec
    bool  m_physical = false;    // use physics simulation
    float m_stopTimer = 0.0f;    // accumulate when nearly stopped
    // physics params (tunable)
    float m_gravity = 1800.0f;   // px/sec^2
    float m_restitution = 0.35f; // bounce factor
    float m_friction = 0.92f;    // per-impact damp
    float m_linearDamp = 0.88f;  // per-frame floor damping
    float m_stopThresholdVel = 30.0f; // px/sec
    float m_stopThresholdAng = 4.0f;  // rad/sec
    // show final result for a short time after stopping
    float m_resultDisplayDuration = 2.0f; // seconds
    float m_resultDisplayTimer = 0.0f;
    // multiple-dice visual state
    struct ActiveDie {
        int face = 0;
        float posX = 0.0f, posY = 0.0f;
        float velX = 0.0f, velY = 0.0f;
        float angle = 0.0f, angVel = 0.0f;
        float scale = 1.0f;
        float stopTimer = 0.0f;
        bool settled = false;
    };
    std::vector<ActiveDie> m_dice;
};
