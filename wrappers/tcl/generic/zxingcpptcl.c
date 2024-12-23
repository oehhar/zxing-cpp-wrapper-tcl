/*
* Copyright 2023 siiky
* Copyright 2023 Axel Waggershauser
* Copyright 2024 Christian Werner
* Copyright 2024 Harald Oehlmann
*/
// SPDX-License-Identifier: Apache-2.0

/*
 * The structure is copied from zbar.c from androwish.org
 * https://www.androwish.org/home/file?name=jni/ZBar/tcl/tclzbar.c
 * with permission by Christian Werner (THANKS!)
 */

#include "ZXingC.h"

#include <tcl.h>
#include <tk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zxingcppUuid.h"

#if (TCL_MAJOR_VERSION > 8)
#undef  TCL_THREADS
#define TCL_THREADS 1
#endif

#ifndef STRINGIFY
#  define STRINGIFY(x) STRINGIFY1(x)
#  define STRINGIFY1(x) #x
#endif

#ifdef __cplusplus
//extern "C" {
#endif  /* __cplusplus */

/*
 *----------------------------------------------------------------------
 * Check availability of Tk.
 *----------------------------------------------------------------------
 */
static int CheckForTk(Tcl_Interp *interp, int *tkFlagPtr)
{
    if (*tkFlagPtr > 0) {
        return TCL_OK;
    }
    if (*tkFlagPtr == 0) {
        if ( ! Tcl_PkgPresent(interp, "Tk", "8.5-10", 0) ) {
            Tcl_SetResult(interp, "package Tk not loaded", TCL_STATIC);
            return TCL_ERROR;
        }
    }
#ifdef USE_TK_STUBS
    if (*tkFlagPtr < 0 || Tk_InitStubs(interp, "8.5-10", 0) == NULL) {
        *tkFlagPtr = -1;
        Tcl_SetResult(interp, "error initializing Tk", TCL_STATIC);
        return TCL_ERROR;
    }
#endif
    *tkFlagPtr = 1;
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ReaderOptionsGet --
 *
 *	extract command arguments and translate them to reader options
 *
 *		optc	count of given parameters and values
 *		objv	parameter object array. Contains arbitrary number of
 *			option/value pairs
 *		opts	An initialized ZXING-CPP reader option object pointer
 *
 *	Result:
 *
 *		reader option object
 *		NULL on error. The error message is set in the interpreter
 *
 *-------------------------------------------------------------------------
 */

static int
ReaderOptionsGet(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[], ZXing_ReaderOptions* opts)
{
    int option;
    const char *options[] = {
	"TryHarder", "TryRotate", "TryInvert", "TryDownscale",
	"IsPure", "ReturnErrors", "Formats", "Binarizer", "EanAddOnSymbol",
	"TextMode", "MinLineCount", "MaxNumberOfSymbols",
	NULL};
    enum iOptions {
	iTryHarder, iTryRotate, iTryInvert, iTryDownscale,
	iIsPure, iReturnErrors, iFormats, iBinarizer,iEanAddOnSymbol,
	iTextMode, iMinLineCount, iMaxNumberOfSymbols,
	};

    /*
     * Check for pair option count
     */
    
    if ( (objc %2) != 0) {
	Tcl_SetResult(interp, "Option without value", TCL_STATIC);
	return TCL_ERROR;
    }
    
    /*
     * Loop over given parameters
     */
    
    for (int argPos = 0; argPos < objc; argPos++) {
	
	int intValue;
	
	/* get the option */
	if (TCL_OK != 
		Tcl_GetIndexFromObj(interp, objv[argPos], options, "subcmd", argPos, &option))
	{
	    return TCL_ERROR;
	}
	/* get the following parameter value */
	argPos++;
	switch (option) {
	case iTryHarder:
	case iTryRotate:
	case iTryInvert:
	case iTryDownscale:
	case iIsPure:
	case iReturnErrors:
	    /* get a boolean value */
	    if (TCL_OK != Tcl_GetBooleanFromObj(interp,objv[argPos], &intValue)) {
		return TCL_ERROR;
	    }
	    break;
	case iMinLineCount:
	case iMaxNumberOfSymbols:
	    /* get an int value */
	    if (TCL_OK != Tcl_GetIntFromObj(interp,objv[argPos], &intValue)) {
		return TCL_ERROR;
	    }
	    break;
	}
	/* set the setting */
	switch (option) {
	case iTryHarder:
	    ZXing_ReaderOptions_setTryHarder(opts, intValue);
	    break;
	case iTryRotate:
	    ZXing_ReaderOptions_setTryRotate(opts, intValue);
	    break;
	case iTryInvert:
	    ZXing_ReaderOptions_setTryInvert(opts, intValue);
	    break;
	case iTryDownscale:
	    ZXing_ReaderOptions_setTryDownscale(opts, intValue);
	    break;
	case iIsPure:
	    ZXing_ReaderOptions_setIsPure(opts, intValue);
	    break;
	case iReturnErrors:
	    ZXing_ReaderOptions_setReturnErrors(opts, intValue);
	    break;
	case iFormats:
	    {
		/*
		 * Translate a list of symbologies to a ZXING-CPP format
		 * The format strings are a field of flags which may be ored.
		 */

		ZXing_BarcodeFormats formats = ZXing_BarcodeFormat_None;
		Tcl_Size listLength, listItem;

		if (TCL_OK != Tcl_ListObjLength(interp,objv[argPos],&listLength)) {
		    return TCL_ERROR;
		}
		/* loop over supplied list items */
		for ( listItem = 0; listItem < listLength; listItem++ ) {
		    ZXing_BarcodeFormat format;
		    Tcl_Obj *formatObj;

		    if (TCL_OK != Tcl_ListObjIndex(interp, objv[argPos], listItem,
			    &formatObj) ) {
			return TCL_ERROR;
		    }
		    /* translate string to format number */
		    format = ZXing_BarcodeFormatFromString(
			    Tcl_GetString(formatObj) );
		    /* check for unknown format string */
		    if (format == ZXing_BarcodeFormat_Invalid) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf(
				"zxing-cpp format \"%s\" not found",
				Tcl_GetString(formatObj) ) );
			return TCL_ERROR;
		    }
		    formats |=format;
		}

		/* set the format list */
		ZXing_ReaderOptions_setFormats(opts, formats);
	    }
	    break;
	case iBinarizer:
	    {
		int value;

		/*
		 * The target value "ZXing_Binarizer" an enum of the following
		 * values.
		 */

		const char *values[] = {
		    "LocalAverage", "GlobalHistogram", "FixedThreshold", "BoolCast",
		    NULL};
		if (TCL_OK != 
			Tcl_GetIndexFromObj(interp, objv[argPos], values, "value", argPos, &value))
		{
		    return TCL_ERROR;
		}
		ZXing_ReaderOptions_setBinarizer(opts, value);
	    }
	    break;
	case iEanAddOnSymbol:
	    {
		
		/*
		 * The target value "ZXing_EanAddOnSymbol" an enum of the
		 * following values.
		 */
		
		int value;
		const char *values[] = { "Ignore", "Read", "Require", NULL};
		if (TCL_OK != 
			Tcl_GetIndexFromObj(interp, objv[argPos], values, "value", argPos, &value))
		{
		    return TCL_ERROR;
		}
		ZXing_ReaderOptions_setEanAddOnSymbol(opts, value);
	    }
	    break;
	case iTextMode:
	    {
		
		/*
		 * The target value "ZXing_TextMode" an enum of the
		 * following values.
		 */
		
		int value;
		const char *values[] = { "Plain", "ECI", "HRI", "Hex", "Escaped", NULL};
		if (TCL_OK != 
			Tcl_GetIndexFromObj(interp, objv[argPos], values, "value", argPos, &value))
		{
		    return TCL_ERROR;
		}
		ZXing_ReaderOptions_setTextMode(opts, value);
	    }
	    break;
	case iMinLineCount:
	    ZXing_ReaderOptions_setMinLineCount(opts, intValue);
	    break;
	case iMaxNumberOfSymbols:
	    ZXing_ReaderOptions_setMaxNumberOfSymbols(opts, intValue);
	    break;
	}
    }
    return TCL_OK;
}
/*
 *-------------------------------------------------------------------------
 *
 * ZxingcppDecodeObjCmd --
 *
 *	zbar::decode Tcl command, synchronous decoding.
 *	Command arguments are
 *
 *		zbar::decode photoEtc ?options1? ?option2? ...
 *
 *		photoEtc	photo image name or list of
 *				{width height bpp bytes}
 *		options		options to the decoder
 *
 *	Result:
 *
 *		time	decode/processing time in milliseconds
 *		resDict1 ?resDict2? ...	result dictionaries
 *
 *-------------------------------------------------------------------------
 */

