// Copyright Cody McCarty. All Rights Reserved.

#include "SandCoreLogToolsBPLibrary.h"

DEFINE_LOG_CATEGORY(LogBPGame);

FString USandCoreLogToolsBPLibrary::GetCallerContext(const UObject* WorldContextObject, const FString& Message, const TCHAR* Function)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	if (WorldContextObject)
	{
		FString Result = FString::Printf(TEXT("\t [%s] | \"%s\"\t | %s"),
			*BuildPieRole(WorldContextObject),
			*Message,
			*BuildStackInfoWithLabel(WorldContextObject, Function));
		return Result;
	}
#endif
	return Message;
}

FString USandCoreLogToolsBPLibrary::BuildPieRole(const UObject* WorldContextObject)
{
	FString Result = TEXT("Invalid");

#if WITH_EDITOR

	if (!GIsEditor)
	{
		return Result;
	}

	if (WorldContextObject == nullptr)
	{
		Result = TEXT("Not in a play world");
		return Result;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		Result = TEXT("(World Being Created)");
	}
	else if (World->WorldType == EWorldType::Editor)
	{
		Result = TEXT("Editor");
	}
	else if (World->IsGameWorld())
	{
		const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World);

		switch (World->GetNetMode())
		{
		case NM_Standalone:
			Result = TEXT("Standalone");
			break;
		case NM_ListenServer:
			Result = TEXT("Server L");
			break;
		case NM_DedicatedServer:
			Result = TEXT("Server D");
			break;
		case NM_Client:
			{
				//~ use either UE::GetPlayInEditorID() or GPlayInEditorID depending on the UE version.
				const int32 PieNum = WorldContext ? WorldContext->PIEInstance : UE::GetPlayInEditorID();
				Result = FString::Printf(TEXT("Client %d"), PieNum);
			}
			break;
		default:
			unimplemented();
		};

		if (WorldContext && !WorldContext->CustomDescription.IsEmpty())
		{
			Result += TEXT(" ") + WorldContext->CustomDescription;
		}
	}

#endif

	return Result;
}

FString USandCoreLogToolsBPLibrary::BuildStackInfoWithLabel(const UObject* WorldContextObject, const TCHAR* Function)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	/* Use this? if (GEditor)
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEditor))
		{
			TOptional<FPlayInEditorSessionInfo> SessionInfo = EditorEngine->GetPlayInEditorSessionInfo();
			double PlayRequestStartTime = SessionInfo->PlayRequestStartTime;
			// ... */

	TStringBuilder<256> Result; // todo reserve
	Result << TEXT("Cpp=") << Function;

	Result.Append(TEXT(" | Label="));
	if (!WorldContextObject)
	{
		Result.Append(TEXT("NA"));
	}
	else
	{
		// todo: spawned in vs placed with _C? update code and docs
		if (const AActor* Actor = Cast<const AActor>(WorldContextObject))
		{
			Result.Append(Actor->GetActorNameOrLabel());
		}
		else if (const AActor* TypedOuter = WorldContextObject->GetTypedOuter<AActor>(); ensure(TypedOuter))
		{
			// todo: it should be able to find the TypedOuter, when can it not?
			Result.Append(TypedOuter->GetActorNameOrLabel());
		}
		else
		{
			Result.Append(WorldContextObject->GetName());
		}
	}

	Result.Append(TEXT(" | BP="));
	const TArrayView<const FFrame* const> ScriptStack = FBlueprintContextTracker::Get().GetCurrentScriptStack();
	if (ScriptStack.IsEmpty())
	{
		Result.Append(TEXT("EmptyBPStack"));
	}
	else
	{
		//~ NOTE: ScriptStack.Last()->Node->GetPackage()->GetFName(); //"/Game/BP_MyActor"
		//~ NOTE: WorldContextObject->GetClass()->GetFName(); //"BP_MyActor_C"
		//~ NOTE: ScriptStack.Last()->Node->GetOuter()->GetName();
		if (!ScriptStack.Last()->Node->GetName().Contains(TEXT("ExecuteUbergraph")))
		{
			Result << ScriptStack.Last()->Node->GetPackage()->GetFName() << TEXT("..") << ScriptStack.Last()->Node->GetName();
		}
		else if (ScriptStack.Num() >= 2 && !ScriptStack[ScriptStack.Num() - 2]->Node->GetName().Contains(TEXT("ExecuteUbergraph")))
		{
			Result << ScriptStack.Last()->Node->GetPackage()->GetFName() << TEXT("..") << ScriptStack[ScriptStack.Num() - 2]->Node->GetName();
		}
		else
		{
			Result.Append(ScriptStack.Last()->Node->GetName());
		}
	}

	return FString(Result.ToString());

#else
	return FString();
#endif
}

