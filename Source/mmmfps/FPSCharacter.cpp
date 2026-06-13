#include "FPSCharacter.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "FPSHUD.h"
#include "FPSGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "mmmfps.h"

AFPSCharacter::AFPSCharacter()
{
	// 联机第一步：打开复制开关。本 pawn 必须从“服务器”复制到所有客户端，
	// 别人才看得见你、你也看得见别人。ACharacter 自带的移动组件在 bReplicates
	// 打开后会自动复制位置/旋转（本地控制端做预测、其他端做平滑插值）——
	// 所以“互相可见、能跑动”这一步，只要这一个开关。
	bReplicates = true;

	// 第一人称武器模型：挂在手臂网格上、只有本玩家看得见。
	// 用哪把枪(SKM_Rifle)、挂在手部哪个插槽，都在 BP 里配——位置要可视化微调，写死在 C++ 反而麻烦。
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	// 直接挂到手臂的握枪插槽 HandGrip_R。
	// 注意：C++（native）组件的挂载插槽必须在这里指定——蓝图里 native 组件的
	// “Parent Socket（父级插槽）”是只读的，改不了，这就是编辑器里选不了套接字的原因。
	WeaponMesh->SetupAttachment(GetFirstPersonMesh(), FName("HandGrip_R"));
	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	WeaponMesh->SetCollisionProfileName(FName("NoCollision"));
	WeaponMesh->bCastDynamicShadow = false;
	WeaponMesh->CastShadow = false;

	// 第三人称枪模型：挂在【身体】的握枪插槽 HandGrip_R 上（和敌人那把枪同一套路）。
	// SetOwnerNoSee(true)=自己看不见它（自己看的是上面那把第一人称枪），但【别的玩家】看你时看到的就是这把。
	// 不加它，联机里别人看你就是空手——因为第一人称那把枪 OnlyOwnerSee、还挂在只有自己可见的手臂上。
	TPWeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPWeaponMesh"));
	TPWeaponMesh->SetupAttachment(GetMesh(), FName("HandGrip_R"));
	TPWeaponMesh->SetOwnerNoSee(true);
	TPWeaponMesh->SetCollisionProfileName(FName("NoCollision"));
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHP = MaxHP;
	CurrentAmmo = MagazineSize;   // 开局满弹匣
}

void AFPSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 登记需要复制的属性：当前血量。服务器一改，引擎就把新值自动发给所有客户端。
	DOREPLIFETIME(AFPSCharacter, CurrentHP);
}

void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 先让基类把移动/视角/跳跃都绑好
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (FireAction)
		{
			// 按下→开火，松开→停火。全自动靠这两端配合：按住期间定时器连发
			EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AFPSCharacter::StartFire);
			EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AFPSCharacter::StopFire);
		}
		if (ReloadAction)
		{
			EIC->BindAction(ReloadAction, ETriggerEvent::Started, this, &AFPSCharacter::Reload);
		}
		if (ToggleFireModeAction)
		{
			EIC->BindAction(ToggleFireModeAction, ETriggerEvent::Started, this, &AFPSCharacter::ToggleFireMode);
		}
	}
}

void AFPSCharacter::StartFire()
{
	if (IsDead())
	{
		return;
	}

	// 先开第一枪（立即响应，不等定时器的第一个周期）
	HandleFire();

	// 全自动：按住期间每 RefireRate 秒自动再开一枪，直到 StopFire 或打空
	if (bFullAuto)
	{
		GetWorld()->GetTimerManager().SetTimer(
			RefireTimer, this, &AFPSCharacter::HandleFire, RefireRate, true);
	}
}

