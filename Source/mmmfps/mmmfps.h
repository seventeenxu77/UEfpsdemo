// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(Logmmmfps, Log, All);

/** 子弹射线专用碰撞通道（在 DefaultEngine.ini 里注册为 "BulletTrace"）。
 *  不能用 ECC_Visibility：角色的胶囊体/网格默认忽略 Visibility，射线会穿过敌人。 */
#define ECC_BulletTrace ECC_GameTraceChannel2