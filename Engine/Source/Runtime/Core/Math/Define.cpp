#include "Define.h"

// 단위 행렬 정의
const FMatrix FMatrix::Identity = { {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1}
} };

// 행렬 덧셈
FMatrix FMatrix::operator+(const FMatrix& Other) const 
{
#if USE_SIMD
    FMatrix Result;
    float* r = &Result.M[0][0];

    __m128 row0 = SIMD::VecAdd(Row1SIMD(), Other.Row1SIMD());
    __m128 row1 = SIMD::VecAdd(Row2SIMD(), Other.Row2SIMD());
    __m128 row2 = SIMD::VecAdd(Row3SIMD(), Other.Row3SIMD());
    __m128 row3 = SIMD::VecAdd(Row4SIMD(), Other.Row4SIMD());

    _mm_storeu_ps(&r[0], row0);
    _mm_storeu_ps(&r[4], row1);
    _mm_storeu_ps(&r[8], row2);
    _mm_storeu_ps(&r[12], row3);

    return Result;
#else
    FMatrix Result;
    for (int32 i = 0; i < 4; i++)
        for (int32 j = 0; j < 4; j++)
            Result.M[i][j] = M[i][j] + Other.M[i][j];
    return Result;
#endif
}

// 행렬 뺄셈
FMatrix FMatrix::operator-(const FMatrix& Other) const 
{
#if USE_SIMD
    FMatrix Result;
    float* r = &Result.M[0][0];

    __m128 row0 = SIMD::VecSub(Row1SIMD(), Other.Row1SIMD());
    __m128 row1 = SIMD::VecSub(Row2SIMD(), Other.Row2SIMD());
    __m128 row2 = SIMD::VecSub(Row3SIMD(), Other.Row3SIMD());
    __m128 row3 = SIMD::VecSub(Row4SIMD(), Other.Row4SIMD());

    _mm_storeu_ps(&r[0], row0);
    _mm_storeu_ps(&r[4], row1);
    _mm_storeu_ps(&r[8], row2);
    _mm_storeu_ps(&r[12], row3);

    return Result;
#else
    FMatrix Result;
    for (int32 i = 0; i < 4; i++)
        for (int32 j = 0; j < 4; j++)
            Result.M[i][j] = M[i][j] - Other.M[i][j];
    return Result;
#endif
}

// 행렬 곱셈
FMatrix FMatrix::operator*(const FMatrix& Other) const 
{
#if USE_SIMD
    FMatrix Result;
    for (int i = 0; i < 4; ++i)
    {
        __m128 row = RowSIMD(i);

        for (int j = 0; j < 4; ++j)
        {
            __m128 col = Other.ColSIMD(j);
            Result.M[i][j] = SIMD::Dot(row, col);
        }
    }
    return Result;
#else
    FMatrix Result = {};
    for (int32 i = 0; i < 4; i++)
        for (int32 j = 0; j < 4; j++)
            for (int32 k = 0; k < 4; k++)
                Result.M[i][j] += M[i][k] * Other.M[k][j];
    return Result;
#endif
}

// 스칼라 곱셈
FMatrix FMatrix::operator*(float Scalar) const 
{
#if USE_SIMD
    FMatrix Result;
    float* r = &Result.M[0][0];
    __m128 scalar = _mm_set1_ps(Scalar);

    _mm_storeu_ps(&r[0], _mm_mul_ps(Row1SIMD(), scalar));
    _mm_storeu_ps(&r[4], _mm_mul_ps(Row2SIMD(), scalar));
    _mm_storeu_ps(&r[8], _mm_mul_ps(Row3SIMD(), scalar));
    _mm_storeu_ps(&r[12], _mm_mul_ps(Row4SIMD(), scalar));

    return Result;
#else
    FMatrix Result;
    for (int32 i = 0; i < 4; i++)
        for (int32 j = 0; j < 4; j++)
            Result.M[i][j] = M[i][j] * Scalar;
    return Result;
#endif
}

