#include "Nodes/BlenderStyleIK.h"

#include "Algo/Reverse.h"
#include "AnimationCore.h"
#include "ControlRig.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlenderStyleIK)

namespace BlenderIKImpl {
// ============================================================
// 复刻 Blender intern/iksolver 的雅可比 + SDLS 求解器
// 参考实现：
//   IK_QTask.cpp         - IK_QPositionTask / IK_QOrientationTask
//   IK_QJacobian.cpp     - Invert / InvertSDLS
//   IK_QSegment.cpp      - IK_QSphericalSegment::UpdateAngle
// ============================================================

constexpr int32 MaxBones = 24;  // 最多 24 根骨骼
constexpr int32 MaxDoF = MaxBones * 3;
constexpr int32 MaxTasks = 6;  // 任务行数上限（当前只用位置任务 3 行）

struct FBlenderSolver {
    int32 NumTasks = 0;  // m：任务行数（3 或 6）
    int32 NumDoF = 0;    // n：自由度数量（骨骼数 * 3）

    // 雅可比矩阵 J（m x n）与残差 beta（m）
    double J[MaxTasks][MaxDoF] = {};
    double Beta[MaxTasks] = {};

    // 求解结果：角度增量 d_theta（n）
    double DTheta[MaxDoF] = {};

    // 每个 dof 的列范数（用于 SDLS 的 M）
    double NormPerDoF[MaxDoF] = {};

    // 每个 dof 的权重（默认 1）
    double Weight[MaxDoF] = {};
    double WeightSqrt[MaxDoF] = {};

    // SVD(J J^T) 的结果：U(m x m)、V thin(n x m)、奇异值 W(m)
    double SvdU[MaxTasks][MaxTasks] = {};
    double SvdV[MaxDoF][MaxTasks] = {};
    double SvdW[MaxTasks] = {};

    // SDLS 单步角度上限（Blender 的 max_angle_change = PI/4）
    double MaxAngleStep = PI / 4.0;

    void ArmMatrices(int32 InTasks, int32 InDoF) {
        NumTasks = InTasks;
        NumDoF = InDoF;
        FMemory::Memzero(&J[0][0], sizeof(J));
        FMemory::Memzero(&Beta[0], sizeof(Beta));
        FMemory::Memzero(&DTheta[0], sizeof(DTheta));
        FMemory::Memzero(&NormPerDoF[0], sizeof(NormPerDoF));
        for (int32 i = 0; i < NumDoF; ++i) {
            Weight[i] = 1.0;
            WeightSqrt[i] = 1.0;
        }
    }

    void SetBetaRow(int32 InRow, const FVector& InV) {
        if (InRow + 2 < MaxTasks) {
            Beta[InRow + 0] = InV.X;
            Beta[InRow + 1] = InV.Y;
            Beta[InRow + 2] = InV.Z;
        }
    }

    void SetJacobianRow(int32 InRow, int32 InDoF, const FVector& InV) {
        if (InRow + 2 < MaxTasks && InDoF >= 0 && InDoF < NumDoF) {
            J[InRow + 0][InDoF] = InV.X;
            J[InRow + 1][InDoF] = InV.Y;
            J[InRow + 2][InDoF] = InV.Z;
        }
    }

