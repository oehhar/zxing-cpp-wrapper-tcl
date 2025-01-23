% zxingcpp(n) | zxingcpp user documentation
# NAME

zxingcpp - Decode barcode symbolgies present in images using the zint-cpp
library.

# SYNOPSIS

package require zxingcpp
[::zxingcpp::formats]{.cmd} [special]{.optarg}

[::zxingcpp::decode]{.cmd} [photoetc]{.arg} [opt value]{.optdot}

[::zxingcpp::async_decode]{.cmd} [photoetc]{.arg} [callback]{.arg} [opt value]{.optdot}

[::zxingcpp::async_decode]{.cmd} [status]{.sub}

[::zxingcpp::async_decode]{.cmd} [stop]{.sub}

# DESCRIPTION

Pixel image data is analysed for barcode symbols.
Detected barcode symbols are decoded and the data and properties are returned.

This package highly uses the names used in the
[zxing-cpp](https://github.com/zxing-cpp/zxing-cpp)
library and exposes its C interface to TCL.
In addition, the [zbar](https://www.androwish.org/home/wiki?name=zbar+command)
interface is used within this package.

The package may be used with or without Tk.
If Tk is not present, the input format of a tk photo is not available.
If Tcl (8.6) was build without thread support, the background decode is not
available.
Supported Tcl versions are 8.6 to 9.

---

[::zxingcpp::formats]{.cmd} [special]{.optarg}
:	Return the list of supported formats.
	**Formats** is the zint-cpp term for bar code symbologies.
	The optional boolean command switch [special]{optarg} (default: false) allows
	to include the special format names **Any**, **LinearCodes** and
	**MatrixCodes**.

---

[::zxingcpp::decode]{.cmd} [photoetc]{.arg} [opt value]{.optdot}
:	Decode an image and return decode result.
	The arguments are:

	[photoetc]{.arg}
	:   image specification. _photoetc_ may be in two formats.
	The first format is a **tk image photo name**.
	The second format is a list describing a greyscale or RGB image with 8 bits
	per color component.
	The list consists of the 4 elements _width_, _height_,
	_number_ and _data_.
	_number_ is the pixel byte length and may be 1 for greyscale or 3 for an
	RGB image.
	_data_ is a byte array containing the image data.

	Note that not all photo pixel formats are supported, as Tk has a relatively
	loose memory organisation.
	In practice, no issue was found.
	An eventual error message will inform about the incompatible format.

	[opt value]{.optdot}
	: decoder options described as alternating key and value items.
	Keys may be abbreviated.
	The following keys are supported:
	
	* **Binarizer**:
		There are 4 binarizer available: **LocalAverage** (default),
		**GlobalHistogram**, **FixedThreshold** or **BoolCast**.
	* **EanAddOnSymbol**:
		There are 3 option values available to handle EAN add-on bars:
		* **Ignore** (default): ignore any bar
		* **Read**: return if present
		* **Require**: only read EAN with add-on bars
	* **Formats**:
		List of formats activated for this decode.
		Any return value of [::zxingcpp::formats] may be given as a list item.
	* **IsPure**:
		Boolean parameter with default value _false_.
		Is it a pure code/image.
	* **MaxNumberOfSymbols**:
		Number 0-255 with default value of 255.
		Maximum number of returned code dicts.
		Remark, that an errorneous result may be returned twice.
	* **MinLineCount**:
		Number with default value of 2.
		Minimum line count of multi-line symbologies like PDF417 and DataBar.
	* **ReturnErrors**:
		Boolean parameter with default value _false_.
		If true, additional checks are performed like a GTIN checksum.
		An additional code containing the errorType key.
	* **TextMode**:
		The following modes are available for the format of the _Text_ return
		key data.
		As an example, the result for a code only containing a binary 0 is
		given.

		* **Plain** : verbatim code data (copy of the return key _bytes_).
			Example content: binary 0
		* **ECI** : data in ECI format (copy of the return key _bytesECI_).
			Example content: \]C0\\000026, followed by a binary 0
		* **HRI** (default): Readable text-only string.
			Example content: <NUL>
		* **Hex** : Hexadecimal encoding.
			Example content: 00
		* **Escaped** : Textual representation for control and special
			characters.
			Example content: <NUL>
	
	* **TryDownscale**:
		Boolean parameter with default value _true_.
		In addition, work on a downscaled image.
	* **TryHarder**:
		Boolean parameter with default value _true_.
		Additional decoding functions are added requiring more time.
	* **TryInvert**:
		Boolean parameter with default value _true_.
		Also try inverted code symbols.
	* **TryRotate**:
		Boolean parameter with default value _true_.
		Also try rotated code symbols.

	The return value is a list of 1 to 256 elements.
	The first element is the computation time in milliseconds.
	Any following list element is a result dict of a decoded code symbol.
	No symbols were found, if there are no following list elements.

	A result dict may have the following keys.
	The given example text is an EAN 13 symbol with a wrong checksum within the
	data.
	The option _ReturnError_ was specified as true:

	* **text**:
		returned data in the format given by the option [TextMode].
		Example: 6711881250884
	* **format**:
		detected symbology. A string as returned by [::zxingcpp::formats]
		Example: EAN-13
	* **bytes**:
		Raw data contents
		Example: 6711881250884
	* **bytesECI**:
		symbology identifier followed by (evetually codepage transformed) data.
		Example: \]E06711881250884
	* **content**:
		Type of content. May be one of: Text, Binary, Mixed, GS1, ISO15434 or UnknownECI.
		Example: Text
	* **symbologyIdentifier**:
		ISO/IEC 15424 symbology identifier
		Example: \]E0
	* **hasECI**:
		flag, if an ECI was present within the data
		Example: 0
	* **ecLevel**:
		Level of error correction for suitable codes like QR-Code
		Example: empty string
	* **position**:
		List of symbol location coordinates: topLeft.x, topLeft.y,
		topRight.x, topRight.y, bottomRight.x bottomRight.y, bottomLeft.x and
		bottomLeft.y.
	Example: 24 36 189 36 189 40 24 40}
	* **orientation**:
		rotation of the code symbol 0-359, clockwise in degrees.
		Example: 0
	* **isMirrored**:
		true for mirrored symbol
		Example: 0
	* **isInverted**:
		true for inverted symbol
		Example: 0
	* **errorType**: type of error.
		This key may arise, if option _ReturnError_ is true and a data error happened.
		Possible values are: None, Format, Checksum, Unsupported
		Example: Checksum
	* **errorMsg**:
		error message. Present, if errorType is present.
		Example: ChecksumError @ ODMultiUPCEANReader.cpp:283

