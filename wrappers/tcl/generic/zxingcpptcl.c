/*
* Copyright 2023 siiky
* Copyright 2023 Axel Waggershauser
* Copyright 2024 Christian Werner
* Copyright 2024-2025 Harald Oehlmann
*/
// SPDX-License-Identifier: Apache-2.0

/*
 * The structure is copied from zbar.c from androwish.org
 * https://www.androwish.org/home/file?name=jni/ZBar/tcl/tclzbar.c
 * with permission by Christian Werner (THANKS!)
 */

/* Decode error simulation for testing */
/* #define ZXINGCPP_SIMULATE_DECODE_ERROR 1 */

#ifdef ZXINGCPP_NO_TK

/* Partial photo image block */

typedef struct {
    int width;
    int height;
    unsigned char *pixelPtr;
} Tk_PhotoImageBlock;

#else

#include <tk.h>

#endif

#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ZXingC.h"
#include "zxingcppUuid.h"

#if (TCL_MAJOR_VERSION > 8)
#undef  TCL_THREADS
#define TCL_THREADS 1
#endif

#ifndef STRINGIFY
#  define STRINGIFY(x) STRINGIFY1(x)
#  define STRINGIFY1(x) #x
#endif

static int CheckForTk(Tcl_Interp *interp, int *tkFlagPtr);
static int ZXingCppDecodeHandleEvent(Tcl_Event *evPtr, int flags);
static int ArgumentToZXingCppVisual(ClientData tkFlagPtr, Tcl_Interp *interp,
	ZXing_ImageView **ivPtr, Tcl_Obj *const argObj);
static int BarcodesToResultList(Tcl_Interp *interp, Tcl_Obj *resultList, 
	Tcl_WideInt td, ZXing_Barcodes* barcodes);

/*
 *-------------------------------------------------------------------------
 *
 * ZxingcppGetFormatsObj --
 *
 *	Return list of all known formats.
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Obj *
ZxingcppGetFormatsObj(int special)
{
    Tcl_Obj *result;
    int i;
    result = Tcl_NewListObj(0, NULL);
    for (i = 0; i < 20; i++) {
	Tcl_ListObjAppendElement(NULL, result,
		Tcl_NewStringObj(ZXing_BarcodeFormatToString(1<<i), -1));
    }
    if (special) {
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj("Any", -1));
	Tcl_ListObjAppendElement(NULL, result,
		Tcl_NewStringObj("LinearCodes", -1));
	Tcl_ListObjAppendElement(NULL, result,
		Tcl_NewStringObj("MatrixCodes", -1));
    }
    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZxingcppSymbolTypesObjCmd --
 *
 *	zxingcpp::symbol_types Tcl command, returns list of symbol types.
 *	Command format
 *
 *		zxingcpp::symbol_types
 *
 *	Result is a list with supported symbologies
 *
 *-------------------------------------------------------------------------
 */

static int
ZxingcppFormatsObjCmd(ClientData unused, Tcl_Interp *interp,
	int objc,  Tcl_Obj *const objv[])
{
    int special = 0;
    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?special?");
	return TCL_ERROR;
    }
    if (objc == 2) {
	if (TCL_OK != Tcl_GetBooleanFromObj(interp,objv[1],&special)) {
	    return TCL_ERROR;
	}
    }
    Tcl_SetObjResult(interp, ZxingcppGetFormatsObj(special));
    return TCL_OK;
}


/*
 *-------------------------------------------------------------------------
 *
 * ReaderOptionsGet --
 *
 *	Extract command arguments and translate them to reader options.
 *	Set the options in the given opts option object.
 *
 *		optc	count of given parameters and values
 *		objv	parameter object array. Contains arbitrary number of
 *			option/value pairs
 *		opts	An initialized ZXING-CPP reader option object pointer
 *
 *	Result:
 *
 *		TCL_OK		Options are processed
 *		TCL_ERROR	An error occured. The error message of the
 *				interpreter is set.
 *
 *-------------------------------------------------------------------------
 */