void AFPSCharacter::StopFire()
{
	// 松开扳机 / 打空 / 换弹时调用：停掉连发循环
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AFPSCharacter::HandleFire()
{
	// 换弹中 / 已死：不开火
	if (bIsReloading || IsDead())
	{
		return;
	}

	// 射速限制：距上次开火不足 RefireRate 就不开（防止半自动狂点超频）
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < RefireRate)
	{
		return;
	}

	// 弹匣空了：停火 + 提示换弹（不自动换，交给玩家按 R）
	if (CurrentAmmo <= 0)
	{
		StopFire();
		UE_LOG(Logmmmfps, Warning, TEXT("咔哒——弹匣空了，按 R 换弹"));
		return;
	}

	// 相机：射线起点 + 基准方向
	const UCameraComponent* Camera = GetFirstPersonCameraComponent();
	if (!Camera)
	{
		return;
	}

	// --- 到这里确定要开一枪 ---

	// 1) 累积扩散：先按“距上次开火过了多久”恢复一部分，再加这一枪的后坐力，
	//    最后夹在 [基础扩散, 最大扩散] 之间。停火越久、恢复越多 → 下一枪更准。
	const float Recovered = (Now - LastFireTime) * SpreadRecoveryPerSec;
	CurrentSpreadDegrees = FMath::Clamp(
		CurrentSpreadDegrees - Recovered + SpreadPerShot,
		BaseSpreadDegrees, MaxSpreadDegrees);
	LastFireTime = Now;

	// 2) 扣子弹
	--CurrentAmmo;

	// 2.5) 开火蒙太奇：在第一人称手臂上播后坐动作（武器挂在手臂上会跟着动）
	if (FireMontage)
	{
		if (UAnimInstance* Anim = GetFirstPersonMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(FireMontage);
		}
	}

	// 3) 方向 = 相机正前方，在“扩散锥”内随机偏移一个角度（在本地把这一枪的朝向算好）
	const FVector Start = Camera->GetComponentLocation();
	const FVector BaseDir = Camera->GetForwardVector();
	const float SpreadRad = FMath::DegreesToRadians(CurrentSpreadDegrees);
	const FVector ShotDir = FMath::VRandCone(BaseDir, SpreadRad);

	// 4) 把这一枪交给服务器做“权威判定”：客户端不自己算“打中了谁、扣多少血”
	//    （不可信、且结果传不到别人屏幕），只上报起点+方向，由服务器重做射线并施加伤害。
	//    在 Listen Server 房主机上，这其实就是本地直接执行（无网络往返）。
	Server_Fire(Start, ShotDir);
}

void AFPSCharacter::Server_Fire_Implementation(FVector Start, FVector ShotDir)
{
	// —— 运行在服务器上 ——
	// 死了不开火（服务器权威判断）
	if (IsDead())
	{
		return;
	}

	const FVector End = Start + ShotDir * FireRange;

	// 服务器重做射线（自定义 BulletTrace 通道，忽略自己）
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_BulletTrace, Params);

	// 命中：施加点伤害 → 对方的 TakeDamage（同样在服务器执行，结算血量）。
	// 传 GetController() 作为 EventInstigator —— 阶段3 会用它认定“凶手”给击杀计分。
	if (bHit)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(
				HitActor, FireDamage, ShotDir, Hit,
				GetController(), this, UDamageType::StaticClass());

			UE_LOG(Logmmmfps, Log, TEXT("[服务器] 命中: %s"), *HitActor->GetName());
		}
	}

	// 不管打没打中，都广播一次开火表现：让所有客户端看到这个角色的身体在射击。
	Multicast_PlayFireFX();
}

void AFPSCharacter::Multicast_PlayFireFX_Implementation()
{
	// 所有客户端：这个角色的第三人称身体播开火动画 —— 给【别的玩家】看你在射击。
	//（开火方自己的第一人称手臂后坐已在 HandleFire 本地播过；这把是身体上的、给别人看的。）
	if (TPFireMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(TPFireMontage);
		}
	}
}

void AFPSCharacter::Reload()
{
	// 已在换弹 / 弹匣已满 / 已死：不换
	if (bIsReloading || CurrentAmmo >= MagazineSize || IsDead())
	{
		return;
	}

	// 换弹期间先停火（把全自动定时器停掉）
	StopFire();

	bIsReloading = true;
	UE_LOG(Logmmmfps, Warning, TEXT("换弹中…（%.1f 秒）"), ReloadTime);

	// 换弹蒙太奇（手臂上播放，可选）
	if (ReloadMontage)
	{
		if (UAnimInstance* Anim = GetFirstPersonMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(ReloadMontage);
		}
	}

	// ReloadTime 秒后调用 FinishReload；最后的 false = 只触发一次（非循环）
	GetWorld()->GetTimerManager().SetTimer(
		ReloadTimer, this, &AFPSCharacter::FinishReload, ReloadTime, false);
}

