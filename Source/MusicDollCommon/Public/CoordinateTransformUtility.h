#pragma once

#include "CoreMinimal.h"

/**
 * 坐标变换工具 —— Unreal ↔ Blender 坐标系互转
 *
 * 对应 Blender 端 common/io_utils.py 的 to_unreal_position /
 * to_unreal_rotation。 Blender → Unreal 是 Y 轴反射变换 M=diag(1,-1,1)：
 *   - 位置：Y 轴取反（[x, y, z] → [x, -y, z]）
 *   - 旋转：四元数 [w,x,y,z] → (w, -x, y, -z)（反射共轭 M·R·M，保留 w、y，对
 * x、z 取反）
 *
 * 反射的逆变换等于其自身，因此 Unreal → Blender 使用同一反射。
 * 各乐器模块导出 Blender 格式时统一调用本工具，避免各自实现造成坐标不一致。
 */
class MUSICDOLLCOMMON_API FCoordinateTransformUtility {
   public:
    /** Unreal 位置 → Blender 位置：Y 轴取反 */
    static FVector ToBlenderPosition(const FVector& InPosition);

    /** Unreal 四元数 [w,x,y,z] → Blender 四元数：(w, -x, y, -z) */
    static FQuat ToBlenderRotation(const FQuat& InRotation);
};
