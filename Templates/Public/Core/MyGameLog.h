// MyGameLog.h
// PROJECT TEMPLATE - log category of the game module.
//
// Copy to Source/<YourGame>/Public/Core/ and rename (see Templates/README.md).
// CoreFramework uses the same pattern: it declares LogCoreUI in a header and defines it in one .cpp.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// PROJECT-SPECIFIC: the rename script turns MYGAME_API into <YOURGAME>_API and LogMyGame into Log<YourGame>.
MYGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogMyGame, Log, All);
