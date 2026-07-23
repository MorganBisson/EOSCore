#pragma once

#include "CoreMinimal.h"

class FEOSSessionCodeGenerator final
{
public:
	static FString Generate(int32 Length);
	static FString Normalize(const FString& Code);
	static bool IsValid(const FString& Code, int32 ExpectedLength);

private:
	static constexpr int32 MaxCodeLength = 25;
};
