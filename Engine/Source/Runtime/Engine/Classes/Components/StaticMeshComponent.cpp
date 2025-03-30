#include "Components/StaticMeshComponent.h"

#include "World.h"
#include "Launch/EngineLoop.h"
#include "UObject/ObjectFactory.h"
#include "UnrealEd/PrimitiveBatch.h"


uint32 UStaticMeshComponent::GetNumMaterials() const
{
    if (staticMesh == nullptr) return 0;

    return staticMesh->GetMaterials().Num();
}

UMaterial* UStaticMeshComponent::GetMaterial(uint32 ElementIndex) const
{
    if (staticMesh != nullptr)
    {
        if (OverrideMaterials[ElementIndex] != nullptr)
        {
            return OverrideMaterials[ElementIndex];
        }
    
        if (staticMesh->GetMaterials().IsValidIndex(ElementIndex))
        {
            return staticMesh->GetMaterials()[ElementIndex]->Material;
        }
    }
    return nullptr;
}

uint32 UStaticMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
    if (staticMesh == nullptr) return -1;

    return staticMesh->GetMaterialIndex(MaterialSlotName);
}

TArray<FName> UStaticMeshComponent::GetMaterialSlotNames() const
{
    TArray<FName> MaterialNames;
    if (staticMesh == nullptr) return MaterialNames;

    for (const FStaticMaterial* Material : staticMesh->GetMaterials())
    {
        MaterialNames.Emplace(Material->MaterialSlotName);
    }

    return MaterialNames;
}

void UStaticMeshComponent::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    if (staticMesh == nullptr) return;
    staticMesh->GetUsedMaterials(Out);
    for (int materialIndex = 0; materialIndex < GetNumMaterials(); materialIndex++)
    {
        if (OverrideMaterials[materialIndex] != nullptr)
        {
            Out[materialIndex] = OverrideMaterials[materialIndex];
        }
    }
}

int UStaticMeshComponent::CheckRayIntersection(const FVector& rayOrigin, const FVector& rayDirection, float& pfNearHitDistance)
{
    if (!staticMesh) return 0;

    OBJ::FStaticMeshRenderData* RenderData = staticMesh->GetRenderData();
    if (!RenderData) return 0;

    const TArray<FVertexCompact>& Vertices = RenderData->Vertices;
    const TArray<UINT>& Indices = RenderData->Indices;

    if (Vertices.IsEmpty() || Indices.IsEmpty()) return 0;

    int nIntersections = 0;
    float fNearHitDistance = FLT_MAX;

    for (int i = 0; i < Indices.Num(); i += 3)
    {
        const FVector& v0 = Vertices[Indices[i + 0]].ToFVector();
        const FVector& v1 = Vertices[Indices[i + 1]].ToFVector();
        const FVector& v2 = Vertices[Indices[i + 2]].ToFVector();

        float hitDist = FLT_MAX;
        if (IntersectRaySphere(rayOrigin, rayDirection, hitDist))
        //if (IntersectRayTriangle(rayOrigin, rayDirection, v0, v1, v2, hitDist))
        {
            if (hitDist < fNearHitDistance)
            {
                fNearHitDistance = hitDist;
                pfNearHitDistance = hitDist;
            }
            nIntersections++;
        }
    }

    return nIntersections;
}