// 스칼라 나눗셈
FMatrix FMatrix::operator/(float Scalar) const 
{
#if USE_SIMD
    FMatrix Result;
    float* r = &Result.M[0][0];
    __m128 scalar = _mm_set1_ps(Scalar);

    _mm_storeu_ps(&r[0], _mm_div_ps(Row1SIMD(), scalar));
    _mm_storeu_ps(&r[4], _mm_div_ps(Row2SIMD(), scalar));
    _mm_storeu_ps(&r[8], _mm_div_ps(Row3SIMD(), scalar));
    _mm_storeu_ps(&r[12], _mm_div_ps(Row4SIMD(), scalar));

    return Result;
#else
    FMatri
    FMatrix Result;
    for (int32 i = 0; i < 4; i++)
        for (int32 j = 0; j < 4; j++)
            Result.M[i][j] = M[i][j] / Scalar;
    return Result;
#endif
}

float* FMatrix::operator[](int row) {
    return M[row];
}

const float* FMatrix::operator[](int row) const
{
    return M[row];
}

// 전치 행렬
FMatrix FMatrix::Transpose(const FMatrix& Mat) 
{
#if USE_SIMD
    FMatrix Result;

    __m128 row0 = _mm_loadu_ps(&Mat.M[0][0]);
    __m128 row1 = _mm_loadu_ps(&Mat.M[1][0]);
    __m128 row2 = _mm_loadu_ps(&Mat.M[2][0]);
    __m128 row3 = _mm_loadu_ps(&Mat.M[3][0]);

    _MM_TRANSPOSE4_PS(row0, row1, row2, row3);

    _mm_storeu_ps(&Result.M[0][0], row0);
    _mm_storeu_ps(&Result.M[1][0], row1);
    _mm_storeu_ps(&Result.M[2][0], row2);
    _mm_storeu_ps(&Result.M[3][0], row3);

    return Result;
#else
    FMatrix Result;
    for (int32 i = 0; i < 4; i++)
        for (int32 j = 0; j < 4; j++)
            Result.M[i][j] = Mat.M[j][i];
    return Result;
#endif
}

// 행렬식 계산 (라플라스 전개, 4x4 행렬)
float FMatrix::Determinant(const FMatrix& Mat) {
    float det = 0.0f;
    for (int32 i = 0; i < 4; i++) {
        float subMat[3][3];
        for (int32 j = 1; j < 4; j++) {
            int32 colIndex = 0;
            for (int32 k = 0; k < 4; k++) {
                if (k == i) continue;
                subMat[j - 1][colIndex] = Mat.M[j][k];
                colIndex++;
            }
        }
        float minorDet =
            subMat[0][0] * (subMat[1][1] * subMat[2][2] - subMat[1][2] * subMat[2][1]) -
            subMat[0][1] * (subMat[1][0] * subMat[2][2] - subMat[1][2] * subMat[2][0]) +
            subMat[0][2] * (subMat[1][0] * subMat[2][1] - subMat[1][1] * subMat[2][0]);
        det += (i % 2 == 0 ? 1 : -1) * Mat.M[0][i] * minorDet;
    }
    return det;
}

// 역행렬 (가우스-조던 소거법)
FMatrix FMatrix::Inverse(const FMatrix& Mat) {
    float det = Determinant(Mat);
    if (fabs(det) < 1e-6) {
        return Identity;
    }

    FMatrix Inv;
    float invDet = 1.0f / det;

    // 여인수 행렬 계산 후 전치하여 역행렬 계산
    for (int32 i = 0; i < 4; i++) {
        for (int32 j = 0; j < 4; j++) {
            float subMat[3][3];
            int32 subRow = 0;
            for (int32 r = 0; r < 4; r++) {
                if (r == i) continue;
                int32 subCol = 0;
                for (int32 c = 0; c < 4; c++) {
                    if (c == j) continue;
                    subMat[subRow][subCol] = Mat.M[r][c];
                    subCol++;
                }
                subRow++;
            }
            float minorDet =
                subMat[0][0] * (subMat[1][1] * subMat[2][2] - subMat[1][2] * subMat[2][1]) -
                subMat[0][1] * (subMat[1][0] * subMat[2][2] - subMat[1][2] * subMat[2][0]) +
                subMat[0][2] * (subMat[1][0] * subMat[2][1] - subMat[1][1] * subMat[2][0]);

            Inv.M[j][i] = ((i + j) % 2 == 0 ? 1 : -1) * minorDet * invDet;
        }
    }
    return Inv;
}

