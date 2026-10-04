#include "StoryPlayer.h"
#include "system/imgui/imgui.h"
#include <algorithm>

void StoryPlayer::RenderNodeGraphCanvas()
{
    // Node graph canvas: 右側にノードのグラフを描画し、ドラッグで位置を編集可能にする
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Node Graph View");
    ImVec2 canvasSize = ImVec2(800, 600);
    ImGui::BeginChild("NodeGraphCanvas", canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGuiIO &io = ImGui::GetIO();
    // handle zoom with mouse wheel when hovering canvas
    if (ImGui::IsWindowHovered()) {
        if (io.MouseWheel != 0.0f) {
            float oldZoom = m_nodeGraphZoom;
            float zoomFactor = 1.0f + io.MouseWheel * 0.1f;
            m_nodeGraphZoom = std::clamp(m_nodeGraphZoom * zoomFactor, 0.25f, 2.5f);
            // optional: adjust pan to zoom towards mouse position
            ImVec2 mouse = io.MousePos;
            ImVec2 before = ImVec2((mouse.x - canvasPos.x - m_nodeGraphPan.x) / oldZoom,
                                   (mouse.y - canvasPos.y - m_nodeGraphPan.y) / oldZoom);
            ImVec2 after = ImVec2(before.x * m_nodeGraphZoom, before.y * m_nodeGraphZoom);
            m_nodeGraphPan.x += (after.x - (mouse.x - canvasPos.x - m_nodeGraphPan.x));
            m_nodeGraphPan.y += (after.y - (mouse.y - canvasPos.y - m_nodeGraphPan.y));
        }
        // middle mouse drag for panning
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            m_nodeGraphPan.x += io.MouseDelta.x;
            m_nodeGraphPan.y += io.MouseDelta.y;
        }
    }
    // background
    draw->AddRectFilled(canvasPos, ImVec2(canvasPos.x + avail.x, canvasPos.y + avail.y), ImGui::GetColorU32(ImVec4(0.07f,0.07f,0.07f,1.0f)));

    // ensure positions exist for nodes (normalized 0..1)
    int idx = 0;
    for (auto &p : m_nodeMap) {
        int id = p.first;
        if (m_nodePositions.find(id) == m_nodePositions.end()) {
            // place in a grid layout normalized
            float cols = 6.0f;
            float gx = (idx % (int)cols) / cols + 0.02f;
            float gy = (idx / (int)cols) / 6.0f + 0.02f;
            m_nodePositions[id] = ImVec2(gx, gy);
        }
        ++idx;
    }
    // Transform helper: converts normalized pos -> screen pos taking into account zoom and pan
    auto ToScreen = [&](const ImVec2 &posNorm)->ImVec2 {
        ImVec2 base = ImVec2(canvasPos.x + posNorm.x * avail.x * m_nodeGraphZoom + m_nodeGraphPan.x,
                             canvasPos.y + posNorm.y * avail.y * m_nodeGraphZoom + m_nodeGraphPan.y);
        return base;
    };
    // draw connections first and support edge selection (hit test)
    const float edgeHitThreshold = 10.0f;
    for (auto &p : m_nodeMap) {
        int id = p.first;
        const EventNode &node = p.second;
        ImVec2 posNorm = m_nodePositions[id];
        ImVec2 nodeCenter = ToScreen(posNorm);
        ImVec2 src = nodeCenter;
        for (int ci = 0; ci < (int)node.choices.size(); ++ci) {
            const Choice &c = node.choices[ci];
            int tid = c.nextNodeID;
            if (tid < 0) continue;
            if (m_nodeMap.find(tid) == m_nodeMap.end()) continue;
            ImVec2 tNorm = m_nodePositions[tid];
            ImVec2 tgt = ToScreen(tNorm);
            // draw bezier line
            ImU32 col = ImGui::GetColorU32(ImVec4(0.6f,0.6f,0.2f,1.0f));
            // highlight if selected
            if (m_selectedEdgeSourceNode == id && m_selectedEdgeChoiceIndex == ci && m_selectedEdgeTargetNode == tid) {
                col = ImGui::GetColorU32(ImVec4(1.0f,0.5f,0.2f,1.0f));
            }
            draw->AddBezierCubic(src, ImVec2(src.x + 80, src.y), ImVec2(tgt.x - 80, tgt.y), tgt, col, 3.0f);

            // hit test: sample points along bezier and check distance to mouse when clicked
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_draggingConnection) {
                ImVec2 mp = io.MousePos;
                auto BezierPoint = [&](float t)->ImVec2 {
                    float u = 1.0f - t;
                    ImVec2 p0 = src;
                    ImVec2 p1 = ImVec2(src.x + 80, src.y);
                    ImVec2 p2 = ImVec2(tgt.x - 80, tgt.y);
                    ImVec2 p3 = tgt;
                    float b0 = u*u*u;
                    float b1 = 3*u*u*t;
                    float b2 = 3*u*t*t;
                    float b3 = t*t*t;
                    return ImVec2(p0.x*b0 + p1.x*b1 + p2.x*b2 + p3.x*b3,
                                  p0.y*b0 + p1.y*b1 + p2.y*b2 + p3.y*b3);
                };
                int samples = 12;
                float bestDist2 = FLT_MAX;
                for (int si = 0; si <= samples; ++si) {
                    float t = si / (float)samples;
                    ImVec2 bp = BezierPoint(t);
                    float dx = bp.x - mp.x;
                    float dy = bp.y - mp.y;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < bestDist2) bestDist2 = d2;
                }
                if (bestDist2 <= edgeHitThreshold * edgeHitThreshold) {
                    // select this edge
                    m_selectedEdgeSourceNode = id;
                    m_selectedEdgeChoiceIndex = ci;
                    m_selectedEdgeTargetNode = tid;
                }
            }
        }
    }

    // draw nodes and handle dragging
    const ImVec2 nodeSize(180, 80);
    for (auto &p : m_nodeMap) {
        int id = p.first;
        EventNode &node = p.second;
        ImVec2 posNorm = m_nodePositions[id];
        ImVec2 center = ToScreen(posNorm);
        ImVec2 topLeft = ImVec2(center.x - nodeSize.x*0.5f, center.y - nodeSize.y*0.5f);
        ImVec2 botRight = ImVec2(topLeft.x + nodeSize.x, topLeft.y + nodeSize.y);
        // draw box
        if (m_nodeEditorSelectedId == id) {
            draw->AddRectFilled(topLeft, botRight, ImGui::GetColorU32(ImVec4(0.16f,0.22f,0.32f,1.0f)), 6.0f);
            draw->AddRect(topLeft, botRight, ImGui::GetColorU32(ImVec4(0.2f,0.6f,1.0f,1.0f)), 6.0f);
        } else {
            draw->AddRectFilled(topLeft, botRight, ImGui::GetColorU32(ImVec4(0.12f,0.12f,0.12f,1.0f)), 6.0f);
            draw->AddRect(topLeft, botRight, ImGui::GetColorU32(ImVec4(1,1,1,0.06f)), 6.0f);
        }
        // label
        std::string label = std::to_string(id) + ": " + (node.text.size() > 40 ? node.text.substr(0,40)+"..." : node.text);
        draw->AddText(NULL, ImGui::GetFontSize(), ImVec2(topLeft.x + 6, topLeft.y + 6), ImGui::GetColorU32(ImVec4(1,1,1,1)), label.c_str());
        // draw per-choice connection handles on the right side
        float handleRadius = 6.0f;
        float spacing = 18.0f;
        ImVec2 mp = io.MousePos;
        int choiceCount = (int)node.choices.size();
        // clamp spacing if too many choices
        if (choiceCount > 0) {
            float totalH = spacing * (choiceCount - 1);
            if (totalH > nodeSize.y - 24.0f) spacing = (nodeSize.y - 24.0f) / std::max(1, choiceCount - 1);
        }
        for (int ci = 0; ci < std::max(1, choiceCount); ++ci) {
            // compute Y position: start a bit below top
            float y = topLeft.y + 16.0f + ci * spacing;
            ImVec2 handleCenter = ImVec2(topLeft.x + nodeSize.x + 12.0f, y);
            bool handleHovered = false;
            float dxh = mp.x - handleCenter.x;
            float dyh = mp.y - handleCenter.y;
            if (dxh*dxh + dyh*dyh <= handleRadius * handleRadius) {
                handleHovered = true;
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("Drag to connect (choice %d)", ci);
            }
            ImU32 handleCol = handleHovered ? ImGui::GetColorU32(ImVec4(0.95f,0.75f,0.2f,1.0f)) : ImGui::GetColorU32(ImVec4(0.75f,0.65f,0.14f,1.0f));
            // if there is a choice for this handle, render index; otherwise render a plus (new)
            if (ci < choiceCount) {
                draw->AddCircleFilled(handleCenter, handleRadius, handleCol);
                // small index label
                draw->AddText(NULL, ImGui::GetFontSize()*0.7f, ImVec2(handleCenter.x - 4, handleCenter.y - 5), ImGui::GetColorU32(ImVec4(0,0,0,1)), std::to_string(ci).c_str());
                // draw truncated choice text to the right of the handle
                std::string ctext = node.choices[ci].text;
                if (ctext.empty()) ctext = "(no text)";
                const int maxChars = 24;
                if ((int)ctext.size() > maxChars) ctext = ctext.substr(0, maxChars-3) + "...";
                ImVec2 txtPos = ImVec2(handleCenter.x + handleRadius + 6.0f, handleCenter.y - ImGui::GetFontSize()*0.45f);
                draw->AddText(NULL, ImGui::GetFontSize()*0.85f, txtPos, ImGui::GetColorU32(ImVec4(1,1,1,0.95f)), ctext.c_str());
                if (handleHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_dragSourceChoiceIndex = ci;
                    m_dragSourceNode = id;
                    m_draggingConnection = true;
                }
            } else {
                // add-new handle
                ImU32 plusCol = handleHovered ? ImGui::GetColorU32(ImVec4(0.2f,0.9f,0.2f,1.0f)) : ImGui::GetColorU32(ImVec4(0.2f,0.8f,0.2f,1.0f));
                draw->AddCircleFilled(handleCenter, handleRadius, plusCol);
                draw->AddText(NULL, ImGui::GetFontSize()*0.7f, ImVec2(handleCenter.x - 3, handleCenter.y - 6), ImGui::GetColorU32(ImVec4(0,0,0,1)), "+");
                if (handleHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    Choice nc; nc.text = "(linked)"; nc.nextNodeID = -1; node.choices.push_back(nc);
                    m_dragSourceChoiceIndex = (int)node.choices.size() - 1;
                    m_dragSourceNode = id;
                    m_draggingConnection = true;
                }
            }
        }

        // interaction: invisible button for dragging the node itself (placed over node rect)
        ImGui::SetCursorScreenPos(topLeft);
        ImGui::InvisibleButton((std::string("nodebtn") + std::to_string(id)).c_str(), nodeSize);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_draggingConnection) {
            m_nodeEditorSelectedId = id;
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !m_draggingConnection) {
            // drag node: update normalized pos using mouse delta (account for zoom)
            ImVec2 delta = io.MouseDelta;
            float dx = delta.x / (avail.x * m_nodeGraphZoom);
            float dy = delta.y / (avail.y * m_nodeGraphZoom);
            m_nodePositions[id].x += dx;
            m_nodePositions[id].y += dy;
            // clamp
            m_nodePositions[id].x = std::clamp(m_nodePositions[id].x, 0.0f, 1.0f);
            m_nodePositions[id].y = std::clamp(m_nodePositions[id].y, 0.0f, 1.0f);
        }
    }

    // if dragging a connection, draw temporary line and handle drop
    if (m_draggingConnection && m_dragSourceNode >= 0) {
        ImVec2 src = ToScreen(m_nodePositions[m_dragSourceNode]);
        ImVec2 mouse = io.MousePos;
        // draw temp bezier
        draw->AddBezierCubic(src, ImVec2(src.x + 80, src.y), ImVec2(mouse.x - 80, mouse.y), mouse, ImGui::GetColorU32(ImVec4(0.9f,0.6f,0.2f,1.0f)), 3.0f);
        // on mouse release, check target
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            int target = -1;
            for (auto &p : m_nodeMap) {
                int id = p.first;
                ImVec2 center = ToScreen(m_nodePositions[id]);
                ImVec2 tl = ImVec2(center.x - nodeSize.x*0.5f, center.y - nodeSize.y*0.5f);
                ImVec2 br = ImVec2(tl.x + nodeSize.x, tl.y + nodeSize.y);
                ImVec2 mpos = io.MousePos;
                if (mpos.x >= tl.x && mpos.x <= br.x && mpos.y >= tl.y && mpos.y <= br.y) { target = id; break; }
            }
            if (target >= 0 && target != m_dragSourceNode) {
                EventNode &srcNode = m_nodeMap[m_dragSourceNode];
                if (m_dragSourceChoiceIndex >= 0 && m_dragSourceChoiceIndex < (int)srcNode.choices.size()) {
                    // assign existing choice to target
                    srcNode.choices[m_dragSourceChoiceIndex].nextNodeID = target;
                } else {
                    // create a new choice on source node linking to target
                    Choice nc; nc.text = "(linked)"; nc.nextNodeID = target; srcNode.choices.push_back(nc);
                }
            }
            m_draggingConnection = false;
            m_dragSourceNode = -1;
            m_dragSourceChoiceIndex = -1;
        }
    }

    // If a choice-selection popup was requested, render it near mouse and handle selection
    if (m_pendingChoicePopupNode >= 0) {
        std::string popupName = std::string("ChooseChoiceForLink##") + std::to_string(m_pendingChoicePopupNode);
        ImGui::SetNextWindowPos(io.MousePos);
        if (ImGui::BeginPopup(popupName.c_str())) {
            auto itn = m_nodeMap.find(m_pendingChoicePopupNode);
            if (itn != m_nodeMap.end()) {
                EventNode &n = itn->second;
                for (int ci = 0; ci < (int)n.choices.size(); ++ci) {
                    std::string lab = std::to_string(ci) + ": " + (n.choices[ci].text.size() > 40 ? n.choices[ci].text.substr(0,40)+"..." : n.choices[ci].text);
                    if (ImGui::Selectable(lab.c_str())) {
                        m_dragSourceNode = m_pendingChoicePopupNode;
                        m_dragSourceChoiceIndex = ci;
                        m_draggingConnection = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
        }
        // clear pending regardless (popup handled or not)
        m_pendingChoicePopupNode = -1;
    }

    // Edge actions UI: allow deleting selected edge
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (m_selectedEdgeSourceNode >= 0) {
        ImGui::Text("Selected Edge: %d -> %d (choice %d)", m_selectedEdgeSourceNode, m_selectedEdgeTargetNode, m_selectedEdgeChoiceIndex);
        if (ImGui::Button("Delete Selected Edge")) {
            auto it = m_nodeMap.find(m_selectedEdgeSourceNode);
            if (it != m_nodeMap.end()) {
                EventNode &sn = it->second;
                if (m_selectedEdgeChoiceIndex >=0 && m_selectedEdgeChoiceIndex < (int)sn.choices.size()) {
                    // clear the connection
                    sn.choices[m_selectedEdgeChoiceIndex].nextNodeID = -1;
                } else {
                    // fallback: remove any choice that points to target
                    for (auto itc = sn.choices.begin(); itc != sn.choices.end(); ) {
                        if (itc->nextNodeID == m_selectedEdgeTargetNode) itc = sn.choices.erase(itc);
                        else ++itc;
                    }
                }
            }
            m_selectedEdgeSourceNode = m_selectedEdgeChoiceIndex = m_selectedEdgeTargetNode = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Selection")) {
            m_selectedEdgeSourceNode = m_selectedEdgeChoiceIndex = m_selectedEdgeTargetNode = -1;
        }
    }
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::EndGroup();
}