static int
ReaderOptionsGet(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[], ZXing_ReaderOptions* opts)
{
    int option;
    const char *options[] = {
#ifdef ZXING_EXPERIMENTAL_API
	"TryDenoise",
#endif
	"TryHarder", "TryRotate", "TryInvert", "TryDownscale",
	"IsPure", "ReturnErrors", "Formats", "Binarizer", "EanAddOnSymbol",
	"TextMode", "MinLineCount", "MaxNumberOfSymbols",
	NULL};
    enum iOptions {
#ifdef ZXING_EXPERIMENTAL_API
	iTryDenoise,
#endif
	iTryHarder, iTryRotate, iTryInvert, iTryDownscale,
	iIsPure, iReturnErrors, iFormats, iBinarizer,iEanAddOnSymbol,
	iTextMode, iMinLineCount, iMaxNumberOfSymbols
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
	
	/*
	 * get parameters of options with common type
	 */
	
	switch (option) {
	case iTryHarder:
#ifdef ZXING_EXPERIMENTAL_API
	case iTryDenoise:
#endif
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

	/*
	 * set the setting
	 */

	switch (option) {
	case iTryHarder:
		/* Default: 1 */
	    ZXing_ReaderOptions_setTryHarder(opts, intValue);
	    break;
#ifdef ZXING_EXPERIMENTAL_API
	case iTryDenoise:
		/* Default: 0 */
	    ZXing_ReaderOptions_setTryDenoise(opts, intValue);
	    break;
#endif
	case iTryRotate:
		/* Default: 1 */
	    ZXing_ReaderOptions_setTryRotate(opts, intValue);
	    break;
	case iTryInvert:
		/* Default: 1 */
	    ZXing_ReaderOptions_setTryInvert(opts, intValue);
	    break;
	case iTryDownscale:
		/* Default: 1 */
	    ZXing_ReaderOptions_setTryDownscale(opts, intValue);
	    break;
	case iIsPure:
		/* Default: 0 */
	    ZXing_ReaderOptions_setIsPure(opts, intValue);
	    break;
	case iReturnErrors:
		/* Default: 0 */
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
		    ZXing_BarcodeFormats format;
		    Tcl_Obj *formatObj;
		    char * formatString;

		    if (TCL_OK != Tcl_ListObjIndex(interp, objv[argPos], listItem,
			    &formatObj) ) {
			return TCL_ERROR;
		    }
		    formatString = Tcl_GetString(formatObj);
		    /* translate string to format number */
		    format = ZXing_BarcodeFormatFromString(formatString);
		    /* check for unknown format string */
		    if (format == ZXing_BarcodeFormat_Invalid) {
			/* check for special values */
			if (0 == strcmp(formatString, "Any") ) {
			    format = ZXing_BarcodeFormat_Any;
			} else if (0 == strcmp(formatString, "LinearCodes") ) {
			    format = ZXing_BarcodeFormat_LinearCodes;
			} else if (0 == strcmp(formatString, "MatrixCodes") ) {
			    format = ZXing_BarcodeFormat_MatrixCodes;
			} else {
			    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
				    "zxing-cpp format \"%s\" not found",
				    formatString ) );
			    return TCL_ERROR;
			}
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
		 * Default: LocalAverage
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
		 * Default: Ignore
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
		 * Format of the text return key.
		 * Example values for a NULL content code 128 code:
		 * Plain: ASCII 0
		 * ECI: \C0\000026 ASCII 0
		 * HRI: <NUL>
		 * Hex: 00
		 * Escaped: <NUL>
		 * Default: HRI
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
		/* Default: 2 */
	    ZXing_ReaderOptions_setMinLineCount(opts, intValue);
	    break;
	case iMaxNumberOfSymbols:
		/* Default: 255 */
	    ZXing_ReaderOptions_setMaxNumberOfSymbols(opts, intValue);
	    break;
	}
    }
    return TCL_OK;
}


#ifdef TCL_THREADS

/*
 * Structure used for asynchronous decoding carried out by
 * a dedicated thread. At most, one decoding job can be
 * outstanding at any one time.
 */

typedef struct {
    int tip609;			/* When true, TIP#609 is available */
    int *tkFlagPtr;		/* Tk presence flag */
    int run;			/* Controls thread loop */
    Tcl_Mutex mutex;		/* Lock for this struct */
    Tcl_Condition cond;		/* For waking up thread */
    Tcl_ThreadId tid;		/* Thread identifier */
    Tcl_Interp *interp;		/* Interpreter using ZXingCpp decoder */
    Tcl_HashTable evts;		/* AsyncEvents in flight */
    Tcl_ThreadId interpTid;	/* Thread identifier of interp */
    ZXing_ImageView* iv;	/* Thread input: ZXingCpp image struct */
    Tcl_Obj *cmdObj;		/* Callback list object */

    /* Thread input: zxingcpp settings */
    ZXing_ReaderOptions *opts;

    /* Thread output: ms, barcodes structure or error message */
    Tcl_WideInt ms;
    ZXing_Barcodes* barcodes;
    char* error;
} AsyncDecode;

/*
 * Event type for the event queue.
 */

typedef struct {
    Tcl_Event header;
    AsyncDecode *aPtr;
    Tcl_HashEntry *hPtr;
} AsyncEvent;


/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppThread --
 *
 *	Decoder thread, waits per condition for a decode request.
 *	Reports the result back by an asynchronous event which
 *	triggers a do-when-idle handler in the requesting thread.
 *
 *-------------------------------------------------------------------------
 */

