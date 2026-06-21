#include "FPSCharacter.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "InputCoreTypes.h"
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
#include "FPSGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Sound/SoundBase.h"
#include "mmmfps.h"

AFPSCharacter::AFPSCharacter()
{
	// 联机第一步：打开复制开关。本 pawn 必须从“服务器”复制到所有客户端，
	// 别人才看得见你、你也看得见别人。ACharacter 自带的移动组件在 bReplicates
	// 打开后会自动复制位置/旋转（本地控制端做预测、其他端做平滑插值）——
	// 所以“互相可见、能跑动”这一步，只要这一个开关。
	bReplicates = true;

	// 需要每帧把相机 FOV 平滑插值（举枪缩放），所以开启 Tick
	PrimaryActorTick.bCanEverTick = true;

	// 第一人称武器模型：挂在手臂网格上、只有本玩家看得见。
	// 用哪把枪(SKM_Rifle)、挂在手部哪个插槽，都在 BP 里配——位置要可视化微调，写死在 C++ 反而麻烦。
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	// 挂回手臂的握枪插槽 HandGrip_R——枪始终被正确握持，不用手调位置。
	// ADS 程序化瞄准改走「IK 移动手」方案（我们的手臂动画没驱动 ik_hand_gun，且枪直接挂它上会脱离握持）：
	// 动画蓝图里把 ik_hand_gun 当【IK 目标】——Copy Bone 把它贴到 hand_r → Modify Bone 加瞄准偏移/旋转 →
	// Two Bone IK 把真手 hand_r 拉到 ik_hand_gun；枪挂在 hand_r 上，就跟着手一起对齐到准星。
	// 注意：native 组件的挂载插槽只能在 C++ 这里指定（BP 里父级插槽只读）。
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

	// ADS 目前只做相机 FOV 缩放 + 散布收紧（见 Tick / HandleFire，按 bIsAiming）。
	// “举枪到准星”的视模型抬枪：Control Rig 方案(绑定是黑盒、不响应)与 C++ 视模型方案(俯仰跟随会乱甩)
	// 都已撤回，相机/手臂保持模板原样、这里不重挂；后续按教程另做。
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHP = MaxHP;
	CurrentAmmo = MagazineSize;   // 开局满弹匣

	// 记下默认视场角（放下瞄准时恢复到它）
	if (const UCameraComponent* Cam = GetFirstPersonCameraComponent())
	{
		DefaultFOV = Cam->FieldOfView;
	}

	// 进游戏后把输入模式切回"仅游戏"+ 隐藏光标。
	// 为什么：从主菜单切过来时，视口可能还停在菜单设的"仅 UI 输入"状态，
	// 会把移动/视角/开火等游戏输入全挡掉。只对【本地控制的玩家】做（别人的代理 pawn 不碰）。
	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}

		// 本地玩家：把自己在主菜单选的名字上报服务器，设到 PlayerState（再复制给所有人，记分板/击杀提示才显示自选名）。
		// 名字存在 GameInstance——它跨关卡切换不销毁，所以"加入并切到游戏关卡"之后这里仍读得到自己选的名。
		// 关键：放在 pawn 的 BeginPlay（此时登录握手已结束）上报，能压过引擎握手期间写进去的电脑名 → 名字才稳定生效。
		if (const UFPSGameInstance* GI = GetGameInstance<UFPSGameInstance>())
		{
			if (!GI->PlayerName.IsEmpty())
			{
				Server_SetPlayerName(GI->PlayerName);
			}
		}
	}
}

