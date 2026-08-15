#include "CoordinateTransformUtility.h"

FVector FCoordinateTransformUtility::ToBlenderPosition(
    const FVector& InPosition) {
    // Y 轴取反（反射 M=diag(1,-1,1)）
    return FVector(InPosition.X, -InPosition.Y, InPosition.Z);
}

FQuat FCoordinateTransformUtility::ToBlenderRotation(const FQuat& InRotation) {
    // 反射共轭 M·R·M：保留 w、y，对 x、z 取反 → 四元数 (w, -x, y, -z)
    // FQuat 构造函数参数顺序为 (X, Y, Z, W)
    return FQuat(-InRotation.X, InRotation.Y, -InRotation.Z, InRotation.W);
}
