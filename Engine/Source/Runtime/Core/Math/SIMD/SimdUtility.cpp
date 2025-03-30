#include "SimdUtility.h"
#include "Core/Math/Vector.h"

#include "Core/Math/Vector.h"
#include "Core/Math/Vector4.h"

__m128 SIMD::LoadVec3(const FVector& v)
{
    return _mm_set_ps(0.f, v.z, v.y, v.x);
}

__m128 SIMD::LoadVec4(const FVector4& v)
{
    return _mm_set_ps(v.a, v.z, v.y, v.x);
}

void SIMD::StoreVec4(const __m128& v, FVector4& out)
{
    alignas(16) float temp[4];
    _mm_store_ps(temp, v);
    out.x = temp[0];
    out.y = temp[1];
    out.z = temp[2];
    out.a = temp[3];
}

void SIMD::Dot4_AoS(const FVector4* a, const FVector4* b, float* r)
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

void SIMD::ConvertAoSToSoA(const FVector4* src, FVector4SoA& dst)
{
    for (int i = 0; i < 4; ++i)
    {
        dst.x[i] = src[i].x;
        dst.y[i] = src[i].y;
        dst.z[i] = src[i].z;
        dst.w[i] = src[i].a;
    }
}

void SIMD::ConvertSoAToAoS(const FVector4SoA& src, FVector4* dst)
{
    for (int i = 0; i < 4; ++i)
    {
        dst[i].x = src.x[i];
        dst[i].y = src.y[i];
        dst[i].z = src.z[i];
        dst[i].a = src.w[i];
    }
}