void AFPSCharacter::Server_SetPlayerName_Implementation(const FString& NewName)
{
	// —— 运行在服务器 ——
	// 清洗 + 限长（客户端上报的内容不可全信，服务器再夹一道：去首尾空白、最多 16 字）。
	FString Clean = NewName;
	Clean.TrimStartAndEndInline();
	Clean = Clean.Left(16);
	if (Clean.IsEmpty())
	{
		return;
	}

	// 优先走 GameMode 的官方改名通道 ChangeName（它内部会 SetPlayerName 并走通知流程，最规范）；
	// 万一拿不到 GameMode，就直接设 PlayerState 兜底。设到的是【服务器端】PlayerState → 引擎自动把名字复制给所有客户端。
	if (AController* C = GetController())
	{
		if (AGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AGameModeBase>() : nullptr)
		{
			GM->ChangeName(C, Clean, /*bNameChange=*/false);
			UE_LOG(Logmmmfps, Warning, TEXT("[名字] 收到客户端上报，经 GameMode 设为「%s」"), *Clean);
			return;
		}
	}
	if (APlayerState* PS = GetPlayerState())
	{
		PS->SetPlayerName(Clean);
		UE_LOG(Logmmmfps, Warning, TEXT("[名字] 收到客户端上报，直设 PlayerState 为「%s」"), *Clean);
	}
}

void AFPSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 只给本地玩家做。① 相机 FOV 缩放(放大)；② 给 FP 动画蓝图算好程序化瞄准 IK 要用的目标值。
	if (IsLocallyControlled())
	{
		if (UCameraComponent* Cam = GetFirstPersonCameraComponent())
		{
			// ① 相机 FOV 缩放：瞄准拉近视场、松开恢复（散布收紧在 HandleFire）
			const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;
			Cam->SetFieldOfView(FMath::FInterpTo(Cam->FieldOfView, TargetFOV, DeltaSeconds, AimInterpSpeed));

			// ①.5 举枪混合系数：和 FOV 同速平滑插值(0↔1)，给动画蓝图的 IK Alpha 用 → 抬枪和放大同步。
			AimAlpha = FMath::FInterpTo(AimAlpha, bIsAiming ? 1.f : 0.f, DeltaSeconds, AimInterpSpeed);

			// ② 程序化瞄准的数据准备：算"瞄准时手该到哪、转多少"，供 FP 动画蓝图的 IK 用。
			//    这里【只算值、不移动任何东西】；真正移动手臂由动画蓝图的 Modify Bone + Two Bone IK 完成。
			//    （公式同文章：目标位置 = 手位置 + (相机位置 − 枪上AimSocket位置)；目标旋转 = 相机旋转。）
			if (const USkeletalMeshComponent* FPMesh = GetFirstPersonMesh())
			{
				const FVector HandLoc = FPMesh->GetSocketLocation(FName("hand_r"));

				// 第一帧缓存枪相对手的固定量(枪刚性挂手上 → 都是常量)：
				//  ① 枪坐标系旋转(X=枪管前向、Z=枪上方)；枪管前向用 Muzzle−AimSocket 的【位置差】算(最稳)。
				//  ② AimSocket 在手局部的位置偏移(给位置前馈用)。
				if (WeaponMesh && !bADSGunBasisCached && WeaponMesh->DoesSocketExist(FName("AimSocket")))
				{
					// 方向参考插槽：优先用 AimDirSocketName(前准星)，没建好则回退 Muzzle(枪口)。
					const FName DirSocket = WeaponMesh->DoesSocketExist(AimDirSocketName) ? AimDirSocketName : FName("Muzzle");
					if (WeaponMesh->DoesSocketExist(DirSocket))
					{
						const FQuat HandQ = FPMesh->GetSocketQuaternion(FName("hand_r"));
						const FVector AimW = WeaponMesh->GetSocketLocation(FName("AimSocket"));
						const FVector DirW = WeaponMesh->GetSocketLocation(DirSocket);
						const FVector BarrelLocal = HandQ.UnrotateVector((DirW - AimW).GetSafeNormal());                  // 瞄准基线前向(手局部)
						const FVector UpLocal = HandQ.UnrotateVector(WeaponMesh->GetSocketQuaternion(FName("AimSocket")).GetUpVector()); // 枪上方(手局部)
						ADSGunBasisLocal = FRotationMatrix::MakeFromXZ(BarrelLocal, UpLocal).ToQuat();
						ADSAimOffsetLocal = HandQ.UnrotateVector(AimW - HandLoc);                                          // AimSocket 在手局部的位置
						bADSGunBasisCached = true;
					}
				}

				// 瞄准目标点：相机前方 AimForwardOffset 处。视线轴上的点都投影到准星中心 → 枪落准星。
				const FVector AimTarget = Cam->GetComponentLocation() + Cam->GetForwardVector() * AimForwardOffset;

				if (bADSGunBasisCached)
				{
					// 旋转：枪管→相机前方、枪顶→相机上方。手世界旋转 = 目标坐标系 · 枪坐标系⁻¹。
					const FQuat TgtBasis = FRotationMatrix::MakeFromXZ(Cam->GetForwardVector(), Cam->GetUpVector()).ToQuat();
					const FQuat DesiredRot = TgtBasis * ADSGunBasisLocal.Inverse();
					ADSSocketRot = DesiredRot.Rotator();
					// 位置(前馈，零滞后)：手 = 目标 − 旋转后的(手→AimSocket)偏移 → AimSocket 精确落到目标点。
					// 全程只用当前相机 + 常量，不读上一帧姿势，所以快速甩视角也不掉准。
					ADSSocketLoc = AimTarget - DesiredRot.RotateVector(ADSAimOffsetLocal);
				}
				else
				{
					// 插槽还没就绪时的退化值(老式：读当前 AimSocket 现位置)
					const FVector AimLoc = WeaponMesh ? WeaponMesh->GetSocketLocation(FName("AimSocket")) : HandLoc;
					ADSSocketRot = Cam->GetComponentRotation();
					ADSSocketLoc = HandLoc + (AimTarget - AimLoc);
				}

				// 调试可视化：直接画出几何关系，肉眼判断对齐。绿=目标点、红=实际AimSocket、品红=实际枪管、黄=视线轴。
				if (bShowAimDebug && WeaponMesh && WeaponMesh->DoesSocketExist(FName("AimSocket")))
				{
					UWorld* W = GetWorld();
					const FVector AimW = WeaponMesh->GetSocketLocation(FName("AimSocket"));
					const FName DbgDir = WeaponMesh->DoesSocketExist(AimDirSocketName) ? AimDirSocketName : FName("Muzzle");
					const FVector MuzW = WeaponMesh->DoesSocketExist(DbgDir) ? WeaponMesh->GetSocketLocation(DbgDir) : AimW;
					const FVector CamL = Cam->GetComponentLocation();
					DrawDebugSphere(W, AimTarget, 1.5f, 8, FColor::Green, false, -1.f, 0, 0.25f);
					DrawDebugSphere(W, AimW, 1.5f, 8, FColor::Red, false, -1.f, 0, 0.25f);
					DrawDebugLine(W, AimW, MuzW, FColor::Magenta, false, -1.f, 0, 0.3f);
					DrawDebugLine(W, CamL, CamL + Cam->GetForwardVector() * 80.f, FColor::Yellow, false, -1.f, 0, 0.2f);
				}
			}
		}
	}
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
		if (AimAction)
		{
			// 右键按下→举枪，松开→放下
			EIC->BindAction(AimAction, ETriggerEvent::Started, this, &AFPSCharacter::StartAiming);
			EIC->BindAction(AimAction, ETriggerEvent::Completed, this, &AFPSCharacter::StopAiming);
		}
	}

	// 直接绑鼠标右键举枪——开箱即用，无需创建/指派 Input Action 资产。
	// （右键默认没被占用；若你以后想换键，可改成在 BP 给 AimAction 指派 IA_Aim，两条都触发无副作用）
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AFPSCharacter::StartAiming);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AFPSCharacter::StopAiming);
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

