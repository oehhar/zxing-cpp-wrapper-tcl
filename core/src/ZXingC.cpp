/*
* Copyright 2023 siiky
* Copyright 2023 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ZXingC.h"

#include "ZXingCpp.h"
#include "Version.h"

#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

using namespace ZXing;

static thread_local std::string lastErrorMsg;
static Barcodes emptyBarcodes{}; // used to prevent new heap allocation for each empty result

template<typename R, typename T> R transmute_cast(const T& v) noexcept
{
	static_assert(sizeof(T) == sizeof(R));
	return *(const R*)(&v);
}

template<typename C, typename P = typename C::pointer>
P copy(const C& c) noexcept
{
	auto ret = (P)malloc(c.size() + 1);
	if (ret) {
		memcpy(ret, c.data(), c.size());
		ret[c.size()] = 0;
	}
	return ret;
}

static uint8_t* copy(const ByteArray& ba, int* len) noexcept
{
	// for convencience and as a safety measure, we NULL terminate even byte arrays
	auto ret = copy(ba);
	if (len)
		*len = ret ? Size(ba) : 0;
	return ret;
}

#define ZX_CHECK( GOOD, MSG ) \
	if (!(GOOD)) { \
		lastErrorMsg = MSG; \
		return {}; \
	}

#define ZX_CATCH(...) \
	catch (std::exception & e) { \
		lastErrorMsg = e.what(); \
	} catch (...) { \
		lastErrorMsg = "Unknown error"; \
	} \
	return __VA_ARGS__;

#define ZX_TRY(...) \
	try { \
		return __VA_ARGS__; \
	} \
	ZX_CATCH({})


extern "C" {
/*
 * ZXing/ImageView.h
 */

ZXing_ImageView* ZXing_ImageView_new(const uint8_t* data, int width, int height, ZXing_ImageFormat format, int rowStride,
									 int pixStride)
{
	ImageFormat cppformat = static_cast<ImageFormat>(format);
	ZX_TRY(new ImageView(data, width, height, cppformat, rowStride, pixStride))
}

ZXing_ImageView* ZXing_ImageView_new_checked(const uint8_t* data, int size, int width, int height, ZXing_ImageFormat format,
											 int rowStride, int pixStride)
{
	ImageFormat cppformat = static_cast<ImageFormat>(format);
	ZX_TRY(new ImageView(data, size, width, height, cppformat, rowStride, pixStride))
}

void ZXing_ImageView_delete(ZXing_ImageView* iv)
{
	delete iv;
}

void ZXing_ImageView_crop(ZXing_ImageView* iv, int left, int top, int width, int height)
{
	*iv = iv->cropped(left, top, width, height);
}

void ZXing_ImageView_rotate(ZXing_ImageView* iv, int degree)
{
	*iv = iv->rotated(degree);
}

void ZXing_Image_delete(ZXing_Image* img)
{
	delete img;
}

const uint8_t* ZXing_Image_data(const ZXing_Image* img)
{
	return img->data();
}

int ZXing_Image_width(const ZXing_Image* img)
{
	return img->width();
}

int ZXing_Image_height(const ZXing_Image* img)
{
	return img->height();
}

ZXing_ImageFormat ZXing_Image_format(const ZXing_Image* img)
{
	return static_cast<ZXing_ImageFormat>(img->format());
}

/*
 * ZXing/BarcodeFormat.h
 */

ZXing_BarcodeFormats ZXing_BarcodeFormatsFromString(const char* str)
{
	if (!str)
		return {};
	try {
		return transmute_cast<ZXing_BarcodeFormats>(BarcodeFormatsFromString(str));
	}
	ZX_CATCH(ZXing_BarcodeFormat_Invalid)
}

ZXing_BarcodeFormat ZXing_BarcodeFormatFromString(const char* str)
{
	ZXing_BarcodeFormat res = ZXing_BarcodeFormatsFromString(str);
	return BitHacks::CountBitsSet(res) == 1 ? res : ZXing_BarcodeFormat_Invalid;
}

char* ZXing_BarcodeFormatToString(ZXing_BarcodeFormat format)
{
	return copy(ToString(static_cast<BarcodeFormat>(format)));
}

/*
 * ZXing/ZXingCpp.h
 */


#ifdef ZXING_EXPERIMENTAL_API
ZXing_BarcodeFormats ZXing_SupportedBarcodeFormats(ZXing_Operation op)
{
	return transmute_cast<ZXing_BarcodeFormats>(SupportedBarcodeFormats(static_cast<Operation>(op)));
}
#endif

/*
 * ZXing/Barcode.h
 */

char* ZXing_ContentTypeToString(ZXing_ContentType type)
{
	return copy(ToString(static_cast<ContentType>(type)));
}

char* ZXing_PositionToString(ZXing_Position position)
{
	return copy(ToString(transmute_cast<Position>(position)));
}


