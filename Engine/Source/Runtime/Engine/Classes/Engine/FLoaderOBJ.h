#pragma once
#include <fstream>
#include <queue>
#include <sstream>

#include "Define.h"
#include "EngineLoop.h"
#include "Container/Map.h"
#include "HAL/PlatformType.h"
#include "Serialization/Serializer.h"

class UStaticMesh;
struct FManagerOBJ;
struct FLoaderOBJ
{
    // Obj Parsing (*.obj to FObjInfo)
    static bool ParseOBJ(const FString& ObjFilePath, FObjInfo& OutObjInfo)
    {
        std::ifstream OBJ(ObjFilePath.ToWideString());
        if (!OBJ)
        {
            return false;
        }

        OutObjInfo.PathName = ObjFilePath.ToWideString().substr(0, ObjFilePath.ToWideString().find_last_of(L"\\/") + 1);
        OutObjInfo.ObjectName = ObjFilePath.ToWideString().substr(ObjFilePath.ToWideString().find_last_of(L"\\/") + 1);
        // ObjectName은 wstring 타입이므로, 이를 string으로 변환 (간단한 ASCII 변환의 경우)
        std::wstring wideName = OutObjInfo.ObjectName;
        std::string fileName(wideName.begin(), wideName.end());

        // 마지막 '.'을 찾아 확장자를 제거
        size_t dotPos = fileName.find_last_of('.');
        if (dotPos != std::string::npos) {
            OutObjInfo.DisplayName = fileName.substr(0, dotPos);
        } else {
            OutObjInfo.DisplayName = fileName;
        }
        
        std::string Line;

        while (std::getline(OBJ, Line))
        {
            if (Line.empty() || Line[0] == '#')
                continue;
            
            std::istringstream LineStream(Line);
            std::string Token;
            LineStream >> Token;

            if (Token == "mtllib")
            {
                LineStream >> Line;
                OutObjInfo.MatName = Line;
                continue;
            }

            if (Token == "usemtl")
            {
                LineStream >> Line;
                FString MatName(Line);

                if (!OutObjInfo.MaterialSubsets.IsEmpty())
                {
                    FMaterialSubset& LastSubset = OutObjInfo.MaterialSubsets[OutObjInfo.MaterialSubsets.Num() - 1];
                    LastSubset.IndexCount = OutObjInfo.VertexIndices.Num() - LastSubset.IndexStart;
                }
                
                FMaterialSubset MaterialSubset;
                MaterialSubset.MaterialName = MatName;
                MaterialSubset.IndexStart = OutObjInfo.VertexIndices.Num();
                MaterialSubset.IndexCount = 0;
                OutObjInfo.MaterialSubsets.Add(MaterialSubset);
            }

            if (Token == "g" || Token == "o")
            {
                LineStream >> Line;
                OutObjInfo.GroupName.Add(Line);
                OutObjInfo.NumOfGroup++;
            }

            if (Token == "v") // Vertex
            {
                float x, y, z;
                LineStream >> x >> y >> z;
                OutObjInfo.Vertices.Add(FVector(x,y,z));
                continue;
            }

            if (Token == "vn") // Normal
            {
                float nx, ny, nz;
                LineStream >> nx >> ny >> nz;
                OutObjInfo.Normals.Add(FVector(nx,ny,nz));
                continue;
            }

            if (Token == "vt") // Texture
            {
                float u, v;
                LineStream >> u >> v;
                OutObjInfo.UVs.Add(FVector2D(u, v));
                continue;
            }

            if (Token == "f")
            {
                TArray<uint32> faceVertexIndices;  // 이번 페이스의 정점 인덱스
                TArray<uint32> faceNormalIndices;  // 이번 페이스의 법선 인덱스
                TArray<uint32> faceTextureIndices; // 이번 페이스의 텍스처 인덱스
                
                while (LineStream >> Token)
                {
                    std::istringstream tokenStream(Token);
                    std::string part;
                    TArray<std::string> facePieces;

                    // '/'로 분리하여 v/vt/vn 파싱
                    while (std::getline(tokenStream, part, '/'))
                    {
                        facePieces.Add(part);
                    }

                    // OBJ 인덱스는 1부터 시작하므로 -1로 변환
                    uint32 vertexIndex = facePieces[0].empty() ? 0 : std::stoi(facePieces[0]) - 1;
                    uint32 textureIndex = (facePieces.Num() > 1 && !facePieces[1].empty()) ? std::stoi(facePieces[1]) - 1 : UINT32_MAX;
                    uint32 normalIndex = (facePieces.Num() > 2 && !facePieces[2].empty()) ? std::stoi(facePieces[2]) - 1 : UINT32_MAX;

                    faceVertexIndices.Add(vertexIndex);
                    faceTextureIndices.Add(textureIndex);
                    faceNormalIndices.Add(normalIndex);
                }

                if (faceVertexIndices.Num() == 4) // 쿼드
                {
                    // 첫 번째 삼각형: 0-1-2
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[0]);
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[1]);
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[2]);

                    OutObjInfo.TextureIndices.Add(faceTextureIndices[0]);
                    OutObjInfo.TextureIndices.Add(faceTextureIndices[1]);
                    OutObjInfo.TextureIndices.Add(faceTextureIndices[2]);

                    OutObjInfo.NormalIndices.Add(faceNormalIndices[0]);
                    OutObjInfo.NormalIndices.Add(faceNormalIndices[1]);
                    OutObjInfo.NormalIndices.Add(faceNormalIndices[2]);

                    // 두 번째 삼각형: 0-2-3
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[0]);
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[2]);
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[3]);

                    OutObjInfo.TextureIndices.Add(faceTextureIndices[0]);
                    OutObjInfo.TextureIndices.Add(faceTextureIndices[2]);
                    OutObjInfo.TextureIndices.Add(faceTextureIndices[3]);

                    OutObjInfo.NormalIndices.Add(faceNormalIndices[0]);
                    OutObjInfo.NormalIndices.Add(faceNormalIndices[2]);
                    OutObjInfo.NormalIndices.Add(faceNormalIndices[3]);
                }
                else if (faceVertexIndices.Num() == 3) // 삼각형
                {
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[0]);
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[1]);
                    OutObjInfo.VertexIndices.Add(faceVertexIndices[2]);

                    OutObjInfo.TextureIndices.Add(faceTextureIndices[0]);
                    OutObjInfo.TextureIndices.Add(faceTextureIndices[1]);
                    OutObjInfo.TextureIndices.Add(faceTextureIndices[2]);

                    OutObjInfo.NormalIndices.Add(faceNormalIndices[0]);
                    OutObjInfo.NormalIndices.Add(faceNormalIndices[1]);
                    OutObjInfo.NormalIndices.Add(faceNormalIndices[2]);
                }
                // // 삼각형화 (삼각형 팬 방식)
                // for (int j = 1; j + 1 < faceVertexIndices.Num(); j++)
                // {
                //     OutObjInfo.VertexIndices.Add(faceVertexIndices[0]);
                //     OutObjInfo.VertexIndices.Add(faceVertexIndices[j]);
                //     OutObjInfo.VertexIndices.Add(faceVertexIndices[j + 1]);
                //
                //     OutObjInfo.TextureIndices.Add(faceTextureIndices[0]);
                //     OutObjInfo.TextureIndices.Add(faceTextureIndices[j]);
                //     OutObjInfo.TextureIndices.Add(faceTextureIndices[j + 1]);
                //
                //     OutObjInfo.NormalIndices.Add(faceNormalIndices[0]);
                //     OutObjInfo.NormalIndices.Add(faceNormalIndices[j]);
                //     OutObjInfo.NormalIndices.Add(faceNormalIndices[j + 1]);
                // }
            }
        }

        if (!OutObjInfo.MaterialSubsets.IsEmpty())
        {
            FMaterialSubset& LastSubset = OutObjInfo.MaterialSubsets[OutObjInfo.MaterialSubsets.Num() - 1];
            LastSubset.IndexCount = OutObjInfo.VertexIndices.Num() - LastSubset.IndexStart;
        }
        
        return true;
    }
    
    // Material Parsing (*.obj to MaterialInfo)
    static bool ParseMaterial(FObjInfo& OutObjInfo, OBJ::FStaticMeshRenderData& OutFStaticMesh)
    {
        // Subset
        OutFStaticMesh.MaterialSubsets = OutObjInfo.MaterialSubsets;
        
        std::ifstream MtlFile(OutObjInfo.PathName + OutObjInfo.MatName.ToWideString());
        if (!MtlFile.is_open())
        {
            return false;
        }

        std::string Line;
        int32 MaterialIndex = -1;
        
        while (std::getline(MtlFile, Line))
        {
            if (Line.empty() || Line[0] == '#')
                continue;
            
            std::istringstream LineStream(Line);
            std::string Token;
            LineStream >> Token;

            // Create new material if token is 'newmtl'
            if (Token == "newmtl")
            {
                LineStream >> Line;
                MaterialIndex++;

                FObjMaterialInfo Material;
                Material.MTLName = Line;
                OutFStaticMesh.Materials.Add(Material);
            }

            if (Token == "Kd")
            {
                float x, y, z;
                LineStream >> x >> y >> z;
                OutFStaticMesh.Materials[MaterialIndex].Diffuse = FVector(x, y, z);
            }

            if (Token == "Ks")
            {
                float x, y, z;
                LineStream >> x >> y >> z;
                OutFStaticMesh.Materials[MaterialIndex].Specular = FVector(x, y, z);
            }

            if (Token == "Ka")
            {
                float x, y, z;
                LineStream >> x >> y >> z;
                OutFStaticMesh.Materials[MaterialIndex].Ambient = FVector(x, y, z);
            }

            if (Token == "Ke")
            {
                float x, y, z;
                LineStream >> x >> y >> z;
                OutFStaticMesh.Materials[MaterialIndex].Emissive = FVector(x, y, z);
            }

            if (Token == "Ns")
            {
                float x;
                LineStream >> x;
                OutFStaticMesh.Materials[MaterialIndex].SpecularScalar = x;
            }

            if (Token == "Ni")
            {
                float x;
                LineStream >> x;
                OutFStaticMesh.Materials[MaterialIndex].DensityScalar = x;
            }

            if (Token == "d" || Token == "Tr")
            {
                float x;
                LineStream >> x;
                OutFStaticMesh.Materials[MaterialIndex].TransparencyScalar = x;
                OutFStaticMesh.Materials[MaterialIndex].bTransparent = true;
            }

            if (Token == "illum")
            {
                uint32 x;
                LineStream >> x;
                OutFStaticMesh.Materials[MaterialIndex].IlluminanceModel = x;
            }

            if (Token == "map_Kd")
            {
                LineStream >> Line;
                OutFStaticMesh.Materials[MaterialIndex].DiffuseTextureName = Line;

                FWString TexturePath = OutObjInfo.PathName + OutFStaticMesh.Materials[MaterialIndex].DiffuseTextureName.ToWideString();
                OutFStaticMesh.Materials[MaterialIndex].DiffuseTexturePath = TexturePath;
                OutFStaticMesh.Materials[MaterialIndex].bHasTexture = true;

                CreateTextureFromFile(OutFStaticMesh.Materials[MaterialIndex].DiffuseTexturePath);
            }
        }
        
        return true;
    }
    
    // Convert the Raw data to Cooked data (FStaticMeshRenderData)
    static bool ConvertToStaticMesh(const FObjInfo& RawData, OBJ::FStaticMeshRenderData& OutStaticMesh)
    {
        OutStaticMesh.ObjectName = RawData.ObjectName;
        OutStaticMesh.PathName = RawData.PathName;
        OutStaticMesh.DisplayName = RawData.DisplayName;

        // 고유 정점을 기반으로 FVertexSimple 배열 생성
        TMap<std::string, uint32> vertexMap; // 중복 체크용

        for (int32 i = 0; i < RawData.VertexIndices.Num(); i++)
        {
            uint32 vIdx = RawData.VertexIndices[i];
            uint32 tIdx = RawData.TextureIndices[i];
            uint32 nIdx = RawData.NormalIndices[i];

            // 키 생성 (v/vt/vn 조합)
            std::string key = std::to_string(vIdx) + "/" + 
                             std::to_string(tIdx) + "/" + 
                             std::to_string(nIdx);

            uint32 index;
            if (vertexMap.Find(key) == nullptr)
            {
                FVertexSimple vertex {};
                vertex.x = RawData.Vertices[vIdx].x;
                vertex.y = RawData.Vertices[vIdx].y;
                vertex.z = RawData.Vertices[vIdx].z;

                vertex.r = 1.0f; vertex.g = 1.0f; vertex.b = 1.0f; vertex.a = 1.0f; // 기본 색상

                if (tIdx != UINT32_MAX && tIdx < RawData.UVs.Num())
                {
                    vertex.u = RawData.UVs[tIdx].x;
                    vertex.v = -RawData.UVs[tIdx].y;
                }

                if (nIdx != UINT32_MAX && nIdx < RawData.Normals.Num())
                {
                    vertex.nx = RawData.Normals[nIdx].x;
                    vertex.ny = RawData.Normals[nIdx].y;
                    vertex.nz = RawData.Normals[nIdx].z;
                }

                for (int32 j = 0; j < OutStaticMesh.MaterialSubsets.Num(); j++)
                {
                    const FMaterialSubset& subset = OutStaticMesh.MaterialSubsets[j];
                    if ( i >= subset.IndexStart && i < subset.IndexStart + subset.IndexCount)
                    {
                        vertex.MaterialIndex = subset.MaterialIndex;
                        break;
                    }
                }
                
                index = OutStaticMesh.Vertices.Num();
                OutStaticMesh.Vertices.Add(vertex);
                vertexMap[key] = index;
            }
            else
            {
                index = vertexMap[key];
            }

            OutStaticMesh.Indices.Add(index);
            
        }

        // Calculate StaticMesh BoundingBox
        ComputeBoundingBox(OutStaticMesh.Vertices, OutStaticMesh.BoundingBoxMin, OutStaticMesh.BoundingBoxMax);
        
        return true;
    }

    static bool CreateTextureFromFile(const FWString& Filename)
    {
        
        if (FEngineLoop::resourceMgr.GetTexture(Filename))
        {
            return true;
        }

        HRESULT hr = FEngineLoop::resourceMgr.LoadTextureFromFile(FEngineLoop::graphicDevice.Device, FEngineLoop::graphicDevice.DeviceContext, Filename.c_str());

        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }

    static void ComputeBoundingBox(const TArray<FVertexSimple>& InVertices, FVector& OutMinVector, FVector& OutMaxVector)
    {
        FVector MinVector = { FLT_MAX, FLT_MAX, FLT_MAX };
        FVector MaxVector = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        
        for (int32 i = 0; i < InVertices.Num(); i++)
        {
            MinVector.x = std::min(MinVector.x, InVertices[i].x);
            MinVector.y = std::min(MinVector.y, InVertices[i].y);
            MinVector.z = std::min(MinVector.z, InVertices[i].z);

            MaxVector.x = std::max(MaxVector.x, InVertices[i].x);
            MaxVector.y = std::max(MaxVector.y, InVertices[i].y);
            MaxVector.z = std::max(MaxVector.z, InVertices[i].z);
        }

        OutMinVector = MinVector;
        OutMaxVector = MaxVector;
    }
};

