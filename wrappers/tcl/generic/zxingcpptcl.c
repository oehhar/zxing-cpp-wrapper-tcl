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
ZxingcppDecodeObjCmd(ClientData unused, Tcl_Interp *interp,
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

    if ((objc < 2) || (objc > 3)) {
	Tcl_WrongNumArgs(interp, 1, objv, "photoEtc ?syms?");
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
	if ((bpp != 1) || (bpp != 3)) {
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
    } else {
#ifdef ZXINGCPP_NO_TK
	Tcl_SetResult(interp, "need list of width, height, bpp, bytes",
		      TCL_STATIC);
	return TCL_ERROR;
#else
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
#endif
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
    ZXing_ReaderOptions_setTextMode(opts, ZXing_TextMode_HRI);
    ZXing_ReaderOptions_setEanAddOnSymbol(opts, ZXing_EanAddOnSymbol_Ignore);
    ZXing_ReaderOptions_setFormats(opts, formats);
    ZXing_ReaderOptions_setReturnErrors(opts, true);

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
    
    for (int i = 0, n = ZXing_Barcodes_size(barcodes); i < n; ++i) {

	Tcl_Obj * resultDict = Tcl_NewDictObj();
	const ZXing_Barcode* barcode = ZXing_Barcodes_at(barcodes, i);
	uint8_t* bytePtr;
	int len;

	/*
	 * Build a dict with the result keys
	 */
	
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("text",-1),
		Tcl_NewStringObj(ZXing_Barcode_text(barcode), -1));

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
   
    /*
     * Return list of result dicts
     */

    Tcl_SetObjResult(interp, resultList);
    return TCL_OK;
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
    Tcl_CmdInfo info;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.5-10", 0) == NULL)
#else
    if (Tcl_PkgRequire(interp, "Tcl", "8.5-10", 0) == NULL)
#endif
    {
	return TCL_ERROR;
    }
#ifndef ZXINGCPP_NO_TK
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8.5-10", 0) == NULL)
#else
    if (Tcl_PkgRequire(interp, "Tk", "8.5-10", 0) == NULL)
#endif
    {
	return TCL_ERROR;
    }
#endif

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

    /* Provide the current package */

    if (Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, NULL) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand(interp, "::zxingcpp::decode",
	    (Tcl_ObjCmdProc *)ZxingcppDecodeObjCmd, NULL, NULL);

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
