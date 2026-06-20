#include "FPSEnemy.h"
#include "FPSEnemyAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/DamageType.h"
#include "FPSGameMode.h"
#include "FPSGameState.h"
#include "Net/UnrealNetwork.h"
#include "mmmfps.h"

AFPSEnemy::AFPSEnemy()
{
	PrimaryActorTick.bCanEverTick = true;   // 需要每帧平滑转向瞄准目标

	// 联机：敌人由服务器生成、AI 在服务器跑；pawn 复制给所有客户端（移动/血量/死亡都同步）
	bReplicates = true;

	// 指定“大脑”类型，并让它无论是摆在关卡里还是运行时生成都自动接管（Possess）
	AIControllerClass = AFPSEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 转身方式：朝“移动方向”自动转身（自然的追击表现），而不是跟随控制器视角
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 480.f, 0.f);

	// 比玩家(600)略慢，给玩家拉开距离的空间
	GetCharacterMovement()->MaxWalkSpeed = 400.f;

	// 敌人手上的枪：挂到第三人称身体的握枪插槽 HandGrip_R（所有人可见，不像玩家武器只自己看）
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), FName("HandGrip_R"));
	WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));
}

void AFPSEnemy::BeginPlay()
{
	Super::BeginPlay();
	CurrentHP = MaxHP;

	// 关键修复：让 AI 寻路走和玩家输入相同的“加速度”管线。
	// UE 寻路默认用“直接速度请求”(RequestDirectMove)推角色——速度被直接写入，
	// 而 GetCurrentAcceleration() 读的 Acceleration 成员只从“移动输入”计算，
	// 默认管线不经过它，所以哪怕全速跑加速度也恒为 0；动画蓝图 ABP_Unarmed 的
	// ShouldMove=「速度>阈值 且 加速度≠0」→ 第二项永假 → 状态机卡 Idle 不播跑步。
	// 开关打开后，寻路改为每帧喂“虚拟移动输入”(RequestPathMove→AddMovementInput)，
	// 加速度像玩家按键走路一样被正常计算。
	// 写在 BeginPlay 而非构造函数：构造函数默认值要重建 CDO 才会落到蓝图实例上，
	// Live Coding 热补丁做不到（需重启编辑器）；BeginPlay 每个实例每次开局都执行。
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}

void AFPSEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 登记复制：当前血量（服务器改、自动同步给所有客户端）
	DOREPLIFETIME(AFPSEnemy, CurrentHP);
}

void AFPSEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 有瞄准目标时（停下射击/近战），每帧平滑插值转向它，而不是瞬间硬切。
	// RInterpTo：从当前朝向向目标朝向靠拢，速度由 AimTurnSpeed 控制。
	if (AimTarget && !IsDead())
	{
		const FRotator Current = GetActorRotation();
		FRotator Desired = (AimTarget->GetActorLocation() - GetActorLocation()).Rotation();
		Desired.Pitch = 0.f;   // 只转水平朝向
		Desired.Roll = 0.f;
		const FRotator Smoothed = FMath::RInterpTo(Current, Desired, DeltaSeconds, AimTurnSpeed);
		SetActorRotation(Smoothed);
	}
}

float AFPSEnemy::TakeDamage(float Damage, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 服务器权威：伤害只在服务器结算；CurrentHP 复制 + Multicast 把表现同步给所有人。
	if (!HasAuthority() || IsDead())
	{
		return 0.f;
	}

	CurrentHP -= Damage;
	UE_LOG(Logmmmfps, Log, TEXT("%s 受到 %.0f 伤害，剩余 HP %.0f"), *GetName(), Damage, CurrentHP);

	if (CurrentHP <= 0.f)
	{
		Die(EventInstigator);   // 把“凶手”传下去，用来给对应玩家记击杀
	}
	else
	{
		Multicast_OnDamaged();  // 没死：广播受击动画给所有客户端
	}
	return Damage;
}

