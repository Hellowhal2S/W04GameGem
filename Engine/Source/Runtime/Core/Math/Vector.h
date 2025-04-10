#pragma once

#include "SIMD/SimdUtility.h"

#include <DirectXMath.h>
#include <stdexcept>

#include "MathUtility.h"

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
        __m128 result = SIMD::VecSub(a, b);
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
        __m128 result = SIMD::VecAdd(a, b);
        //return SIMD::ToFVector(result);f
        float f[3];
        SIMD::StoreVec3(result, f);
        return FVector(f[0], f[1], f[2]);
#else
        return FVector(x + other.x, y + other.y, z + other.z);
#endif
    }
    FVector operator/(const FVector& Other) const;
    FVector operator/(float Scalar) const;
    FVector& operator/=(float Scalar);

    // 벡터 내적
    float Dot(const FVector& other) const 
    {
#if USE_SIMD
        __m128 a = SIMD::LoadVec3(x, y, z);
        __m128 b = SIMD::LoadVec3(other.x, other.y, other.z);
        return SIMD::Dot(a, b);
#else
        return x * other.x + y * other.y + z * other.z;
#endif
    }

    // 벡터 크기
    float Magnitude() const {
#if USE_SIMD
        __m128 v = SIMD::LoadVec3(x, y, z);
        return sqrt(SIMD::Dot(v, v));
#else
        return sqrt(x * x + y * y + z * z);
#endif
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
        __m128 result = SIMD::VecMul(vec, scale);
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
    // 인덱스로 접근할 수 있도록 연산자 오버로딩
    float& operator[](int index)
    {
        switch (index)
        {
        case 0: return x;
        case 1: return y;
        case 2: return z;
        default: throw std::out_of_range("FVector index out of range");
        }
    }

    const float& operator[](int index) const
    {
       switch (index)
        {
        case 0: return x;
        case 1: return y;
        case 2: return z;
        default: throw std::out_of_range("FVector index out of range");
        }
    }
    float Distance(const FVector& other) const {
        // 두 벡터의 차 벡터의 크기를 계산
        return ((*this - other).Magnitude());
    }
    DirectX::XMFLOAT3 ToXMFLOAT3() const
    {
        return DirectX::XMFLOAT3(x, y, z);
    }
    static FVector Min(const FVector& A, const FVector& B)
    {
        return FVector(
            FMath::Min(A.x, B.x),
            FMath::Min(A.y, B.y),
            FMath::Min(A.z, B.z)
        );
    }

    static FVector Max(const FVector& A, const FVector& B)
    {
        return FVector(
            FMath::Max(A.x, B.x),
            FMath::Max(A.y, B.y),
            FMath::Max(A.z, B.z)
        );
    }
    static const FVector ZeroVector;
    static const FVector OneVector;
    static const FVector UpVector;
    static const FVector ForwardVector;
    static const FVector RightVector;
};
inline FVector FVector::operator/(const FVector& Other) const
{
#if USE_SIMD
    __m128 a = SIMD::LoadVec3(x, y, z);
    __m128 b = SIMD::LoadVec3(Other.x, Other.y, Other.z);
    __m128 result = SIMD::VecDivCorrect(a, b);
    float f[3];
    SIMD::StoreVec3(result, f);
    return FVector(f[0], f[1], f[2]);
#else
    return {x / Other.x, y / Other.y, z / Other.z};
#endif
}

inline FVector FVector::operator/(float Scalar) const
{
#if USE_SIMD
    __m128 vec = SIMD::LoadVec3(x, y, z);
    __m128 scale = _mm_set1_ps(Scalar);
    __m128 result = SIMD::VecDivCorrect(vec, scale);
    float f[3];
    SIMD::StoreVec3(result, f);
    return FVector(f[0], f[1], f[2]);
#else
    return {x / Scalar,  y / Scalar, z / Scalar};
#endif
}

inline FVector& FVector::operator/=(float Scalar)
{
#if USE_SIMD
    __m128 vec = SIMD::LoadVec3(x, y, z);
    __m128 scalarVec = _mm_set1_ps(Scalar);
    __m128 result = SIMD::VecDivCorrect(vec, scalarVec);
    float f[3];
    SIMD::StoreVec3(result, f);
    x = f[0]; y = f[1]; z = f[2];
    return *this;
#else
    x /= Scalar; y /= Scalar; z /= Scalar;
    return *this;
#endif
}