    // ----------------------------------------------------------
    // SVD：对对称半正定矩阵 G = J * J^T（m x m）做 Jacobi 特征分解
    // 得到 G = U * diag(W^2) * U^T，再由 V = J^T * U / W 得到 thin V。
    // 数学上：J = U * diag(W) * V^T，因此 V 列 i = J^T * U 列 i / W[i]。
    // m 只有 3 或 6，Jacobi 迭代收敛极快且稳定。
    // ----------------------------------------------------------
    void ComputeSVD() {
        double G[MaxTasks][MaxTasks] = {};

        for (int32 i = 0; i < NumTasks; ++i) {
            for (int32 j = 0; j < NumTasks; ++j) {
                double S = 0.0;
                for (int32 k = 0; k < NumDoF; ++k) {
                    S += J[i][k] * J[j][k];
                }
                G[i][j] = S;
            }
        }

        // U = I
        for (int32 i = 0; i < NumTasks; ++i) {
            for (int32 j = 0; j < NumTasks; ++j) {
                SvdU[i][j] = (i == j) ? 1.0 : 0.0;
            }
        }

        constexpr int32 MaxJacobiIter = 80;
        constexpr double Eps = 1e-12;

        for (int32 Iter = 0; Iter < MaxJacobiIter; ++Iter) {
            bool bConverged = true;

            for (int32 p = 0; p < NumTasks; ++p) {
                for (int32 q = p + 1; q < NumTasks; ++q) {
                    const double Gpp = G[p][p];
                    const double Gqq = G[q][q];
                    const double Gpq = G[p][q];

                    if (FMath::Abs(Gpq) <= Eps * FMath::Sqrt(Gpp * Gqq)) {
                        continue;
                    }

                    bConverged = false;

                    const double Tau = (Gqq - Gpp) / (2.0 * Gpq);
                    const double T =
                        (Tau >= 0.0 ? 1.0 : -1.0) /
                        (FMath::Abs(Tau) + FMath::Sqrt(1.0 + Tau * Tau));
                    const double C = 1.0 / FMath::Sqrt(1.0 + T * T);
                    const double S = T * C;

                    // 更新 G 的对角与 p,q 元素
                    G[p][p] = Gpp - T * Gpq;
                    G[q][q] = Gqq + T * Gpq;
                    G[p][q] = G[q][p] = 0.0;

                    // 更新其余行/列
                    for (int32 k = 0; k < NumTasks; ++k) {
                        if (k == p || k == q) {
                            continue;
                        }
                        const double Gkp = G[k][p];
                        const double Gkq = G[k][q];
                        G[k][p] = G[p][k] = C * Gkp - S * Gkq;
                        G[k][q] = G[q][k] = S * Gkp + C * Gkq;
                    }

                    // 累积特征向量到 U
                    for (int32 k = 0; k < NumTasks; ++k) {
                        const double Ukp = SvdU[k][p];
                        const double Ukq = SvdU[k][q];
                        SvdU[k][p] = C * Ukp - S * Ukq;
                        SvdU[k][q] = S * Ukp + C * Ukq;
                    }
                }
            }

            if (bConverged) {
                break;
            }
        }

        // 奇异值
        for (int32 i = 0; i < NumTasks; ++i) {
            SvdW[i] = FMath::Sqrt(FMath::Max(G[i][i], 0.0));
        }

        // thin V（n x m）
        FMemory::Memzero(&SvdV[0][0], sizeof(SvdV));
        for (int32 i = 0; i < NumTasks; ++i) {
            if (SvdW[i] <= 1e-10) {
                continue;
            }
            const double InvW = 1.0 / SvdW[i];
            for (int32 j = 0; j < NumDoF; ++j) {
                double S = 0.0;
                for (int32 k = 0; k < NumTasks; ++k) {
                    S += J[k][j] * SvdU[k][i];
                }
                SvdV[j][i] = S * InvW;
            }
        }
    }

