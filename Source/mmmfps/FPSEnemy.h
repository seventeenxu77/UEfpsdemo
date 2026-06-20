// 敌人角色（“身体”）：血量、挨打、死亡布娃娃、近战攻击。
// “大脑”在 FPSEnemyAIController 里——它负责决定什么时候追、什么时候调用 TryMeleeAttack。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FPSEnemy.generated.h"

// 死亡事件（动态多播委托）：谁想知道“这个敌人死了”就来订阅。
// 现在还没人订阅；第 3 课 GameMode 会订阅它来加分。
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemyDeathDelegate);

class UAnimMontage;
class USkeletalMeshComponent;

UCLASS()
class MMMFPS_API AFPSEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	AFPSEnemy();

	/** AI 槽位号：由刷怪笼分配（0,1,2…）。它的击杀数按这个号存在 GameState.BotKills 里——
	 *  死了重生还是同一个号，分数接着累加。-1 = 没分配（不参与计分）。仅服务器使用。 */
	int32 BotId = -1;

	/** 最大血量。玩家一枪 20，默认 5 枪打死 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Health")
	float MaxHP = 100.f;

	/** 死亡后尸体（布娃娃）保留几秒再消失 */
	UPROPERTY(EditAnywhere, Category = "Enemy|Health")
	float CorpseLifetime = 5.f;

	/** 一次近战打多少血 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
	float AttackDamage = 10.f;

	/** 离目标多近才算“够得着”（厘米） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
	float AttackRange = 200.f;

	/** 两次攻击之间的冷却（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
	float AttackCooldown = 1.5f;

	// ---------- 远程射击 ----------

	/** 射击距离：进入这个范围就停下开枪 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	float ShootRange = 1500.f;

	/** 每发子弹伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	float ShootDamage = 10.f;

	/** 两次射击间隔（秒）。比玩家慢，给玩家反应/走位空间 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	float ShootInterval = 1.2f;

	/** 射击散布半角（度）。敌人不百发百中，玩家走位才有意义 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	float ShootSpreadDegrees = 4.f;

	/** 停下瞄准时的转身插值速度（越大转得越快、越跟手） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	float AimTurnSpeed = 8.f;

	/** 第三人称射击动画（BP 里设 MM_Rifle_Fire 转成的蒙太奇） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	UAnimMontage* ShootMontage;

	/** 敌人手上的枪模型（挂第三人称身体的 HandGrip_R 插槽，所有人可见）。BP 里设 SKM_Rifle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	USkeletalMeshComponent* WeaponMesh;

	/** 受击动画（中弹未死时播，第三人称身体）。BP 里设 MM_HitReact 转的蒙太奇 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Shoot")
	UAnimMontage* HitReactMontage;

	/** 死亡时广播（第 3 课计分用） */
	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FEnemyDeathDelegate OnEnemyDeath;

	/** 伤害接收端：玩家的 ApplyPointDamage 最终会走到这里 */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** 声明要复制的属性（登记 CurrentHP） */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 血量耗尽即死亡 */
	bool IsDead() const { return CurrentHP <= 0.f; }

	// ---------- 联机表现广播（服务器调用、所有端各自执行） ----------

	/** 受击：所有客户端播这个敌人的受击动画 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnDamaged();

	/** 开枪：所有客户端播这个敌人的射击动画 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayShootFX();

	/** 死亡：所有客户端把这个敌人变布娃娃 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDeath();

	/** AI 调用：若目标在攻击范围内且冷却完毕，就打一下 */
	void TryMeleeAttack(AActor* Target);

	/** AI 调用：朝目标开枪（射线命中 + 伤害 + 射击动画） */
	void TryShoot(AActor* Target);

	/** AI 设置当前转向目标：射击/近战时设玩家（平滑转向），追击时设 nullptr（朝移动方向） */
	void SetAimTarget(AActor* Target) { AimTarget = Target; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 血量耗尽时调用：广播事件 → 计分给凶手玩家 → 关 AI → 布娃娃 → 定时消失。仅服务器。 */
	void Die(AController* Killer);

	/** 当前血量。联机：UPROPERTY(Replicated) 让所有客户端看到一致的血量/死亡状态 */
	UPROPERTY(Replicated)
	float CurrentHP = 0.f;

	/** 上次攻击发生的游戏时间，用来做冷却 */
	float LastAttackTime = -999.f;

	/** 上次射击的游戏时间，用来做射击冷却 */
	float LastShootTime = -999.f;

	/** 当前要平滑转向的目标（由 AI 设置；nullptr 时身体朝移动方向） */
	UPROPERTY()
	AActor* AimTarget = nullptr;
};
