#pragma once

#include "Core/HAL/PlatformType.h"
//#include "Core/Math/Vector.h"
#include "Core/Math/Vector4.h"

#include <xmmintrin.h>
#include <smmintrin.h>

namespace SIMD
{
    // FVector Add (x, y, z)
    inline __m128 Vec3Add(const __m128& a, const __m128& b)
    {
        return _mm_add_ps(a, b);
    }

    // FVector Sub
    inline __m128 Vec3Sub(const __m128& a, const __m128& b)
    {
        return _mm_sub_ps(a, b);
    }

    // FVector Mul
    inline __m128 Vec3Mul(const __m128& a, const __m128& b)
    {
        return _mm_mul_ps(a, b);
    }

    // FVector Dot
    inline float Vec3Dot(const __m128& a, const __m128& b)
    {
        return _mm_cvtss_f32(_mm_dp_ps(a, b, 0x71));
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

    inline void Dot4_AoS(const FVector4* a, const FVector4* b, float* r)
    {
        __m128 aX = _mm_set_ps(a[3].x, a[2].x, a[1].x, a[0].x);
        __m128 aY = _mm_set_ps(a[3].y, a[2].y, a[1].y, a[0].y);
        __m128 aZ = _mm_set_ps(a[3].z, a[2].z, a[1].z, a[0].z);
        __m128 aW = _mm_set_ps(a[3].a, a[2].a, a[1].a, a[0].a);

        __m128 bX = _mm_set_ps(b[3].x, b[2].x, b[1].x, b[0].x);
        __m128 bY = _mm_set_ps(b[3].y, b[2].y, b[1].y, b[0].y);
        __m128 bZ = _mm_set_ps(b[3].z, b[2].z, b[1].z, b[0].z);
        __m128 bW = _mm_set_ps(b[3].a, b[2].a, b[1].a, b[0].a);

        __m128 result = _mm_mul_ps(aX, bX);
        result = _mm_add_ps(result, _mm_mul_ps(aY, bY));
        result = _mm_add_ps(result, _mm_mul_ps(aZ, bZ));
        result = _mm_add_ps(result, _mm_mul_ps(aW, bW));

        _mm_storeu_ps(r, result);
    }

    // SoA 구조체
    struct FVector4SoA
    {
        float x[4];
        float y[4];
        float z[4];
        float w[4];
    };

    // AoS -> SoA 변환
    inline void ConvertAoSToSoA(const FVector4* src, FVector4SoA& dst)
    {
        for (int i = 0; i < 4; ++i)
        {
            dst.x[i] = src[i].x;
            dst.y[i] = src[i].y;
            dst.z[i] = src[i].z;
            dst.w[i] = src[i].a;
        }
    }

    // SoA -> AoS 변환
    inline void ConvertSoAToAoS(const FVector4SoA& src, FVector4* dst)
    {
        for (int i = 0; i < 4; ++i)
        {
            dst[i].x = src.x[i];
            dst[i].y = src.y[i];
            dst[i].z = src.z[i];
            dst[i].a = src.w[i];
        }
    }

}