void AFPSCharacter::StartAiming()
{
	if (IsDead())
	{
		return;
	}
	bIsAiming = true;   // Tick 会据此把 FOV 缩到 AimFOV；HandleFire 据此缩小散布
}

void AFPSCharacter::StopAiming()
{
	bIsAiming = false;
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
	// 举枪(ADS)时散布大幅缩小，更精准；不举枪用原始散布
	const float EffectiveSpread = bIsAiming ? CurrentSpreadDegrees * AimSpreadMultiplier : CurrentSpreadDegrees;
	const float SpreadRad = FMath::DegreesToRadians(EffectiveSpread);
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

	// 不管打没打中，都广播一次开火表现：身体开火动画 + 枪口火光/闪光/枪声 +（命中则）命中特效。
	// 带上命中点(没中就传射线终点)，供客户端在正确位置播命中特效。
	Multicast_PlayFireFX(bHit ? Hit.Location : End, bHit);
}

void AFPSCharacter::Multicast_PlayFireFX_Implementation(FVector HitLocation, bool bHit)
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

	// 枪口火光 + 闪光灯 + 枪声（每台机器各自挂在自己看得见的那把枪上，见函数内部）。
	PlayMuzzleFlash();

	// 命中特效：这一枪打中了、且 BP 里指定了 ImpactFX，就在命中点播一下。
	if (bHit && ImpactFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactFX, HitLocation);
	}
}

void AFPSCharacter::PlayMuzzleFlash()
{
	// 挂哪把枪？本机玩家看的是第一人称枪(WeaponMesh)，其他客户端看你的是第三人称枪(TPWeaponMesh)。
	// 各台机器都用"自己这台看得见的那把"，火光才出现在对的地方。
	USkeletalMeshComponent* MuzzleMesh = IsLocallyControlled() ? WeaponMesh : TPWeaponMesh;
	if (!MuzzleMesh || !MuzzleMesh->DoesSocketExist(MuzzleSocketName))
	{
		return;
	}

	// ① 枪口火光粒子：只有在 BP 指定了 Niagara 时才播；贴在枪口插槽上、自动销毁。
	if (MuzzleFlashFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlashFX, MuzzleMesh, MuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true);
	}

	// ② 枪口闪光灯：无需任何素材、永远有效。在枪口生成一盏瞬时点光源，定时销毁 → 一闪即灭。
	if (MuzzleFlashLightIntensity > 0.f)
	{
		UPointLightComponent* Flash = NewObject<UPointLightComponent>(this);
		if (Flash)
		{
			Flash->SetMobility(EComponentMobility::Movable);            // 运行时创建、要跟着枪动，必须 Movable
			Flash->RegisterComponent();                                // 注册后才会进入世界、参与渲染
			Flash->AttachToComponent(MuzzleMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, MuzzleSocketName);
			Flash->SetIntensity(MuzzleFlashLightIntensity);
			Flash->SetLightColor(FLinearColor(MuzzleFlashLightColor));
			Flash->SetAttenuationRadius(MuzzleFlashLightRadius);
			Flash->SetCastShadows(false);                              // 闪光不投影，省性能也更干净

			// MuzzleFlashLightTime 秒后销毁这盏灯。用弱指针防止角色已销毁时回调崩溃。
			TWeakObjectPtr<UPointLightComponent> WeakFlash(Flash);
			FTimerHandle FlashTimer;
			GetWorld()->GetTimerManager().SetTimer(FlashTimer, [WeakFlash]()
			{
				if (UPointLightComponent* L = WeakFlash.Get())
				{
					L->DestroyComponent();
				}
			}, MuzzleFlashLightTime, false);
		}
	}

	// ③ 枪声：只有在 BP 指定了音效时才播；挂在枪口随枪移动。
	if (FireSound)
	{
		UGameplayStatics::SpawnSoundAttached(FireSound, MuzzleMesh, MuzzleSocketName);
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