void AFPSEnemy::TryMeleeAttack(AActor* Target)
{
	if (IsDead() || !Target)
	{
		return;
	}

	// 冷却：用“游戏运行秒数”比上次攻击时间，简单可靠
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return;
	}

	// 距离复核（AI 的决策有 0.25s 间隔，目标可能已经跑远）
	const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (Dist > AttackRange * 1.2f)
	{
		return;
	}

	LastAttackTime = Now;

	// 转向交给 Tick 平滑处理（AI 已设 AimTarget），这里不再瞬间硬转

	// 走标准伤害管线：对方的 TakeDamage 会被调用
	UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, nullptr);
	UE_LOG(Logmmmfps, Log, TEXT("%s 攻击了 %s"), *GetName(), *Target->GetName());
}

void AFPSEnemy::TryShoot(AActor* Target)
{
	if (IsDead() || !Target)
	{
		return;
	}

	// 射击冷却：和近战一样用“游戏运行秒数”比上次射击时间
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastShootTime < ShootInterval)
	{
		return;
	}
	LastShootTime = Now;

	// 转向交给 Tick 平滑处理（AI 已设 AimTarget），这里不再瞬间硬转

	// 射线起点抬到“眼睛”高度（胶囊中心往上 50），方向朝目标 + 随机散布
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector BaseDir = (Target->GetActorLocation() - Start).GetSafeNormal();
	const float SpreadRad = FMath::DegreesToRadians(ShootSpreadDegrees);
	const FVector ShotDir = FMath::VRandCone(BaseDir, SpreadRad);
	const FVector End = Start + ShotDir * ShootRange * 1.2f;

	// 用子弹通道射线检测，忽略自己
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_BulletTrace, Params);

	if (bHit)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			// 走标准伤害管线：命中玩家就走到玩家的 TakeDamage
			UGameplayStatics::ApplyPointDamage(
				HitActor, ShootDamage, ShotDir, Hit,
				GetController(), this, UDamageType::StaticClass());
		}
	}

	// 播第三人称射击动画——广播给所有客户端（敌人 AI 在服务器跑，客户端也要看到它开枪）
	Multicast_PlayShootFX();

	UE_LOG(Logmmmfps, Log, TEXT("%s 开枪"), *GetName());
}

void AFPSEnemy::Die(AController* Killer)
{
	// —— 仅服务器执行（TakeDamage 在 HasAuthority 下调来）——

	// 1) 广播死亡事件（保留：刷怪笼/其他系统可订阅）
	OnEnemyDeath.Broadcast();

	// 2) 计分：把这一杀记到“凶手”头上（凶手是玩家→记 PlayerState；是 AI→记 GameState 槽位）
	if (AFPSGameMode* GM = GetWorld()->GetAuthGameMode<AFPSGameMode>())
	{
		GM->RegisterKill(Killer);
	}

	// 2.5) 记分板：给这个 AI 自己记一次死亡（按槽位号存进 GameState，会复制给所有人）
	if (BotId >= 0)
	{
		if (AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>())
		{
			if (GS->BotDeaths.Num() <= BotId)
			{
				GS->BotDeaths.SetNumZeroed(BotId + 1);
			}
			++GS->BotDeaths[BotId];
		}
	}

	// 3) 断开并销毁“大脑”：尸体不需要 AI
	DetachFromControllerPendingDestroy();

	// 4) 布娃娃表现广播给所有客户端
	Multicast_OnDeath();

	// 5) CorpseLifetime 秒后销毁这个 Actor（服务器销毁会复制给所有客户端）
	SetLifeSpan(CorpseLifetime);

	UE_LOG(Logmmmfps, Warning, TEXT("%s 被击杀！"), *GetName());
}

void AFPSEnemy::Multicast_OnDamaged_Implementation()
{
	// 所有客户端：敌人身体播受击动画
	if (HitReactMontage && !IsDead())
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(HitReactMontage);
		}
	}
}

void AFPSEnemy::Multicast_PlayShootFX_Implementation()
{
	// 所有客户端：敌人身体播开枪动画
	if (ShootMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(ShootMontage);
		}
	}
}

void AFPSEnemy::Multicast_OnDeath_Implementation()
{
	// 所有客户端各自把这个敌人变布娃娃：关碰撞 + 停移动 + 身体开物理
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
}