static int
ZxingcppDecodeObjCmd(ClientData tkFlagPtr, Tcl_Interp *interp,
	int objc,  Tcl_Obj *const objv[])
{
    Tk_PhotoImageBlock block;
    int bpp, nElems;
    unsigned char *pixPtr = NULL;
    ZXing_ImageView* iv = NULL;
    ZXing_ReaderOptions* opts;
    ZXing_BarcodeFormats formats = ZXing_BarcodeFormat_None;
    ZXing_Barcodes* barcodes;
    Tcl_Time now;
    Tcl_WideInt tw[2], td;
    Tcl_Obj **elems;
    Tcl_Obj *resultList;
    Tcl_Encoding utf8Encoding;
    Tcl_DString recode;


    if ( objc < 2 ) {
	Tcl_WrongNumArgs(interp, 1, objv, "photoEtc ?opt...?");
	return TCL_ERROR;
    }
    Tcl_GetTime(&now);
    tw[0] = (Tcl_WideInt) now.sec * 1000 + now.usec / 1000;
    if (Tcl_ListObjGetElements(interp, objv[1], &nElems, &elems) != TCL_OK) {
	return TCL_ERROR;
    }
    if (nElems < 1) {
	Tcl_SetResult(interp, "need photo image or list", TCL_STATIC);
	return TCL_ERROR;
    } else if (nElems >= 4) {
	int size, length;

	if ((Tcl_GetIntFromObj(interp, elems[0], &block.width) != TCL_OK) ||
	    (Tcl_GetIntFromObj(interp, elems[1], &block.height) != TCL_OK) ||
	    (Tcl_GetIntFromObj(interp, elems[2], &bpp) != TCL_OK)) {
	    return TCL_ERROR;
	}
	if (!(bpp == 1 || bpp == 3)) {
	    Tcl_SetResult(interp, "unsupported image depth", TCL_STATIC);
	    return TCL_ERROR;
	}
	size = block.width * block.height;
	if (size <= 0) {
	    Tcl_SetResult(interp, "invalid image size", TCL_STATIC);
	    return TCL_ERROR;
	}
	block.pixelPtr = Tcl_GetByteArrayFromObj(elems[3], &length);
	if ((block.pixelPtr == NULL) || (length < size)) {
	    Tcl_SetResult(interp, "malformed image", TCL_STATIC);
	    return TCL_ERROR;
	}
	if (bpp == 1) {
	    block.offset[0] = 0;
	    block.offset[1] = 0;
	    block.offset[2] = 0;
	    block.offset[3] = -1;
	    block.pitch = block.width;
	    block.pixelSize = 1;
	} else {
	    block.offset[0] = 0;
	    block.offset[1] = 1;
	    block.offset[2] = 2;
	    block.offset[3] = -1;
	    block.pitch = block.width * 3;
	    block.pixelSize = 3;
	}
    } else if (CheckForTk(interp, (int *)tkFlagPtr) != TCL_OK) {
	Tcl_SetResult(interp, "need list of width, height, bpp, bytes",
		      TCL_STATIC);
	return TCL_ERROR;
    } else {
	Tk_PhotoHandle handle;

	handle = Tk_FindPhoto(interp, Tcl_GetString(objv[1]));
	if (handle == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("photo \"%s\" not found",
						   Tcl_GetString(objv[1])));
	    return TCL_ERROR;
	}
	if (Tk_PhotoGetImage(handle, &block) != 1) {
	    Tcl_SetResult(interp, "error retrieving photo image", TCL_STATIC);
	    return TCL_ERROR;
	}
    }

    /*
     * Translate the block to a txing image view object
     * 
     * zxing-cpp supports the following image formats:
     * (see ImageView.h)
     * ZXing_ImageFormat_Lum : Greyscale (1 byte)
     * ZXing_ImageFormat_LumA : Greyscale, Alpha (2 bytes)
     * ZXing_ImageFormat_RGB: 3 bytes
     * ZXing_ImageFormat_BGR: 3 bytes
     * ZXing_ImageFormat_RGBA
     * ZXing_ImageFormat_ARGB
     * ZXing_ImageFormat_BGRA
     * ZXing_ImageFormat_ABGR
     * Additional parameters:
     * width
     * height
     * rowStride: row stride in bytes, default (0) is width * pixStride
     * pixStride: pixel stride in bytes. default (0) is pixel width (1-4)
     *
     * The image block format is:
     * width : image width
     * height: image height
     * pitch: data row length
     * pixelSize: width of one pixel
     * offset[4]: r,g,b,alpha offset
     * - examples:
     * 	- greyscale: 0,0,0,-1
     * 	- greyscale+alpha: 0,0,0,1
     * 	- RGB: 0,1,2,-1
     * 	- RGBA: 0,1,2,3
     */
    
    if (    block.offset[0] == 0
	    && block.offset[1] == 0
	    && block.offset[2] == 0
	    && block.offset[3] < 0) {
	/* greyscale image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_Lum, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 0
	    && block.offset[1] == 0
	    && block.offset[2] == 0
	    && block.offset[3] == 1) {
	/* greyscale image + alpha */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_LumA, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 0
	    && block.offset[1] == 1
	    && block.offset[2] == 2
	    && block.offset[3] < 0) {
	/* RGB image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_RGB, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 2
	    && block.offset[1] == 1
	    && block.offset[2] == 0
	    && block.offset[3] < 0) {
	/* BGR image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_BGR, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 0
	    && block.offset[1] == 1
	    && block.offset[2] == 2
	    && block.offset[3] ==3) {
	/* RGBA image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_RGBA, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 1
	    && block.offset[1] == 2
	    && block.offset[2] == 3
	    && block.offset[3] ==0) {
	/* ARGB image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_ARGB, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 1
	    && block.offset[1] == 2
	    && block.offset[2] == 3
	    && block.offset[3] ==0) {
	/* ARGB image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_ARGB, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 2
	    && block.offset[1] == 1
	    && block.offset[2] == 0
	    && block.offset[3] == 3) {
	/* BGRA image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_BGRA, block.pitch, block.pixelSize);
    } else if (block.offset[0] == 3
	    && block.offset[1] == 2
	    && block.offset[2] == 1
	    && block.offset[3] == 0) {
	/* ABGR image */
	iv = ZXing_ImageView_new(block.pixelPtr, block.width, block.height,
		ZXing_ImageFormat_ABGR, block.pitch, block.pixelSize);
    } else {
	Tcl_SetObjResult( interp, Tcl_ObjPrintf(
		"Unsupported pixel layout: R:%d,G:%d,B:%d,A:%d",
		block.offset[0], block.offset[1], block.offset[2],
		block.offset[3] ) );
	return TCL_ERROR;
    }

    if (iv == NULL) {
	char* error = ZXing_LastErrorMsg();
	Tcl_SetObjResult( interp, Tcl_NewStringObj(error,-1) );
	ZXing_free(error);
	return TCL_ERROR;
    }

    /*
     * Handle reader options
     */

    opts = ZXing_ReaderOptions_new();
    if (TCL_ERROR == ReaderOptionsGet(interp, objc-2,  &objv[2], opts) ) {
	ZXing_ReaderOptions_delete(opts);
	return TCL_ERROR;
    }

    /*
     * Read the bar code
     */

    barcodes = ZXing_ReadBarcodes(iv, opts);

    ZXing_ImageView_delete(iv);
    ZXing_ReaderOptions_delete(opts);

    /*
     * Check for read error
     */
    if (NULL == barcodes) {
	char* error = ZXing_LastErrorMsg();
	Tcl_SetObjResult( interp, Tcl_NewStringObj(error,-1) );
	ZXing_free(error);
	return TCL_ERROR;
    }

    /*
     * Init result list with time value
     */
    
    Tcl_GetTime(&now);
    tw[1] = (Tcl_WideInt) now.sec * 1000 + now.usec / 1000;
    td = tw[1] - tw[0];
    if (td < 0) {
	td = -1;
    }

    resultList = Tcl_NewListObj(0,NULL);
    Tcl_ListObjAppendElement(interp, resultList, Tcl_NewWideIntObj(td) );
    
    /*
     * Loop over read bar codes
     */
    
    utf8Encoding = Tcl_GetEncoding(interp, "utf-8"); 
    Tcl_DStringInit(&recode);

    
    for (int i = 0, n = ZXing_Barcodes_size(barcodes); i < n; ++i) {

	Tcl_Obj * resultDict = Tcl_NewDictObj();
	const ZXing_Barcode* barcode = ZXing_Barcodes_at(barcodes, i);
	uint8_t* bytePtr;
	int len;
	char *zxingcppString;

	/*
	 * Build a dict with the result keys
	 */
	
	zxingcppString = ZXing_Barcode_text(barcode);
	Tcl_DStringAppend(&recode, zxingcppString, -1);
	ZXing_free(zxingcppString);
	Tcl_ExternalToUtfDString(utf8Encoding, ZXing_Barcode_text(barcode), -1, &recode); 
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("text",-1),
		Tcl_NewStringObj(Tcl_DStringValue (&recode), Tcl_DStringLength (&recode)));
	Tcl_DStringFree(&recode);

	zxingcppString = ZXing_BarcodeFormatToString(ZXing_Barcode_format(barcode));
	Tcl_DStringAppend(&recode, zxingcppString, -1);
	ZXing_free(zxingcppString);
	Tcl_ExternalToUtfDString(utf8Encoding, ZXing_Barcode_text(barcode), -1, &recode); 
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("format",-1),
		Tcl_NewStringObj(Tcl_DStringValue (&recode), Tcl_DStringLength (&recode)));
	Tcl_DStringFree(&recode);

	bytePtr=ZXing_Barcode_bytes(barcode, &len);
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("bytes",-1),
		Tcl_NewByteArrayObj(bytePtr,len));

	bytePtr=ZXing_Barcode_bytesECI(barcode, &len);
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("bytesECI",-1),
		Tcl_NewByteArrayObj(bytePtr,len));

	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("format",-1),
		Tcl_NewStringObj( ZXing_BarcodeFormatToString(
		    ZXing_Barcode_format(barcode)),
		-1));

	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("content",-1),
		Tcl_NewStringObj( ZXing_ContentTypeToString(
		    ZXing_Barcode_contentType(barcode)),
		-1));

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("symbologyIdentifier", -1),
		Tcl_NewStringObj( ZXing_Barcode_symbologyIdentifier(barcode), -1));

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("hasECI", -1),
		Tcl_NewBooleanObj( ZXing_Barcode_hasECI(barcode) ) );

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("ecLevel", -1),
		Tcl_NewStringObj( ZXing_Barcode_ecLevel(barcode), -1));

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("position", -1),
		Tcl_NewStringObj( ZXing_PositionToString(
		    ZXing_Barcode_position(barcode)),
		-1));

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("orientation", -1),
		Tcl_NewIntObj( ZXing_Barcode_orientation(barcode) ) );

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("isMirrored", -1),
		Tcl_NewBooleanObj( ZXing_Barcode_isMirrored(barcode) ) );

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("isInverted", -1),
		Tcl_NewBooleanObj( ZXing_Barcode_isInverted(barcode) ) );

	if (!ZXing_Barcode_isValid(barcode)) {
	    char * typeText;
	    switch ( ZXing_Barcode_errorType(barcode) ) {
	    case ZXing_ErrorType_None:
		typeText = "None";
		break;
	    case ZXing_ErrorType_Format:
		typeText = "Format";
		break;
	    case ZXing_ErrorType_Checksum:
		typeText = "Checksum";
		break;
	    case ZXing_ErrorType_Unsupported:
	    default:
		typeText = "Unsupported";
		break;
	    }
	    Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("errorType",-1),
		    Tcl_NewStringObj(typeText,-1));
	    Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("errorMsg",-1),
		    Tcl_NewStringObj(ZXing_Barcode_errorMsg(barcode),-1));
	}
	
	/*
	 * Append result dict to result list
	 */
	
	Tcl_ListObjAppendElement(interp, resultList, resultDict);
    }

    ZXing_Barcodes_delete(barcodes);
    
    Tcl_FreeEncoding(utf8Encoding);
    Tcl_DStringFree(&recode);
   
    /*
     * Return list of result dicts
     */

    Tcl_SetObjResult(interp, resultList);
    return TCL_OK;
}
/*----------------------------------------------------------------------------*/
/* >>>> Cleanup procedure */
/*----------------------------------------------------------------------------*/
/* This routine is called, if a thread is terminated */
static void InterpCleanupProc(ClientData clientData, Tcl_Interp *interp)
{
    ckfree( (char *)clientData );
}

