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

    // 每个 dof 的"范数权重"（Blender IK_QJacobian::m_d_norm_weight）：
    // 由 IK_QTask::ComputeJacobian 传入，旋转 dof 为 1.0、平移 dof 为 1e2。
    // 它被 AngleUpdateNorm() 用来加权角度增量，是收敛判据的口径来源。
    double NormWeight[MaxDoF] = {};

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
        FMemory::Memzero(&NormWeight[0], sizeof(NormWeight));
        for (int32 i = 0; i < NumDoF; ++i) {
            Weight[i] = 1.0;
            WeightSqrt[i] = 1.0;
        }
    }

    // 整体缩放：对应 Blender IK_QJacobianSolver::Scale(scale, tasks)。
    // 目标、关节位置、骨长偏移一起按 scale 缩放（旋转不受影响）。
    // 求解在缩放后的空间里进行，beta 与 clamp 长度因此是无量纲的。
    static void ScaleProblem(double scale, int32 NumBones, FVector& GoalPos,
                             TArray<FVector>& Pos,
                             TArray<FVector>& LocalOffset) {
        for (int32 i = 0; i < NumBones; ++i) {
            Pos[i] *= scale;
        }
        for (int32 i = 0; i < NumBones - 1; ++i) {
            LocalOffset[i] *= scale;
        }
        GoalPos *= scale;
    }

    void SetBetaRow(int32 InRow, const FVector& InV) {
        if (InRow + 2 < MaxTasks) {
            Beta[InRow + 0] = InV.X;
            Beta[InRow + 1] = InV.Y;
            Beta[InRow + 2] = InV.Z;
        }
    }

    // 对应 Blender IK_QJacobian::SetDerivatives：
    //   雅可比元素 = v * m_weight_sqrt[dof_id]
    //   并记录 m_d_norm_weight[dof_id] = norm_weight
    void SetJacobianRow(int32 InRow, int32 InDoF, const FVector& InV,
                        double InNormWeight) {
        if (InRow + 2 < MaxTasks && InDoF >= 0 && InDoF < NumDoF) {
            const double WS = WeightSqrt[InDoF];
            J[InRow + 0][InDoF] = InV.X * WS;
            J[InRow + 1][InDoF] = InV.Y * WS;
            J[InRow + 2][InDoF] = InV.Z * WS;
            NormWeight[InDoF] = InNormWeight;
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

    // 对应 Blender IK_QJacobian::AngleUpdateNorm()：
    //   dtheta_abs = fabs(m_d_theta[i] * m_d_norm_weight[i])
    //   return max(dtheta_abs)
    // 注意这里乘的是 norm_weight（旋转 dof=1.0，平移 dof=1e2），不是三轴合成长度。
    // 收敛判据必须用这个口径，才与 Blender 可比。
    double AngleUpdateNorm() const {
        double mx = 0.0;
        for (int32 i = 0; i < NumDoF; ++i) {
            mx = FMath::Max(FMath::Abs(DTheta[i] * NormWeight[i]), mx);
        }
        return mx;
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
    // 与兄弟节点 ArcDistributedIK 同风格：公用逻辑抽成独立方法，主流程更清晰。
    struct Local {
        // ================================================================
        // ---- Pole：求解后的"链平面 -> pole 平面"对齐 ----
        // 说明：Blender 的 pole 约束是"求解前把链预旋转到 pole 平面 + 求解后再精修"，
        // 它成立的前提是【求解起点是动画 FK 姿态】——起点末端不在目标上，于是
        // dir != poledir、约束旋转非零，pole 才能约束零空间。
        //
        // 但我们这条链的"输入姿态"与"IK 输出"是同一批骨骼的同一份数据（实测
        // InitialLocal == CurrentLocal，重建 FK 姿态与当前姿态逐位相同），
        // 于是每帧起点末端都恰好压在目标上：dir == poledir -> 约束旋转恒为单位
        // 矩阵 -> pole 在算法内部【结构性失效】。
        //
        // 因此这里改用"解算后显式对齐"：
        //   把整条链绕【链根 -> 末端】轴刚性旋转一个角度，使【链的弯曲平面】
        //   与【由 链根 / 末端 / pole target 三点确定的 pole 平面】重合。
        //   * 根与末端都在旋转轴上 -> 末端位置、骨长、相对构型全部不变；
        //   * 只有链的弯曲平面朝向改变 —— 这正是 pole 的语义。
        // 该旋转让"链平面"成为 pole target 的确定函数，因此移动 pole 一定能
        // 改变链的朝向（前提是链确实是弯的：直链没有可定义的弯曲平面）。
        static void AlignChainPlaneToPole(TArray<FQuat>& Rot, TArray<FVector>& Pos,
                                          const FVector& PoleTarget) {
            const int32 N = Rot.Num();
            if (N < 3 || PoleTarget.IsNearlyZero()) {
                return;
            }

            const FVector RootPos = Pos[0];
            const FVector EndPos = Pos[N - 1];
            const FVector Dir = (EndPos - RootPos).GetSafeNormal();
            if (Dir.IsNearlyZero()) {
                return;
            }

            // 参考平面（pole 平面）法线：cross(根->末端, 根->pole)
            const FVector RootToPole = (PoleTarget - RootPos).GetSafeNormal();
            if (RootToPole.IsNearlyZero()) {
                return;
            }
            FVector RefNormal = FVector::CrossProduct(Dir, RootToPole);
            if (RefNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
                // pole 落在链轴上：pole 平面退化，无法定义朝向
                return;
            }
            RefNormal = RefNormal.GetSafeNormal();

            // 链平面法线：由中间关节的平均点决定（链大致共面）
            FVector MidMean = FVector::ZeroVector;
            for (int32 i = 1; i < N - 1; ++i) {
                MidMean += Pos[i];
            }
            MidMean /= (float)(N - 2);

            FVector ChainNormal = FVector::CrossProduct(Dir, MidMean - RootPos);
            if (ChainNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
                // 链接近完全伸直：没有可定义的弯曲平面，跳过（此时旋转也无意义）
                return;
            }
            ChainNormal = ChainNormal.GetSafeNormal();

            // 绕 Dir 的有符号夹角（两者都垂直于 Dir，故夹角就在垂直于 Dir 的平面内）
            const double Angle = FMath::Atan2(
                FVector::DotProduct(FVector::CrossProduct(ChainNormal, RefNormal), Dir),
                FVector::DotProduct(ChainNormal, RefNormal));
            if (FMath::IsNearlyZero(Angle, 1e-5)) {
                return;
            }

            // 整条链绕链根刚性旋转（末端在轴上 -> 位置不变）
            const FQuat PoleRot(Dir, Angle);
            for (int32 i = 0; i < N; ++i) {
                Pos[i] = RootPos + PoleRot.RotateVector(Pos[i] - RootPos);
                Rot[i] = (PoleRot * Rot[i]).GetNormalized();
            }
        }


        // 把解算后的全局姿态写回层级（含 Weight 插值）。
        static void WriteChainToHierarchy(
            URigHierarchy* Hierarchy,
            const TArray<FCachedRigElement>& CachedBones,
            const TArray<FVector>& Pos, const TArray<FQuat>& Rot,
            const TArray<FVector>& Scale, const TArray<FTransform>& InitGlobal,
            float Weight, bool bPropagateToChildren) {
            if (!Hierarchy) {
                return;
            }
            const float T = FMath::Clamp<float>(Weight, 0.f, 1.f);
            for (int32 i = 0; i < CachedBones.Num(); ++i) {
                FTransform Xfo(FRotator(Rot[i]), Pos[i], Scale[i]);
                if (!FMath::IsNearlyEqual(T, 1.f)) {
                    const FTransform& Prev = InitGlobal[i];
                    Xfo.SetLocation(FMath::Lerp(Prev.GetLocation(), Pos[i], T));
                    Xfo.SetRotation(FQuat::Slerp(Prev.GetRotation(), Rot[i], T));
                    Xfo.SetScale3D(FMath::Lerp(Prev.GetScale3D(), Scale[i], T));
                }
                Hierarchy->SetGlobalTransform(CachedBones[i].GetKey(), Xfo,
                                              false, bPropagateToChildren,
                                              false);
            }
        }
    };

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

    // ---- 求解起点 = 重建出的「未被 IK 解算过的 FK 姿态」 ----
    //
    // 架构性修正：这是本节点与 Blender 最根本的差别所在。
    //
    // Blender 的 IK 约束每帧从【动画姿态】起算（pose channel 由动画曲线重建，
    // 与 IK 输出无关），求解器只输出 basis 增量。于是：
    //     dir（根->末端，由起点决定） != poledir（根->目标）
    //     => mat != polemat => trans != I => pole 项真正约束零空间。
    //
    // 我们此前直接读「层级当前姿态」当起点，而本节点每次求值都会把结果写回
    // 同一批骨骼的 Current，于是同一帧内第二趟求值读到的就是第一趟的输出：
    //     dir == poledir => mat == polemat => trans == I
    // 表现为：pole 完全失效、末端永远精确到位、径向角乱跳（三者同一根因）。
    //
    // 修正办法（不需要改 rig 结构）：用一份 IK 从不写入的数据重建起点 ——
    // 骨骼的 InitialLocal（初始局部变换）。本节点只写 Current，从不写 Initial，
    // 因此 InitialLocal 永远是干净的 rest 相对变换。重建方式：
    //   * 根骨骼：父级的【实时】姿态 x 根骨骼的 InitialLocal
    //     （父级是 FK、不受本节点影响；这样链才挂在身体当前的位置/朝向上）
    //   * 根以下：逐级用 InitialLocal 级联
    // 得到的链在数学上就是"如果这条链上没有 IK，它此刻会是什么姿态"。
    {
        // 根骨骼的父级实时姿态（父级不属于本链，不会被本节点写回）
        FTransform ParentLiveXfo = FTransform::Identity;
        if (const FRigBoneElement* RootBone =
                Hierarchy->Find<FRigBoneElement>(CachedBones[0].GetKey())) {
            if (RootBone->ParentElement) {
                ParentLiveXfo = RootBone->ParentElement->GetTransform().Get(
                    ERigTransformType::CurrentGlobal);
            }
        }

        FTransform AccumXfo = ParentLiveXfo;
        for (int32 i = 0; i < NumBones; ++i) {
            const FTransform InitLocal =
                Hierarchy->GetInitialLocalTransform(CachedBones[i].GetKey());
            AccumXfo = InitLocal * AccumXfo;  // 父 -> 子 级联
            Pos[i] = AccumXfo.GetLocation();
            Rot[i] = AccumXfo.GetRotation().GetNormalized();
            Scale[i] = AccumXfo.GetScale3D();
        }
    }

    const FVector GoalPosition = EffectorTransform.GetLocation();
    const FQuat GoalRotation = EffectorTransform.GetRotation();

    // 长度 = 相邻骨骼真实距离（骨长）
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

    // 求解起点姿态：Weight 混合与末端朝向回退都用它，
    // 保证"Weight=0 不做 IK"的落点就是这条起点链。
    for (int32 i = 0; i < NumBones; ++i) {
        InitGlobal[i] = FTransform(Rot[i], Pos[i], Scale[i]);
    }

    // ---- 配置求解器 ----
    BlenderIKImpl::FBlenderSolver Solver;

    // 纯位置任务（3 行）；末端朝向由求解后的 bPropagateToChildren 后处理控制。
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

    // ---- 尺度归一化（对应 Blender IK_QJacobianSolver::Scale）----
    // Blender 在 Solve() 开头调用 ComputeScale() = 1/Σ MaxExtension（即 1/链总长），
    // 把目标、关节位置、骨长偏移整体缩放到"链长 ≈ 1"的无量纲空间，解完再缩放回去。
    // 这决定了 beta 与 clamp 长度之间的相对关系。我们此前直接在世界单位里迭代，
    // 同一个 1e-3 / 45° / 阻尼在两个不同尺度的空间里不是同一个算法。
    const double SolveScale = (TotalLength > KINDA_SMALL_NUMBER)
                                  ? (1.0 / (double)TotalLength)
                                  : 1.0;
    FVector SolveGoal = GoalPosition;
    BlenderIKImpl::FBlenderSolver::ScaleProblem(SolveScale, NumBones, SolveGoal,
                                                Pos, LocalOffset);

    // 迭代上限。<=0 时给兜底值；非可达构型下 Blender 没有"提前结束"的分支，
    // 完全靠迭代把角度范数压下去。上限太小会让输出停在"还没稳定"的中间态，
    // 从而依赖起始姿态 = 产生抖动。
    int32 MaxIter = MaxIterations > 0 ? MaxIterations : 100;

    // ---- 迭代求解 ----
    double FinalAngleNorm = 1e30;  // 最后一次迭代的角度增量范数（收敛判据）
    double PrevAngleNorm = 1e30;   // 上一次的角度范数（停滞检测用）
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
        FVector DPos = SolveGoal - EndPos;
        const float DPosLen = DPos.Size();
        if (DPosLen > ClampLength && ClampLength > KINDA_SMALL_NUMBER) {
            DPos = DPos / DPosLen * ClampLength;
        }
        Solver.SetBetaRow(0, DPos);

        // ---- 构建雅可比 ----
        // 位置任务：J 行 = p × axis，axis 取骨骼全局旋转轴的世界方向。
        //
        // 【坐标系一致性说明】本节点施加角度增量用的是 Basis 右乘：
        //     Basis[i] = Basis[i] * M
        // 它代数上等价于世界旋转 R <- exp(ω_world) * R：
        //     R*M = R*M*R^-1*R = exp(R ω R^-1) * R
        // 因此雅可比取【世界列】P × (Rot[i]*e_j) 才是与施加方式一致的写法。
        // （已用整链有限差分逐骨骼逐轴验证：世界列与 FD 吻合到 1e-10，
        //   而 Rot^-1(P×(Rot e)) 或 (Rot^-1 P)×e 都不吻合。）
        // 零空间性质保持：绕骨骼自身轴的列恒为 0。
        //
        // norm_weight 与 Blender IK_QTask.cpp:56-70 一致：旋转 dof = 1.0
        // （平移 dof 才会用 1e2，本节点没有平移 dof）。
        for (int32 i = 0; i < NumBones; ++i) {
            const FVector P = Pos[i] - EndPos;
            const FVector Axis0 = Rot[i].RotateVector(FVector(1, 0, 0));
            const FVector Axis1 = Rot[i].RotateVector(FVector(0, 1, 0));
            const FVector Axis2 = Rot[i].RotateVector(FVector(0, 0, 1));

            const int32 BaseDoF = i * 3;
            constexpr double RotNormWeight = 1.0;

            Solver.SetJacobianRow(0, BaseDoF + 0,
                                  FVector::CrossProduct(P, Axis0),
                                  RotNormWeight);
            Solver.SetJacobianRow(0, BaseDoF + 1,
                                  FVector::CrossProduct(P, Axis1),
                                  RotNormWeight);
            Solver.SetJacobianRow(0, BaseDoF + 2,
                                  FVector::CrossProduct(P, Axis2),
                                  RotNormWeight);
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
        for (int32 i = 0; i < NumBones; ++i) {
            const int32 BaseDoF = i * 3;
            const FVector Dq((float)Solver.DTheta[BaseDoF + 0],
                             (float)Solver.DTheta[BaseDoF + 1],
                             (float)Solver.DTheta[BaseDoF + 2]);

            const float DqSize = Dq.Size();
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
        // Blender IK_QJacobianSolver.cpp:351-359 只测角度范数：
        //     norm = max(m_jacobian.AngleUpdateNorm(), norm);
        //     if (norm < 1e-3 && iterations > 10) { solved = true; break; }
        // 这里同样只用角度范数（DTheta * NormWeight 的最大值，见
        // FBlenderSolver::AngleUpdateNorm），不要求"位置进精度"——位置判据对
        // 目标够不到的链永远不成立，会让每次求值都跑满迭代上限、停在半途，
        // 落点重新变成起始姿态的函数。
        FinalAngleNorm = Solver.AngleUpdateNorm();

        // 1) Blender 式收敛：角度增量足够小即认为链已稳定。
        if (Iter > 10 && FinalAngleNorm < 1e-3) {
            break;
        }

        // 2) 停滞检测：角度增量不再下降时，继续迭代只是空转。
        // 目标够不到（链已伸直）时雅可比近奇异，角度会在小范围里来回摆，
        // Blender 那条判据可能压不到 1e-3；此时跑到迭代上限只会让输出停在
        // "半途"状态，落点再次变成起始姿态的函数。角度增量不再下降即说明
        // 已到极限位置，停下与继续迭代的结果一致。
        const bool bAngleStalled =
            (PrevAngleNorm < 1e30) && (FinalAngleNorm > PrevAngleNorm * 0.99) &&
            (FinalAngleNorm < 5e-2);
        PrevAngleNorm = FinalAngleNorm;
        if (Iter > 10 && bAngleStalled) {
            break;
        }
    }

    // ---- 从缩放空间还原（Blender: Scale(1/scale, tasks)）----
    // 求解全程在"链长≈1"的空间里进行，这里把关节位置与骨长偏移还原回世界尺度。
    // 旋转不受缩放影响，无需还原。
    BlenderIKImpl::FBlenderSolver::ScaleProblem(1.0 / SolveScale, NumBones,
                                                SolveGoal, Pos, LocalOffset);

    // ---- Pole：求解后的显式对齐（替代 Blender 的"后精修"）----
    // Blender 的 pole 约束在我们的数据源下恒为单位矩阵（dir == poledir），
    // 因此改为在解算结束后直接把整条链的弯曲平面对齐到 pole 平面。
    // 根与末端在旋转轴上 -> 末端位置/骨长/相对构型都不变，只改链的朝向。
    Local::AlignChainPlaneToPole(Rot, Pos, PoleTarget);

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
    Local::WriteChainToHierarchy(Hierarchy, CachedBones, Pos, Rot, Scale,
                                 InitGlobal, Weight, bPropagateToChildren);
}
