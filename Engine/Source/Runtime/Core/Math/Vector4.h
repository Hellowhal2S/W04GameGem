#pragma once

#include "SIMD/SimdUtility.h"

// 4D Vector
struct FVector4 {
    float x, y, z, a;
    FVector4(float _x = 0, float _y = 0, float _z = 0, float _a = 0) : x(_x), y(_y), z(_z), a(_a) {}

    FVector4 operator-(const FVector4& other) const {
#if USE_SIMD
        __m128 a = SIMD::LoadVec4(*this);
        __m128 b = SIMD::LoadVec4(other);
        __m128 result = SIMD::VecSub(a, b);
        FVector4 out;
        SIMD::StoreVec4(result, out);
        return out;
#else
        return FVector4(x - other.x, y - other.y, z - other.z, a - other.a);
#endif
    }
    FVector4 operator+(const FVector4& other) const {
#if USE_SIMD
        __m128 a = SIMD::LoadVec4(*this);
        __m128 b = SIMD::LoadVec4(other);
        __m128 result = SIMD::VecAdd(a, b);
        FVector4 out;
        SIMD::StoreVec4(result, out);
        return out;
#else
        return FVector4(x + other.x, y + other.y, z + other.z, a + other.a);
#endif
    }
    FVector4 operator/(float scalar) const
    {
#if USE_SIMD
        __m128 vec = SIMD::LoadVec4(*this);
        __m128 scalarVec = _mm_set1_ps(scalar);
        __m128 result = SIMD::VecDivCorrect(vec, scalarVec);
        FVector4 out;
        SIMD::StoreVec4(result, out);
        return out;
#else
        return FVector4{ x / scalar, y / scalar, z / scalar, a / scalar };
#endif
    }

    float Dot(const FVector4& other) const
    {
#if USE_SIMD
        __m128 lhs = SIMD::LoadVec4(x, y, z, a);
        __m128 rhs = SIMD::LoadVec4(other.x, other.y, other.z, other.a);
        return SIMD::Dot(lhs, rhs);
#else
        return x * other.x + y * other.y + z * other.z + a * other.a;
#endif
    }
};
