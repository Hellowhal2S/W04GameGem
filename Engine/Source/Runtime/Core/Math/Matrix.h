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

    // 사용 안하면 삭제
//    FVector4 TransformFVector4(const FVector4& vector)
//    {
//#if USE_SIMD
//        __m128 vec = SIMD::LoadVec4(vector);
//
//        float x = SIMD::Dot(vec, Col1SIMD());
//        float y = SIMD::Dot(vec, Col2SIMD());
//        float z = SIMD::Dot(vec, Col3SIMD());
//        float w = SIMD::Dot(vec, Col4SIMD());
//
//        return FVector4(x, y, z, w);
//#else
//        return FVector4(
//            M[0][0] * vector.x + M[1][0] * vector.y + M[2][0] * vector.z + M[3][0] * vector.a,
//            M[0][1] * vector.x + M[1][1] * vector.y + M[2][1] * vector.z + M[3][1] * vector.a,
//            M[0][2] * vector.x + M[1][2] * vector.y + M[2][2] * vector.z + M[3][2] * vector.a,
//            M[0][3] * vector.x + M[1][3] * vector.y + M[2][3] * vector.z + M[3][3] * vector.a
//        );
//#endif
//    }

    FVector TransformPosition(const FVector& vector) const
    {
#if USE_SIMD
        __m128 b0 = _mm_loadu_ps(&this->M[0][0]);  // Row 0
        __m128 b1 = _mm_loadu_ps(&this->M[1][0]);  // Row 1
        __m128 b2 = _mm_loadu_ps(&this->M[2][0]);  // Row 2
        __m128 b3 = _mm_loadu_ps(&this->M[3][0]);  // Row 3

        __m128 a = _mm_set_ps(1.f, vector.z, vector.y, vector.x);  // (x, y, z, 1)

        __m128 e0 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 e1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1));
        __m128 e2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2));
        __m128 e3 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3));

        __m128 r = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(e0, b0), _mm_mul_ps(e1, b1)),
            _mm_add_ps(_mm_mul_ps(e2, b2), _mm_mul_ps(e3, b3))
        );

        // 결과를 저장하고 w로 나누기
        alignas(16) float temp[4];
        _mm_store_ps(temp, r);

        float w = temp[3];
        return w != 0.0f
            ? FVector(temp[0] / w, temp[1] / w, temp[2] / w)
            : FVector(temp[0], temp[1], temp[2]);
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

    FVector GetColumn(int columnIndex) const
	{
	    if (columnIndex < 0 || columnIndex > 3)
	        return FVector::ZeroVector;

	    return FVector{
	        M[0][columnIndex],
            M[1][columnIndex],
            M[2][columnIndex]
        };
	}
};