void AFPSCharacter::FinishReload()
{
	CurrentAmmo = MagazineSize;   // 简化：备弹无限，直接填满弹匣
	bIsReloading = false;
	UE_LOG(Logmmmfps, Warning, TEXT("换弹完成，弹匣 %d 发"), CurrentAmmo);
}

void AFPSCharacter::ToggleFireMode()
{
	bFullAuto = !bFullAuto;
	StopFire();   // 切换瞬间停掉连发，避免残留的定时器继续打
	UE_LOG(Logmmmfps, Warning, TEXT("开火模式：%s"), bFullAuto ? TEXT("全自动") : TEXT("半自动"));
}

float AFPSCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 服务器权威：伤害只在服务器结算，再靠 CurrentHP 复制 + Multicast 同步给所有人。
	// 客户端即便收到这调用也直接忽略（HasAuthority 只有服务器为真）。
	if (!HasAuthority() || IsDead())
	{
		return 0.f;
	}

	CurrentHP -= Damage;   // CurrentHP 是 Replicated，改完自动同步到各客户端
	UE_LOG(Logmmmfps, Warning, TEXT("[服务器] 玩家受到 %.0f 伤害，剩余 HP %.0f"), Damage, CurrentHP);

	// 受击表现：广播给所有端（受害者身体播受击动画；仅受害者本人屏幕红屏）
	Multicast_OnDamaged();

	if (CurrentHP <= 0.f)
	{
		Die(EventInstigator);
	}
	return Damage;
}

void AFPSCharacter::Multicast_OnDamaged_Implementation()
{
	// 所有客户端：这个角色的第三人称身体播受击动画（致命一击时跳过，交给死亡表现接管）
	if (HitReactMontage && !IsDead())
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(HitReactMontage);
		}
	}

	// 仅“受害者本人”这台机器：触发一次屏幕红屏（IsLocallyControlled 只在受害者本地为真）
	if (IsLocallyControlled())
	{
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (AFPSHUD* HUD = Cast<AFPSHUD>(PC->GetHUD()))
			{
				HUD->NotifyPlayerDamaged();
			}
		}
	}
}

void AFPSCharacter::Die(AController* Killer)
{
	// —— 服务器权威死亡 ——
	StopFire();
	Multicast_OnDeath();   // 所有客户端把这具身体变布娃娃

	UE_LOG(Logmmmfps, Error, TEXT("[服务器] 玩家死亡（凶手：%s）"),
		Killer ? *Killer->GetName() : TEXT("无"));

	// 把“被谁杀、我的控制器是谁”交给 GameMode：记分 + 安排重生
	AController* VictimController = GetController();
	if (AFPSGameMode* GM = GetWorld()->GetAuthGameMode<AFPSGameMode>())
	{
		GM->OnPlayerKilled(Killer, VictimController);
	}

	// 脱离控制器（空出来好让 GameMode 重生它），尸体留 CorpseLifetime 秒后自动清理
	DetachFromControllerPendingDestroy();
	SetLifeSpan(CorpseLifetime);
}

void AFPSCharacter::Multicast_OnDeath_Implementation()
{
	// 所有客户端各自把这个角色变成布娃娃：关移动、关胶囊碰撞、身体开物理模拟
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DisableMovement();
	}
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetCollisionProfileName(TEXT("Ragdoll"));
		Body->SetSimulatePhysics(true);
		Body->WakeAllRigidBodies();
	}
	// 第一人称手臂 + 武器藏掉（自己死后不该再看到悬空的手臂）
	if (USkeletalMeshComponent* FP = GetFirstPersonMesh())
	{
		FP->SetVisibility(false, true);
	}
	if (WeaponMesh)
	{
		WeaponMesh->SetVisibility(false, true);
	}
}
