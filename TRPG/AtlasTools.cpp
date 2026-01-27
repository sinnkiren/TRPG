#include "AtlasTools.h"
#include "system/stb_image.h"
#include <filesystem>
#include <vector>
#include <queue>
#include <array>
#include <algorithm>
#include <cstdint>  
#include <utility>

namespace AtlasTools {

static std::filesystem::path ResolvePath(const std::string& rel) {
    std::filesystem::path p1 = std::filesystem::path("assets/") / rel;
    if (std::filesystem::exists(p1)) return p1;
    std::filesystem::path p2 = std::filesystem::current_path() / rel;
    if (std::filesystem::exists(p2)) return p2;
    return p1;
}

static std::vector<std::array<int,4>> ExtractRegions(const unsigned char* data, int w, int h, int alphaThreshold) {
    std::vector<uint8_t> visited((size_t)w*h, 0);
    std::vector<std::array<int,4>> regions;
    auto idx = [w](int x,int y){return y*w + x;};
    for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
        int i = idx(x,y);
        if (visited[i]) continue;
        unsigned char a = data[(i<<2)+3];
        if (a <= alphaThreshold) { visited[i]=1; continue; }
        // flood
        int minx=x,miny=y,maxx=x,maxy=y;
        std::queue<std::pair<int,int>> q;
        q.push({x,y}); visited[i]=1;
        while(!q.empty()){ auto p = q.front(); q.pop(); int px=p.first, py=p.second;
            minx = std::min(minx, px); miny = std::min(miny, py); maxx = std::max(maxx, px); maxy = std::max(maxy, py);
            const int DX[4]={1,-1,0,0}; const int DY[4]={0,0,1,-1};
            for (int k=0;k<4;++k){ int nx=px+DX[k], ny=py+DY[k]; if (nx<0||nx>=w||ny<0||ny>=h) continue; int ni=idx(nx,ny); if (visited[ni]) continue; unsigned char aa=data[(ni<<2)+3]; if (aa>alphaThreshold){ visited[ni]=1; q.push({nx,ny}); } else visited[ni]=1; }
        }
        regions.push_back(std::array<int,4>{minx,miny,maxx-minx+1,maxy-miny+1});
    }
    return regions;
}

AtlasMap AnalyzeAtlas(const std::string& relativePath, int alphaThreshold) {
    AtlasMap map;
    auto p = ResolvePath(relativePath);
    int w=0,h=0,n=0;
    unsigned char* data = stbi_load(p.string().c_str(), &w, &h, &n, 4);
    if (!data) return map;
    auto regs = ExtractRegions(data,w,h,alphaThreshold);
    // crude heuristics: sort by area desc
    std::sort(regs.begin(), regs.end(), [](auto &a, auto &b){ return (a[2]*a[3]) > (b[2]*b[3]); });
    if (regs.size() >= 6) {
        // assume largest is dialog box at bottom -> status box is second largest; bars are smaller long rectangles
        auto r0 = regs[0]; // largest
        auto r1 = regs[1];
        map.dialogBox.uv0 = ImVec2(double(r0[0]) / w, double(r0[1]) / h);
        map.dialogBox.uv1 = ImVec2(double(r0[0] + r0[2]) / w, double(r0[1] + r0[3]) / h);
        map.statusBox.uv0 = ImVec2(double(r1[0]) / w, double(r1[1]) / h);
        map.statusBox.uv1 = ImVec2(double(r1[0] + r1[2]) / w, double(r1[1] + r1[3]) / h);
        // find long thin regions for bars
        int found = 0;
        for (size_t i=2;i<regs.size() && found<3;++i) {
            auto r = regs[i];
            if (r[2] > r[3] * 6) {
                ImVec2 uv0(double(r[0])/w,double(r[1])/h);
                ImVec2 uv1(double(r[0]+r[2])/w,double(r[1]+r[3])/h);
                if (found==0) map.redBar = {uv0,uv1};
                else if (found==1) map.blueBar = {uv0,uv1};
                else map.greenBar = {uv0,uv1};
                ++found;
            }
        }
        // pick a medium box for button area
        for (size_t i=2;i<regs.size(); ++i) {
            auto r = regs[i];
            if (r[2] > 80 && r[3] > 40 && !(r[2] > r[3]*6)) { map.buttonBox.uv0 = ImVec2(double(r[0])/w,double(r[1])/h); map.buttonBox.uv1 = ImVec2(double(r[0]+r[2])/w,double(r[1]+r[3])/h); break; }
        }
        map.valid = true;
    }
    stbi_image_free(data);
    return map;
}

}
