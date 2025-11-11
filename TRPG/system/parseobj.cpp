#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <unordered_map>
#include "parseobj.h"

namespace OBJParse {

    // 文字列を指定の区切り文字で分割する
    std::vector<std::string> split(const std::string& inputstring, char delimiter) {

        std::vector<std::string> tokens;
        std::string token;

        std::stringstream stringstream(inputstring);

        while (getline(stringstream, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }

    // ファイルを読み込んでメモリに展開する
    std::vector<char> readFileToMemory(const std::string& filename) {

        // 入力ファイルストリームを開く（バイナリモードで開く）
        std::ifstream file(filename, std::ios::binary | std::ios::ate);

        if (file.is_open() == false) {
            std::cerr << "ファイルが開けません: " << filename << std::endl;
            return {};
        }

        // ファイルサイズを取得する
        std::streamsize size = file.tellg();

        std::vector<char> buffer(size);   // ファイルの内容を格納するためのバッファ

        // ファイルの内容をベクターに読み込む
        file.seekg(0, std::ios::beg);     // ファイルポインタを先頭に戻す
        if (file.read(buffer.data(), size)) {
            std::cout << "ファイルが正常に読み込まれました。サイズ: " << size << " バイト" << std::endl;
        }
        else {
            std::cerr << "ファイルの読み込みに失敗しました。" << std::endl;
            buffer.clear();
        }

        return buffer;
    }


    bool isSpace(const char c) {

        if (c == ' ' || c == '\t') {
            return true;
        }

        return false;
    }

    std::vector<VERTEX> GetVertex(const std::vector<std::string>& lines)
    {
        std::vector<VERTEX> vertexes{};

        // 頂点データを取得する
        for (const auto& line : lines) {
            if (line[0] == 'v' && isSpace(line[1])) {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 4) {
                    VERTEX vertex;
                    vertex.x = std::stof(tokens[1]);
                    vertex.y = std::stof(tokens[2]);
                    vertex.z = std::stof(tokens[3]);
                    vertexes.push_back(vertex);
                }
            }
        }

        return vertexes;
    }


    std::vector<TEXCOORD> GetTexCoord(const std::vector<std::string>& lines)
    {
        std::vector<TEXCOORD> texcoords{};

        // テクスチャ座標を取得する
        for (const auto& line : lines) {
            if (line[0] == 'v' && line[1] == 't') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 3) {
                    TEXCOORD tex;
                    tex.u = std::stof(tokens[1]);
                    tex.v = std::stof(tokens[2]);
                    texcoords.push_back(tex);
                }
            }
        }

