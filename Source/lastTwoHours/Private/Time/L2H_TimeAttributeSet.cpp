// Copyright Epic Games, Inc. All Rights Reserved.

#include "Time/L2H_TimeAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Time/L2HTimeLog.h"

UL2H_TimeAttributeSet::UL2H_TimeAttributeSet()
{
	// Starting values only. DA_L2H_LevelTimeConfig overwrites all of these on BeginPlay.
	InitTime(120.0f);
	InitMaxTime(120.0f);
	InitTimeDrainRate(1.0f);

	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitIncomingDamage(0.0f);
}

void UL2H_TimeAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UL2H_TimeAttributeSet, Time, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UL2H_TimeAttributeSet, MaxTime, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UL2H_TimeAttributeSet, TimeDrainRate, COND_None, REPNOTIFY_Always);

	// IncomingDamage is absent on purpose: it is authority-only scratch, consumed inside the same
	// effect execution that wrote it, so there is nothing for a client to receive.
	DOREPLIFETIME_CONDITION_NOTIFY(UL2H_TimeAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UL2H_TimeAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UL2H_TimeAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetTimeAttribute())
	{
		// Clamped against MaxTime so a big restore effect can never overshoot the clock.
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxTime());
	}
	else if (Attribute == GetMaxTimeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetTimeDrainRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(GetMaxHealth(), 0.0f));
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		// 0 is legal and means "this actor has no health layer", so it is only floored, not forced up.
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UL2H_TimeAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetTimeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxTime());
	}
	else if (Attribute == GetMaxTimeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetTimeDrainRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(GetMaxHealth(), 0.0f));
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UL2H_TimeAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetTimeAttribute())
	{
		// Nothing is destroyed here. Time reaching 0 switches the owner to Downed, and the
		// listener (the pawn and the time director) reacts to the attribute change.
		L2HTime::LogVerbose(FString::Printf(TEXT("Time = %.2f / %.2f"), GetTime(), GetMaxTime()));
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// The canonical health pattern: damage arrives on a scratch attribute and is converted here,
		// so a heal and a hit can never disagree about what the current value was. The conversion is
		// deliberately local -- it needs no level config, which is what keeps this the one place a hit
		// is resolved and the director the one place Time moves.
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (Damage <= 0.0f)
		{
			return;
		}

		// A target with MaxHealth 0 never opted into the health layer; its damage stays pure time.
		if (GetMaxHealth() <= 0.0f)
		{
			return;
		}

		const float HealthBefore = GetHealth();
		const float HealthAfter = FMath::Clamp(HealthBefore - Damage, 0.0f, GetMaxHealth());
		SetHealth(HealthAfter);

		L2HTime::LogImportant(FString::Printf(TEXT("Health: %.1f - %.1f = %.1f / %.1f"),
			HealthBefore, Damage, HealthAfter, GetMaxHealth()));
	}
}

void UL2H_TimeAttributeSet::OnRep_Time(const FGameplayAttributeData& OldTime)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UL2H_TimeAttributeSet, Time, OldTime);
}

void UL2H_TimeAttributeSet::OnRep_MaxTime(const FGameplayAttributeData& OldMaxTime)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UL2H_TimeAttributeSet, MaxTime, OldMaxTime);
}

void UL2H_TimeAttributeSet::OnRep_TimeDrainRate(const FGameplayAttributeData& OldTimeDrainRate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UL2H_TimeAttributeSet, TimeDrainRate, OldTimeDrainRate);
}

void UL2H_TimeAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UL2H_TimeAttributeSet, Health, OldHealth);
}

void UL2H_TimeAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UL2H_TimeAttributeSet, MaxHealth, OldMaxHealth);
}
