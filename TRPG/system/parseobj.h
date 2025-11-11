#pragma once
#include <unordered_map>
#include <string>
#include <filesystem>

namespace OBJParse {

    // 頂点データ
    struct VERTEX {
        float x, y, z;
    };

    // テクスチャ座標
    struct TEXCOORD {
        float u, v;
    };

    // 法線ベクトル
    struct NORMAL {
        float nx, ny, nz;
    };

    // 頂点情報を構成する情報のインデックスを規定する
    struct VERTEXINFOINDEX {
        std::string mtrlname;     // マテリアル名
        unsigned int vIndex;       // 頂点インデックス
        unsigned int tIndex;       // テクスチャインデックス
        unsigned int nIndex;	   // 法線インデックス  
    };

    // マテリアル情報
    struct MATERIAL {
        std::string mtrlname;
        float Ka[3]{};       // アンビエント
        float Kd[3]{};        // ディフューズ
        float Ks[3]{};        // スペキュラ
        float Ke[3]{};        // エミッション
        float Ns{};           // スペキュラの強さ
        std::string map_Kd{}; // ディフューズマップ
    };

    // OBJファイルのデータを取得
    void GetObjData(std::string filename,
        std::vector<VERTEX>& vertices,
        std::vector<TEXCOORD>& texcoords,
        std::vector<NORMAL>& normals,
        std::unordered_map<std::string, std::vector<std::vector<VERTEXINFOINDEX>>>& mtrlfaces,
        std::unordered_map<std::string, MATERIAL>& materials,
        std::vector<std::vector<OBJParse::VERTEXINFOINDEX>>& polygonindexes);

    // std::string 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::string& path);

    // std::u8string 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::u8string& path);

    // std::wstring 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::wstring& path);
    
};