static Tcl_ThreadCreateType
ZXingCppThread(ClientData clientData)
{
    AsyncDecode *aPtr = (AsyncDecode *) clientData;
    ZXing_Barcodes* barcodes;
    char * error;
    Tcl_Time now;
    int isNew;
    Tcl_WideInt tw[2], ms;
    AsyncEvent *event;

    Tcl_MutexLock(&aPtr->mutex);
    for (;;) {
	while (aPtr->run && (aPtr->iv == NULL)) {
	    Tcl_ConditionWait(&aPtr->cond, &aPtr->mutex, NULL);
	}
	if (!aPtr->run) {
	    break;
	}
	if (aPtr->iv == NULL) {
	    continue;
	}
	if (aPtr->barcodes != NULL) {
	    ZXing_Barcodes_delete(aPtr->barcodes);
	    aPtr->barcodes = NULL;
	}
	if (aPtr->error != NULL) {
	    ZXing_free(aPtr->error);
	    aPtr->error = NULL;
	}
	Tcl_MutexUnlock(&aPtr->mutex);
	Tcl_GetTime(&now);
	tw[0] = (Tcl_WideInt) now.sec * 1000 + now.usec / 1000;

#ifdef ZXINGCPP_SIMULATE_DECODE_ERROR
	barcodes = ZXing_ReadBarcodes(NULL, aPtr->opts);
#else
	barcodes = ZXing_ReadBarcodes(aPtr->iv, aPtr->opts);
#endif
	if (barcodes == NULL) {
	    error = ZXing_LastErrorMsg();
	} else {
	    error = NULL;
	}
	
	Tcl_GetTime(&now);
	tw[1] = (Tcl_WideInt) now.sec * 1000 + now.usec / 1000;
	ms = tw[1] - tw[0];
	if (ms < 0) {
	    ms = -1;
	}
	Tcl_MutexLock(&aPtr->mutex);

	ZXing_ImageView_delete(aPtr->iv);
	aPtr->iv = NULL;
	ZXing_ReaderOptions_delete(aPtr->opts);
	aPtr->opts = NULL;

	if (aPtr->cmdObj != NULL) {
	    aPtr->ms = ms;
	    aPtr->barcodes = barcodes;
	    aPtr->error = error;
	    event = (AsyncEvent *) ckalloc(sizeof(AsyncEvent));
	    event->header.proc = ZXingCppDecodeHandleEvent;
	    event->header.nextPtr = NULL;
	    event->aPtr = aPtr;
	    event->hPtr = Tcl_CreateHashEntry(&aPtr->evts,
					      (ClientData) event, &isNew);
	    if (aPtr->tip609) {
		/* TCL_QUEUE_TAIL_ALERT_IF_EMPTY */
		Tcl_ThreadQueueEvent(aPtr->interpTid, &event->header,
				     TCL_QUEUE_TAIL | 4);
	    } else {
		Tcl_ThreadQueueEvent(aPtr->interpTid, &event->header,
				     TCL_QUEUE_TAIL);
		Tcl_ThreadAlert(aPtr->interpTid);
	    }
	} else {
	    if (barcodes != NULL) {
		ZXing_Barcodes_delete(barcodes);
	    }
	    if (error != NULL) {
		ZXing_free(error);
	    }
	}
    }
    Tcl_MutexUnlock(&aPtr->mutex);
    Tcl_ExitThread(0);
    TCL_THREAD_CREATE_RETURN;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppDecodeHandleEvent --
 *
 *	Process decode completion event.
 *
 *-------------------------------------------------------------------------
 */

static int
ZXingCppDecodeHandleEvent(Tcl_Event *evPtr, int flags)
{
    AsyncEvent *aevPtr = (AsyncEvent *) evPtr;
    AsyncDecode *aPtr = aevPtr->aPtr;
    int ret = TCL_OK;
    ZXing_Barcodes* barcodes;
    char * error;
    Tcl_Obj *cmdObj;
    Tcl_WideInt ms;

    if ((aPtr == NULL) || (aPtr->interpTid == NULL)) {
	return 1;
    }
    Tcl_Preserve(aPtr);
    Tcl_Preserve(aPtr->interp);
    Tcl_MutexLock(&aPtr->mutex);
    if (aevPtr->hPtr != NULL) {
	Tcl_DeleteHashEntry(aevPtr->hPtr);
    }
    cmdObj = aPtr->cmdObj;
    aPtr->cmdObj = NULL;
    ms = aPtr->ms;
    barcodes = aPtr->barcodes;
    aPtr->barcodes = NULL;
    error = aPtr->error;
    aPtr->error = NULL;
    
    /*
     * Delete if not used
     */
    
    if (cmdObj == NULL) {
	if (barcodes != NULL) {
	    ZXing_Barcodes_delete(barcodes);
	}
	if (error != NULL) {
	    ZXing_free(error);
	}
    }

    Tcl_MutexUnlock(&aPtr->mutex);

    if (cmdObj != NULL) {
    
	/*
	 * The ref count is incremented when the object is put into the
	 * object structure.
	 * If not shared (unlikely) it is disposed below.
	 * If shared, a copy is made and the ref count is decremented.
	 * Then, the copy is disposed below.
	 * Note that the interpreter crashed, when the ref count was not
	 * decremented (TCL 8.6.16) in the next unrelated event
	 * (in my case: a Tk configure event).
	 */
	
	if (Tcl_IsShared(cmdObj)) {
	    Tcl_Obj *cmdObj2 = cmdObj;
	    cmdObj = Tcl_DuplicateObj(cmdObj2);
	    Tcl_DecrRefCount(cmdObj2);
	    Tcl_IncrRefCount(cmdObj);
	}
	
	if (barcodes != NULL) {

	    /*
	     * Report a barcode scan
	     * Note that barcode or error may by != 0.
	     */
	
	    ret = BarcodesToResultList(aPtr->interp, cmdObj, ms, barcodes);
	    ZXing_Barcodes_delete(barcodes);

	} else {

	    /*
	     * Report a decoder error with a time and a dict with keys:
	     * - errorType: DecoderFailure
	     * - errorMsg: message from decoder
	     */

	    Tcl_Obj *timeObj;
	    timeObj = Tcl_NewWideIntObj(ms);
	    ret = Tcl_ListObjAppendElement(aPtr->interp, cmdObj, timeObj);
	    if (ret != TCL_OK) {
		Tcl_DecrRefCount(timeObj);
	    } else {
		char * errorCur;
		
		Tcl_Obj * resultDict = Tcl_NewDictObj();

		/* Key errorType: */
		Tcl_DictObjPut(aPtr->interp, resultDict,
			Tcl_NewStringObj("errorType",-1),
			Tcl_NewStringObj("DecoderFailure",-1));
    
		/*
		 * Key errorMsg:
		 * The error message is provided by zxing in pointer "error".
		 * This should not be NULL. Nevertheless, zxingcpp controls
		 * this. So, provide a generic error message, if none provided.
		 */

		if (error != NULL) {
		    errorCur = error;
		} else {
		    errorCur = "No error details reported by ZXing-Cpp";
		}
		Tcl_DictObjPut(aPtr->interp, resultDict,
			Tcl_NewStringObj("errorMsg",-1),
			Tcl_NewStringObj(errorCur,-1));
		
		ret = Tcl_ListObjAppendElement(aPtr->interp, cmdObj,
			resultDict);
	    }
	}
	if (error != NULL) {
	    ZXing_free(error);
	}
	
	/*
	 * Invoke passed command.
	 * It is important to free anything before this call.
	 * Anything may happen here: events - long calculation - another
	 * decode call.
	 */
	    
	if (ret == TCL_OK) {
	    ret = Tcl_EvalObjEx(aPtr->interp, cmdObj, TCL_GLOBAL_ONLY);
	}
	Tcl_DecrRefCount(cmdObj);
    }

    if (ret == TCL_ERROR) {
	Tcl_AddErrorInfo(aPtr->interp, "\n    (zxingcpp event handler)");
	Tcl_BackgroundException(aPtr->interp, ret);
    }
    Tcl_Release(aPtr->interp);
    Tcl_Release(aPtr);
    return 1;	/* event handled */
}

/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppAsyncStop --
 *
 *	Stop the decoder thread, if any.
 *
 *-------------------------------------------------------------------------
 */

static int
ZXingCppAsyncStop(Tcl_Interp *interp, AsyncDecode *aPtr)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    Tcl_MutexLock(&aPtr->mutex);
    if (aPtr->run) {
	int dummy;

	aPtr->run = 0;
	Tcl_ConditionNotify(&aPtr->cond);
	Tcl_MutexUnlock(&aPtr->mutex);
	Tcl_JoinThread(aPtr->tid, &dummy);
	aPtr->tid = NULL;
	Tcl_MutexLock(&aPtr->mutex);
    }
    aPtr->interpTid = NULL;
    /* Invalidate AsyncEvents in flight. */
    hPtr = Tcl_FirstHashEntry(&aPtr->evts, &search);
    while (hPtr != NULL) {
	AsyncEvent *event = (AsyncEvent *) Tcl_GetHashKey(&aPtr->evts, hPtr);

	event->aPtr = NULL;
	event->hPtr = NULL;
	Tcl_DeleteHashEntry(hPtr);
	hPtr = Tcl_NextHashEntry(&search);
    }
    if (aPtr->cmdObj != NULL) {
	Tcl_DecrRefCount(aPtr->cmdObj);
	aPtr->cmdObj = NULL;
    }
    if (aPtr->barcodes != NULL) {
	ZXing_Barcodes_delete(aPtr->barcodes);
	aPtr->barcodes = NULL;
    }
    if (aPtr->error != NULL) {
	ZXing_free(aPtr->error);
	aPtr->error = NULL;
    }
    Tcl_MutexUnlock(&aPtr->mutex);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppAsyncState --
 *
 *	Return status of decoder thread, if any.
 *
 *-------------------------------------------------------------------------
 */

static int
ZXingCppAsyncStatus(Tcl_Interp *interp, AsyncDecode *aPtr)
{
    int state;
    char *stateString;

    Tcl_MutexLock(&aPtr->mutex);
    if (!aPtr->run) {
	state = 0;
    } else if ((aPtr->iv != NULL) ||
	       (aPtr->cmdObj != NULL)) {
	state = 2;
    } else {
	state = 1;
    }
    Tcl_MutexUnlock(&aPtr->mutex);
    switch (state) {
    case 1:
	stateString = "ready";
	break;
    case 2:
	stateString = "running";
	break;
    default:
	stateString = "stopped";
	break;
    }
    Tcl_SetResult(interp, stateString, TCL_STATIC);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppAsyncStart --
 *
 *	Check/start the decoder thread. Error cases:
 *	- thread creation failed, could not be started
 *	- thread is already started but still processing a request
 *	  and the reporting event was not processed jet.
 *
 *-------------------------------------------------------------------------
 */

static int
ZXingCppAsyncStart(Tcl_Interp *interp, AsyncDecode *aPtr)
{
    int success = 0;

    Tcl_MutexLock(&aPtr->mutex);
    if (!aPtr->run) {
	if (Tcl_CreateThread(&aPtr->tid, ZXingCppThread, (ClientData) aPtr,
			     TCL_THREAD_STACK_DEFAULT, TCL_THREAD_JOINABLE)
	    == TCL_OK) {
	    aPtr->interp = interp;
	    aPtr->interpTid = Tcl_GetCurrentThread();
	    aPtr->run = success = 1;
	}
    } else if ((aPtr->iv != NULL) ||
	       (aPtr->cmdObj != NULL)) {
	success = -1;
    } else {
	success = 1;
    }
    Tcl_MutexUnlock(&aPtr->mutex);
    if (success < 0) {
	Tcl_SetResult(interp, "decode process still running", TCL_STATIC);
	return TCL_ERROR;
    }
    if (success == 0) {
	Tcl_SetResult(interp, "decode process not started", TCL_STATIC);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppAsyncFree --
 *
 *	Free zbar::async_decode command client data structure.
 *
 *-------------------------------------------------------------------------
 */

static void
ZXingCppAsyncFree(char *clientData)
{
    AsyncDecode *aPtr = (AsyncDecode *) clientData;

    ZXingCppAsyncStop(aPtr->interp, aPtr);
    Tcl_MutexLock(&aPtr->mutex);
    Tcl_DeleteHashTable(&aPtr->evts);
    Tcl_MutexUnlock(&aPtr->mutex);
    Tcl_ConditionFinalize(&aPtr->cond);
    Tcl_MutexFinalize(&aPtr->mutex);
    ckfree((char *) aPtr);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppAsyncCmdDeleted --
 *
 *	Callback for deletion of zxingcpp::async_decode command.
 *
 *-------------------------------------------------------------------------
 */

static void
ZXingCppAsyncCmdDeleted(ClientData clientData)
{
    Tcl_EventuallyFree(clientData, ZXingCppAsyncFree);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppAsyncDecodeObjCmd --
 *
 *	zxingcpp::async_decode Tcl command, asynchronous decoding.
 *	Command formats/arguments are
 *
 *	Stop (finish) decoder thread, releasing resources
 *
 *		zxingcpp::async_decode stop
 *
 *	Return status of asynchronous decoding process
 *
 *		zxingcpp::async_decode status
 *
 *	Start decoding an image
 *
 *		zxingcpp::async_decode photoEtc callback ?syms?
 *
 *		photoEtc	photo image name or list of
 *				{width height bpp bytes}
 *		callback	procedure to invoke at end of
 *				decoding process
 *		?opt1 val1? ...	decoder options key-value pairs
 *
 *	Arguments appended to callback
 *
 *		time		decode/processing time in milliseconds
 *		decoded1 ...	decoded data dicts, one per code
 *
 *-------------------------------------------------------------------------
 */

static int
ZXingCppAsyncDecodeObjCmd(ClientData clientData, Tcl_Interp *interp,
		      int objc,  Tcl_Obj *const objv[])
{
    AsyncDecode *aPtr = (AsyncDecode *) clientData;
    int nCmdObjs;
    ZXing_ImageView* iv = NULL;
    ZXing_ReaderOptions* opts;

    if ((objc < 2)) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"status|stop|photoEtc ?callback? ?opt1 val1? ...");
	return TCL_ERROR;
    }
    if (objc == 2) {
	const char *cmd = Tcl_GetString(objv[1]);

	if (strcmp(cmd, "status") == 0) {
	    return ZXingCppAsyncStatus(interp, aPtr);
	}
	if (strcmp(cmd, "stop") == 0) {
	    return ZXingCppAsyncStop(interp, aPtr);
	}
	Tcl_WrongNumArgs(interp, 1, objv, "status|stop");
	return TCL_ERROR;
    }

    /*
     * background decode command follows
     * First get zxingcpp visual from image argument
     */
    
    if (TCL_OK != ArgumentToZXingCppVisual(aPtr->tkFlagPtr, interp, &iv,
	    objv[1]) ) {
	return TCL_ERROR;
    }

    /*
     * Start thread and check if eventual last decode event was fired
     */
    
    if (ZXingCppAsyncStart(interp, aPtr) != TCL_OK) {
	ZXing_ImageView_delete(iv);
	return TCL_ERROR;
    }
    
    /*
     * Check command object to be a list and to contain more than 1 element
     */
    
    if (Tcl_ListObjLength(interp, objv[2], &nCmdObjs) != TCL_OK) {
	ZXing_ImageView_delete(iv);
	return TCL_ERROR;
    }
    if (nCmdObjs <= 0) {
	Tcl_SetResult(interp, "empty callback", TCL_STATIC);
	ZXing_ImageView_delete(iv);
	return TCL_ERROR;
    }

    /*
     * Handle reader options
     */

    opts = ZXing_ReaderOptions_new();
    if (TCL_ERROR == ReaderOptionsGet(interp, objc-3,  &objv[3], opts) ) {
	ZXing_ReaderOptions_delete(opts);
	ZXing_ImageView_delete(iv);
	return TCL_ERROR;
    }

    /*
     * Start decode in worker thread
     */
    
    Tcl_MutexLock(&aPtr->mutex);
    aPtr->opts = opts;
    aPtr->iv = iv;
    aPtr->cmdObj = objv[2];
    Tcl_IncrRefCount(aPtr->cmdObj);
    Tcl_ConditionNotify(&aPtr->cond);
    Tcl_MutexUnlock(&aPtr->mutex);
    return TCL_OK;
}
#endif /* TCL_THREADS */

#ifndef ZXINGCPP_NO_TK
/*
 *-------------------------------------------------------------------------
 *
 * CheckForTk --
 *
 *	Check for availability of Tk package.
 *
 *-------------------------------------------------------------------------
 */

static int
CheckForTk(Tcl_Interp *interp, int *tkFlagPtr)
{
    if (*tkFlagPtr > 0) {
	return TCL_OK;
    } else if (*tkFlagPtr < 0) {
	Tcl_SetResult(interp, "can't find package Tk", TCL_STATIC);
	return TCL_ERROR;
    }
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, TK_VERSION, 0) == NULL) {
	*tkFlagPtr = -1;
	return TCL_ERROR;
    }
#else
    if (Tcl_PkgRequire(interp, "Tk", TK_VERSION, 0) == NULL) {
	*tkFlagPtr = -1;
	return TCL_ERROR;
    }
#endif
    *tkFlagPtr = 1;
    return TCL_OK;
}
#endif

/*
 *-------------------------------------------------------------------------
 *
 * ArgumentToZXingCppVisual --
 *
 *	Transform a provided argument to a zxingcpp visual
 *
 *		tkFlagPtr	thread global memory to save, if tk is
 *				present
 *		interp		interpreter for error reporting
 *		ivPtr		to save zxingcpp visual pointer to.
 *		argObj		Tcl object which may contain:
 *				- a list of 4 items width height bpp blop
 *				- a tk image name
 *
 *	Result is standard Tcl result.
 *	On success, ivPtr is set.
 *	On error, the result of the intrpreter is set
 *
 *-------------------------------------------------------------------------
 */

static int 
ArgumentToZXingCppVisual(int *tkFlagPtr, Tcl_Interp *interp,
	ZXing_ImageView **ivPtr, Tcl_Obj *const argObj)
{
    Tk_PhotoImageBlock block;
    int bpp, nElems;
    unsigned char *pixPtr = NULL;
    ZXing_ImageView *iv = NULL;
    Tcl_Obj **elems;

    if (Tcl_ListObjGetElements(interp, argObj, &nElems, &elems) != TCL_OK) {
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
	if ((bpp != 1) && (bpp != 3)) {
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
    } else {
#ifdef ZXINGCPP_NO_TK
	Tcl_SetResult(interp, "need list of width, height, bpp, bytes",
		      TCL_STATIC);
	return TCL_ERROR;
#else
	Tk_PhotoHandle handle;

	if (CheckForTk(interp, tkFlagPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	handle = Tk_FindPhoto(interp, Tcl_GetString(argObj));
	if (handle == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("photo \"%s\" not found",
						   Tcl_GetString(argObj)));
	    return TCL_ERROR;
	}
	if (Tk_PhotoGetImage(handle, &block) != 1) {
	    Tcl_SetResult(interp, "error retrieving photo image", TCL_STATIC);
	    return TCL_ERROR;
	}
    }
#endif

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
     * Return visual value
     */
    
    *ivPtr = iv;
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * BarcodesToResultList --
 *
 *	Transform the zxingcpp result to a result list
 *
 *		interp		TCL interpreter for error reporting
 *		resultList	List object to append the data to
 *		td		Time argument
 *		barcodes	zxingcpp barcodes object
 *
 *	Result is a standard TCL result.
 *	Errors may arise, if the passed object is not a list or shared.
 *
 *-------------------------------------------------------------------------
 */

static int
BarcodesToResultList(Tcl_Interp *interp, Tcl_Obj *resultList, 
	Tcl_WideInt td, ZXing_Barcodes* barcodes
	)
{
    Tcl_Encoding utf8Encoding;
    Tcl_DString recode;
    Tcl_Obj *timeObj;

    /*
     * Add time as first argument.
     * Also check, if passed argument is a list.
     */
    
    timeObj = Tcl_NewWideIntObj(td);
    if (TCL_OK != Tcl_ListObjAppendElement(interp, resultList, timeObj) ) {
	Tcl_DecrRefCount(timeObj);
	return TCL_ERROR;
    }
    
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
	ZXing_Position zxing_position;
	Tcl_Obj * positionArray[8];

	/*
	 * Build a dict with the result keys
	 */
	
	/* Key text: interpretation line, utf-8 encoded */
	zxingcppString = ZXing_Barcode_text(barcode);
	Tcl_DStringAppend(&recode, zxingcppString, -1);
	ZXing_free(zxingcppString);
	Tcl_ExternalToUtfDString(utf8Encoding, ZXing_Barcode_text(barcode), -1, &recode); 
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("text",-1),
		Tcl_NewStringObj(Tcl_DStringValue (&recode),
		    Tcl_DStringLength (&recode)));
	Tcl_DStringFree(&recode);

	/* Key format: symbology, ASCII encoded and zero terminated */
	zxingcppString = ZXing_BarcodeFormatToString(
		ZXing_Barcode_format( barcode) );
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("format", -1),
		Tcl_NewStringObj( zxingcppString, -1) );
	ZXing_free(zxingcppString);

	/* Key bytes: */
	bytePtr=ZXing_Barcode_bytes(barcode, &len);
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("bytes",-1),
		Tcl_NewByteArrayObj(bytePtr,len));

	/* Key bytesECI: */
	bytePtr=ZXing_Barcode_bytesECI(barcode, &len);
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("bytesECI",-1),
		Tcl_NewByteArrayObj(bytePtr,len));

	/*
	 * Key content: one of: Text, Binary, Mixed, GS1, ISO15434, UnknownECI
	 */
	zxingcppString = ZXing_ContentTypeToString(
		ZXing_Barcode_contentType(barcode));
	Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("content",-1),
		Tcl_NewStringObj(zxingcppString ,-1));
	ZXing_free(zxingcppString);

	/* Key symbologyIdentifier: Example: ]C0 for Code128 */
	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("symbologyIdentifier", -1),
		Tcl_NewStringObj( ZXing_Barcode_symbologyIdentifier(barcode),
		    -1));

	/* Key hasECI: true if ECI present */
	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("hasECI", -1),
		Tcl_NewBooleanObj( ZXing_Barcode_hasECI(barcode) ) );

	/*
	 * Key ecLevel: string representing the EC level. Empty string if not
	 * used by the symbology.
	 */

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("ecLevel", -1),
		Tcl_NewStringObj( ZXing_Barcode_ecLevel(barcode), -1));

	/*
	 * Key position: list of numbers: topLeft.x, topLeft.y, topRight.x,
	 * topRight.y, bottomRight.x, bottomRight.y,, bottomLeft.x,
	 * bottomLeft.y.
	 */
	
	zxing_position = ZXing_Barcode_position(barcode);
	positionArray[0] = Tcl_NewIntObj( zxing_position.topLeft.x );
	positionArray[1] = Tcl_NewIntObj( zxing_position.topLeft.y );
	positionArray[2] = Tcl_NewIntObj( zxing_position.topRight.x );
	positionArray[3] = Tcl_NewIntObj( zxing_position.topRight.y );
	positionArray[4] = Tcl_NewIntObj( zxing_position.bottomRight.x );
	positionArray[5] = Tcl_NewIntObj( zxing_position.bottomRight.y );
	positionArray[6] = Tcl_NewIntObj( zxing_position.bottomLeft.x );
	positionArray[7] = Tcl_NewIntObj( zxing_position.bottomLeft.y );

	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("position", -1),
		Tcl_NewListObj(8, positionArray) );

	/* Key orientation: symbol orientation in degrees, clockwise */
	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("orientation", -1),
		Tcl_NewIntObj( ZXing_Barcode_orientation(barcode) ) );

	/* Key isMirrored: 1 if code was up-side down */
	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("isMirrored", -1),
		Tcl_NewBooleanObj( ZXing_Barcode_isMirrored(barcode) ) );

	/* Key isInverted: 1 if code was inverted */
	Tcl_DictObjPut(interp, resultDict,
		Tcl_NewStringObj("isInverted", -1),
		Tcl_NewBooleanObj( ZXing_Barcode_isInverted(barcode) ) );

	if (!ZXing_Barcode_isValid(barcode)) {
	    char * typeText;
	    /* Key errorType: */
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

	    /* Key errorMsg: */
	    Tcl_DictObjPut(interp, resultDict, Tcl_NewStringObj("errorMsg",-1),
		    Tcl_NewStringObj(ZXing_Barcode_errorMsg(barcode),-1));
	}
	
	/*
	 * Append result dict to result list
	 */
	
	Tcl_ListObjAppendElement(interp, resultList, resultDict);
    }

    Tcl_FreeEncoding(utf8Encoding);
    Tcl_DStringFree(&recode);
   
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZxingcppDecodeObjCmd --
 *
 *	zxingcpp::decode Tcl command, synchronous decoding.
 *	Command arguments are
 *
 *		zxingcpp::decode photoEtc ?option1 value1? ...
 *
 *		photoEtc	photo image name or list of
 *				{width height bpp bytes}
 *		options		option /value pairs to the decoder
 *
 *	Result is a list composed of the following elements
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
    unsigned char *pixPtr = NULL;
    ZXing_ImageView* iv = NULL;
    ZXing_ReaderOptions* opts;
    ZXing_Barcodes* barcodes;
    Tcl_Time now;
    Tcl_WideInt tw[2], td;
    Tcl_Obj *resultList;

    if ( objc < 2 ) {
	Tcl_WrongNumArgs(interp, 1, objv, "photoEtc ?opt1 val1? ...");
	return TCL_ERROR;
    }
    Tcl_GetTime(&now);
    tw[0] = (Tcl_WideInt) now.sec * 1000 + now.usec / 1000;
    
    /*
     * Transform 2nd argument to a zxingcpp image
     */
    
    if (TCL_OK != ArgumentToZXingCppVisual( (int *)tkFlagPtr, interp, &iv,
	    objv[1]) ) {
	return TCL_ERROR;
    }

    /*
     * Handle reader options
     */

    opts = ZXing_ReaderOptions_new();
    if (TCL_ERROR == ReaderOptionsGet(interp, objc-2,  &objv[2], opts) ) {
	ZXing_ReaderOptions_delete(opts);
	ZXing_ImageView_delete(iv);
	return TCL_ERROR;
    }

    /*
     * Read the bar code
     */

