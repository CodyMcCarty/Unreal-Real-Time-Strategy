// Copyright Cody McCarty. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SandCoreLogToolsBPLibrary.generated.h"

SANDCORELOGTOOLS_API DECLARE_LOG_CATEGORY_EXTERN(LogBPGame, Log, All);

#if !NO_LOGGING
/** Use like a normal UE_LOG. eg. INFO_LOG(LogTemp, Warning, TEXT("MyNum=%.2f IsCrouching=%s"), Num, *LexToString(bIsCrouching)); */
#define INFO_LOG(CategoryName, Verbosity, Format, ...) \
	{ \
		FString _MyLogMsgWithContextInfo = USandCoreLogToolsBPLibrary::GetCallerContext(this, FString::Printf(Format, ##__VA_ARGS__), ANSI_TO_TCHAR(__FUNCTION__)); \
		UE_LOG(CategoryName, Verbosity, TEXT("%s"), *_MyLogMsgWithContextInfo); \
	};

/** Use like a normal UE_CLOG. eg. INFO_CLOG(bMyCondition, LogTemp, Warning, TEXT("MyNum=%.2f IsCrouching=%s"), Num, *LexToString(bIsCrouching)); */
#define INFO_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
	{ \
		if (Condition) \
		{ \
			FString _MyLogMsgWithContextInfo = USandCoreLogToolsBPLibrary::GetCallerContext(this, FString::Printf(Format, ##__VA_ARGS__), ANSI_TO_TCHAR(__FUNCTION__)); \
			UE_CLOG(Condition, CategoryName, Verbosity, TEXT("%s"), *_MyLogMsgWithContextInfo); \
		} \
		else { UE_CLOG(Condition, CategoryName, Verbosity, Format, ##__VA_ARGS__); } \
	}
#else
#define INFO_LOG(CategoryName, Verbosity, Format, ...) \
	UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__);

#define INFO_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
	UE_CLOG(Condition, CategoryName, Verbosity, Format, ##__VA_ARGS__);
#endif


/** todo WIP */
// #define INFO_LOGFMT()

/* todo:
 * I'm unsure about #if !NO_LOGGING. Is that the correct one? Test by making a heavy function in shipping.
 * more tests in BP.
 * FunctionLibrary & WorldContextObject? Consider passing a ContextObject to a macro. This could cover any other case I couldn't think of.
 * more test in shipping & shipping with logging.
 * BP CLOG when I need one.
 * /

/** Enum that mirrors the verbosity levels of the logging system for BP. */
UENUM(BlueprintType)
enum class ESandCoreLogVerbosity : uint8
{
	/** Always prints a fatal error to console (and log file) and crashes (even if logging is disabled) */
	Fatal,

	/** 
	 * Prints an error to console and onscreen message. (and log file). 
	 * Commandlets and the editor collect and report errors. Error messages result in commandlet failure.
	 */
	Error,

	/** 
	 * Prints a warning to console and onscreen message (and log file).
	 * Commandlets and the editor collect and report warnings. Warnings can be treated as an error.
	 */
	Warning,

	/** Prints a message to console (and log file) */
	Display,

	/** Prints a message to a log file (does not print to console) */
	Log,

	/** 
	 * Prints a verbose message to a log file (if Verbose logging is enabled for the given category, 
	 * usually used for detailed logging) 
	 */
	Verbose,

	/** 
	 * Prints a verbose message to a log file (if VeryVerbose logging is enabled, 
	 * usually used for detailed logging that would otherwise spam output) 
	 */
	VeryVerbose,
};

/**
 * UGameLoggingBPLibrary
 *
 * A utility Blueprint function library that provides enhanced logging tools for both C++ and Blueprint gameplay programmers.
 * 
 * Features:
 * - Adds context to logs including:
 *   - Network role (e.g., Listen Server, Client)
 *   - Actor label (as it appears in the World Outliner)
 *   - Calling function name
 *   - Last relevant Blueprint or C++ call in the call stack
 * - Supports both Blueprint and C++ usage via:
 *   - A Blueprint-callable `LogGame()` function
 *   - A C++ macro `INFO_LOG(...)` for convenient use in native code
 *
 * Intended Usage:
 * - Helpful for debugging multiplayer games with replicated actors
 * - Designed for gameplay programmers to identify which actor or system is producing logs
 * - Works in both PIE and standalone modes
 *
 * Limitations:
 * - Is used in development builds only
 * - Not intended for performance-critical code due to overhead of getting contextual info.
 */
UCLASS()
class SANDCORELOGTOOLS_API USandCoreLogToolsBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** You're probably looking for the macro `INFO_LOG()` above */
	static FString GetCallerContext(const UObject* WorldContextObject, const FString& Message, const TCHAR* Function);

	/** Is used by the INFO_LOG() macro. Generally returns "Server T" or "Client #" */
	static FString BuildPieRole(const UObject* WorldContextObject);

	/** Is used by the INFO_LOG() macro. Returns the caller's function, Actor label, and the last BP call. Function should be `ANSI_TO_TCHAR(__FUNCTION__)` */
	static FString BuildStackInfoWithLabel(const UObject* WorldContextObject, const TCHAR* Function);

	/**
	 * Logs a message with enhanced contextual information.
	 *
	 * The log includes:
	 * - The local network role (e.g., client, server)
	 * - Your message
	 * - The calling function name (e.g., BP_MyActor..BeginPlay)
	 * - The actor label (as seen in the World Outliner)
	 * - The last relevant call in the C++ call stack (e.g., AActor::BeginPlay)
	 *
	 * Additional Behavior:
	 * - Warnings and errors will also display on screen (in addition to the output log)
	 * - Output is pipe-delimited (|) for easy export to spreadsheets
	 * - (todo: is this correct?) Actor labels with '_C' indicate spawned Blueprints (which appear yellow in the World Outliner)
	 *
	 * Usage Notes:
	 * - Meant for development/debugging only (hidden in shipping builds)
	 *		- Best avoid use in tick for performance reasons
	 * - Ideal for multiplayer debugging to identify which instance or player context logged the message
	 * - Uses the log category 'LogBPGame'
	 *
	 * @param Verbosity The severity level (Log, Warning, Error, etc.)
	 * @param Message The message text to display/log
	 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", CallableWithoutWorldContext, Keywords = "log print", DevelopmentOnly, DisplayName="LogGame"), Category="Development")
	static void SandCoreLogGame(const UObject* WorldContextObject, ESandCoreLogVerbosity Verbosity = ESandCoreLogVerbosity::Log, const FText Message = INVTEXT("Hello"));
};