bool ZXing_Barcode_isValid(const ZXing_Barcode* barcode)
{
	return barcode != NULL && barcode->isValid();
}

ZXing_ErrorType ZXing_Barcode_errorType(const ZXing_Barcode* barcode)
{
	return static_cast<ZXing_ErrorType>(barcode->error().type());
}

char* ZXing_Barcode_errorMsg(const ZXing_Barcode* barcode)
{
	return copy(ToString(barcode->error()));
}

uint8_t* ZXing_Barcode_bytes(const ZXing_Barcode* barcode, int* len)
{
	return copy(barcode->bytes(), len);
}

uint8_t* ZXing_Barcode_bytesECI(const ZXing_Barcode* barcode, int* len)
{
	return copy(barcode->bytesECI(), len);
}

#define ZX_GETTER(TYPE, GETTER, TRANS) \
	TYPE ZXing_Barcode_##GETTER(const ZXing_Barcode* barcode) { return TRANS(barcode->GETTER()); }

ZX_GETTER(ZXing_BarcodeFormat, format, static_cast<ZXing_BarcodeFormat>)
ZX_GETTER(ZXing_ContentType, contentType, static_cast<ZXing_ContentType>)
ZX_GETTER(char*, text, copy)
ZX_GETTER(char*, ecLevel, copy)
ZX_GETTER(char*, symbologyIdentifier, copy)
ZX_GETTER(ZXing_Position, position, transmute_cast<ZXing_Position>)

ZX_GETTER(int, orientation,)
ZX_GETTER(bool, hasECI,)
ZX_GETTER(bool, isInverted,)
ZX_GETTER(bool, isMirrored,)
ZX_GETTER(int, lineCount,)

void ZXing_Barcode_delete(ZXing_Barcode* barcode)
{
	delete barcode;
}

void ZXing_Barcodes_delete(ZXing_Barcodes* barcodes)
{
	if (barcodes != &emptyBarcodes)
		delete barcodes;
}

int ZXing_Barcodes_size(const ZXing_Barcodes* barcodes)
{
	return barcodes ? Size(*barcodes) : 0;
}

const ZXing_Barcode* ZXing_Barcodes_at(const ZXing_Barcodes* barcodes, int i)
{
	if (!barcodes || i < 0 || i >= Size(*barcodes))
		return NULL;
	return &(*barcodes)[i];
}

ZXing_Barcode* ZXing_Barcodes_move(ZXing_Barcodes* barcodes, int i)
{
	if (!barcodes || i < 0 || i >= Size(*barcodes))
		return NULL;

	ZX_TRY(new Barcode(std::move((*barcodes)[i])));
}

/*
 * ZXing/ReaderOptions.h
 */

ZXing_ReaderOptions* ZXing_ReaderOptions_new()
{
	ZX_TRY(new ReaderOptions());
}

void ZXing_ReaderOptions_delete(ZXing_ReaderOptions* opts)
{
	delete opts;
}

#define ZX_PROPERTY(TYPE, GETTER, SETTER) \
	TYPE ZXing_ReaderOptions_get##SETTER(const ZXing_ReaderOptions* opts) { return opts->GETTER(); } \
	void ZXing_ReaderOptions_set##SETTER(ZXing_ReaderOptions* opts, TYPE val) { opts->set##SETTER(val); }

ZX_PROPERTY(bool, tryHarder, TryHarder)
ZX_PROPERTY(bool, tryRotate, TryRotate)
ZX_PROPERTY(bool, tryInvert, TryInvert)
ZX_PROPERTY(bool, tryDownscale, TryDownscale)
ZX_PROPERTY(bool, isPure, IsPure)
ZX_PROPERTY(bool, returnErrors, ReturnErrors)
ZX_PROPERTY(int, minLineCount, MinLineCount)
ZX_PROPERTY(int, maxNumberOfSymbols, MaxNumberOfSymbols)

#undef ZX_PROPERTY

void ZXing_ReaderOptions_setFormats(ZXing_ReaderOptions* opts, ZXing_BarcodeFormats formats)
{
	opts->setFormats(static_cast<BarcodeFormat>(formats));
}

ZXing_BarcodeFormats ZXing_ReaderOptions_getFormats(const ZXing_ReaderOptions* opts)
{
	auto v = opts->formats();
	return transmute_cast<ZXing_BarcodeFormats>(v);
}

#define ZX_ENUM_PROPERTY(TYPE, GETTER, SETTER) \
	ZXing_##TYPE ZXing_ReaderOptions_get##SETTER(const ZXing_ReaderOptions* opts) { return static_cast<ZXing_##TYPE>(opts->GETTER()); } \
	void ZXing_ReaderOptions_set##SETTER(ZXing_ReaderOptions* opts, ZXing_##TYPE val) { opts->set##SETTER(static_cast<TYPE>(val)); }

ZX_ENUM_PROPERTY(Binarizer, binarizer, Binarizer)
ZX_ENUM_PROPERTY(EanAddOnSymbol, eanAddOnSymbol, EanAddOnSymbol)
ZX_ENUM_PROPERTY(TextMode, textMode, TextMode)