struct FManagerOBJ
{
public:
    static OBJ::FStaticMeshRenderData* LoadObjStaticMeshAsset(const FString& PathFileName);

    static void CombineMaterialIndex(OBJ::FStaticMeshRenderData& OutFStaticMesh);

    static bool SaveStaticMeshToBinary(const FWString& FilePath, const OBJ::FStaticMeshRenderData& StaticMesh);

    static bool LoadStaticMeshFromBinary(const FWString& FilePath, OBJ::FStaticMeshRenderData& OutStaticMesh);

    static UMaterial* CreateMaterial(FObjMaterialInfo materialInfo);
    static TMap<FString, UMaterial*>& GetMaterials() { return materialMap; }
    static UMaterial* GetMaterial(FString name);
    static int GetMaterialNum() { return materialMap.Num(); }
    static UStaticMesh* CreateStaticMesh(FString filePath);
    static const TMap<FWString, UStaticMesh*>& GetStaticMeshes() { return staticMeshMap; }
    static UStaticMesh* GetStaticMesh(FWString name);
    static int GetStaticMeshNum() { return staticMeshMap.Num(); }

private:
    inline static TMap<FString, OBJ::FStaticMeshRenderData*> ObjStaticMeshMap;
    inline static TMap<FWString, UStaticMesh*> staticMeshMap;
    inline static TMap<FString, UMaterial*> materialMap;
};



