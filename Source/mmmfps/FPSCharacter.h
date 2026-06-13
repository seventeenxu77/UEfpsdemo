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

	/** 玩家最大血量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	float MaxHP = 100.f;

	/** 死后尸体（布娃娃）保留多少秒再清理（之后玩家会在重生点重生） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Health")
	float CorpseLifetime = 3.f;

	// ---------- 生命周期 / 输入绑定 ----------

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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

	// ---------- 联机：服务器权威开火 / 受击 / 死亡 ----------

	/** 开火 RPC：客户端把“射线起点 + 方向”上报服务器，由服务器做权威射线判定与伤害。
	 *  Server=只在服务器执行；Reliable=可靠送达（不丢包）。 */
	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector Start, FVector ShotDir);

	/** 开火表现广播：所有客户端播这个角色第三人称身体的开火动画（让别人看到你在射击）。 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireFX();

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
