#include "DiceVisual.h"
#include "system/imgui/imgui.h"
#include "TextureManager.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

DiceVisual& DiceVisual::Instance() {
    static DiceVisual inst;
    return inst;
}

void DiceVisual::StartRollFaces(int sides, const std::vector<int>& faces) {
    // initialize multiple dice with spread positions and random velocities
    m_sides = (sides > 0) ? sides : 6;
    if (faces.empty()) return;
    ImGuiIO& io = ImGui::GetIO();
    float cx = io.DisplaySize.x * 0.5f;
    float startY = -120.0f;
    std::uniform_real_distribution<float> vx(-180.0f, 180.0f);
    std::uniform_real_distribution<float> ang(-16.0f, 16.0f);
    // clear previous dice and create new set for this roll
    m_dice.clear();
    size_t totalCount = faces.size();
    // adapt spacing based on count to avoid overlap
    float baseSpacing = 80.0f;
    // increase spacing slightly with count, clamp to reasonable range
    float spacing = baseSpacing + std::min(10.0f, (float)totalCount * 6.0f);
    for (size_t i = 0; i < faces.size(); ++i) {
        ActiveDie d;
        d.face = faces[i];
        // spread horizontally across this roll with adjustable spacing
        float spread = ((float)i - (float)totalCount * 0.5f) * spacing;
        d.posX = cx + spread;
        // small random vertical offset so dice don't overlap exactly
        std::uniform_real_distribution<float> randY(-18.0f, 18.0f);
        d.posY = startY + randY(m_rng) - std::fabs(spread) * 0.06f;
        d.velX = vx(m_rng) * 0.7f + (spread * 0.02f);
        d.velY = 240.0f + std::abs(vx(m_rng)) * 0.4f; // initial downward
        // scale down slightly when many dice to help fit
        d.scale = std::max(0.6f, 1.0f - (float)totalCount * 0.035f);
        d.angle = 0.0f;
        d.angVel = ang(m_rng);
        d.scale = 1.0f;
        d.stopTimer = 0.0f;
        d.settled = false;
        m_dice.push_back(d);
    }
    m_active = true;
    m_physical = true;
    m_timer = 0.0f;
}

void DiceVisual::Cancel()
{
    m_active = false;
    m_physical = false;
    m_currentFace = 0;
    m_resultDisplayTimer = 0.0f;
}

void DiceVisual::StartRoll(int sides, int face) {
    m_sides = (sides > 0) ? sides : 6;
    if (face < 1) m_targetFace = 1;
    else if (face > m_sides) m_targetFace = m_sides;
    else m_targetFace = face;
    m_currentFace = 1;
    m_active = true;
    m_timer = 0.0f;
    m_duration = 1.6f; // total anim time if not physics
    m_scale = 1.0f;
    m_spin = 0.0f;
    // initialize physical drop from above
    m_physical = true;
    ImGuiIO& io = ImGui::GetIO();
    m_posX = io.DisplaySize.x * 0.5f;
    m_posY = -120.0f; // start above screen
    // give an initial random horizontal velocity and downward impulse
    std::uniform_real_distribution<float> vx(-220.0f, 220.0f);
    std::uniform_real_distribution<float> ang(-18.0f, 18.0f);
    m_velX = vx(m_rng);
    m_velY = 0.0f;
    m_angle = 0.0f;
    m_angVel = ang(m_rng);
    m_stopTimer = 0.0f;
}