/*
 * ZXing/ReadBarcode.h
 */

ZXing_Barcodes* ZXing_ReadBarcodes(const ZXing_ImageView* iv, const ZXing_ReaderOptions* opts)
{
	ZX_CHECK(iv, "ImageView param is NULL")
	try {
		auto res = ReadBarcodes(*iv, opts ? *opts : ReaderOptions{});
		return res.empty() ? &emptyBarcodes : new Barcodes(std::move(res));
	}
	ZX_CATCH(NULL);
}


#ifdef ZXING_EXPERIMENTAL_API
/*
 * ZXing/WriteBarcode.h
 */

ZXing_CreatorOptions* ZXing_CreatorOptions_new(ZXing_BarcodeFormat format)
{
	ZX_TRY(new CreatorOptions(static_cast<BarcodeFormat>(format)));
}

void ZXing_CreatorOptions_delete(ZXing_CreatorOptions* opts)
{
	delete opts;
}

#define ZX_PROPERTY(TYPE, GETTER, SETTER) \
	TYPE ZXing_CreatorOptions_get##SETTER(const ZXing_CreatorOptions* opts) { return opts->GETTER(); } \
	void ZXing_CreatorOptions_set##SETTER(ZXing_CreatorOptions* opts, TYPE val) { opts->GETTER(val); }

ZX_PROPERTY(bool, readerInit, ReaderInit)
ZX_PROPERTY(bool, forceSquareDataMatrix, ForceSquareDataMatrix)

#undef ZX_PROPERTY

//ZX_PROPERTY(BarcodeFormat, format, Format)

char* ZXing_CreatorOptions_getEcLevel(const ZXing_CreatorOptions* opts)
{
	return copy(opts->ecLevel());
}

void ZXing_CreatorOptions_setEcLevel(ZXing_CreatorOptions* opts, const char* val)
{
	opts->ecLevel(val);
}


ZXing_WriterOptions* ZXing_WriterOptions_new()
{
	ZX_TRY(new ZXing_WriterOptions());
}

void ZXing_WriterOptions_delete(ZXing_WriterOptions* opts)
{
	delete opts;
}

#define ZX_PROPERTY(TYPE, GETTER, SETTER) \
	TYPE ZXing_WriterOptions_get##SETTER(const ZXing_WriterOptions* opts) { return opts->GETTER(); } \
	void ZXing_WriterOptions_set##SETTER(ZXing_WriterOptions* opts, TYPE val) { opts->GETTER(val); }

ZX_PROPERTY(int, scale, Scale)
ZX_PROPERTY(int, sizeHint, SizeHint)
ZX_PROPERTY(int, rotate, Rotate)
ZX_PROPERTY(bool, withHRT, WithHRT)
ZX_PROPERTY(bool, withQuietZones, WithQuietZones)

#undef ZX_PROPERTY

ZXing_Barcode* ZXing_CreateBarcodeFromText(const char* data, int size, const ZXing_CreatorOptions* opts)
{
	ZX_CHECK(data && opts, "Data and/or options param in CreateBarcodeFromText is NULL")
	ZX_TRY(new Barcode(CreateBarcodeFromText({data, size ? static_cast<size_t>(size) : strlen(data)}, *opts));)
}

ZXing_Barcode* ZXing_CreateBarcodeFromBytes(const void* data, int size, const ZXing_CreatorOptions* opts)
{
	ZX_CHECK(data && size && opts, "Data and/or options param in CreateBarcodeFromBytes is NULL")
	ZX_TRY(new Barcode(CreateBarcodeFromBytes(data, size, *opts)))
}

char* ZXing_WriteBarcodeToSVG(const ZXing_Barcode* barcode, const ZXing_WriterOptions* opts)
{
	ZX_CHECK(barcode, "Barcode param in WriteBarcodeToSVG is NULL")
	ZX_TRY(copy(opts ? WriteBarcodeToSVG(*barcode, *opts) : WriteBarcodeToSVG(*barcode)))
}

ZXing_Image* ZXing_WriteBarcodeToImage(const ZXing_Barcode* barcode, const ZXing_WriterOptions* opts)
{
	ZX_CHECK(barcode, "Barcode param in WriteBarcodeToSVG is NULL")
	ZX_TRY(new Image(opts ? WriteBarcodeToImage(*barcode, *opts) : WriteBarcodeToImage(*barcode)))
}

#endif

/*
 * ZXingC.h
 */

char* ZXing_LastErrorMsg()
{
	if (lastErrorMsg.empty())
		return NULL;

	return copy(std::exchange(lastErrorMsg, {}));
}

const char* ZXing_Version()
{
	return ZXING_VERSION_STR;
}

void ZXing_free(void* ptr)
{
	if (ptr != ZXing_Version())
		free(ptr);
}

} // extern "C"
