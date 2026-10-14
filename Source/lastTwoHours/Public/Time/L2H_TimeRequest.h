// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "L2H_TimeRequest.generated.h"

class AL2H_BaseCharacter;

/**
 * One live time request, in the shape the answerer's screen needs it.
 *
 * It is deliberately the whole story and nothing more: who is asking, what they want, what it costs the
 * person answering, and when it stops mattering. The pair of numbers is the point -- a request is the
 * same gift a transfer is, so the answerer always pays more than the requester receives, and showing
 * both is what makes the answer an informed one rather than a reflex.
 */
USTRUCT(BlueprintType)
struct FL2H_TimeRequest
{
	GENERATED_BODY()

	/** Who is asking. Null when no request is live. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request")
	TObjectPtr<AL2H_BaseCharacter> Requester = nullptr;

	/** Seconds the requester gains if the answer is yes. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request")
	float Amount = 0.0f;

	/** Seconds the answerer spends -- Amount plus the transfer tax. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request")
	float Cost = 0.0f;

	/** World seconds at which the request lapses unanswered. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request")
	float ExpiresAtSeconds = 0.0f;

	/** True while this request is waiting for an answer. */
	UPROPERTY(BlueprintReadOnly, Category = "L2H|Request")
	bool bActive = false;
};