    // ----------------------------------------------------------
    // 选择性阻尼最小二乘（SDLS），逐奇异值阻尼，对应 Blender 的
    // IK_QJacobian::InvertSDLS。最小范数解会把末端修正均匀分摊到
    // 所有能产生该效果的骨骼上，这正是"每根骨骼弯曲都很平均"的原因。
    // ----------------------------------------------------------
    void InvertSDLS() {
        // 每 dof 的列范数（按每 3 行一组，即每个任务块的 x/y/z）
        for (int32 j = 0; j < NumDoF; ++j) {
            NormPerDoF[j] = 0.0;
            for (int32 Row = 0; Row + 2 < NumTasks; Row += 3) {
                double N = 0.0;
                N += J[Row + 0][j] * J[Row + 0][j];
                N += J[Row + 1][j] * J[Row + 1][j];
                N += J[Row + 2][j] * J[Row + 2][j];
                NormPerDoF[j] += FMath::Sqrt(N);
            }
        }

        FMemory::Memzero(&DTheta[0], sizeof(DTheta));

        constexpr double Epsilon = 1e-10;
        const double MaxAngle = MaxAngleStep;

        for (int32 i = 0; i < NumTasks; ++i) {
            const double W = SvdW[i];
            if (W <= Epsilon) {
                continue;
            }
            const double WInv = 1.0 / W;

            // alpha = (U 列 i) · beta / W
            double Alpha = 0.0;
            for (int32 j = 0; j < NumTasks; ++j) {
                Alpha += SvdU[j][i] * Beta[j];
            }
            Alpha *= WInv;

            // N = 每 3 行一组的 U 列范数之和
            double N = 0.0;
            for (int32 Row = 0; Row + 2 < NumTasks; Row += 3) {
                double UBlock = 0.0;
                UBlock += SvdU[Row + 0][i] * SvdU[Row + 0][i];
                UBlock += SvdU[Row + 1][i] * SvdU[Row + 1][i];
                UBlock += SvdU[Row + 2][i] * SvdU[Row + 2][i];
                N += FMath::Sqrt(UBlock);
            }

            // M = Σ_j |V[j][i]| * NormPerDoF[j]
            double M = 0.0;
            for (int32 j = 0; j < NumDoF; ++j) {
                M += FMath::Abs(SvdV[j][i]) * NormPerDoF[j];
            }
            M *= WInv;

            // gamma：限制该奇异值方向上的角度变化
            double Gamma = MaxAngle;
            if (N < M) {
                Gamma *= N / M;
            }

            // max_dtheta：该方向上的最大角度分量
            double Tmp[MaxDoF];
            double MaxDTheta = 0.0;
            for (int32 j = 0; j < NumDoF; ++j) {
                Tmp[j] = SvdV[j][i] * Alpha;
                MaxDTheta =
                    FMath::Max(MaxDTheta, FMath::Abs(Tmp[j]) * WeightSqrt[j]);
            }

            const double Damp = (Gamma < MaxDTheta) ? Gamma / MaxDTheta : 1.0;

            for (int32 j = 0; j < NumDoF; ++j) {
                // 0.80 系数（Blender 原版）。
                // 不收敛/发散的根本原因是角度应用方式（左乘 vs 右乘），
                // 不是阻尼系数。仿真对比：0.30/0.80 左乘都从 ~0.3 振荡
                // 发散到 8+；改为 Blender 的右乘局部后 0.80 收敛到
                // 0.002 @ iter16。不可达已由"太远分流"提前拉直，不会进入
                // 迭代，因此无需保守降低阻尼。
                double DoFDamp = Damp / Weight[j];
                DoFDamp = FMath::Min(DoFDamp, 1.0);
                DTheta[j] += 0.80 * DoFDamp * Tmp[j];
            }
        }

        // 每 dof 权重 + 全局角度上限
        double MaxAngle2 = 0.0;
        for (int32 j = 0; j < NumDoF; ++j) {
            DTheta[j] *= Weight[j];
            MaxAngle2 = FMath::Max(MaxAngle2, FMath::Abs(DTheta[j]));
        }

        if (MaxAngle2 > MaxAngle) {
            const double Damp = MaxAngle / (MaxAngle + MaxAngle2);
            for (int32 j = 0; j < NumDoF; ++j) {
                DTheta[j] *= Damp;
            }
        }
    }

    void Solve() {
        ComputeSVD();
        InvertSDLS();
    }
};
}  // namespace BlenderIKImpl

// ================================================================
// 求解主逻辑
// ================================================================

