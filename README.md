# UnRAWer
 Small GUI utility to Batch process Camera RAW images with OpenColorIO 3D Lut support. A tool designed as a
 helper tool for photogrammetry (3D scanning) image batch processing tool where speed is more important than
 feature-rich raw image processors.

![UnRAWer2](https://github.com/ssh4net/UnRAWer/assets/3924000/798b24ff-bdac-451c-aebc-47256d0fff7a)

 ## Core features
 - Support all camera raws supported in Libraw library, including ProRAW DNG.
 - Multithreaded and asynchronous batch processing
 - Drag and drop interface with recursive subfolders support
 - Half Resolution camera raws import
 - Export as raw sensor data (bw), Bayers pattern (RGB), and different demosaic methods (supported in libraw)
 - Smart (per folder_suffix/filename_suffix) 3D Lut grading presets (via OpenColorIO)
 - Export as 8/16/32bit int/float tiff/jpeg/jpeg2000/jpegxl/heic/PPM/PNG
 - Export Exif and XMP metatags (see Limitations)
 - tool configuration via TOML config file

![UnRAWer](https://github.com/ssh4net/UnRAWer/assets/3924000/c8414525-ab87-4ce7-8110-f7a18161a658)

Limitations
-------
For this moment, all EXIF meta-data is not supported in Jpeg XL output files. 

User Manual
-------
Check unrw_config.toml file for different app settings.
...
TODO

### Required dependencies
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

Copyright © 2023 Erium Vladlen.

UnRAWer is licensed under the GNU General Public License, Version 3.
Individual files may have a different but compatible license.