        return texcoords;
    }

    std::vector<NORMAL> GetNormal(const std::vector<std::string>& lines)
    {
        std::vector<NORMAL> normals{};

        // 法線ベクトルを取得する
        for (const auto& line : lines) {
            if (line[0] == 'v' && line[1] == 'n') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 4) {
                    NORMAL n;
                    n.nx = std::stof(tokens[1]);
                    n.ny = std::stof(tokens[2]);
                    n.nz = std::stof(tokens[3]);
                    normals.push_back(n);
                }
            }
        }

        return normals;
    }

    // USEMATERIAL取得
    std::vector<std::string> GetUseMaterial(const std::vector<std::string>& lines)
    {
        std::vector<std::string> usematerials{};

        // 
        for (const auto& line : lines) {
            if (line[0] == 'u' && line[1] == 's' && line[2] == 'e' && line[3] == 'm' && line[4] == 't' && line[5] == 'l') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 2) {
                    std::string mtrlname;
                    mtrlname = tokens[1];
                    usematerials.push_back(mtrlname);
                }
            }
        }

        return usematerials;
    }

    // 引数の中にはスペースで区切られた文字列群が入っている(f 1/2/3 4/5/6 7/8/9 )
    std::vector<VERTEXINFOINDEX> GetPolygonIndex(std::vector<std::string>& spaceSeparatedTokens)
    {
        std::vector<VERTEXINFOINDEX> polygonindexes{};

        for (int i = 1; i < spaceSeparatedTokens.size(); i++) {
            std::vector<std::string> vertexinfoinedxes = split(spaceSeparatedTokens[i], '/');
            if (vertexinfoinedxes.size() == 3) {
                VERTEXINFOINDEX pidx;
                pidx.vIndex = std::stoi(vertexinfoinedxes[0]) - 1;
                pidx.tIndex = std::stoi(vertexinfoinedxes[1]) - 1;
                pidx.nIndex = std::stoi(vertexinfoinedxes[2]) - 1;
                polygonindexes.push_back(pidx);
            }
            else {
                std::cout << "頂点インデックス・テクスチャインデックス・法線インデックス以外があります" << std::endl;
            }
        }

        return polygonindexes;
    }

    // 面を構成する頂点インデックスを取得する
    std::vector<std::vector<VERTEXINFOINDEX>> GetFace(const std::vector<std::string>& lines)
    {
        // ポリゴンを構成する頂点情報インデックスを格納する
        std::vector<std::vector<VERTEXINFOINDEX>> polygonindexes{};

        // マテリアルの名前
        std::string mtrlname = { "default" };

        for (const auto& line : lines) {
            if (line[0] == '#') continue;   // コメント行はスキップする

            if (line[0] == 'u' && line[1] == 's' && line[2] == 'e' && line[3] == 'm' && line[4] == 't' && line[5] == 'l')
            {
                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 2) {
                    mtrlname = tokens[1];
                }
            }

            // FACE情報を取得する
            if (line[0] == 'f' && isSpace(line[1])) {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 4) {

                    std::vector<VERTEXINFOINDEX> polygonindex{};
                    // ポリゴンインデックスを取得する
                    polygonindex = GetPolygonIndex(tokens);
                    // ポリゴンインデックス群に格納する

                    for (auto& pidx : polygonindex) {                       // マテリアル対応
                        pidx.mtrlname = mtrlname;
                    }

                    polygonindexes.push_back(polygonindex);
                }
                else {
                    std::cout << "３角形ポリゴン以外が存在します" << std::endl;
                }
            }
        }

        return polygonindexes;
    }

    std::vector<std::string> GetMtllib(std::vector<std::string> lines)
    {
        std::vector<std::string> mtrlfilenames{};

        for (const auto& line : lines) {
            if (line[0] == 'm' && line[1] == 't' && line[2] == 'l' && line[3] == 'l' && line[4] == 'i' && line[5] == 'b') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 2) {
                    std::string mtrlfilename{};
                    mtrlfilename = tokens[1];
                    mtrlfilenames.push_back(mtrlfilename);
                }
            }
        }

        return mtrlfilenames;
    }


    std::unordered_map<std::string, MATERIAL> GetMaterials(std::string filename)
    {
        std::vector<char> buffer{};
        std::vector<std::string> lines{};

        std::vector<MATERIAL> Materials{};

        buffer = readFileToMemory(filename);       // ファイルを読み込んでメモリに展開する

        // ファイルを行単位で分割する
        lines = split(buffer.data(), '\n');

        std::string mtrlname{};

        // マテリアルマップ
        std::unordered_map<std::string, MATERIAL> mtrlmap{};

        std::string nowmtrl{ "default" };

        for (const auto& line : lines) {

            if (line[0] == 'n' && line[1] == 'e' && line[2] == 'w' && line[3] == 'm' && line[4] == 't' && line[5] == 'l') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 2) {
                    nowmtrl = tokens[1];
                }
                continue;
            }

            if (line[0] == 'K' && line[1] == 'a') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 4) {
                    mtrlmap[nowmtrl].Ka[0] = std::stof(tokens[1]);
                    mtrlmap[nowmtrl].Ka[1] = std::stof(tokens[2]);
                    mtrlmap[nowmtrl].Ka[2] = std::stof(tokens[3]);
                }
                continue;
            }

            if (line[0] == 'K' && line[1] == 'd') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 4) {
                    mtrlmap[nowmtrl].Kd[0] = std::stof(tokens[1]);
                    mtrlmap[nowmtrl].Kd[1] = std::stof(tokens[2]);
                    mtrlmap[nowmtrl].Kd[2] = std::stof(tokens[3]);
                }
                continue;
            }

            if (line[0] == 'K' && line[1] == 's') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 4) {
                    mtrlmap[nowmtrl].Ks[0] = std::stof(tokens[1]);
                    mtrlmap[nowmtrl].Ks[1] = std::stof(tokens[2]);
                    mtrlmap[nowmtrl].Ks[2] = std::stof(tokens[3]);
                }
                continue;
            }

            if (line[0] == 'K' && line[1] == 'e') {

                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 4) {
                    mtrlmap[nowmtrl].Ke[0] = std::stof(tokens[1]);
                    mtrlmap[nowmtrl].Ke[1] = std::stof(tokens[2]);
                    mtrlmap[nowmtrl].Ke[2] = std::stof(tokens[3]);
                }
                continue;
            }

            if (line[0] == 'N' && line[1] == 's') {
                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 2) {
                    mtrlmap[nowmtrl].Ns = std::stof(tokens[1]);
                }
                continue;
            }

            if (line[0] == 'm' && line[1] == 'a' && line[2] == 'p' && line[3] == '_' && line[4] == 'K' && line[5] == 'd') {
                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() == 2) {
                    mtrlmap[nowmtrl].map_Kd = tokens[1];
                }
                continue;
            }
        }

        return mtrlmap;
    }

    // std::string 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::string& path) {
        return std::filesystem::path(path).parent_path();
    }

    // std::u8string 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::u8string& path) {
        return std::filesystem::path(path).parent_path();
    }

    // std::wstring 用のディレクトリ取得関数
    std::filesystem::path get_directory(const std::wstring& path) {
        return std::filesystem::path(path).parent_path();
    }

    // マテリアルに関連するポリゴン構成情報を取得する
    std::unordered_map<std::string, std::vector<std::vector<VERTEXINFOINDEX>>> GetMaterialFaces(std::string filename)
    {
        std::vector<char> buffer{};
        std::vector<std::string> lines{};

        buffer = readFileToMemory(filename);        // ファイルを読み込んでメモリに展開する
        buffer.push_back('\0');                     // バッファの最後にヌル文字を追加する

        // ファイルを行単位で分割する
        lines = split(buffer.data(), '\n');         // 面を構成する頂点インデックスを取得する

        /*
            // 頂点情報を構成する情報のインデックスを規定する
            struct VERTEXINFOINDEX {
                std::string mtrlname;      // 使用しているマテリアル名
                unsigned int vIndex;       // 頂点インデックス
                unsigned int tIndex;       // テクスチャインデックス
                unsigned int nIndex;	   // 法線インデックス
        };,
        */

        // 全ての面情報を取得する
        std::vector<std::vector<VERTEXINFOINDEX>> vertexindices = GetFace(lines);

        // マテリアル毎に整理する 
        std::unordered_map<std::string, std::vector<std::vector<VERTEXINFOINDEX>>> mtrlfaces{};

        // 面の構成情報数文ループする（１個めの頂点が持つマテリアル名で登録する） 
        for (auto& vindices : vertexindices)
        {
            mtrlfaces[vindices[0].mtrlname].push_back(vindices);
        }

        return mtrlfaces;
    }

    void GetObjData(std::string filename,
        std::vector<VERTEX>& vertices, 
        std::vector<TEXCOORD>& texcoords,
        std::vector<NORMAL>& normals,
        std::unordered_map<std::string, std::vector<std::vector<VERTEXINFOINDEX>>>& mtrlfaces,
        std::unordered_map<std::string, MATERIAL>& materials,
        std::vector<std::vector<OBJParse::VERTEXINFOINDEX>>& polygonindexes)
    {
        std::vector<char> buffer{};
        std::vector<std::string> lines{};

        buffer = readFileToMemory(filename);       // ファイルを読み込んでメモリに展開する

        // ファイルを行単位で分割する
        lines = split(buffer.data(), '\n');

        // 頂点座標データを取得する
        vertices = GetVertex(lines);

        // テクスチャ座標を取得する
        texcoords = GetTexCoord(lines);

        // 法線ベクトルを取得する
        normals = GetNormal(lines);

        // usematerialを取得する 
        std::vector<std::string> usematerials{};
        usematerials = GetUseMaterial(lines);

        // FACE情報を取得する
        // ポリゴンを構成する頂点情報インデックスを格納する
        polygonindexes = GetFace(lines);
        std::cout << "ポリゴンインデックス数: " << polygonindexes.size() << std::endl;

        // マテリアルファイル名を取得する
        std::vector<std::string> mtrlfilenames{};
        mtrlfilenames = GetMtllib(lines);

        for (const auto& n : mtrlfilenames) {
            std::cout << "マテリアルファイル名" << n << std::endl;
        }

        // ディレクトリ名を取得する
        auto directory = get_directory(filename);
        std::cout << "ディレクトリ名: " << directory << std::endl;

        // objファイルに記述されていたマテリアルファイルを読み込み
        // マテリアル情報を取得する
        for (auto n : mtrlfilenames) {
            directory += "/";
            directory += n;

            std::cout << "マテリアルファイル名（PATHNAME）: " << directory << std::endl;

            materials = GetMaterials(directory.generic_string());

            for (auto& mtrl : materials) {
                std::cout << mtrl.first << std::endl;
                std::cout << "Ka: " << mtrl.second.Ka[0] << " " << mtrl.second.Ka[1] << " " << mtrl.second.Ka[2] << std::endl;
                std::cout << "Kd: " << mtrl.second.Kd[0] << " " << mtrl.second.Kd[1] << " " << mtrl.second.Kd[2] << std::endl;
                std::cout << "Ks: " << mtrl.second.Ks[0] << " " << mtrl.second.Ks[1] << " " << mtrl.second.Ks[2] << std::endl;
                std::cout << "Ke: " << mtrl.second.Ke[0] << " " << mtrl.second.Ke[1] << " " << mtrl.second.Ke[2] << std::endl;
                std::cout << "Ns: " << mtrl.second.Ns << std::endl;
                std::cout << "Map_Kd: " << mtrl.second.map_Kd << std::endl;
            }
        }

        // マテリアルに関連するポリゴン構成情報を取得する
        mtrlfaces = GetMaterialFaces(filename);

        for (auto mtrl : mtrlfaces)
        {
            //マテリアル名表示
            std::cout << mtrl.first << std::endl;

            std::cout << mtrl.second.size() << std::endl;

        }
    }
}