FMatrix FMatrix::CreateRotation(float roll, float pitch, float yaw)
{
    float radRoll = roll * (3.14159265359f / 180.0f);
    float radPitch = pitch * (3.14159265359f / 180.0f);
    float radYaw = yaw * (3.14159265359f / 180.0f);

    float cosRoll = cos(radRoll), sinRoll = sin(radRoll);
    float cosPitch = cos(radPitch), sinPitch = sin(radPitch);
    float cosYaw = cos(radYaw), sinYaw = sin(radYaw);

    // Z축 (Yaw) 회전
    FMatrix rotationZ = { {
        { cosYaw, sinYaw, 0, 0 },
        { -sinYaw, cosYaw, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    } };

    // Y축 (Pitch) 회전
    FMatrix rotationY = { {
        { cosPitch, 0, -sinPitch, 0 },
        { 0, 1, 0, 0 },
        { sinPitch, 0, cosPitch, 0 },
        { 0, 0, 0, 1 }
    } };

    // X축 (Roll) 회전
    FMatrix rotationX = { {
        { 1, 0, 0, 0 },
        { 0, cosRoll, sinRoll, 0 },
        { 0, -sinRoll, cosRoll, 0 },
        { 0, 0, 0, 1 }
    } };

    // DirectX 표준 순서: Z(Yaw) → Y(Pitch) → X(Roll)  
    return rotationX * rotationY * rotationZ;  // 이렇게 하면  오른쪽 부터 적용됨
}


// 스케일 행렬 생성
FMatrix FMatrix::CreateScale(float scaleX, float scaleY, float scaleZ)
{
    return { {
        { scaleX, 0, 0, 0 },
        { 0, scaleY, 0, 0 },
        { 0, 0, scaleZ, 0 },
        { 0, 0, 0, 1 }
    } };
}

FMatrix FMatrix::CreateTranslationMatrix(const FVector& position)
{
    FMatrix translationMatrix = FMatrix::Identity;
    translationMatrix.M[3][0] = position.x;
    translationMatrix.M[3][1] = position.y;
    translationMatrix.M[3][2] = position.z;
    return translationMatrix;
}

// TODO 이거 w = 0으로 처리 하면 안될거같은데
FVector FMatrix::TransformVector(const FVector& v, const FMatrix& m)
{
#if USE_SIMD
    __m128 vec = SIMD::LoadVec3(v);

    float x = SIMD::Dot(vec, m.Col1SIMD());
    float y = SIMD::Dot(vec, m.Col2SIMD());
    float z = SIMD::Dot(vec, m.Col3SIMD());

    return FVector(x, y, z);
#else
    FVector result;

    // 4x4 행렬을 사용하여 벡터 변환 (W = 0으로 가정, 방향 벡터)
    result.x = v.x * m.M[0][0] + v.y * m.M[1][0] + v.z * m.M[2][0] + 0.0f * m.M[3][0];
    result.y = v.x * m.M[0][1] + v.y * m.M[1][1] + v.z * m.M[2][1] + 0.0f * m.M[3][1];
    result.z = v.x * m.M[0][2] + v.y * m.M[1][2] + v.z * m.M[2][2] + 0.0f * m.M[3][2];
    return result;
#endif
}

// FVector4를 변환하는 함수
FVector4 FMatrix::TransformVector(const FVector4& v, const FMatrix& m)
{
#if USE_SIMD
    __m128 vec = SIMD::LoadVec4(v);

    float x = SIMD::Dot(vec, m.Col1SIMD());
    float y = SIMD::Dot(vec, m.Col2SIMD());
    float z = SIMD::Dot(vec, m.Col3SIMD());
    float w = SIMD::Dot(vec, m.Col4SIMD());

    return FVector4(x, y, z, w);
#else
    FVector4 result;
    result.x = v.x * m.M[0][0] + v.y * m.M[1][0] + v.z * m.M[2][0] + v.a * m.M[3][0];
    result.y = v.x * m.M[0][1] + v.y * m.M[1][1] + v.z * m.M[2][1] + v.a * m.M[3][1];
    result.z = v.x * m.M[0][2] + v.y * m.M[1][2] + v.z * m.M[2][2] + v.a * m.M[3][2];
    result.a = v.x * m.M[0][3] + v.y * m.M[1][3] + v.z * m.M[2][3] + v.a * m.M[3][3];

    return result;
#endif
}