FRigUnit_BlenderStyleIK_Execute() {
    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy) {
        return;
    }

    // ---- 从末端骨骼沿父级链向上构建骨骼链（根在最前、末端在最后）----
    TArray<FCachedRigElement> CachedBones;
    CachedBones.Reserve(ChainLength);

    {
        int32 CurIndex = Hierarchy->GetIndex(EndBone);
        for (int32 i = 0; i < ChainLength && CurIndex != INDEX_NONE; ++i) {
            const FRigElementKey Key = Hierarchy->GetKey(CurIndex);
            if (!Hierarchy->Find<FRigBoneElement>(Key)) {
                return;
            }
            CachedBones.Add(FCachedRigElement(Key, Hierarchy, true));
            CurIndex = Hierarchy->GetFirstParent(CurIndex);
        }
        // 反转：根在最前、末端在最后
        Algo::Reverse(CachedBones);
    }

    const int32 NumBones = CachedBones.Num();
    if (NumBones < 2) {
        return;
    }

    if (NumBones > BlenderIKImpl::MaxBones) {
        return;
    }

    // ---- 读取当前全局姿态并记录初始值（用于 Weight 插值）----
    TArray<FTransform> InitGlobal;
    TArray<FVector> Pos;
    TArray<FQuat> Rot;
    TArray<FVector> Scale;
    TArray<FVector> LocalOffset;
    TArray<FQuat> Basis;  // 每根骨骼的局部旋转（相对父），Blender 的 m_basis

    InitGlobal.SetNum(NumBones);
    Pos.SetNum(NumBones);
    Rot.SetNum(NumBones);
    Scale.SetNum(NumBones);
    LocalOffset.SetNum(NumBones - 1);
    Basis.SetNum(NumBones);

    for (int32 i = 0; i < NumBones; ++i) {
        InitGlobal[i] = Hierarchy->GetGlobalTransform(CachedBones[i].GetKey());
        Pos[i] = InitGlobal[i].GetLocation();
        Rot[i] = InitGlobal[i].GetRotation().GetNormalized();
        Scale[i] = InitGlobal[i].GetScale3D();
    }

    // 【关键修复】LocalOffset 必须在所有 Pos 都赋值之后再计算！
    // 原实现把它放在同一个循环里，导致计算 LocalOffset[i] 时 Pos[i+1]
    // 尚未被后续循环轮次读取（还是 (0,0,0)），于是：
    //   LocalOffset[i] = Rot[i]^-1 * ((0,0,0) - Pos[i]) = -Rot[i]^-1 * Pos[i]
    // 其长度 = |Pos[i]|（从世界原点到骨骼的距离，约 107），而非真实骨长
    // （约 2.4，见 GetLocalTransform 的 LPos）。这使 chainLen 虚高到 322、
    // 位置级联完全失真，求解第一步就把链甩飞。拆成独立循环后：
    //   LocalOffset[i] = Rot[i]^-1 * (Pos[i+1] - Pos[i])
    // 长度 = 相邻骨骼真实距离（骨长），求解恢复正常。
    for (int32 i = 0; i < NumBones - 1; ++i) {
        LocalOffset[i] = Rot[i].Inverse().RotateVector(Pos[i + 1] - Pos[i]);
    }

    // 初始化局部旋转（Blender 的 m_basis）：G_i = G_{i-1} * B_i
    //   -> B_0 = G_0（根无父，全局即局部）
    //   -> B_i = G_{i-1}^{-1} * G_i
    Basis[0] = Rot[0];
    for (int32 i = 1; i < NumBones; ++i) {
        Basis[i] = (Rot[i - 1].Inverse() * Rot[i]).GetNormalized();
    }

    const FVector GoalPosition = EffectorTransform.GetLocation();
    const FQuat GoalRotation = EffectorTransform.GetRotation();

    // ---- 极点（PoleTarget）----
    // 不在求解前预旋转（预旋转会把链扭成病态构型、导致求解发散或末端
    // 无法到达 Effector），而是在位置求解（雅可比迭代）完成之后再沿
    // root->end 轴整体旋转整条链（见下方 "Pole 后旋转"）。根与末端都在
    // 旋转轴上，位置不变；只有链的弯曲平面朝向改变。

    // ---- 配置求解器 ----
    BlenderIKImpl::FBlenderSolver Solver;

    // 纯位置任务（已移除 bUseOrientationTask 朝向任务）：
    // 末端朝向由求解后的 bPropagateToChildren 后处理控制。
    const int32 NumTasks = 3;
    const int32 NumDoF = NumBones * 3;

    Solver.MaxAngleStep =
        FMath::DegreesToRadians(MaxAnglePerStep > 0.f ? MaxAnglePerStep : 45.f);
    Solver.ArmMatrices(NumTasks, NumDoF);

    // 位置任务残差单步 clamp（Blender 位置任务构造函数里的 m_clamp_length）
    float TotalLength = 0.0f;
    for (int32 i = 0; i < NumBones - 1; ++i) {
        TotalLength += LocalOffset[i].Size();
    }
    const float ClampLength = TotalLength / (2.0f * NumBones);

    // 收敛阈值 = 用户设定的 Precision：表示"末端离 effector 的距离达标线"，
    // 迭代结果距离小于它就停止迭代。默认 1e-3。
    // 注意：直接使用用户精度，不做 min() 裁剪——否则会覆盖用户的精度意图。
    const double ConvergeThreshold =
        (Precision > 0.f) ? (double)Precision : 1e-3;

    int32 MaxIter = MaxIterations > 0 ? MaxIterations : 10;

    // ---- 可达性分流（参考 ArcDistributedIK 的 Algorithm Branching）----
    // 先算根到目标的距离与链长：目标不可达（太远）时直接把链拉直朝目标，
    // 不进入雅可比迭代——链完全拉直时雅可比病态（秩≈1），SDLS 会振荡发散
    // （目标不可达时 dist 从 0.30 飙到 2.5 的根因）。提前分流可稳定停在
    // 可达边界（残差 = RootToGoal - 链长）。
    const float RootToGoal = FVector::Dist(Pos[0], GoalPosition);
    const bool bHandleTooFar = (RootToGoal > TotalLength);

    if (bHandleTooFar) {
        const FVector Dir = (GoalPosition - Pos[0]).GetSafeNormal();
        if (!Dir.IsNearlyZero()) {
            for (int32 i = 0; i < NumBones - 1; ++i) {
                const FVector CurSegDir = Rot[i].RotateVector(LocalOffset[i]);
                if (!CurSegDir.IsNearlyZero()) {
                    const FQuat Align = FQuat::FindBetweenVectors(
                        CurSegDir.GetSafeNormal(), Dir);
                    Rot[i] = (Align * Rot[i]).GetNormalized();
                }
                Pos[i + 1] = Pos[i] + Rot[i].RotateVector(LocalOffset[i]);
            }
        }
        MaxIter = 0;  // 跳过迭代，直接写回拉直后的链
    }

    // ---- 迭代求解 ----
    double FinalNorm = 1e30;
    double LastNorm = 1e30;  // 上一轮残差（保留声明，兼容后续扩展）

    // 目标与链末端几乎重合：位置已满足，无需求解。
    // 直接跳过迭代并返回（保持初始姿态），防止朝向任务/极点/数值误差
    // 在位置残差为 0 时仍把链推飞（此前日志中 iter 0 后 dist 跳到 322
    // 的根因）。
    const float InitDist = FVector::Dist(GoalPosition, Pos[NumBones - 1]);
    if (InitDist < 0.01f) {
        return;
    }

    for (int32 Iter = 0; Iter < MaxIter; ++Iter) {
        // NaN 防护：一旦求解产生 NaN，中止并跳过写回，避免污染层级
        bool bHasNaN = false;
        for (int32 i = 0; i < NumBones; ++i) {
            if (Pos[i].ContainsNaN() || Rot[i].ContainsNaN()) {
                bHasNaN = true;
                break;
            }
        }
        if (bHasNaN) {
            return;
        }

        const FVector EndPos = Pos[NumBones - 1];

        // ---- 位置任务残差（行 0-2）----
        FVector DPos = GoalPosition - EndPos;
        const float DPosLen = DPos.Size();
        if (DPosLen > ClampLength && ClampLength > KINDA_SMALL_NUMBER) {
            DPos = DPos / DPosLen * ClampLength;
        }
        Solver.SetBetaRow(0, DPos);

        // ---- 构建雅可比 ----
        // 位置任务：J 行 = p × axis（p = 骨骼起点到末端，axis =
        // 骨骼全局旋转轴） 朝向任务：J 行 = axis
        for (int32 i = 0; i < NumBones; ++i) {
            const FVector P = Pos[i] - EndPos;
            const FVector Axis0 = Rot[i].RotateVector(FVector(1, 0, 0));
            const FVector Axis1 = Rot[i].RotateVector(FVector(0, 1, 0));
            const FVector Axis2 = Rot[i].RotateVector(FVector(0, 0, 1));

            const int32 BaseDoF = i * 3;

            Solver.SetJacobianRow(0, BaseDoF + 0,
                                  FVector::CrossProduct(P, Axis0));
            Solver.SetJacobianRow(0, BaseDoF + 1,
                                  FVector::CrossProduct(P, Axis1));
            Solver.SetJacobianRow(0, BaseDoF + 2,
                                  FVector::CrossProduct(P, Axis2));
        }

        // ---- SDLS 伪逆求解 ----
        Solver.Solve();

        // ---- 应用角度增量（Blender 原版：局部 basis 右乘 + 从根级联）----
        // 【关键修复】此前用"全局左乘 + 手动传播下游"导致不收敛/发散。
        // 仿真严格复刻对比（真实数据）：
        //   0.30/0.80 左乘：从 ~0.3 振荡发散到 8+
        //   0.80 右乘局部（Blender 原版）：收敛到 0.002 @ iter16
        // Blender 的 IK_QSphericalSegment::UpdateAngle 用 Rodrigues 构造
        // M 并右乘局部 basis（m_new_basis = m_basis * M）；全局旋转通过
        // root->UpdateTransform 从根级联重算（下游自动跟随）。这里逐根
        // 右乘 Basis，再统一从根级联出全局旋转 Rot，最后做位置级联。
        // 同时记录角度更新范数（Blender 的 AngleUpdateNorm）用于收敛判断。
        double AngleNorm = 0.0;
        for (int32 i = 0; i < NumBones; ++i) {
            const int32 BaseDoF = i * 3;
            const FVector Dq((float)Solver.DTheta[BaseDoF + 0],
                             (float)Solver.DTheta[BaseDoF + 1],
                             (float)Solver.DTheta[BaseDoF + 2]);

            const float DqSize = Dq.Size();
            AngleNorm = FMath::Max(AngleNorm, (double)DqSize);
            if (DqSize > 1e-8f) {
                const FQuat M(Dq / DqSize, DqSize);
                Basis[i] = (Basis[i] * M).GetNormalized();
            }
        }

        // 从根级联重算全局旋转（Blender root->UpdateTransform 的等价物）
        Rot[0] = Basis[0];
        for (int32 i = 1; i < NumBones; ++i) {
            Rot[i] = (Rot[i - 1] * Basis[i]).GetNormalized();
        }

        // ---- 位置级联 ----
        for (int32 i = 0; i < NumBones - 1; ++i) {
            Pos[i + 1] = Pos[i] + Rot[i].RotateVector(LocalOffset[i]);
        }

        // ---- 收敛判断 ----
        FinalNorm = FVector::Dist(GoalPosition, Pos[NumBones - 1]);

        // 位置残差到位：收敛。
        if (FinalNorm < ConvergeThreshold && Iter > 10) {
            break;
        }

        // Blender 式收敛：角度更新足够小即认为链已稳定，继续迭代无益。
        // 关键：不再用"位置残差增加"检测——可达目标在近奇异构型时残差
        // 会有小幅振荡（如 0.148→0.164），旧检测连续 4 次就误判为发散，
        // 在离目标 0.15 处错误终止迭代（用户报告的 bug）。角度更新小
        // 即认为达标；不可达场景已由上方"太远分流"处理，不会走到这里。
        if (Iter > 10 && AngleNorm < 1e-3) {
            break;
        }
    }

    // ---- Pole 后旋转（把链平面转到参考平面）----
    // 位置求解（雅可比迭代）已完成：根固定、末端精确到位。
    // 此时沿 root->end 轴整体旋转整条链，使【链平面】与由【根骨骼、末端、
    // pole target】三点确定的参考平面重合。根与末端都在旋转轴上 ->
    // 位置不变；只有链的弯曲平面朝向改变。这符合"pole 只影响链朝向、
    // 不改变末端位置"的语义。
    //
    // 参考平面法线（ArcDistributedIK::CalculateReferencePlaneNormal）：
    //   RootToEnd   = normalize(End - Root)
    //   RootToPole  = normalize(Pole - Root)
    //   PlaneNormal = normalize(cross(RootToEnd, RootToPole))
    //
    // 链平面法线：迭代后简单手指链大致共面，取中间关节的平均点作为
    // 平面内一点：
    //   MidPos      = average(Pos[1..N-2])
    //   ChainNormal = normalize(cross(RootToEnd, MidPos - Root))
    //
    // 两个平面都包含 root->end 轴，因此只需绕 Dir 旋转一个角度即可重合：
    //   Angle = atan2(cross(ChainNormal, PlaneNormal)·Dir,
    //                 ChainNormal·PlaneNormal)
    //
    // 【关键】整条链统一左乘同一个旋转（绕 Dir），因此每根骨骼的局部
    // Basis（相对父）不变：B_i = G_{i-1}^{-1} * G_i，整体左乘后相互抵消。
    // 所以只需更新全局 Rot 与 Pos，无需动 Basis。
    if (!PoleTarget.IsNearlyZero()) {
        const FVector RootPos = Pos[0];
        const FVector EndPos = Pos[NumBones - 1];
        const FVector Dir = (EndPos - RootPos).GetSafeNormal();

        if (!Dir.IsNearlyZero()) {
            // 参考平面法线（ArcDistributedIK 同款退化回退）
            const FVector RootToPole = (PoleTarget - RootPos).GetSafeNormal();
            FVector PlaneNormal = FVector::CrossProduct(Dir, RootToPole);
            if (PlaneNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
                PlaneNormal = FVector::CrossProduct(Dir, FVector::UpVector);
                if (PlaneNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
                    PlaneNormal =
                        FVector::CrossProduct(Dir, FVector::ForwardVector);
                }
            }
            PlaneNormal = PlaneNormal.GetSafeNormal();

            // 链平面法线：由中间关节的平均点决定（链大致共面）
            FVector ChainNormal = FVector::ZeroVector;
            if (NumBones >= 3) {
                FVector MidPos = FVector::ZeroVector;
                for (int32 i = 1; i < NumBones - 1; ++i) {
                    MidPos += Pos[i];
                }
                MidPos /= (float)(NumBones - 2);
                ChainNormal = FVector::CrossProduct(Dir, MidPos - RootPos);
            }

            if (!ChainNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
                ChainNormal = ChainNormal.GetSafeNormal();

                // 绕 Dir 把链平面法线转到参考平面法线的有符号角度
                const double Angle = FMath::Atan2(
                    FVector::DotProduct(
                        FVector::CrossProduct(ChainNormal, PlaneNormal), Dir),
                    FVector::DotProduct(ChainNormal, PlaneNormal));

                if (!FMath::IsNearlyZero(Angle, 1e-5)) {
                    const FQuat PoleRot(Dir, Angle);

                    for (int32 i = 0; i < NumBones; ++i) {
                        Pos[i] =
                            RootPos + PoleRot.RotateVector(Pos[i] - RootPos);
                        Rot[i] = (PoleRot * Rot[i]).GetNormalized();
                    }
                }
            }
        }
    }

    // ---- 末端关节旋转 ----
    // bPropagateToChildren 语义（用户要求）：
    //   true  -> 末端关节的世界旋转 = Effector 的世界旋转（GoalRotation），
    //            即两者在世界（Control Rig 全局）坐标系的旋转值一致。
    //   false -> 末端关节保持迭代开始时的局部旋转不变（相对父骨骼不旋转）。
    //            该行为由级联 Rot[N-1] = Rot[N-2] * Basis[N-1] 自然保证：
    //            末端关节的雅可比列为 0（P=0）故 dtheta 恒为 0，Basis[N-1]
    //            在整个迭代中保持初始值；Pole 后旋转整体左乘也不改 Basis。
    if (bPropagateToChildren) {
        Rot[NumBones - 1] = GoalRotation;
    }

    // ---- 写回层级 ----
    const float T = FMath::Clamp<float>(Weight, 0.f, 1.f);

    for (int32 i = 0; i < NumBones; ++i) {
        FTransform Xfo(FRotator(Rot[i]), Pos[i], Scale[i]);

        if (!FMath::IsNearlyEqual(T, 1.f)) {
            const FTransform& Prev = InitGlobal[i];
            Xfo.SetLocation(FMath::Lerp(Prev.GetLocation(), Pos[i], T));
            Xfo.SetRotation(FQuat::Slerp(Prev.GetRotation(), Rot[i], T));
            Xfo.SetScale3D(FMath::Lerp(Prev.GetScale3D(), Scale[i], T));
        }

        Hierarchy->SetGlobalTransform(CachedBones[i].GetKey(), Xfo, false,
                                      bPropagateToChildren, false);
    }
}