/*
 *----------------------------------------------------------------------
 *
 * Zxingcpp_Init --
 *
 *	Initialize the Zxingcpp package.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

DLLEXPORT int
Zxingcpp_Init(
    Tcl_Interp* interp)		/* Tcl interpreter */
{
    int * tkFlagPtr;
    Tcl_CmdInfo info;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL)
#else
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL)
#endif
    {
	return TCL_ERROR;
    }

    if (Tcl_GetCommandInfo(interp, "::tcl::build-info", &info)) {
	Tcl_CreateObjCommand(interp, "::zxingcpp::build-info",
		info.objProc, (void *)(
		    PACKAGE_VERSION "+" STRINGIFY(SAMPLE_VERSION_UUID)
#if defined(__clang__) && defined(__clang_major__)
			    ".clang-" STRINGIFY(__clang_major__)
#if __clang_minor__ < 10
			    "0"
#endif
			    STRINGIFY(__clang_minor__)
#endif
#if defined(__cplusplus) && !defined(__OBJC__)
			    ".cplusplus"
#endif
#ifndef NDEBUG
			    ".debug"
#endif
#if !defined(__clang__) && !defined(__INTEL_COMPILER) && defined(__GNUC__)
			    ".gcc-" STRINGIFY(__GNUC__)
#if __GNUC_MINOR__ < 10
			    "0"
#endif
			    STRINGIFY(__GNUC_MINOR__)
#endif
#ifdef __INTEL_COMPILER
			    ".icc-" STRINGIFY(__INTEL_COMPILER)
#endif
#ifdef TCL_MEM_DEBUG
			    ".memdebug"
#endif
#if defined(_MSC_VER)
			    ".msvc-" STRINGIFY(_MSC_VER)
#endif
#ifdef USE_NMAKE
			    ".nmake"
#endif
#ifndef TCL_CFG_OPTIMIZED
			    ".no-optimize"
#endif
#ifdef __OBJC__
			    ".objective-c"
#if defined(__cplusplus)
			    "plusplus"
#endif
#endif
#ifdef TCL_CFG_PROFILED
			    ".profile"
#endif
#ifdef PURIFY
			    ".purify"
#endif
#ifdef STATIC_BUILD
			    ".static"
#endif
		), NULL);
    }

    /*------------------------------------------------------------------------*/
    /* This procedure is called once per thread and any thread local data     */
    /* should be allocated and initialized here (and not in static variables) */
    
    /* Create a flag if Tk is loaded */
    tkFlagPtr = (int *)ckalloc(sizeof(int));
    *tkFlagPtr = 0;
    Tcl_CallWhenDeleted(interp, InterpCleanupProc, (ClientData)tkFlagPtr);

    /* Provide the current package */

    if (Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, NULL) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand(interp, "::zxingcpp::decode",
	    (Tcl_ObjCmdProc *)ZxingcppDecodeObjCmd, (ClientData)tkFlagPtr, NULL);

    return TCL_OK;
}

#ifdef __cplusplus
//}
#endif  /* __cplusplus */


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