#ifdef ZXINGCPP_SIMULATE_DECODE_ERROR
    barcodes = ZXing_ReadBarcodes(NULL, opts);
#else
    barcodes = ZXing_ReadBarcodes(iv, opts);
#endif

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
    if (TCL_OK != BarcodesToResultList(interp, resultList, td, barcodes)) {

	/*
	 * This may currently not fail, as the provided object is always a list
	 * Nevertheless, have fail code here.
	 */

	ZXing_Barcodes_delete(barcodes);
	return TCL_ERROR;
    }

    ZXing_Barcodes_delete(barcodes);
    
    /*
     * Return list of result dicts
     */

    Tcl_SetObjResult(interp, resultList);
    return TCL_OK;
}

#ifndef TCL_THREADS
/*
 *-------------------------------------------------------------------------
 *
 * ZXingCppAsyncDecodeObjCmd --
 *
 *	zxingcpp::async_decode Tcl command, asynchronous decoding.
 *	Dummy for non-threaded builds
 *
 *-------------------------------------------------------------------------
 */

static int
ZXingCppAsyncDecodeObjCmd_NoThreads(ClientData clientData,
	Tcl_Interp *interp, int objc,  Tcl_Obj *const objv[])
{
    Tcl_SetResult(interp, "unsupported in non-threaded builds",
		  TCL_STATIC);
    return TCL_OK;
}
#endif