struct Quadric {
    float data[10] = {0};

    void Add(const Quadric& q) {
        for (int i = 0; i < 10; i++) data[i] += q.data[i];
    }
};

struct EdgeCollapse {
    int v1, v2;
    float cost;
    FVector newPos;

    bool operator<(const EdgeCollapse& other) const {
        return cost > other.cost; // 최소 힙을 위해 부등호 반대로 설정
    }
};

class QEMSimplifier {
public:
    static void Simplify(FObjInfo& obj, int targetVertexCount) {
        int numVertices = obj.Vertices.Num();
        int numFaces = obj.VertexIndices.Num() / 3;

        // 1. 각 정점에 대한 QEM 행렬 계산
        std::vector<Quadric> quadrics(numVertices);
        for (int i = 0; i < numFaces; i++) {
            int v0 = obj.VertexIndices[i * 3 + 0];
            int v1 = obj.VertexIndices[i * 3 + 1];
            int v2 = obj.VertexIndices[i * 3 + 2];
            
            FVector p0 = obj.Vertices[v0];
            FVector p1 = obj.Vertices[v1];
            FVector p2 = obj.Vertices[v2];
            
            FVector normal = (p1 - p0).Cross(p2 - p0).Normalize();
            float d = -normal.Dot(p0);
            
            Quadric q;
            q.data[0] = normal.x * normal.x;
            q.data[1] = normal.x * normal.y;
            q.data[2] = normal.x * normal.z;
            q.data[3] = normal.x * d;
            q.data[4] = normal.y * normal.y;
            q.data[5] = normal.y * normal.z;
            q.data[6] = normal.y * d;
            q.data[7] = normal.z * normal.z;
            q.data[8] = normal.z * d;
            q.data[9] = d * d;
            
            quadrics[v0].Add(q);
            quadrics[v1].Add(q);
            quadrics[v2].Add(q);
        }

        // 2. 엣지 콜랩스를 위한 우선순위 큐 생성
        std::priority_queue<EdgeCollapse> collapseQueue;
        for (int i = 0; i < numVertices; i++) {
            for (int j = i + 1; j < numVertices; j++) {
                FVector midpoint = (obj.Vertices[i] + obj.Vertices[j]) * 0.5f;
                float cost = ComputeCollapseCost(quadrics[i], quadrics[j], midpoint);
                collapseQueue.push({i, j, cost, midpoint});
            }
        }

        // 3. 목표 정점 개수까지 단순화 수행
        while (obj.Vertices.Num() > targetVertexCount && !collapseQueue.empty()) {
            EdgeCollapse bestCollapse = collapseQueue.top();
            collapseQueue.pop();
            
            int v1 = bestCollapse.v1;
            int v2 = bestCollapse.v2;
            
            obj.Vertices[v1] = bestCollapse.newPos;
            
            quadrics[v1].Add(quadrics[v2]);
            quadrics[v2] = quadrics[v1];
            
            // 정점 삭제
            obj.Vertices.RemoveAt(v2);
            obj.Normals.RemoveAt(v2);
            obj.UVs.RemoveAt(v2);
            
            // 인덱스 재매핑
            for (uint32& index : obj.VertexIndices) {
                if (index == v2) index = v1;
                if (index > v2) index--;
            }
        }
    }

private:
    static float ComputeCollapseCost(const Quadric& q1, const Quadric& q2, FVector& newPos) {
        Quadric q = q1;
        q.Add(q2);
        
        return q.data[9]; // 단순히 d*d 값 활용 (더 정교한 방법 가능)
    }
};
