// 玩家角色：在第一人称基类 AmmmfpsCharacter 之上，加入武器系统——
// hitscan 射线开火、弹匣/换弹、单发与全自动开火模式、连发后坐力扩散。
// 移动 / 视角 / 跳跃 / 第一人称相机 都由基类提供，这里只加武器相关。

#pragma once

#include "CoreMinimal.h"
#include "mmmfpsCharacter.h"
#include "FPSCharacter.generated.h"

class UInputAction;
class USkeletalMeshComponent;
class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

UCLASS()
class MMMFPS_API AFPSCharacter : public AmmmfpsCharacter
{
	GENERATED_BODY()

public:
	AFPSCharacter();

protected:
	// ---------- 输入动作（在编辑器里把 IA_* 资产拖进这些槽） ----------

	/** 开火输入：按下开火、松开停火（全自动靠这两端配合） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input")
	UInputAction* FireAction;

	/** 换弹输入：建议绑 R 键 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input")
	UInputAction* ReloadAction;

	/** 切换单发/全自动：建议绑 V 键（可选，不绑就保持默认模式） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input")
	UInputAction* ToggleFireModeAction;

	/** 瞄准(ADS)输入：建议绑鼠标右键。按住举枪缩放 + 更准；松开恢复 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Input")
	UInputAction* AimAction;

	// ---------- 武器数值（可在编辑器调） ----------

	/** 射线最大射程（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	float FireRange = 10000.f;

	/** 每发造成的伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	float FireDamage = 20.f;

	/** 弹匣容量：一梭子多少发 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	int32 MagazineSize = 30;

	/** 连发间隔（秒）。0.1 = 每秒 10 发；也是半自动连点的最小间隔 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	float RefireRate = 0.1f;

	/** 换弹耗时（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	float ReloadTime = 1.5f;

	/** 默认开火模式：true=全自动（按住连发），false=半自动（按一下一发） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	bool bFullAuto = true;

	// ---------- 后坐力 / 扩散 ----------

	/** 静止时的基础扩散半角（度）。0 = 绝对精准 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil")
	float BaseSpreadDegrees = 0.5f;

	/** 扩散上限（度）：连发再久也不超过它 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil")
	float MaxSpreadDegrees = 6.f;

	/** 每开一枪增加多少扩散（度） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil")
	float SpreadPerShot = 1.f;

	/** 停火后扩散每秒恢复多少（度/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Recoil")
	float SpreadRecoveryPerSec = 8.f;

	// ---------- 瞄准 (ADS) ----------

	/** 举枪时的相机视场角(度)。比默认小 = 放大，越小放大越多 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Aim")
	float AimFOV = 50.f;

	/** 举枪/放下的缩放过渡速度（越大越跟手） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Aim")
	float AimInterpSpeed = 12.f;

	/** 举枪时散布缩小到原来的比例（0.3 = 精度提升约三倍） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Aim")
	float AimSpreadMultiplier = 0.3f;

	/** 举枪时枪上瞄准点停在【相机前方】多远（厘米）。太小→枪怼到眼前、手被拉到脸上；太大→枪伸太远。
	 *  视线轴上的点都落在准星中心，所以改这个不影响"对准准星"，只改枪离脸的远近。可在 BP / 运行时调。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Aim")
	float AimForwardOffset = 40.f;

	/** 瞄准方向参考插槽：代码用 (此插槽位置 − AimSocket位置) 当"瞄准基线方向"。
	 *  在枪的【前准星】上建个插槽(如 FrontSight)、与 AimSocket(照门)【同一高度】，瞄准时三点(照门-前准星-准星)才共线。
	 *  找不到此插槽时回退用 Muzzle(枪口，因低于瞄准线会略带俯视)。Muzzle 因此可安心留在枪眼做枪口火光。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Aim")
	FName AimDirSocketName = "FrontSight";

	/** 调试可视化：在世界里画出 瞄准目标点(绿)/实际AimSocket(红)/瞄准方向(品红)/视线轴(黄)，
	 *  用来一眼判断"位置对齐没、瞄准线平不平行视线"。调好后在 BP 关掉。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Aim")
	bool bShowAimDebug = true;

	// ---------- 武器外观 / 动画 ----------

	/** 第一人称手里的枪模型（挂在手臂上、只有自己看得见）。在 BP 里设 SKM_Rifle + 手部插槽 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	USkeletalMeshComponent* WeaponMesh;

	/** 第三人称枪模型（挂在身体的手上、只有别人看得见）。联机里别的玩家看你时看到的就是这把；
	 *  在 BP 里把它设成和 WeaponMesh 同一把枪（SKM_Rifle）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	USkeletalMeshComponent* TPWeaponMesh;

	/** 开火蒙太奇：开火瞬间在第一人称手臂上播放（后坐动作）。BP 里设 FP_Rifle_Shoot_Montage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Anim")
	UAnimMontage* FireMontage;

	/** 第三人称开火蒙太奇：开火时在身体上播（给别人看你在射击）。
	 *  BP 里设 MM_Rifle_Fire 转的 Montage（可与敌人 ShootMontage 用同一个）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Anim")
	UAnimMontage* TPFireMontage;

	/** 换弹蒙太奇（可选，有就播）。BP 里设换弹动画 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Anim")
	UAnimMontage* ReloadMontage;

	/** 受击动画（第三人称身体播；第一人称看不到自己，主要给联机时别人看，配合屏幕红屏） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Anim")
	UAnimMontage* HitReactMontage;

	// ---------- 开火特效 (FX) ----------

	/** 枪口火光粒子（可选）。在 BP 指定 Niagara 系统；贴在枪口 MuzzleSocketName 插槽上播放。不填则只有下面的闪光灯。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	UNiagaraSystem* MuzzleFlashFX = nullptr;

	/** 命中特效粒子（可选）。在 BP 指定 Niagara 系统；在子弹命中点播放。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	UNiagaraSystem* ImpactFX = nullptr;

	/** 枪声（可选）。在 BP 指定音效；在枪口播放。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	USoundBase* FireSound = nullptr;

	/** 枪口插槽名（火光/枪声/闪光灯都挂这里）。默认 Muzzle——它一直留在真实枪口，不参与瞄准。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	FName MuzzleSocketName = "Muzzle";

	/** 枪口闪光灯强度（瞬时点光源，无需任何素材、永远有效）。设 0 可关闭闪光灯。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	float MuzzleFlashLightIntensity = 5000.f;

	/** 枪口闪光灯颜色（暖橙色，模拟火药燃烧）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	FColor MuzzleFlashLightColor = FColor(255, 170, 70);

	/** 枪口闪光灯衰减半径（厘米，照亮多大范围）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	float MuzzleFlashLightRadius = 300.f;

	/** 枪口闪光灯持续时间（秒）。一闪即灭，太长会显得发光不真实。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FX")
	float MuzzleFlashLightTime = 0.05f;

	/** 玩家最大血量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	float MaxHP = 100.f;

	/** 死后尸体（布娃娃）保留多少秒再清理（之后玩家会在重生点重生） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	float CorpseLifetime = 3.f;

	// ---------- 生命周期 / 输入绑定 ----------

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 每帧把相机 FOV 平滑插值到（举枪→AimFOV / 放下→DefaultFOV），做出缩放过渡 */
	virtual void Tick(float DeltaSeconds) override;

