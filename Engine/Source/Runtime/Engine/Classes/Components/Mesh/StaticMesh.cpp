#include "StaticMesh.h"
#include "Engine/FLoaderOBJ.h"
#include "UObject/ObjectFactory.h"

UStaticMesh::UStaticMesh()
{

}

UStaticMesh::~UStaticMesh()
{
    for (auto& Pair : LODRenderDataMap)
    {
        delete Pair.Value->IndexBuffer;
        delete Pair.Value->VertexBuffer;
        delete Pair.Value;
    }
    LODRenderDataMap.Empty();
}

uint32 UStaticMesh::GetMaterialIndex(FName MaterialSlotName) const
{
    for (uint32 materialIndex = 0; materialIndex < materials.Num(); materialIndex++) {
        if (materials[materialIndex]->MaterialSlotName == MaterialSlotName)
            return materialIndex;
    }

    return -1;
}

void UStaticMesh::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    for (const FStaticMaterial* Material : materials)
    {
        Out.Emplace(Material->Material);
    }
}

void UStaticMesh::SetData(ELODLevel LOD, OBJ::FStaticMeshRenderData* renderData)
{
    if (!renderData) return;

    // LODRenderDataMap에 저장
    LODRenderDataMap.Add(LOD, renderData);

    // LOD0일 때만 머티리얼 생성 (한 번만)
    if (LOD == ELODLevel::LOD0 && materials.Num() == 0)
    {
        for (int materialIndex = 0; materialIndex < renderData->Materials.Num(); materialIndex++)
        {
            FStaticMaterial* newMaterialSlot = new FStaticMaterial();
            UMaterial* newMaterial = FManagerOBJ::CreateMaterial(renderData->Materials[materialIndex]);

            newMaterialSlot->Material = newMaterial;
            newMaterialSlot->MaterialSlotName = renderData->Materials[materialIndex].MTLName;

            materials.Add(newMaterialSlot);
        }
    }
}

OBJ::FStaticMeshRenderData* UStaticMesh::GetRenderData(ELODLevel LOD)
{
    if (OBJ::FStaticMeshRenderData** Found = LODRenderDataMap.Find(LOD))
    {
        return *Found;
    }
    return nullptr;
}