/*
 *-------------------------------------------------------------------------
 *
 * InterpCleanupProc --
 *
 *	Called when interp is destroyed.
 *
 *-------------------------------------------------------------------------
 */

static void
InterpCleanupProc(ClientData clientData, Tcl_Interp *interp)
{
    ckfree((char *) clientData);
}

/*
 *-------------------------------------------------------------------------
 *
 * Zxingcpp_Init --
 *
 *	Module initializer
 *
 *-------------------------------------------------------------------------
 */

DLLEXPORT int
Zxingcpp_Init(Tcl_Interp *interp)
{
#ifdef TCL_THREADS
    AsyncDecode *aPtr;
    int major = 0, minor = 0;
    const char *val;
#endif
    int *tkFlagPtr;
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

    tkFlagPtr = (int *) ckalloc(sizeof(int));
    *tkFlagPtr = 0;
    Tcl_CallWhenDeleted(interp, InterpCleanupProc, (ClientData) tkFlagPtr);
#ifdef TCL_THREADS
    aPtr = (AsyncDecode *) ckalloc(sizeof(AsyncDecode));
    memset(aPtr, 0, sizeof(AsyncDecode));
    aPtr->tkFlagPtr = tkFlagPtr;
    Tcl_InitHashTable(&aPtr->evts, TCL_ONE_WORD_KEYS);
    Tcl_CreateObjCommand(interp, "zxingcpp::async_decode",
			 ZXingCppAsyncDecodeObjCmd, (ClientData) aPtr,
			 ZXingCppAsyncCmdDeleted);
    Tcl_GetVersion(&major, &minor, NULL, NULL);
    if ((major > 8) || ((major == 8) && (minor > 6))) {
	aPtr->tip609 = 1;
    } else {
	val = Tcl_GetVar2(interp, "tcl_platform", "tip609", TCL_GLOBAL_ONLY);
	if ((val != NULL) && *val && (*val != '0')) {
	    aPtr->tip609 = 1;
	}
    }
#else
    Tcl_CreateObjCommand(interp, "zxingcpp::async_decode",
			 ZXingCppAsyncDecodeObjCmd_NoThreads, NULL, NULL);
#endif
    Tcl_CreateObjCommand(interp, "::zxingcpp::decode", ZxingcppDecodeObjCmd,
			 (ClientData) tkFlagPtr, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateObjCommand(interp, "zxingcpp::formats", ZxingcppFormatsObjCmd,
			 (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