void DiceVisual::Update(float dt) {
    // always tick result display timer even when not active
    if (m_resultDisplayTimer > 0.0f) {
        m_resultDisplayTimer = std::max(0.0f, m_resultDisplayTimer - dt);
        if (m_resultDisplayTimer <= 0.0f) {
            m_currentFace = 0;
        }
    }

    if (!m_active) return;
    m_timer += dt;

    if (m_physical) {
        // If multiple dice active, simulate each independently
        if (!m_dice.empty()) {
            ImGuiIO& io = ImGui::GetIO();
            float groundY = io.DisplaySize.y * 0.6f;
            float halfSizeBase = 96.0f * m_scale * 0.5f;
            for (auto &d : m_dice) {
                if (d.settled) continue;
                d.velY += m_gravity * dt;
                d.posX += d.velX * dt;
                d.posY += d.velY * dt;
                d.angle += d.angVel * dt;

                float halfSize = halfSizeBase * d.scale;
                if (d.posY + halfSize >= groundY) {
                    d.posY = groundY - halfSize;
                    if (d.velY > 0.0f) {
                        d.velY = -d.velY * m_restitution;
                        d.velX *= m_friction;
                        d.angVel *= m_friction;
                    }
                }
                d.velX *= std::pow(m_linearDamp, dt * 60.0f);
                d.angVel *= std::pow(0.92f, dt * 60.0f);

                float speed = std::sqrt(d.velX*d.velX + d.velY*d.velY);
                if (std::abs(d.angVel) < m_stopThresholdAng && speed < m_stopThresholdVel && d.posY + halfSize >= groundY - 0.5f) {
                    d.stopTimer += dt;
                    if (d.stopTimer > 0.28f) {
                        d.settled = true;
                    }
                } else d.stopTimer = 0.0f;
            }
            // if all settled -> finalize
            bool all = true; for (auto &d : m_dice) if (!d.settled) { all = false; break; }
            if (all) {
                m_physical = false;
                m_active = false;
                // show results for timer
                m_resultDisplayTimer = m_resultDisplayDuration;
                // set current face to last die so old single-die code still shows something
                m_currentFace = m_dice.back().face;
            }
            return;
        }

        // Single die physical integrator (legacy)
        // integrator (semi-implicit Euler)
        m_velY += m_gravity * dt;
        m_posX += m_velX * dt;
        m_posY += m_velY * dt;
        m_angle += m_angVel * dt;

        // ground plane at 40% down the screen
        ImGuiIO& io = ImGui::GetIO();
        float groundY = io.DisplaySize.y * 0.6f;
        float halfSize = 96.0f * m_scale * 0.5f;

        if (m_posY + halfSize >= groundY) {
            // simple bounce
            m_posY = groundY - halfSize;
            // reflect vertical velocity
            if (m_velY > 0.0f) {
                m_velY = -m_velY * m_restitution;
                // apply friction to horizontal and angular velocities
                m_velX *= m_friction;
                m_angVel *= m_friction;
            }
        }

        // floor damping
        m_velX *= std::pow(m_linearDamp, dt * 60.0f);
        m_angVel *= std::pow(0.92f, dt * 60.0f);

        // when velocities are small, accumulate stop timer
        float speed = std::sqrt(m_velX*m_velX + m_velY*m_velY);
        if (std::abs(m_angVel) < m_stopThresholdAng && speed < m_stopThresholdVel && m_posY + halfSize >= groundY - 0.5f) {
            m_stopTimer += dt;
            if (m_stopTimer > 0.28f) {
                // finalize: honor the predetermined target face (logical roll result)
                // The 2D rotation angle does not reliably map to cube faces, so use
                // the authoritative m_targetFace provided when the roll started.
                m_physical = false;
                m_active = false;
                m_currentFace = m_targetFace;
                // start result display timer so the face remains visible
                m_resultDisplayTimer = m_resultDisplayDuration;
            }
        } else {
            m_stopTimer = 0.0f;
        }

        // while rolling, occasionally flip displayed face for visual
        m_faceFlipTimer -= dt;
        if (m_faceFlipTimer <= 0.0f) {
            std::uniform_int_distribution<int> dist(1, m_sides);
            m_currentFace = dist(m_rng);
            m_faceFlipTimer = 0.05f + 0.18f * (1.0f - std::min(1.0f, m_timer / m_duration));
        }
        return;
    }

    // fallback non-physical animation (legacy)
    float t = m_timer / m_duration;
    if (t >= 1.0f) {
        m_active = false;
        m_currentFace = m_targetFace;
        m_spin = 0.0f;
        m_resultDisplayTimer = m_resultDisplayDuration;
        return;
    }
    // spin faster early, then slow
    m_spin += 40.0f * (1.0f - t) * dt;
    float steps = 6.0f + 24.0f * (1.0f - t);
    int face = 1 + static_cast<int>(std::floor(std::fmod(m_timer * steps, (float)m_sides)));
    m_currentFace = 1 + (face % m_sides);
    m_scale = 1.0f + 0.25f * (1.0f - t);
}

