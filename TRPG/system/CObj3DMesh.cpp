#include	"CObj3DMesh.h"
#include	<iostream>

void CObj3DMesh::Load(std::string filename)
{
    // OBJファイルのデータ
    std::vector<OBJParse::VERTEX> objvertices;              // 頂点座標データ
    std::vector<OBJParse::TEXCOORD> objtexcoords;		    // テクスチャ座標
    std::vector<OBJParse::NORMAL> objnormals;			    // 法線ベクトル    
    std::unordered_map<std::string, OBJParse::MATERIAL> objmaterials;  // マテリアル情報
    std::vector<std::vector<OBJParse::VERTEXINFOINDEX>> objpolygonindexes{};  // 面情報
    std::unordered_map<std::string, std::vector<std::vector<OBJParse::VERTEXINFOINDEX>>> objmtrlfaces;  // マテリアルごとの面情報



    //OBJファイルの読み込み(オブジェのデータを取り込む)
    GetObjData(
        filename,           // ファイル名
        objvertices,		// 頂点座標データ      
        objtexcoords,		// テクスチャ座標  
        objnormals,			// 法線ベクトル   
        objmtrlfaces,   	// マテリアルごとの面情報
        objmaterials,		// マテリアル情報  
        objpolygonindexes);	// 面情報 

    // 表示するための３Ｄ頂点データとインデックスデータを作成する
    for (auto& vertexinfoidx : objpolygonindexes) {

        for (auto& idx : vertexinfoidx) {
            VERTEX_3D v{};

            v.Position = Vector3(
                objvertices[idx.vIndex].x,
                objvertices[idx.vIndex].y,
                objvertices[idx.vIndex].z);

            v.Normal = Vector3(
                objnormals[idx.nIndex].nx,
                objnormals[idx.nIndex].ny,
                objnormals[idx.nIndex].nz);

            v.TexCoord = Vector2(
                objtexcoords[idx.tIndex].u,
                objtexcoords[idx.tIndex].v);

            v.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);

            m_vertices.emplace_back(v);

            m_indices.emplace_back(static_cast<int>(m_vertices.size()) - 1);
        }
    }

    // マテリアルデータ作成
    for (auto& objmtrl : objmaterials) {
        MATERIAL mtrl{};

        // マテリアル情報  
        mtrl.Ambient = Color(
            objmtrl.second.Ka[0],
            objmtrl.second.Ka[1],
            objmtrl.second.Ka[2], 1.0f);

        mtrl.Diffuse = Color(
            objmtrl.second.Kd[0],
            objmtrl.second.Kd[1],
            objmtrl.second.Kd[2], 1.0f);

        mtrl.Specular = Color(
            objmtrl.second.Ks[0],
            objmtrl.second.Ks[1],
            objmtrl.second.Ks[2], 1.0f);

        mtrl.Emission = Color(
            objmtrl.second.Ke[0],
            objmtrl.second.Ke[1],
            objmtrl.second.Ke[2], 1.0f);

        // ディフューズテクスチャがあるかどうか？
        if (objmtrl.second.map_Kd.empty()) {
            mtrl.TextureEnable = FALSE;

            m_diffusetexturenames.emplace_back("");      // ディフーズテクスチャ名空で保存
        }
        else {
            mtrl.TextureEnable = TRUE;

            m_diffusetexturenames.emplace_back(objmtrl.second.map_Kd);      // ディフーズテクスチャ名保存
        }

        m_materials.emplace_back(mtrl);
    }

    /*
    // メッシュ（マテリアル毎にサブセットが存在する）
    struct SUBSET {
        std::string		Name;							//サブセット名
        unsigned int	IndexNum = 0;					// インデックス数
        unsigned int	VertexNum = 0;					// 頂点数
        unsigned int	IndexBase = 0;					// 開始インデックス
        unsigned int	VertexBase = 0;					// 頂点ベース
        unsigned int	MaterialIdx = 0;				// マテリアルインデックス
    };
    */

    // サブセットデータ(マテリアル数分作成)作成
    // 頂点の順番通りでマテリアル順番を取得    
    std::vector<std::string> mtrnamevertexorder{};

    std::string mtrlname{};
    for (const auto& plygonindex : objpolygonindexes)
    {
        if (mtrlname != plygonindex[0].mtrlname)
        {
            mtrnamevertexorder.emplace_back(plygonindex[0].mtrlname);
            mtrlname = plygonindex[0].mtrlname;
        }
    }

    // 頂点の格納順番マテリアルでサブセットを作成する
    int loopcnt = 0;
    for (const auto& mtrln : mtrnamevertexorder)
    {
        SUBSET subset{};
        subset.VertexBase = 0;              // 頂点ベース
        subset.IndexBase = 0;			    // インデックスベース
        subset.MaterialIdx = loopcnt;	    // マテリアルインデックス   
        subset.MtrlName = mtrln;                // マテリアル名
        subset.IndexNum = static_cast<unsigned int>(objmtrlfaces[mtrln].size() * 3);        // インデックス数
        subset.VertexNum = static_cast<unsigned int>(objmtrlfaces[mtrln].size() * 3);        // 頂点数
        loopcnt++;

        m_subsets.emplace_back(subset);
    }

    // サブセット数分ループしてベースの計算
    for (int m = 0; m < m_subsets.size(); m++)
    {
        // 頂点バッファのベースを計算
        m_subsets[m].VertexBase = 0;

        // インデックスバッファのベースを計算
        m_subsets[m].IndexBase = 0;
        for (int i = m - 1; i >= 0; i--) {
            m_subsets[m].IndexBase += m_subsets[i].IndexNum;
        }
    }

    // サブセットの表示
    for (const auto& s: m_subsets) 
    {
        std::cout << "サブセット名:" << s.MtrlName << std::endl;
		std::cout << "マテリアルインデックス:" << s.MaterialIdx << std::endl;
        std::cout << "インデックスベース:" << s.IndexBase << std::endl;
        std::cout << "インデックス数:" << s.IndexNum << std::endl;
        std::cout << "頂点ベース:" << s.VertexBase << std::endl;
        std::cout << "頂点数:" << s.VertexNum << std::endl;
    }
}