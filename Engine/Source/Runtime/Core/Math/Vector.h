#pragma once

#include "SIMD/FVectorSimdUtility.h"

#include <DirectXMath.h>

struct FVector2D
{
	float x,y;
	FVector2D(float _x = 0, float _y = 0) : x(_x), y(_y) {}

	FVector2D operator+(const FVector2D& rhs) const
	{
		return FVector2D(x + rhs.x, y + rhs.y);
	}
	FVector2D operator-(const FVector2D& rhs) const
	{
		return FVector2D(x - rhs.x, y - rhs.y);
	}
	FVector2D operator*(float rhs) const
	{
		return FVector2D(x * rhs, y * rhs);
	}
	FVector2D operator/(float rhs) const
	{
		return FVector2D(x / rhs, y / rhs);
	}
	FVector2D& operator+=(const FVector2D& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		return *this;
	}
};

// 3D 벡터
struct FVector
{
    float x, y, z;
    FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

    FVector operator-(const FVector& other) const 
    {
#if USE_SIMD
        __m128 a = SIMD::LoadVec3(x, y, z);
        __m128 b = SIMD::LoadVec3(other.x, other.y, other.z);
        __m128 result = SIMD::Vec3Sub(a, b);
        //return SIMD::ToFVector(result);
        float f[3];
        SIMD::StoreVec3(result, f);
        return FVector(f[0], f[1], f[2]);
#else
        return FVector(x - other.x, y - other.y, z - other.z);
#endif
    }

    FVector operator+(const FVector& other) const 
    {
#if USE_SIMD
        __m128 a = SIMD::LoadVec3(x, y, z);
        __m128 b = SIMD::LoadVec3(other.x, other.y, other.z);
        __m128 result = SIMD::Vec3Add(a, b);
        //return SIMD::ToFVector(result);f
        float f[3];
        SIMD::StoreVec3(result, f);
        return FVector(f[0], f[1], f[2]);
#else
        return FVector(x + other.x, y + other.y, z + other.z);
#endif
    }

    // 벡터 내적
    float Dot(const FVector& other) const 
    {
#if USE_SIMD
        __m128 a = SIMD::LoadVec3(x, y, z);
        __m128 b = SIMD::LoadVec3(other.x, other.y, other.z);
        return SIMD::Vec3Dot(a, b);
#else
        return x * other.x + y * other.y + z * other.z;
#endif
    }

    // 벡터 크기
    float Magnitude() const {
        return sqrt(x * x + y * y + z * z);
    }

    // 벡터 정규화
    FVector Normalize() const {
        float mag = Magnitude();
        return (mag > 0) ? FVector(x / mag, y / mag, z / mag) : FVector(0, 0, 0);
    }

    FVector Cross(const FVector& Other) const
    {
#if USE_SIMD
        __m128 a = SIMD::LoadVec3(x, y, z);
        __m128 b = SIMD::LoadVec3(Other.x, Other.y, Other.z);
        __m128 result = SIMD::Vec3Cross(a, b);
        //return SIMD::ToFVector(result);
        float f[3];
        SIMD::StoreVec3(result, f);
        return FVector(f[0], f[1], f[2]);
#else
        return FVector{
            y * Other.z - z * Other.y,
            z * Other.x - x * Other.z,
            x * Other.y - y * Other.x
        };
#endif
    }

    // 스칼라 곱셈
    FVector operator*(float scalar) const {
#if USE_SIMD
        __m128 vec = SIMD::LoadVec3(x, y, z);
        __m128 scale = _mm_set1_ps(scalar);
        __m128 result = SIMD::Vec3Mul(vec, scale);
        float f[3];
        SIMD::StoreVec3(result, f);
        return FVector(f[0], f[1], f[2]);
#else
        return FVector(x * scalar, y * scalar, z * scalar);
#endif
    }

    bool operator==(const FVector& other) const {
        return (x == other.x && y == other.y && z == other.z);
    }

    float Distance(const FVector& other) const {
        // 두 벡터의 차 벡터의 크기를 계산
        return ((*this - other).Magnitude());
    }
    DirectX::XMFLOAT3 ToXMFLOAT3() const
    {
        return DirectX::XMFLOAT3(x, y, z);
    }

    static const FVector ZeroVector;
    static const FVector OneVector;
    static const FVector UpVector;
    static const FVector ForwardVector;
    static const FVector RightVector;
};