	// ---------- 开火流程 ----------

	/** 扳机按下：开第一枪；若全自动则启动连发定时器 */
	void StartFire();

	/** 扳机松开：停止连发 */
	void StopFire();

	/** 真正打一枪：检查换弹/射速/弹药 → 扣弹 → 扩散射线 → 伤害 */
	void HandleFire();

	/** 开始换弹（启动换弹定时器） */
	void Reload();

	/** 换弹完成：填满弹匣 */
	void FinishReload();

	/** 切换单发/全自动 */
	void ToggleFireMode();

	/** 举枪 / 放下（ADS）：右键按下/松开触发，改变 bIsAiming，Tick 据此缩放 FOV */
	void StartAiming();
	void StopAiming();

	// ---------- 联机：服务器权威开火 / 受击 / 死亡 ----------

	/** 开火 RPC：客户端把“射线起点 + 方向”上报服务器，由服务器做权威射线判定与伤害。
	 *  Server=只在服务器执行；Reliable=可靠送达（不丢包）。 */
	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector Start, FVector ShotDir);

	/** 客户端→服务器上报玩家自选名字：进游戏(本 pawn 的 BeginPlay)后由本地玩家调用，
	 *  把 GameInstance 里存的名字告诉服务器，由服务器 ChangeName 设到 PlayerState、再复制给所有人。
	 *  为什么不只靠连接 URL 的 ?Name=：登录握手期间引擎会用【在线子系统昵称】(没接在线服务时=电脑用户名)
	 *  覆盖 PlayerState 名字，时机比 ?Name= 晚 → 把自选名顶掉、显示成电脑名。改在 pawn 生成后(BeginPlay)上报，
	 *  此调用跑在握手之后 → 不会再被覆盖，名字稳定生效。Server=只在服务器执行；Reliable=可靠送达。 */
	UFUNCTION(Server, Reliable)
	void Server_SetPlayerName(const FString& NewName);

	/** 开火表现广播：所有客户端播第三人称身体开火动画 + 枪口火光/闪光/枪声 +（命中则）命中特效。
	 *  HitLocation=命中点(没中则是射线终点)，bHit=这一枪是否命中(决定要不要播命中特效)。 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireFX(FVector HitLocation, bool bHit);

	/** 在枪口播放：火光粒子(若指定) + 瞬时闪光灯(永远有效) + 枪声(若指定)。
	 *  本机玩家挂在第一人称枪、其他客户端挂在第三人称枪——各自看得见自己该看的那把。 */
	void PlayMuzzleFlash();

	/** 受击表现广播：所有客户端都播这个角色的受击动画；仅受害者本人触发屏幕红屏。
	 *  NetMulticast=服务器调用、所有端各自执行。 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnDamaged();

	/** 死亡表现广播：所有客户端把这个角色变成布娃娃。 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDeath();

	/** 服务器权威死亡处理（停火 + 广播布娃娃；计分/重生在后续阶段补） */
	void Die(AController* Killer);

	// ---------- 运行时状态 ----------

	/** 当前血量。UPROPERTY(Replicated)：服务器改、引擎自动同步到所有客户端——
	 *  这样各端看到的血量一致，IsDead() 在客户端也判断得准。 */
	UPROPERTY(Replicated)
	float CurrentHP = 0.f;

	/** 当前弹匣内剩余子弹 */
	int32 CurrentAmmo = 0;

	/** 是否正在换弹（换弹时不能开火） */
	bool bIsReloading = false;

	/** 当前累积扩散（度）：连发增大、停火恢复 */
	float CurrentSpreadDegrees = 0.f;

	/** 是否正在瞄准(ADS)：右键按住时为 true。
	 *  BlueprintReadOnly → FP 动画蓝图能读它，按它混合到"举枪瞄准"姿势（枪抬到准星中心）。
	 *  仅本地有效（右键只在本地玩家触发）；要让别人也看到你举枪需另做复制，暂不做。 */
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Aim")
	bool bIsAiming = false;

	/** 默认视场角：BeginPlay 时从相机读取，放下瞄准时恢复到它 */
	float DefaultFOV = 90.f;

	/** 举枪混合系数(0=放下、1=举到准星)。每帧朝 bIsAiming 用 AimInterpSpeed 平滑插值——和 FOV 缩放同速同步。
	 *  动画蓝图把它接到 ModifyBone / TwoBoneIK 的 Alpha：右键→平滑抬枪、松开→平滑放下，不再是常开/突变。 */
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Aim")
	float AimAlpha = 0.f;

	// ---------- ADS 程序化瞄准：给 FP 动画蓝图的 IK 用的两个目标值（每帧 Tick 算好，本地玩家） ----------

	/** 瞄准时 ik_hand_gun 该移到的世界位置 = 手(hand_r)位置 + (相机位置 − 枪上 AimSocket 位置)。
	 *  把枪的瞄准点 AimSocket 拽回相机视线上 → 枪对准准星。BlueprintReadOnly：FP 动画蓝图读它喂 Modify Bone 的平移。 */
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Aim")
	FVector ADSSocketLoc = FVector::ZeroVector;

	/** 瞄准时 hand_r 该转成的世界旋转。注意：不是"相机旋转"，而是"相机旋转 × HandToAim⁻¹"——
	 *  目标是让【枪上 AimSocket 的朝向】对齐相机，而不是把手腕骨硬掰到和相机同朝向(那会拧成麻花)。
	 *  BlueprintReadOnly：FP 动画蓝图读它喂 Modify Bone 的旋转。 */
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Aim")
	FRotator ADSSocketRot = FRotator::ZeroRotator;

	/** 缓存：枪的"枪坐标系"(X=枪管前向、Z=枪上方)相对 hand_r 局部坐标系的旋转。枪刚性挂手上 → 此值是【常量】。
	 *  枪管前向用 Muzzle 与 AimSocket 两插槽的【位置差】算(与插槽自身朝向无关，最稳)；枪上方用 AimSocket 的 Z 轴。
	 *  瞄准时拿它把"枪管对齐相机前方、枪保持竖直"，不再依赖 AimSocket 的 X 轴朝向。 */
	FQuat ADSGunBasisLocal = FQuat::Identity;
	/** 缓存：AimSocket 在 hand_r 局部坐标系中的位置(常量，枪刚性挂手上)。位置前馈用——
	 *  先算好这一帧的旋转，再直接反推手该在的位置，使 AimSocket 精确落在目标点，无滞后。 */
	FVector ADSAimOffsetLocal = FVector::ZeroVector;
	bool bADSGunBasisCached = false;

	/** 上次开火的游戏时间，用于射速限制与扩散恢复计算 */
	float LastFireTime = -999.f;

	/** 全自动连发定时器 */
	FTimerHandle RefireTimer;

	/** 换弹定时器 */
	FTimerHandle ReloadTimer;

public:
	/** 伤害接收端：开火的 ApplyPointDamage 会走到这里（仅服务器结算） */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** 声明哪些属性要复制（这里登记 CurrentHP）。联机必写的样板函数。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 是否已死亡（敌人 AI / 输入门控用；CurrentHP 已复制，客户端也准） */
	bool IsDead() const { return CurrentHP <= 0.f; }

	/** 给 HUD 显示血量用 */
	float GetCurrentHP() const { return CurrentHP; }
	float GetMaxHP() const { return MaxHP; }
};
