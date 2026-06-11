#include "DiceVisual.h"
#include "system/imgui/imgui.h"
#include "TextureManager.h"
#include "ImGuiHelpers.h"
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
        d.targetFace = faces[i];
        // start with a random interim face for visual variety
        std::uniform_int_distribution<int> faceDist(1, std::max(1, sides));
        d.face = faceDist(m_rng);
        d.faceFlipTimer = 0.05f + 0.18f * (1.0f - std::min(1.0f, m_timer / m_duration));
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
        // keep computed scale (do not overwrite)
        d.stopTimer = 0.0f;
        d.settled = false;
        m_dice.push_back(d);
    }
    m_active = true;
    m_physical = true;
    // mark grouped visual active (prevent unrelated single rolls from appending)
    m_groupedVisual = true;
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
    // If multi-dice visuals are active, append this single roll as another active die
    if (!m_dice.empty()) {
        // if grouped visual active, cap appended dice to avoid accidental growth
        if (m_groupedVisual && (int)m_dice.size() >= m_groupMaxDice) return;
        ActiveDie d;
        d.targetFace = face;
        std::uniform_int_distribution<int> faceDist(1, std::max(1, sides));
        d.face = faceDist(m_rng);
        d.faceFlipTimer = 0.05f + 0.18f * (1.0f - std::min(1.0f, m_timer / m_duration));
        // position this new die to the right of existing set
        float avgX = 0.0f; for (auto &ad : m_dice) avgX += ad.posX; avgX /= (float)m_dice.size();
        float offset =  (float)m_dice.size() * 70.0f + 40.0f;
        d.posX = avgX + offset;
        d.posY = -120.0f;
        d.velX = vx(m_rng) * 0.6f;
        d.velY = 240.0f + std::abs(vx(m_rng)) * 0.4f;
        d.scale = std::max(0.6f, 1.0f - (float)m_dice.size() * 0.035f);
        d.angle = 0.0f;
        d.angVel = ang(m_rng);
        d.stopTimer = 0.0f;
        d.settled = false;
        m_dice.push_back(d);
        // ensure physical simulation is active
        m_active = true;
        m_physical = true;
        return;
    }

    // No multi-dice active: perform single-die start (legacy behavior)
    m_dice.clear();
    m_stopParticles.clear();
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

    // update stop particles
    for (auto it = m_stopParticles.begin(); it != m_stopParticles.end(); ) {
        it->life += dt;
        if (it->life >= it->ttl) { it = m_stopParticles.erase(it); continue; }
        // integrate particle
        it->vy += m_gravity * 0.5f * dt; // lighter gravity for particles
        it->x += it->vx * dt;
        it->y += it->vy * dt;
        // simple damping
        it->vx *= 0.96f;
        it->vy *= 0.96f;
        ++it;
    }

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

                // interim face flipping for multi-dice visuals
                d.faceFlipTimer -= dt;
                if (d.faceFlipTimer <= 0.0f) {
                    std::uniform_int_distribution<int> distFace(1, std::max(1, m_sides));
                    d.face = distFace(m_rng);
                    d.faceFlipTimer = 0.05f + 0.18f * (1.0f - std::min(1.0f, m_timer / m_duration));
                }

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
                        // snap displayed face to target
                        if (d.targetFace > 0) d.face = d.targetFace;
                        // spawn stop particles for visual flair
                        int spawn = std::min(m_maxStopParticles, 6 + (int)(std::abs(d.angVel) * 2.0f));
                        for (int pi = 0; pi < spawn; ++pi) {
                            StopParticle p;
                            p.x = d.posX + ((float)pi - spawn*0.5f) * 2.4f;
                            p.y = d.posY - halfSize * 0.2f;
                            std::uniform_real_distribution<float> ang(-3.14f, 3.14f);
                            float a = ang(m_rng);
                            p.vx = std::cos(a) * (m_particleSpawnSpeed * (0.4f + 0.6f * ((float)pi / spawn)));
                            p.vy = -std::abs(std::sin(a)) * (m_particleSpawnSpeed * 0.4f);
                            p.life = 0.0f;
                            p.ttl = m_particleTTL * (0.8f + 0.6f * ((float)pi / spawn));
                            p.size = 6.0f * d.scale * (0.6f + 0.8f * ((float)pi / spawn));
                            m_stopParticles.push_back(p);
                        }
                    }
                } else d.stopTimer = 0.0f;
            }
            // simple pairwise collisions (circle approximation)
            for (size_t i = 0; i < m_dice.size(); ++i) {
                for (size_t j = i + 1; j < m_dice.size(); ++j) {
                    auto &a = m_dice[i];
                    auto &b = m_dice[j];
                    if (a.settled && b.settled) continue; // no need to collide fully settled dice
                    float halfA = halfSizeBase * a.scale;
                    float halfB = halfSizeBase * b.scale;
                    float rx = b.posX - a.posX;
                    float ry = b.posY - a.posY;
                    float dist2 = rx*rx + ry*ry;
                    float minDist = halfA + halfB;
                    if (dist2 <= 1e-6f) {
                        // jitter to avoid exact overlap
                        std::uniform_real_distribution<float> jitter(-0.5f, 0.5f);
                        float jx = jitter(m_rng), jy = jitter(m_rng);
                        a.posX -= jx; a.posY -= jy;
                        b.posX += jx; b.posY += jy;
                        continue;
                    }
                    if (dist2 < minDist*minDist) {
                        float dist = std::sqrt(dist2);
                        float overlap = minDist - dist;
                        float nx = rx / dist;
                        float ny = ry / dist;
                        // positional correction (split)
                        float corr = 0.5f * overlap;
                        a.posX -= nx * corr;
                        a.posY -= ny * corr;
                        b.posX += nx * corr;
                        b.posY += ny * corr;
                        // relative velocity along normal
                        float rvx = b.velX - a.velX;
                        float rvy = b.velY - a.velY;
                        float reln = rvx * nx + rvy * ny;
                        if (reln < 0.0f) {
                            float e = m_collisionRestitution;
                            float j = -(1.0f + e) * reln * 0.5f; // equal mass
                            float ix = j * nx;
                            float iy = j * ny;
                            a.velX -= ix;
                            a.velY -= iy;
                            b.velX += ix;
                            b.velY += iy;
                            // simple tangential friction: reduce tangential relative velocity
                            float tx = -ny; float ty = nx;
                            float rvt = rvx * tx + rvy * ty;
                            float ft = rvt * (1.0f - m_collisionFriction);
                            a.velX += tx * ft * 0.5f;
                            a.velY += ty * ft * 0.5f;
                            b.velX -= tx * ft * 0.5f;
                            b.velY -= ty * ft * 0.5f;
                            // small angular change based on impulse
                            a.angVel -= (ix * 0.02f);
                            b.angVel += (ix * 0.02f);
                        }
                    }
                }
            }
            // if all settled -> finalize
            bool all = true; for (auto &d : m_dice) if (!d.settled) { all = false; break; }
            if (all) {
                // snap settled dice to an evenly spaced line on the tray for a tidy final layout
                size_t totalCount = m_dice.size();
                float cx = 0.0f;
                for (auto &d : m_dice) cx += d.posX;
                cx /= (float)totalCount;
                // recompute spacing same as StartRollFaces logic
                float baseSpacing = 80.0f;
                float spacing = baseSpacing + std::min(10.0f, static_cast<float>(totalCount) * 6.0f);
                // compute half size per die and target Y on ground
                float targetY = groundY - halfSizeBase;
                for (size_t i = 0; i < m_dice.size(); ++i) {
                    auto &d = m_dice[i];
                    float spread = ((float)i - (float)totalCount * 0.5f) * spacing;
                    d.posX = cx + spread;
                    d.posY = targetY;
                    d.velX = d.velY = 0.0f;
                    d.angVel = 0.0f;
                    // small canonical angle so pips align nicely
                    d.angle = 0.0f;
                    d.settled = true;
                }
                // end physics and show results for timer
                m_physical = false;
                m_active = false;
                m_resultDisplayTimer = m_resultDisplayDuration;
                // set current face to last die so old single-die code still shows something
                m_currentFace = m_dice.back().face;
                // clear grouped visual flag
                m_groupedVisual = false;
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
    float half = size * 0.5f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;

    // compute draw center depending on physical sim or static center
    ImVec2 drawCenter = center;
    if (m_physical || m_active || m_resultDisplayTimer > 0.0f) drawCenter = ImVec2(m_posX, m_posY);

    // if multiple dice, draw each die separately
    if (!m_dice.empty()) {
        // draw a simple dice tray under the dice (rounded rect)
        // compute bounding width from dice count and spacing similar to StartRollFaces
        size_t totalCount = m_dice.size();
        float baseSpacing = 80.0f;
        float spacing = baseSpacing + std::min(10.0f, (float)totalCount * 6.0f);
        float trayWidth = std::max(160.0f, spacing * (float)totalCount + 40.0f);
        // center at screen center X or average of dice
        float cx = 0.0f;
        for (auto &d : m_dice) cx += d.posX;
        cx /= (float)totalCount;
        ImGuiIO& io = ImGui::GetIO();
        float vpY = io.DisplaySize.y * 0.6f;
        float trayHeight = 110.0f * m_scale;
        ImVec2 tA(cx - trayWidth * 0.5f, vpY - trayHeight * 0.5f + 8.0f);
        ImVec2 tB(cx + trayWidth * 0.5f, vpY + trayHeight * 0.5f + 8.0f);
        // background shadow
        dl->AddRectFilled(tA, tB, IM_COL32(20,20,20,120), 12.0f);
        // inner tray (lighter)
        dl->AddRectFilled(ImVec2(tA.x+4.0f, tA.y+4.0f), ImVec2(tB.x-4.0f, tB.y-4.0f), IM_COL32(240,240,235,230), 10.0f);
        // outline
        dl->AddRect(ImVec2(tA.x+4.0f, tA.y+4.0f), ImVec2(tB.x-4.0f, tB.y-4.0f), IM_COL32(40,40,40,160), 10.0f, 0, 2.0f);
        // draw soft shadows for each die (under the tray, before drawing dice)
        float groundY = io.DisplaySize.y * 0.6f;
        for (auto &d : m_dice) {
            // shadow position is aligned to tray ground
            float shadowY = groundY - half * 0.15f;
            // directional offset based on rolling angle to add depth
            float dirOff = std::sin(d.angle) * half * 0.08f;
            ImVec2 sc(d.posX + dirOff, shadowY + std::abs(dirOff) * 0.12f);
            float sr = half * d.scale * 0.95f; // base radius
            // layered soft shadow (improved)
            dl->AddCircleFilled(sc, sr * 1.9f, IM_COL32(0,0,0,28), 24);
            dl->AddCircleFilled(sc, sr * 1.35f, IM_COL32(0,0,0,60), 22);
            dl->AddCircleFilled(sc, sr * 0.95f, IM_COL32(0,0,0,110), 20);
        }

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
                    ImVec2 uv_tl(col / cols, row / rows);
                    ImVec2 uv_tr((col+1)/cols, row / rows);
                    ImVec2 uv_br((col+1)/cols, (row+1)/rows);
                    ImVec2 uv_bl(col / cols, (row+1)/rows);
                    // draw rotated image aligned with die angle
                    ImGui_AddImageQuadRotated(dl, sheet, ImVec2(c.x, c.y), size * d.scale, d.angle, uv_tl, uv_tr, uv_br, uv_bl, IM_COL32(255,255,255,255));
                }
                // draw numeric fallback/overlay only when die is settled or result display active
                if (d.face > 0 && (d.settled || m_resultDisplayTimer > 0.0f)) {
                    int displayVal = d.face;
                    // for d10 tens/ones usage we encode 10 as 0; map accordingly for display
                    if (m_sides == 10 && displayVal == 10) displayVal = 0;
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "%d", displayVal);
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
        // draw texture centered at drawCenter with size 'size', rotated by m_angle
        ImGui_AddImageQuadRotated(dl, sheet, drawCenter, size, m_angle, uv0, ImVec2(uv1.x, uv0.y), uv1, ImVec2(uv0.x, uv1.y), IM_COL32(255,255,255,255));
    }
    else if (m_currentFace > 0) {
        // draw face number centered (fallback)
        char buf[16];
        int n = std::snprintf(buf, sizeof(buf), "%d", m_currentFace);
        if (n < 0) buf[0] = '\0';
        // hide number while single die is physically rolling to avoid mismatch
        if (!(m_physical && m_active)) {
            int displayVal = m_currentFace;
            if (m_sides == 10 && displayVal == 10) displayVal = 0;
            char dbuf[16]; std::snprintf(dbuf, sizeof(dbuf), "%d", displayVal);
            ImVec2 txtSize = ImGui::CalcTextSize(dbuf);
            dl->AddText(ImVec2(drawCenter.x - txtSize.x*0.5f, drawCenter.y - txtSize.y*0.5f), IM_COL32(24,24,24,255), dbuf);
        }
    }

    // render stop particles
    for (auto &p : m_stopParticles) {
        float t = p.life / p.ttl;
        float alpha = (1.0f - t) * 0.85f;
        ImU32 col = IM_COL32(220, 200, 140, (int)(alpha * 255));
        // draw faded circle
        dl->AddCircleFilled(ImVec2(p.x, p.y), p.size * (1.0f - 0.5f * t), col, 8);
    }
}
