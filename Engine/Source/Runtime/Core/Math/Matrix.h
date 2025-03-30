#pragma once

#include <DirectXMath.h>

// 4x4 행렬 연산
struct FMatrix
{
    float M[4][4];
    static const FMatrix Identity;
    // 기본 연산자 오버로딩
    FMatrix operator+(const FMatrix& Other) const;
    FMatrix operator-(const FMatrix& Other) const;
    FMatrix operator*(const FMatrix& Other) const;
    FMatrix operator*(float Scalar) const;
    FMatrix operator/(float Scalar) const;
    float* operator[](int row);
    const float* operator[](int row) const;

    // 유틸리티 함수
    static FMatrix Transpose(const FMatrix& Mat);
    static float Determinant(const FMatrix& Mat);
    static FMatrix Inverse(const FMatrix& Mat);
    static FMatrix CreateRotation(float roll, float pitch, float yaw);
    static FMatrix CreateScale(float scaleX, float scaleY, float scaleZ);
    static FVector TransformVector(const FVector& v, const FMatrix& m);
    static FVector4 TransformVector(const FVector4& v, const FMatrix& m);
    static FMatrix CreateTranslationMatrix(const FVector& position);


    DirectX::XMMATRIX ToXMMATRIX() const
    {
        return DirectX::XMMatrixSet(
            M[0][0], M[1][0], M[2][0], M[3][0], // 첫 번째 열
            M[0][1], M[1][1], M[2][1], M[3][1], // 두 번째 열
            M[0][2], M[1][2], M[2][2], M[3][2], // 세 번째 열
            M[0][3], M[1][3], M[2][3], M[3][3]  // 네 번째 열
        );
    }
    FVector4 TransformFVector4(const FVector4& vector)
    {
#if USE_SIMD
        __m128 vec = SIMD::LoadVec4(vector);

        float x = SIMD::Dot(vec, Col1SIMD());
        float y = SIMD::Dot(vec, Col2SIMD());
        float z = SIMD::Dot(vec, Col3SIMD());
        float w = SIMD::Dot(vec, Col4SIMD());

        return FVector4(x, y, z, w);
#else
        return FVector4(
            M[0][0] * vector.x + M[1][0] * vector.y + M[2][0] * vector.z + M[3][0] * vector.a,
            M[0][1] * vector.x + M[1][1] * vector.y + M[2][1] * vector.z + M[3][1] * vector.a,
            M[0][2] * vector.x + M[1][2] * vector.y + M[2][2] * vector.z + M[3][2] * vector.a,
            M[0][3] * vector.x + M[1][3] * vector.y + M[2][3] * vector.z + M[3][3] * vector.a
        );
#endif
    }
    FVector TransformPosition(const FVector& vector) const
    {
#if USE_SIMD
        __m128 vec = SIMD::LoadVec4(vector.x, vector.y, vector.z, 1.f); // W = 1

        float x = SIMD::Dot(vec, Col1SIMD());
        float y = SIMD::Dot(vec, Col2SIMD());
        float z = SIMD::Dot(vec, Col3SIMD());
        float w = SIMD::Dot(vec, Col4SIMD());

        return w != 0.0f ? FVector{ x / w, y / w, z / w } : FVector{ x, y, z };
#else
        float x = M[0][0] * vector.x + M[1][0] * vector.y + M[2][0] * vector.z + M[3][0];
        float y = M[0][1] * vector.x + M[1][1] * vector.y + M[2][1] * vector.z + M[3][1];
        float z = M[0][2] * vector.x + M[1][2] * vector.y + M[2][2] * vector.z + M[3][2];
        float w = M[0][3] * vector.x + M[1][3] * vector.y + M[2][3] * vector.z + M[3][3];
        return w != 0.0f ? FVector{ x / w, y / w, z / w } : FVector{ x, y, z };
#endif
    }

    FVector4 Row1() const { return FVector4(M[0][0], M[0][1], M[0][2], M[0][3]); }
    FVector4 Row2() const { return FVector4(M[1][0], M[1][1], M[1][2], M[1][3]); }
    FVector4 Row3() const { return FVector4(M[2][0], M[2][1], M[2][2], M[2][3]); }
    FVector4 Row4() const { return FVector4(M[3][0], M[3][1], M[3][2], M[3][3]); }

    FVector4 Col1() const { return FVector4(M[0][0], M[1][0], M[2][0], M[3][0]); }
    FVector4 Col2() const { return FVector4(M[0][1], M[1][1], M[2][1], M[3][1]); }
    FVector4 Col3() const { return FVector4(M[0][2], M[1][2], M[2][2], M[3][2]); }
    FVector4 Col4() const { return FVector4(M[0][3], M[1][3], M[2][3], M[3][3]); }

#if USE_SIMD
    __m128 Row1SIMD() const { return _mm_loadu_ps(&M[0][0]); }
    __m128 Row2SIMD() const { return _mm_loadu_ps(&M[1][0]); }
    __m128 Row3SIMD() const { return _mm_loadu_ps(&M[2][0]); }
    __m128 Row4SIMD() const { return _mm_loadu_ps(&M[3][0]); }

    __m128 Col1SIMD() const { return _mm_set_ps(M[3][0], M[2][0], M[1][0], M[0][0]); }
    __m128 Col2SIMD() const { return _mm_set_ps(M[3][1], M[2][1], M[1][1], M[0][1]); }
    __m128 Col3SIMD() const { return _mm_set_ps(M[3][2], M[2][2], M[1][2], M[0][2]); }
    __m128 Col4SIMD() const { return _mm_set_ps(M[3][3], M[2][3], M[1][3], M[0][3]); }

    inline __m128 RowSIMD(int i) const {
        return _mm_loadu_ps(&M[i][0]);
    }

    inline __m128 ColSIMD(int j) const {
        return _mm_set_ps(M[3][j], M[2][j], M[1][j], M[0][j]);
    }
#endif
};