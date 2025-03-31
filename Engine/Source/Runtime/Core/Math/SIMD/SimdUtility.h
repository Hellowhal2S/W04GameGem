#pragma once

#include "Core/HAL/PlatformType.h"

#include <xmmintrin.h>
#include <smmintrin.h>

struct FVector;
struct FVector4;

namespace SIMD
{
#pragma
    // FVector Add (x, y, z)
    inline __m128 VecAdd(const __m128& a, const __m128& b)
    {
        return _mm_add_ps(a, b);
    }

    // FVector Sub
    inline __m128 VecSub(const __m128& a, const __m128& b)
    {
        return _mm_sub_ps(a, b);
    }

    // FVector Mul
    inline __m128 VecMul(const __m128& a, const __m128& b)
    {
        return _mm_mul_ps(a, b);
    }
    
    // FVector Div
    inline __m128 VecDivSlow(const __m128& a, const __m128& b)
    {
        return _mm_div_ps(a, b);
    }

    // Fast but, approximation
    inline __m128 VecDivFast(const __m128& a, const __m128& b)
    {
        __m128 rcp = _mm_rcp_ps(b);
        return _mm_mul_ps(a, rcp);
    }

    inline __m128 VecDivCorrect(const __m128& a, const __m128& b)
    {
        __m128 rcp = _mm_rcp_ps(b); // 근사 역수
        __m128 two = _mm_set1_ps(2.0f);
        __m128 refined = _mm_mul_ps(rcp, _mm_sub_ps(two, _mm_mul_ps(b, rcp))); // 보정 1회
        return _mm_mul_ps(a, refined);
    }

    // FVector Dot
    inline float Dot(const __m128& a, const __m128& b)
    {
        return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xFF));
    }

    // FVector Cross
    inline __m128 Vec3Cross(const __m128& a, const __m128& b)
    {
        __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)); // (y, z, x)
        __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)); // (y, z, x)

        __m128 c = _mm_sub_ps(_mm_mul_ps(a, b_yzx), _mm_mul_ps(a_yzx, b));

        return _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 2, 1)); // 다시 xyz 순서로 정렬
    }

    // FVector Normalize -> 근사값이라 사용 안할 예정
    //inline __m128 Vec3Normalize(const __m128& v)

    // Load
    inline __m128 LoadVec3(float x, float y, float z)
    {
        return _mm_set_ps(0.0f, z, y, x);
    }

    inline __m128 LoadVec4(float x, float y, float z, float w)
    {
        return _mm_set_ps(w, z, y, x);
    }

    __m128 LoadVec3(const FVector& v);
    __m128 LoadVec4(const FVector4& v);

    //FVector SIMD::ToFVector(const __m128& v)
    //{
    //    alignas(16) float temp[4];
    //    _mm_store_ps(temp, v);
    //    return FVector(temp[0], temp[1], temp[2]);
    //}

    // Store
    inline void StoreVec3(const __m128& v, float* out) // TODO: float* -> FVector로 변경
    {
        alignas(16) float temp[4];
        _mm_store_ps(temp, v);

        for (size_t i = 0; i < 3; i++)
            out[i] = temp[i];
    }

    inline void StoreVec4(const __m128& v, float* out)
    {
        alignas(16) float temp[4];
        _mm_store_ps(temp, v);

        for (size_t i = 0; i < 4; i++)
            out[i] = temp[i];
    }

    void StoreVec4(const __m128& v, FVector4& out);


    // Dot 4 pairs of 4D vectors (SoA format)
    inline void Dot4x4(const float* a, const float* b, float* r)
    {
        __m128 vaX = _mm_load_ps(&a[0 * 4]);
        __m128 vaY = _mm_load_ps(&a[1 * 4]);
        __m128 vaZ = _mm_load_ps(&a[2 * 4]);
        __m128 vaW = _mm_load_ps(&a[3 * 4]);

        __m128 vbX = _mm_load_ps(&b[0 * 4]);
        __m128 vbY = _mm_load_ps(&b[1 * 4]);
        __m128 vbZ = _mm_load_ps(&b[2 * 4]);
        __m128 vbW = _mm_load_ps(&b[3 * 4]);

        __m128 result = _mm_mul_ps(vaX, vbX);
        result = _mm_add_ps(result, _mm_mul_ps(vaY, vbY));
        result = _mm_add_ps(result, _mm_mul_ps(vaZ, vbZ));
        result = _mm_add_ps(result, _mm_mul_ps(vaW, vbW));

        _mm_store_ps(r, result);
    }

    void Dot4_AoS(const FVector4* a, const FVector4* b, float* r);

    // SoA 구조체
    struct FVector4SoA
    {
        float x[4];
        float y[4];
        float z[4];
        float w[4];
    };

    // AoS -> SoA 변환
    void ConvertAoSToSoA(const FVector4* src, FVector4SoA& dst);

    // SoA -> AoS 변환
    void ConvertSoAToAoS(const FVector4SoA& src, FVector4* dst);


}