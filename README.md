# UnRAWer
 Small GUI utility to Batch process Camera RAW images with OpenColorIO 3D Lut support. A tool designed as a
 helper tool for photogrammetry (3D scanning) image batch processing tool where speed is more important than
 feature-rich raw image processors.

![UnRAWer2](https://github.com/ssh4net/UnRAWer/assets/3924000/798b24ff-bdac-451c-aebc-47256d0fff7a)

 ## Core features
 - Support all camera raws supported in Libraw library, including ProRAW DNG
 - Headless (CLI) mode
 - Multithreaded and asynchronous batch processing
 - Drag and drop interface with recursive subfolders support
 - Half Resolution camera raws import
 - Export as raw sensor data (bw), Bayers pattern (RGB), and different demosaic methods (supported in libraw)
 - Smart (per folder_suffix/filename_suffix) 3D Lut grading presets (via OpenColorIO)
 - Smart per-camera (Make, Model) LUT
 - Export as 8/16/32bit int/float tiff/jpeg/jpeg2000/jpegxl/heic/PPM/PNG
 - Export Exif and XMP metatags (only subset)
 - tool configuration via TOML config file

![UnRAWer](https://github.com/ssh4net/UnRAWer/assets/3924000/c8414525-ab87-4ce7-8110-f7a18161a658)

Limitations
-------
Cropping RAWs not garantee to be identical to commercial raw processors crop.

# User Manual 
Settings available through *.toml config file and UI menu

## Global
Global app settings

`Console = true/false`

### Threads count for read and write
Choose this settings depending on your CPU, memory and IO specs

`Threads = 10`

### Threads multiplier for processing 1.0 equal all cores/threads
(TODO: check if still used)

`ThredsMult = 1.0`

### Export into subfolders
If set to true, processed images will be stored in lut_name folder, otherwise lut_name will be added as a suffix (aka. filename_lut_name.ext)

`ExportSubf = true`

### Global subfolders preffix
relative path preffix. For example "../Proc" will create a folder Proc in a parent folder to a camera raw images source folder.

`PathPrefix = ""`

### log level is 0-5, 0 is most verbose
0 = trace, 1 = debug, 2 = info, 3 = warning, 4 = error, 5 = fatal
- 0 = only fatal errors, 
- 1 = error,
- 2 = warning, 
- 3 = info (default), 
- 4 = debug, 
- 5 = trace (most outputs)

`Verbosity = 3`

## Range

### Range conversion mode
(TODO: check if still needed, this part of code from Solidify)
- 0 - Unsigned 0.0 ~ 1.0
- 1 - Signed -1.0 ~ 1.0
- 2 - Signed to Unsigned -1.0~1.0 -> 0.0~1.0
- 3 - Unsigned to Signed 0.0~1.0 -> -1.0~1.0
  
`RangeMode = 0`

## Export
Export settings

### FileFormat:
- -1 - Same as input. If input format can't be
    used for export (for example CameraRAWs) 
    app will use default 
- 0 - TIFF
- 1 - OpenEXR
- 2 - PNG
- 3 - JPEG
- 4 - JPEG-2000
- 5 - JPEG XL
- 6 - HEIC
- 7 - PPM

`DefaultFormat = 3`
`FileFormat = -1`

### Bit depth: 
- -1 - Original
- 0 - uint8 (8bit unsigned int)
- 1 - uint16 (16bit unsigned int)
- 2 - uint32 (64bit unsigned int) !! most file formats have not support 32bit unsigned int
- 3 - uint64 (64bit unsigned int) !! most file formats have not support double precision
- 4 - half (16bit float)
- 5 - float (32bit float)
- 6 - double (64bit float) !! most file formats have not support double precision

`DefaultBit = 4`
`BitDepth = -1`

### Quality
- 100 - lossless or best quality
- 0 - worst quality

`Quality = 95`


## CameraRaw

### Raw rotation:
- -1 - Auto EXIF
- 0 - Unrotated/Horisontal
- 3 - 180 Horisontal
- 5 - 90 CW Vertical
- 6 - 90 CCW Vertical
  
`RawRotation = -1`

### Raw Color Space
(TODO: Check if this is an actual color conversion in LibRAW or just a tagging)
- 0 - raw
- 1 - sRGB
- 2 - sRGB-linear (sRGB primaries, but a linear transfer function)
- 3 - Adobe
- 4 - Wide
- 5 - ProPhoto
- 6 - ProPhoto-linear
- 7 - XYZ
- 8 - ACES (only supported by LibRaw >= 0.18)
- 9 - DCI-P3 (LibRaw >= 0.21)
- 10 - Rec2020 (LibRaw >= 0.2)

`RawColorSpace = 1`

### Demosaic
- -2 - raw data (Single channel)
- -1 - no demosaic (RGB)
- 0 - linear 
- 1 - VNG
- 2 - PPG
- 3 - AHD (default)
- 4 - DCB
- 5 ~ 10 is not used
- 11 - DHT
- 12 - AAHD (Modified AHD)

`Demosaic = 3`

### Import Camera RAW in half resolution

`half_size = false`

### use_auto_wb
Use automatic white balance obtained after averaging over the entire image.

`use_auto_wb = false`

### use_camera_wb
If possible, use the white balance from the camera.

`use_camera_wb = true`

### use_camera_matrix
- 0: do not use embedded color profile
- 1 (default): use embedded color profile (if present) for DNG files (always); for other files only if use_camera_wb is set;
-# 3: use embedded color data (if present) regardless of white balance setting.

`use_camera_matrix = 1`

### highlight 
0-9: Highlight mode (0=clip, 1=unclip, 2=blend, 3+=rebuild).

`highlights = 3`

### aberrations
Correction of chromatic aberrations; (red multiplier, blue multiplier)

`aberrations = 1.0, 1.0`

### Denoise before debayering. 
Do not recommended to use both wavelet denoising and FBDD noise reduction!
- 0 - disabled
- 1 - wavelength
- 2 - fbdd
- 3 - both
  
`denoise_mode = 1`

### threshold
Use wavelets to erase noise while preserving real detail. The best threshold should be somewhere between 100 and 1000.

`dnz_threshold = 100.0`

### fbdd_noiserd
Controls FBDD noise reduction before demosaic.
- 0 - do not use FBDD noise reduction
- 1 - light FBDD reduction
- 2 (and more) - full FBDD reduction
  
`fbdd_noiserd = 2`

### Exif crop
At this moment both auto and force work the same. TODO: merge them into the one
- -1 - disabled
- 0 - auto
- 1 - force
 
`exif_crop = 0`

## OCIO settings
OCIO config file:
If empty, OpenColorIO library will use $OCIO environment variable
### If $OCIO is not set, app will give an error

`OCIO_Config = "aces_1.2/config.ocio"`

## Transform image (color transform)
Luts folder, absolute or relative to a program folder. **UnRAWer** automatically load list of LUTs in this folder in load time.
*Use a console to check if LUTs correctly recognised.*
**No LUT check in load time!!**
(TODO: check if non default handle correctly)

`LutFolder = "LUTs"`

### LUT transform mode
LUT mode.
If set to **Smart** check if path or image file name have included lut preset name.
For example, several folders with raw files: diffuse, cross, parallel and you have dedicated LUT presets for such images.
**UnRAWer** should automatically recognise and use dedicated LUT preset.
- -1 - disabled
- 0 - Smart (file path/name)
- 1 - Force
  
`LutTransform = 1`

Force/Default preset

`LutDefault = "hdr"`

Per camera model presets

`exif_lut = false`


## Unsharp
### Unsharp mask
Unsharp mode:
- -1 - disabled
- 0 - smart
- 1 - Force

`sharp_mode = 1`

## Unsharp kernel
- 0 - gaussian (default)
- 1 - sharp-gaussian
- 2 - box
- 3- triangle
- 4 - blackman-harris
- 5 - mitchell
- 6 - b-spline
- 7 - catmull-rom
- 8 - lanczos3
- 9 - disk
- 10 - binomial
- 11 - laplacian
- 12 - median

`sharp_kernel = 0`

sharp window size
`sharp_width = 3.0`
sharp strenght
`sharp_contrast = 0.5`
threshold
`sharp_treshold = 0.125`

*source + contrast * (source - blur)*
*if (source - blur) < threshold => result == source (no sharp)*

## Required dependencies
* OpenImageIO
* LibRAW
* OpenImageIO
* libTIFF
* OpenJpeg (jpeg2000)
* libJpeg-turbo (jpeg)
* libjxl (jpeg xl)
* zlib-ng
* QT6
* etc.

![UnRAWer3](https://github.com/ssh4net/UnRAWer/assets/3924000/3e5b2cd8-349b-47da-8ee0-7959c22bfc70)


Support
-------
Please consider supporting this project via https://www.patreon.com/MadProcessor

License
-------

Copyright © 2021-2024 Erium Vladlen.

UnRAWer is licensed under the GNU General Public License, Version 3.
Individual files may have a different but compatible license.