void USandCoreLogToolsBPLibrary::SandCoreLogGame(const UObject* WorldContextObject, ESandCoreLogVerbosity Verbosity/*Log*/, const FText Message/*Hello*/)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING //~ Do not Print in Shipping or Test unless explicitly enabled.
	// todo: test with USE_LOGGING_IN_SHIPPING
	TStringBuilder<512> Result;
	Result << TEXT("\t [") << BuildPieRole(WorldContextObject) << TEXT("]");

	Result << TEXT(" | \"") << Message.ToString() << TEXT("\"\t");

	Result.Append(TEXT(" | BP="));
	const TArrayView<const FFrame* const> ScriptStack = FBlueprintContextTracker::Get().GetCurrentScriptStack();
	if (ScriptStack.IsEmpty())
	{
		if (WorldContextObject)
		{
			Result.Append(WorldContextObject->GetClass()->GetName());
		}
		else
		{
			Result.Append(TEXT("Empty BP stack"));
		}
	}
	else
	{
		if (!ScriptStack.Last()->Node->GetName().Contains(TEXT("ExecuteUbergraph")))
		{
			Result << ScriptStack.Last()->Node->GetPackage()->GetFName() << TEXT("..") << ScriptStack.Last()->Node->GetName();
		}
		else if (ScriptStack.Num() >= 2 && !ScriptStack[ScriptStack.Num() - 2]->Node->GetName().Contains(TEXT("ExecuteUbergraph")))
		{
			Result << ScriptStack.Last()->Node->GetPackage()->GetFName() << TEXT("..") << ScriptStack[ScriptStack.Num() - 2]->Node->GetName();
		}
		else
		{
			Result.Append(ScriptStack.Last()->Node->GetName());
		}
	}

	Result.Append(TEXT(" | Label="));
	if (!WorldContextObject)
	{
		Result.Append(TEXT("Label Unavailable"));
	}
	else
	{
		// todo: spawned in vs placed with _C? update code and docs
		if (const AActor* Actor = Cast<const AActor>(WorldContextObject))
		{
			Result.Append(Actor->GetActorNameOrLabel());
		}
		else if (const AActor* TypedOuter = WorldContextObject->GetTypedOuter<AActor>(); ensure(TypedOuter))
		{
			Result.Append(TypedOuter->GetActorNameOrLabel());
		}
		else
		{
			Result.Append(WorldContextObject->GetName());
		}
	}

	Result.Append(TEXT(" | Cpp="));
	FString LastCppCall(TEXT("No Useful Symbol"));
	{
		constexpr int32 MaxDepth = 32;
		uint64 BackTrace[MaxDepth] = {0};
		const int32 Depth = FPlatformStackWalk::CaptureStackBackTrace(BackTrace, UE_ARRAY_COUNT(BackTrace));
		static const TSet<FString> ModuleBlackList =
		{
			TEXT("UnrealEditor-CoreUObject.dll"),
			TEXT("UnrealEditor-Core.dll"),
			TEXT("SandCoreLogTools"),
		};
		for (int32 i = 0; i < Depth; ++i)
		{
			FProgramCounterSymbolInfo SymbolInfo{};
			FPlatformStackWalk::ProgramCounterToSymbolInfo(BackTrace[i], SymbolInfo);
			FString ModuleName = ANSI_TO_TCHAR(SymbolInfo.ModuleName);
			bool bSkip = false;
			for (const FString& SkipModule : ModuleBlackList)
			{
				if (ModuleName.Contains(SkipModule))
				{
					bSkip = true;
					break;
				}
			}
			if (!bSkip)
			{
				LastCppCall = ANSI_TO_TCHAR(SymbolInfo.FunctionName);
				if (!LastCppCall.Contains(TEXT("ProcessEvent")))
				{
					break;
				}
			}
		}
	}
	Result.Append(LastCppCall);

	const FString FinalLogString = Result.ToString();

	switch (Verbosity)
	{
	case ESandCoreLogVerbosity::Fatal:
		UE_LOG(LogBPGame, Fatal, TEXT("%s"), *FinalLogString);
		break;
	case ESandCoreLogVerbosity::Error:
		{
			const uint64 Key = GetTypeHash(GetNameSafe(WorldContextObject));
			GEngine->AddOnScreenDebugMessage(Key, 10.f, FColor::Red, FinalLogString);
			UE_LOG(LogBPGame, Error, TEXT("%s"), *FinalLogString);
		}
		break;
	case ESandCoreLogVerbosity::Warning:
		{
			const uint64 Key = GetTypeHash(GetNameSafe(WorldContextObject));
			GEngine->AddOnScreenDebugMessage(Key, 10.f, FColor::Orange, FinalLogString);
			UE_LOG(LogBPGame, Warning, TEXT("%s"), *FinalLogString);
		}
		break;
	case ESandCoreLogVerbosity::Display:
		UE_LOG(LogBPGame, Display, TEXT("%s"), *FinalLogString);
		break;
	case ESandCoreLogVerbosity::Log:
		UE_LOG(LogBPGame, Log, TEXT("%s"), *FinalLogString);
		break;
	case ESandCoreLogVerbosity::Verbose:
		UE_LOG(LogBPGame, Verbose, TEXT("%s"), *FinalLogString);
		break;
	case ESandCoreLogVerbosity::VeryVerbose:
		UE_LOG(LogBPGame, VeryVerbose, TEXT("%s"), *FinalLogString);
		break;
	default: unimplemented();
	}
#endif
}
