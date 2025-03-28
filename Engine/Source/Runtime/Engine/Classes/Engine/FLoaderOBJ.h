#pragma once
#include <fstream>
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
    static OBJ::FStaticMeshRenderData* LoadObjStaticMeshAsset(const FString& PathFileName)
    {
        OBJ::FStaticMeshRenderData* NewStaticMesh = new OBJ::FStaticMeshRenderData();
        
        if ( const auto It = ObjStaticMeshMap.Find(PathFileName))
        {
            return *It;
        }
        
        FWString BinaryPath = (PathFileName + ".bin").ToWideString();
        if (std::ifstream(BinaryPath).good())
        {
            if (LoadStaticMeshFromBinary(BinaryPath, *NewStaticMesh))
            {
                ObjStaticMeshMap.Add(PathFileName, NewStaticMesh);
                return NewStaticMesh;
            }
        }
        
        // Parse OBJ
        FObjInfo NewObjInfo;
        bool Result = FLoaderOBJ::ParseOBJ(PathFileName, NewObjInfo);

        if (!Result)
        {
            delete NewStaticMesh;
            return nullptr;
        }

        // Material
        if (NewObjInfo.MaterialSubsets.Num() > 0)
        {
            Result = FLoaderOBJ::ParseMaterial(NewObjInfo, *NewStaticMesh);

            if (!Result)
            {
                delete NewStaticMesh;
                return nullptr;
            }

            CombineMaterialIndex(*NewStaticMesh);

            for (int materialIndex = 0; materialIndex < NewStaticMesh->Materials.Num(); materialIndex++) {
                CreateMaterial(NewStaticMesh->Materials[materialIndex]);
            }
        }
        
        // Convert FStaticMeshRenderData
        Result = FLoaderOBJ::ConvertToStaticMesh(NewObjInfo, *NewStaticMesh);
        if (!Result)
        {
            delete NewStaticMesh;
            return nullptr;
        }

        SaveStaticMeshToBinary(BinaryPath, *NewStaticMesh);
        ObjStaticMeshMap.Add(PathFileName, NewStaticMesh);
        return NewStaticMesh;
    }
    
    static void CombineMaterialIndex(OBJ::FStaticMeshRenderData& OutFStaticMesh)
    {
        for (int32 i = 0; i < OutFStaticMesh.MaterialSubsets.Num(); i++)
        {
            FString MatName = OutFStaticMesh.MaterialSubsets[i].MaterialName;
            for (int32 j = 0; j < OutFStaticMesh.Materials.Num(); j++)
            {
                if (OutFStaticMesh.Materials[j].MTLName == MatName)
                {
                    OutFStaticMesh.MaterialSubsets[i].MaterialIndex = j;
                    break;
                }
            }
        }
    }

    static bool SaveStaticMeshToBinary(const FWString& FilePath, const OBJ::FStaticMeshRenderData& StaticMesh)
    {
        std::ofstream File(FilePath, std::ios::binary);
        if (!File.is_open())
        {
            assert("CAN'T SAVE STATIC MESH BINARY FILE");
            return false;
        }

        // Object Name
        Serializer::WriteFWString(File, StaticMesh.ObjectName);

        // Path Name
        Serializer::WriteFWString(File, StaticMesh.PathName);

        // Display Name
        Serializer::WriteFString(File, StaticMesh.DisplayName);

        // Vertices
        uint32 VertexCount = StaticMesh.Vertices.Num();
        File.write(reinterpret_cast<const char*>(&VertexCount), sizeof(VertexCount));
        File.write(reinterpret_cast<const char*>(StaticMesh.Vertices.GetData()), VertexCount * sizeof(FVertexSimple));

        // Indices
        uint32 IndexCount = StaticMesh.Indices.Num();
        File.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
        File.write(reinterpret_cast<const char*>(StaticMesh.Indices.GetData()), IndexCount * sizeof(UINT));

        // Materials
        uint32 MaterialCount = StaticMesh.Materials.Num();
        File.write(reinterpret_cast<const char*>(&MaterialCount), sizeof(MaterialCount));
        for (const FObjMaterialInfo& Material : StaticMesh.Materials)
        {
            Serializer::WriteFString(File, Material.MTLName);
            File.write(reinterpret_cast<const char*>(&Material.bHasTexture), sizeof(Material.bHasTexture));
            File.write(reinterpret_cast<const char*>(&Material.bTransparent), sizeof(Material.bTransparent));
            File.write(reinterpret_cast<const char*>(&Material.Diffuse), sizeof(Material.Diffuse));
            File.write(reinterpret_cast<const char*>(&Material.Specular), sizeof(Material.Specular));
            File.write(reinterpret_cast<const char*>(&Material.Ambient), sizeof(Material.Ambient));
            File.write(reinterpret_cast<const char*>(&Material.Emissive), sizeof(Material.Emissive));
            File.write(reinterpret_cast<const char*>(&Material.SpecularScalar), sizeof(Material.SpecularScalar));
            File.write(reinterpret_cast<const char*>(&Material.DensityScalar), sizeof(Material.DensityScalar));
            File.write(reinterpret_cast<const char*>(&Material.TransparencyScalar), sizeof(Material.TransparencyScalar));
            File.write(reinterpret_cast<const char*>(&Material.IlluminanceModel), sizeof(Material.IlluminanceModel));

            Serializer::WriteFString(File, Material.DiffuseTextureName);
            Serializer::WriteFWString(File, Material.DiffuseTexturePath);
            Serializer::WriteFString(File, Material.AmbientTextureName);
            Serializer::WriteFWString(File, Material.AmbientTexturePath);
            Serializer::WriteFString(File, Material.SpecularTextureName);
            Serializer::WriteFWString(File, Material.SpecularTexturePath);
            Serializer::WriteFString(File, Material.BumpTextureName);
            Serializer::WriteFWString(File, Material.BumpTexturePath);
            Serializer::WriteFString(File, Material.AlphaTextureName);
            Serializer::WriteFWString(File, Material.AlphaTexturePath);
        }

        // Material Subsets
        uint32 SubsetCount = StaticMesh.MaterialSubsets.Num();
        File.write(reinterpret_cast<const char*>(&SubsetCount), sizeof(SubsetCount));
        for (const FMaterialSubset& Subset : StaticMesh.MaterialSubsets)
        {
            Serializer::WriteFString(File, Subset.MaterialName);
            File.write(reinterpret_cast<const char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
            File.write(reinterpret_cast<const char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
            File.write(reinterpret_cast<const char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        }

        // Bounding Box
        File.write(reinterpret_cast<const char*>(&StaticMesh.BoundingBoxMin), sizeof(FVector));
        File.write(reinterpret_cast<const char*>(&StaticMesh.BoundingBoxMax), sizeof(FVector));
        
        File.close();
        return true;
    }

    static bool LoadStaticMeshFromBinary(const FWString& FilePath, OBJ::FStaticMeshRenderData& OutStaticMesh)
    {
        std::ifstream File(FilePath, std::ios::binary);
        if (!File.is_open())
        {
            assert("CAN'T OPEN STATIC MESH BINARY FILE");
            return false;
        }

        TArray<FWString> Textures;

        // Object Name
        Serializer::ReadFWString(File, OutStaticMesh.ObjectName);

        // Path Name
        Serializer::ReadFWString(File, OutStaticMesh.PathName);

        // Display Name
        Serializer::ReadFString(File, OutStaticMesh.DisplayName);

        // Vertices
        uint32 VertexCount = 0;
        File.read(reinterpret_cast<char*>(&VertexCount), sizeof(VertexCount));
        OutStaticMesh.Vertices.SetNum(VertexCount);
        File.read(reinterpret_cast<char*>(OutStaticMesh.Vertices.GetData()), VertexCount * sizeof(FVertexSimple));

        // Indices
        uint32 IndexCount = 0;
        File.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
        OutStaticMesh.Indices.SetNum(IndexCount);
        File.read(reinterpret_cast<char*>(OutStaticMesh.Indices.GetData()), IndexCount * sizeof(UINT));

        // Material
        uint32 MaterialCount = 0;
        File.read(reinterpret_cast<char*>(&MaterialCount), sizeof(MaterialCount));
        OutStaticMesh.Materials.SetNum(MaterialCount);
        for (FObjMaterialInfo& Material : OutStaticMesh.Materials)
        {
            Serializer::ReadFString(File, Material.MTLName);
            File.read(reinterpret_cast<char*>(&Material.bHasTexture), sizeof(Material.bHasTexture));
            File.read(reinterpret_cast<char*>(&Material.bTransparent), sizeof(Material.bTransparent));
            File.read(reinterpret_cast<char*>(&Material.Diffuse), sizeof(Material.Diffuse));
            File.read(reinterpret_cast<char*>(&Material.Specular), sizeof(Material.Specular));
            File.read(reinterpret_cast<char*>(&Material.Ambient), sizeof(Material.Ambient));
            File.read(reinterpret_cast<char*>(&Material.Emissive), sizeof(Material.Emissive));
            File.read(reinterpret_cast<char*>(&Material.SpecularScalar), sizeof(Material.SpecularScalar));
            File.read(reinterpret_cast<char*>(&Material.DensityScalar), sizeof(Material.DensityScalar));
            File.read(reinterpret_cast<char*>(&Material.TransparencyScalar), sizeof(Material.TransparencyScalar));
            File.read(reinterpret_cast<char*>(&Material.IlluminanceModel), sizeof(Material.IlluminanceModel));
            Serializer::ReadFString(File, Material.DiffuseTextureName);
            Serializer::ReadFWString(File, Material.DiffuseTexturePath);
            Serializer::ReadFString(File, Material.AmbientTextureName);
            Serializer::ReadFWString(File, Material.AmbientTexturePath);
            Serializer::ReadFString(File, Material.SpecularTextureName);
            Serializer::ReadFWString(File, Material.SpecularTexturePath);
            Serializer::ReadFString(File, Material.BumpTextureName);
            Serializer::ReadFWString(File, Material.BumpTexturePath);
            Serializer::ReadFString(File, Material.AlphaTextureName);
            Serializer::ReadFWString(File, Material.AlphaTexturePath);

            if (!Material.DiffuseTexturePath.empty())
            {
                Textures.AddUnique(Material.DiffuseTexturePath);
            }
            if (!Material.AmbientTexturePath.empty())
            {
                Textures.AddUnique(Material.AmbientTexturePath);
            }
            if (!Material.SpecularTexturePath.empty())
            {
                Textures.AddUnique(Material.SpecularTexturePath);
            }
            if (!Material.BumpTexturePath.empty())
            {
                Textures.AddUnique(Material.BumpTexturePath);
            }
            if (!Material.AlphaTexturePath.empty())
            {
                Textures.AddUnique(Material.AlphaTexturePath);
            }
        }

        // Material Subset
        uint32 SubsetCount = 0;
        File.read(reinterpret_cast<char*>(&SubsetCount), sizeof(SubsetCount));
        OutStaticMesh.MaterialSubsets.SetNum(SubsetCount);
        for (FMaterialSubset& Subset : OutStaticMesh.MaterialSubsets)
        {
            Serializer::ReadFString(File, Subset.MaterialName);
            File.read(reinterpret_cast<char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
            File.read(reinterpret_cast<char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
            File.read(reinterpret_cast<char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        }

        // Bounding Box
        File.read(reinterpret_cast<char*>(&OutStaticMesh.BoundingBoxMin), sizeof(FVector));
        File.read(reinterpret_cast<char*>(&OutStaticMesh.BoundingBoxMax), sizeof(FVector));
        
        File.close();

        // Texture Load
        if (Textures.Num() > 0)
        {
            for (const FWString& Texture : Textures)
            {
                if (FEngineLoop::resourceMgr.GetTexture(Texture) == nullptr)
                {
                    FEngineLoop::resourceMgr.LoadTextureFromFile(FEngineLoop::graphicDevice.Device, FEngineLoop::graphicDevice.DeviceContext, Texture.c_str());
                }
            }
        }
        
        return true;
    }

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