void DiceVisual::Render() {
    // allow rendering when active or when showing final result timer
    if (ImGui::GetCurrentContext() == nullptr) return;
    if (!m_active && m_resultDisplayTimer <= 0.0f && m_currentFace == 0) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.35f);
    float size = 96.0f * m_scale;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;

    // compute draw center depending on physical sim or static center
    ImVec2 drawCenter = center;
    if (m_physical || m_active || m_resultDisplayTimer > 0.0f) drawCenter = ImVec2(m_posX, m_posY);

    // if multiple dice, draw each die separately
    if (!m_dice.empty()) {
        ImGuiIO& io = ImGui::GetIO();
        float half = size * 0.5f;
        for (auto &d : m_dice) {
            if (d.settled == false || m_resultDisplayTimer > 0.0f || m_active) {
                ImVec2 c(d.posX, d.posY);
                float angle = d.angle;
                ImVec2 corners[4];
                for (int i = 0; i < 4; ++i) {
                    float a = angle + i * 3.1415926f * 0.5f;
                    corners[i] = ImVec2(c.x + std::cos(a) * half, c.y + std::sin(a) * half);
                }
                dl->AddConvexPolyFilled(corners, 4, IM_COL32(240,240,240,255));
                dl->AddPolyline(corners, 4, IM_COL32(28,28,28,220), true, 2.0f);
                // draw pip texture if available
                ImTextureID sheet = TextureManager::GetImGuiTextureID("texture/dice.jpg");
                if (sheet && d.face > 0) {
                    const float cols = 3.0f; const float rows = 2.0f;
                    int face = d.face;
                    if (face < 1) face = 1; if (face > m_sides) face = m_sides;
                    int idx = face - 1; int col = idx % 3; int row = idx / 3;
                    ImVec2 uv0(col / cols, row / rows);
                    ImVec2 uv1((col+1)/cols, (row+1)/rows);
                    ImVec2 a(c.x - half, c.y - half), b(c.x + half, c.y + half);
                    dl->AddImage(sheet, a, b, uv0, uv1, IM_COL32(255,255,255,255));
                }
                // draw numeric fallback/overlay so each die's value is always visible
                if (d.face > 0) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "%d", d.face);
                    ImVec2 ts = ImGui::CalcTextSize(buf);
                    dl->AddText(ImVec2(c.x - ts.x*0.5f, c.y - half - ts.y - 2.0f), IM_COL32(20,20,20,255), buf);
                }
            }
        }
        return;
    }

    // draw face using pip texture if available, otherwise fall back to number
    ImTextureID sheet = TextureManager::GetImGuiTextureID("texture/dice.jpg");
    if (sheet && m_currentFace > 0) {
        const float cols = 3.0f;
        const float rows = 2.0f;
        int face = m_currentFace;
        if (face < 1) face = 1;
        if (face > m_sides) face = m_sides;
        int idx = face - 1;
        int col = idx % 3;
        int row = idx / 3;
        ImVec2 uv0(col / cols, row / rows);
        ImVec2 uv1((col + 1) / cols, (row + 1) / rows);
        // draw texture centered at drawCenter with size 'size'
        ImVec2 a(drawCenter.x - size*0.5f, drawCenter.y - size*0.5f);
        ImVec2 b(drawCenter.x + size*0.5f, drawCenter.y + size*0.5f);
        dl->AddImage(sheet, a, b, uv0, uv1, IM_COL32(255,255,255,255));
    }
    else if (m_currentFace > 0) {
        // draw face number centered (fallback)
        char buf[16];
        int n = std::snprintf(buf, sizeof(buf), "%d", m_currentFace);
        if (n < 0) buf[0] = '\0';
        ImVec2 txtSize = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(drawCenter.x - txtSize.x*0.5f, drawCenter.y - txtSize.y*0.5f), IM_COL32(24,24,24,255), buf);
    }
}
