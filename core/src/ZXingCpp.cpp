/*
* Copyright 2024 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ZXingCpp.h"

#include "Version.h"

namespace ZXing {

const std::string& Version()
{
	static std::string res = ZXING_VERSION_STR;
	return res;
}

#ifdef ZXING_EXPERIMENTAL_API

BarcodeFormats SupportedBarcodeFormats(Operation op)
{
	switch (op) {
	case Operation::Read:
#ifdef ZXING_READERS
		return BarcodeFormat::Any;
#else
		return BarcodeFormat::None;
#endif
	case Operation::Create:
#if defined(ZXING_WRITERS) && defined(ZXING_EXPERIMENTAL_API)
		return BarcodeFormats(BarcodeFormat::Any).setFlag(BarcodeFormat::DXFilmEdge, false);
#else
		return BarcodeFormat::None;
#endif
	case Operation::CreateAndRead: return SupportedBarcodeFormats(Operation::Create) & SupportedBarcodeFormats(Operation::Read);
	case Operation::CreateOrRead: return SupportedBarcodeFormats(Operation::Create) | SupportedBarcodeFormats(Operation::Read);
	}

	return {}; // unreachable code
}

#endif // ZXING_EXPERIMENTAL_API

} // namespace ZXing
