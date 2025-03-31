#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"
#include "Octree/Octree.h"

enum class ELODLevel : uint8;

class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh();
    virtual ~UStaticMesh() override;

    const TArray<FStaticMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& Out) const;

    OBJ::FStaticMeshRenderData* GetRenderData(ELODLevel LOD = ELODLevel::LOD0);
    void SetData(ELODLevel LOD, OBJ::FStaticMeshRenderData* renderData);

private:
    TMap<ELODLevel, OBJ::FStaticMeshRenderData*> LODRenderDataMap;
    TArray<FStaticMaterial*> materials;
};
