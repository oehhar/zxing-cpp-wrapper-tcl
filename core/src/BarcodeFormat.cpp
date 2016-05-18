/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "BarcodeFormat.h"

#include <type_traits>
#include <string>

namespace ZXing {

static const char* FORMAT_STR[] = {
	"AZTEC",
	"CODABAR",
	"CODE_39",
	"CODE_93",
	"CODE_128",
	"DATA_MATRIX",
	"EAN_8",
	"EAN_13",
	"ITF",
	"MAXICODE",
	"PDF_417",
	"QR_CODE",
	"RSS_14",
	"RSS_EXPANDED",
	"UPC_A",
	"UPC_E",
	"UPC_EAN_EXTENSION",
};

static_assert(std::extent<decltype(FORMAT_STR)>::value == (int)BarcodeFormat::FORMAT_COUNT, "FORMAT_STR array is out of sync with BarcodeFormat");

const char * ToString(BarcodeFormat format)
{
	return FORMAT_STR[(int)format];
}

BarcodeFormat FromString(const char* str)
{
	int length = (int)BarcodeFormat::FORMAT_COUNT;
	for (int i = 0; i < length; ++i) {
		if (std::strcmp(FORMAT_STR[i], str) == 0) {
			return BarcodeFormat(i);
		}
	}
	return BarcodeFormat::FORMAT_COUNT;
}


} // ZXing