#include "Session/EOSSessionCodeGenerator.h"

#include "Misc/Guid.h"

FString FEOSSessionCodeGenerator::Generate(const int32 Length)
{
	const int32 SafeLength = FMath::Clamp(Length, 1, MaxCodeLength);
	return FGuid::NewGuid().ToString(EGuidFormats::Base36Encoded).Left(SafeLength).ToUpper();
}

FString FEOSSessionCodeGenerator::Normalize(const FString& Code)
{
	FString Normalized = Code.TrimStartAndEnd().ToUpper();
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	return Normalized;
}

bool FEOSSessionCodeGenerator::IsValid(const FString& Code, const int32 ExpectedLength)
{
	const FString Normalized = Normalize(Code);
	if (Normalized.Len() != ExpectedLength || ExpectedLength <= 0 || ExpectedLength > MaxCodeLength)
	{
		return false;
	}

	for (const TCHAR Character : Normalized)
	{
		const bool bIsAsciiDigit = Character >= TEXT('0') && Character <= TEXT('9');
		const bool bIsAsciiUppercase = Character >= TEXT('A') && Character <= TEXT('Z');
		if (!bIsAsciiDigit && !bIsAsciiUppercase)
		{
			return false;
		}
	}

	return true;
}