---

[::zxingcpp::async_decode]{.cmd} [photoetc]{.arg} [callback]{.arg} [opt value]{.optdot}
:	This command starts a decode using a background thread and reports the result
	using a callback.
	It requires the Tcl core being built with thread support, and a running event
	loop since the callback is invoked as an event or do-when-idle handler.

	Only a single thread instance is supported per Tcl interpreter,
	i.e. another asynchronous decode process can only be started when a previous
	decode process has finished.
	An error is raised, if a decode is still running.

	The arguments are as follows:

	[photoetc]{.arg}
	: as described for [::zxingcpp::decode]

	[callback]{.arg}
	: A command prefix called when the decoding is finished.
	The arguments _time_ and as many _result dicts_ as detected code symbols
	are appended to the command prefix.

	[opt value]{.optdot}
	: as described for [::zxingcpp::decode]

[::zxingcpp::async_decode]{.cmd} [status]{.sub}
:	Returns the current state of the asynchronous decode thread as a string:
_stopped_ when no asynchronous decode thread has been started,
_running_ when a asynchronous decode is in progress,
and _ready_ when the next asynchronous decode can be started.

[::zxingcpp::async_decode]{.cmd} [stop]{.sub}
:	Stops the background thread for asynchronous decoding if it has been
implicitely started by a prior [zxingcpp::async_decode].
This can be useful to conserve memory resources.

# WEBCAM DECODER

A webcam reader could be implemented as described for zbar in the
androwish/undroidwish sample file
[zbartool.tcl](https://www.androwish.org/home/file?name=assets/zbartool0/zbartool.tcl&ci=tip).

# CREDITS

Christian Werner for the framework, which was copied from zbar TCL interface

# DEPENDENCIES

Tcl >= 8.6.0
