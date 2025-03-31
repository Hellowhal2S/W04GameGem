#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"
#include "Engine/FLoaderOBJ.h"
#include "Octree/Octree.h"

struct FManagerOBJ;
struct FLoaderOBJ;

class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh();
    virtual ~UStaticMesh() override;
    const TArray<FStaticMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& Out) const;
    OBJ::FStaticMeshRenderData* GetRenderData(ELODLevel LODLevel) const
    {
        if (LODLevel == ELODLevel::LOD0)
            return staticMeshRenderData;
        else if (LODLevel == ELODLevel::LOD1)
        {
            return FManagerOBJ::GetStaticMesh(staticMeshRenderData->ObjectName + L"X5")->GetRenderData();
        }
        else if (LODLevel == ELODLevel::LOD2)
        {
            return FManagerOBJ::GetStaticMesh(staticMeshRenderData->ObjectName + L"X1")->GetRenderData();
        }
    }
    OBJ::FStaticMeshRenderData* GetRenderData() const
    {
            return staticMeshRenderData;
    }
    void SetData(OBJ::FStaticMeshRenderData* renderData);

private:
    OBJ::FStaticMeshRenderData* staticMeshRenderData = nullptr;
    TArray<FStaticMaterial*> materials;
};
