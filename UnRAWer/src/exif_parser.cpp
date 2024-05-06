#include "exif_parser.h"
#include "Log.h"

#include <iostream>
#include <memory>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/imageio.h>

OIIO::ImageSpec* m_spec;
std::string m_make;

template<typename T> static bool allval(OIIO::cspan<T> d, T v = T(0))
{
	return std::all_of(d.begin(), d.end(), [&](const T& a) { return a == v; });
}

static std::string prefixedname(OIIO::string_view prefix, std::string& name)
{
	return prefix.size() ? (std::string(prefix) + ':' + name) : name;
}

void add(OIIO::string_view prefix, std::string name, int data, bool force = true,
    int ignval = 0)
{
    if (force || data != ignval)
        m_spec->attribute(prefixedname(prefix, name), data);
}
void add(OIIO::string_view prefix, std::string name, float data,
    bool force = true, float ignval = 0)
{
    if (force || data != ignval)
        m_spec->attribute(prefixedname(prefix, name), data);
}
void add(OIIO::string_view prefix, std::string name, OIIO::string_view data,
    bool force = true, int /*ignval*/ = 0)
{
    if (force || (data.size() && data[0]))
        m_spec->attribute(prefixedname(prefix, name), data);
}
void add(OIIO::string_view prefix, std::string name, unsigned long long data,
    bool force = true, unsigned long long ignval = 0)
{
    if (force || data != ignval)
        m_spec->attribute(prefixedname(prefix, name), OIIO::TypeDesc::UINT64,
            &data);
}
void add(OIIO::string_view prefix, std::string name, unsigned int data,
    bool force = true, int ignval = 0)
{
    add(prefix, name, (int)data, force, ignval);
}
void add(OIIO::string_view prefix, std::string name, unsigned short data,
    bool force = true, int ignval = 0)
{
    add(prefix, name, (int)data, force, ignval);
}
void add(OIIO::string_view prefix, std::string name, unsigned char data,
    bool force = true, int ignval = 0)
{
    add(prefix, name, (int)data, force, ignval);
}
void add(OIIO::string_view prefix, std::string name, double data,
    bool force = true, float ignval = 0)
{
    add(prefix, name, float(data), force, ignval);
}

void add(OIIO::string_view prefix, std::string name, OIIO::cspan<int> data,
    bool force = true, int ignval = 0)
{
    if (force || !allval(data, ignval)) {
        int size = data.size() > 1 ? data.size() : 0;
        m_spec->attribute(prefixedname(prefix, name),
            OIIO::TypeDesc(OIIO::TypeDesc::INT, size), data.data());
    }
}
void add(OIIO::string_view prefix, std::string name, OIIO::cspan<short> data,
    bool force = true, short ignval = 0)
{
    if (force || !allval(data, ignval)) {
        int size = data.size() > 1 ? data.size() : 0;
        m_spec->attribute(prefixedname(prefix, name),
            OIIO::TypeDesc(OIIO::TypeDesc::INT16, size), data.data());
    }
}
void add(OIIO::string_view prefix, std::string name, OIIO::cspan<unsigned short> data,
    bool force = true, unsigned short ignval = 0)
{
    if (force || !allval(data, ignval)) {
        int size = data.size() > 1 ? data.size() : 0;
        m_spec->attribute(prefixedname(prefix, name),
            OIIO::TypeDesc(OIIO::TypeDesc::UINT16, size), data.data());
    }
}
void add(OIIO::string_view prefix, std::string name, OIIO::cspan<unsigned char> data,
    bool force = true, unsigned char ignval = 0)
{
    if (force || !allval(data, ignval)) {
        int size = data.size() > 1 ? data.size() : 0;
        m_spec->attribute(prefixedname(prefix, name),
            OIIO::TypeDesc(OIIO::TypeDesc::UINT8, size), data.data());
    }
}
void add(OIIO::string_view prefix, std::string name, OIIO::cspan<float> data,
    bool force = true, float ignval = 0)
{
    if (force || !allval(data, ignval)) {
        int size = data.size() > 1 ? data.size() : 0;
        m_spec->attribute(prefixedname(prefix, name),
            OIIO::TypeDesc(OIIO::TypeDesc::FLOAT, size), data.data());
    }
}
void add(OIIO::string_view prefix, std::string name, OIIO::cspan<double> data,
    bool force = true, float ignval = 0)
{
    float* d = OIIO_ALLOCA(float, data.size());
    for (auto i = 0; i < data.size(); ++i)
        d[i] = data[i];
    add(prefix, name, OIIO::cspan<float>(d, data.size()), force, ignval);
}

bool get_soft(std::unique_ptr<LibRaw>& m_processor, OIIO::ImageSpec& m_spec) {
	LOG(trace) << "EXIF::get_soft()" << std::endl;

    const libraw_image_sizes_t& sizes(m_processor->imgdata.sizes);
    m_spec.attribute("PixelAspectRatio", (float)sizes.pixel_aspect);

    const libraw_iparams_t& idata(m_processor->imgdata.idata);
    const libraw_colordata_t& color(m_processor->imgdata.color);

    if (idata.make[0]) {
        m_make = std::string(idata.make);
        m_spec.attribute("Make", idata.make);
    }
    else {
        m_make.clear();
    }
    if (idata.model[0])
        m_spec.attribute("Model", idata.model);
    if (idata.software[0])
        m_spec.attribute("Software", idata.software);
    else if (color.model2[0])
        m_spec.attribute("Software", color.model2);

    // FIXME: idata. dng_version, is_foveon, colors, filters, cdesc

    m_spec.attribute("Exif:Flash", (int)color.flash_used);

    // FIXME -- all sorts of things in this struct

    const libraw_imgother_t& other(m_processor->imgdata.other);
    m_spec.attribute("Exif:ISOSpeedRatings", (int)other.iso_speed);
    m_spec.attribute("ExposureTime", other.shutter);
    m_spec.attribute("Exif:ShutterSpeedValue", -std::log2(other.shutter));
    m_spec.attribute("FNumber", other.aperture);
    m_spec.attribute("Exif:ApertureValue", 2.0f * std::log2(other.aperture));
    m_spec.attribute("Exif:FocalLength", other.focal_len);
    struct tm m_tm;
    OIIO::Sysutil::get_local_time(&m_processor->imgdata.other.timestamp, &m_tm);
    char datetime[20];
    strftime(datetime, 20, "%Y-%m-%d %H:%M:%S", &m_tm);
    m_spec.attribute("DateTime", datetime);
    add("raw", "ShotOrder", other.shot_order, false);
    // FIXME: other.shot_order
    if (other.desc[0])
        m_spec.attribute("ImageDescription", other.desc);
    if (other.artist[0])
        m_spec.attribute("Artist", other.artist);
    if (other.parsed_gps.gpsparsed) {
        add("GPS", "Latitude", other.parsed_gps.latitude, false, 0.0f);
        add("GPS", "Longitude", other.parsed_gps.longitude, false, 0.0f);
        add("GPS", "TimeStamp", other.parsed_gps.gpstimestamp, false, 0.0f);
        add("GPS", "Altitude", other.parsed_gps.altitude, false, 0.0f);
        add("GPS", "LatitudeRef", OIIO::string_view(&other.parsed_gps.latref, 1),
            false);
        add("GPS", "LongitudeRef", OIIO::string_view(&other.parsed_gps.longref, 1),
            false);
        add("GPS", "AltitudeRef", OIIO::string_view(&other.parsed_gps.altref, 1),
            false);
        add("GPS", "Status", OIIO::string_view(&other.parsed_gps.gpsstatus, 1),
            false);
    }
    const libraw_makernotes_t& makernotes(m_processor->imgdata.makernotes);
    const libraw_metadata_common_t& common(makernotes.common);
    // float FlashEC;
    // float FlashGN;
    // float CameraTemperature;
    // float SensorTemperature;
    // float SensorTemperature2;
    // float LensTemperature;
    // float AmbientTemperature;
    // float BatteryTemperature;
    // float exifAmbientTemperature;
    add("Exif", "Humidity", common.exifHumidity, false, 0.0f);
    add("Exif", "Pressure", common.exifPressure, false, 0.0f);
    add("Exif", "WaterDepth", common.exifWaterDepth, false, 0.0f);
    add("Exif", "Acceleration", common.exifAcceleration, false, 0.0f);
    add("Exif", "CameraElevationAngle", common.exifCameraElevationAngle, false,
        0.0f);
    // float real_ISO;

	return true;
}

bool EXIF::get_exif(std::unique_ptr<LibRaw>& m_processor, OIIO::ImageSpec& inp_spec) {

    LOG(trace) << "EXIF::get_exif()" << std::endl;
	m_spec = &inp_spec;

	if (!get_soft(m_processor, *m_spec))
		return false;

    EXIF::get_lensinfo(m_processor, m_make);
    EXIF::get_shootinginfo(m_processor, m_make);
    EXIF::get_colorinfo(m_processor);
    EXIF::get_makernotes(m_processor, m_make);

#if _DEBUG
	std::cout << "debug EXIF data:" << std::endl;
	// print all extra attributes from m_spec
	for (auto&& attr : m_spec->extra_attribs) {
		std::cout << attr.name().string() << " : " << attr.get_string() << std::endl;
	}
#endif

    return true;
};

void EXIF::get_makernotes(std::unique_ptr<LibRaw>& m_processor, std::string& m_make) {
    LOG(trace) << "EXIF::get_makernotes()" << std::endl;

	if (OIIO::Strutil::istarts_with(m_make, "Canon"))
		get_makernotes_canon(m_processor, m_make);
	else if (OIIO::Strutil::istarts_with(m_make, "Nikon"))
		get_makernotes_nikon(m_processor, m_make);
	else if (OIIO::Strutil::istarts_with(m_make, "Olympus"))
		get_makernotes_olympus(m_processor, m_make);
	else if (OIIO::Strutil::istarts_with(m_make, "Fuji"))
		get_makernotes_fuji(m_processor, m_make);
	else if (OIIO::Strutil::istarts_with(m_make, "Kodak"))
		get_makernotes_kodak(m_processor, m_make);
	else if (OIIO::Strutil::istarts_with(m_make, "Panasonic"))
		get_makernotes_panasonic(m_processor, m_make);
	else if (OIIO::Strutil::istarts_with(m_make, "Pentax"))
		get_makernotes_pentax(m_processor, m_make);
	else if (OIIO::Strutil::istarts_with(m_make, "Sony"))
		get_makernotes_sony(m_processor, m_make);
}

// Helper macro: add metadata with the same name as mn.name, but don't
// force it if it's the `ignore` value.
#define MAKER(name, ignore) add(m_make, #name, mn.name, false, ignore)

// Helper: add metadata, no matter what the value.
#define MAKERF(name) add(m_make, #name, mn.name, true)

void EXIF::get_makernotes_canon(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_canon()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.canon);
    // MAKER (CanonColorDataVer, 0);
    // MAKER (CanonColorDataSubVer, 0);
    MAKERF(SpecularWhiteLevel);
    MAKERF(ChannelBlackLevel);
    MAKERF(AverageBlackLevel);
    MAKERF(MeteringMode);
    MAKERF(SpotMeteringMode);
    MAKERF(FlashMeteringMode);
    MAKERF(FlashExposureLock);
    MAKERF(ExposureMode);
    MAKERF(AESetting);
    MAKERF(ImageStabilization);
    MAKERF(FlashMode);
    MAKERF(FlashActivity);
    MAKER(FlashBits, 0);
    MAKER(ManualFlashOutput, 0);
    MAKER(FlashOutput, 0);
    MAKER(FlashGuideNumber, 0);
    MAKERF(ContinuousDrive);
    MAKER(SensorWidth, 0);
    MAKER(SensorHeight, 0);
    add(m_make, "SensorLeftBorder", mn.DefaultCropAbsolute.l, false, 0);
    add(m_make, "SensorTopBorder", mn.DefaultCropAbsolute.t, false, 0);
    add(m_make, "SensorRightBorder", mn.DefaultCropAbsolute.r, false, 0);
    add(m_make, "SensorBottomBorder", mn.DefaultCropAbsolute.b, false, 0);
    add(m_make, "BlackMaskLeftBorder", mn.LeftOpticalBlack.l, false, 0);
    add(m_make, "BlackMaskTopBorder", mn.LeftOpticalBlack.t, false, 0);
    add(m_make, "BlackMaskRightBorder", mn.LeftOpticalBlack.r, false, 0);
    add(m_make, "BlackMaskBottomBorder", mn.LeftOpticalBlack.b, false, 0);
    // Extra added with libraw 0.19:
    // unsigned int mn.multishot[4]
    MAKER(AFMicroAdjMode, 0);
    MAKER(AFMicroAdjValue, 0.0f);
}

void EXIF::get_makernotes_nikon(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_nikon()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.nikon);
    MAKER(ExposureBracketValue, 0.0f);
    MAKERF(ActiveDLighting);
    MAKERF(ShootingMode);
    MAKERF(ImageStabilization);
    MAKER(VibrationReduction, 0);
    MAKERF(VRMode);
    MAKER(FlashSetting, 0);
    MAKER(FlashType, 0);
    MAKERF(FlashExposureCompensation);
    MAKERF(ExternalFlashExposureComp);
    MAKERF(FlashExposureBracketValue);
    MAKERF(FlashMode);
    // signed char FlashExposureCompensation2;
    // signed char FlashExposureCompensation3;
    // signed char FlashExposureCompensation4;
    MAKERF(FlashSource);
    MAKERF(FlashFirmware);
    MAKERF(ExternalFlashFlags);
    MAKERF(FlashControlCommanderMode);
    MAKER(FlashOutputAndCompensation, 0);
    MAKER(FlashFocalLength, 0);
    MAKER(FlashGNDistance, 0);
    MAKERF(FlashGroupControlMode);
    MAKERF(FlashGroupOutputAndCompensation);
    MAKER(FlashColorFilter, 0);

    MAKER(NEFCompression, 0);
    MAKER(ExposureMode, -1);
    MAKER(nMEshots, 0);
    MAKER(MEgainOn, 0);
    MAKERF(ME_WB);
    MAKERF(AFFineTune);
    MAKERF(AFFineTuneIndex);
    MAKERF(AFFineTuneAdj);
}

void EXIF::get_makernotes_olympus(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_olympus()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.olympus);
    MAKERF(SensorCalibration);
    MAKERF(FocusMode);
    MAKERF(AutoFocus);
    MAKERF(AFPoint);
    // unsigned     AFAreas[64];
    MAKERF(AFPointSelected);
    MAKERF(AFResult);
    // MAKERF(ImageStabilization);  Removed after 0.19
    MAKERF(ColorSpace);
    MAKERF(AFFineTune);
    if (mn.AFFineTune)
        MAKERF(AFFineTuneAdj);
}

void EXIF::get_makernotes_panasonic(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_panasonic()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.panasonic);
    MAKERF(Compression);
    MAKER(BlackLevelDim, 0);
    MAKERF(BlackLevel);
}

void EXIF::get_makernotes_pentax(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_pentax()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.pentax);
    MAKERF(FocusMode);
    MAKERF(AFPointsInFocus);
    MAKERF(DriveMode);
    MAKERF(AFPointSelected);
    MAKERF(FocusPosition);
    MAKERF(AFAdjustment);
}

void EXIF::get_makernotes_kodak(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_kodak()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.kodak);
    MAKERF(BlackLevelTop);
    MAKERF(BlackLevelBottom);
    MAKERF(offset_left);
    MAKERF(offset_top);
    MAKERF(clipBlack);
    MAKERF(clipWhite);
    // float romm_camDaylight[3][3];
    // float romm_camTungsten[3][3];
    // float romm_camFluorescent[3][3];
    // float romm_camFlash[3][3];
    // float romm_camCustom[3][3];
    // float romm_camAuto[3][3];
}

void EXIF::get_makernotes_fuji(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_fuji()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.fuji);

    add(m_make, "ExpoMidPointShift", mn.ExpoMidPointShift);
    add(m_make, "DynamicRange", mn.DynamicRange);
    add(m_make, "FilmMode", mn.FilmMode);
    add(m_make, "DynamicRangeSetting", mn.DynamicRangeSetting);
    add(m_make, "DevelopmentDynamicRange", mn.DevelopmentDynamicRange);
    add(m_make, "AutoDynamicRange", mn.AutoDynamicRange);

    MAKERF(FocusMode);
    MAKERF(AFMode);
    MAKERF(FocusPixel);
    MAKERF(ImageStabilization);
    MAKERF(FlashMode);
    MAKERF(WB_Preset);
    MAKERF(ShutterType);
    MAKERF(ExrMode);
    MAKERF(Macro);
    MAKERF(Rating);
}

void EXIF::get_makernotes_sony(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_makernotes_sony()" << std::endl;

    auto const& mn(m_processor->imgdata.makernotes.sony);

    MAKERF(CameraType);

    // uchar Sony0x9400_version; / 0 if not found/deciphered, 0xa, 0xb, 0xc following exiftool convention /
    // uchar Sony0x9400_ReleaseMode2;
    // unsigned Sony0x9400_SequenceImageNumber;
    // uchar Sony0x9400_SequenceLength1;
    // unsigned Sony0x9400_SequenceFileNumber;
    // uchar Sony0x9400_SequenceLength2;
    MAKERF(AFMicroAdjValue);
    MAKERF(AFMicroAdjOn);
    MAKER(AFMicroAdjRegisteredLenses, 0);
    MAKERF(group2010);
    if (mn.real_iso_offset != 0xffff)
        MAKERF(real_iso_offset);
    MAKERF(firmware);
    MAKERF(ImageCount3_offset);
    MAKER(ImageCount3, 0);
    if (mn.ElectronicFrontCurtainShutter == 0
        || mn.ElectronicFrontCurtainShutter == 1)
        MAKERF(ElectronicFrontCurtainShutter);
    MAKER(MeteringMode2, 0);
    add(m_make, "DateTime", mn.SonyDateTime);
    // MAKERF(TimeStamp);  Removed after 0.19, is in 'other'
    MAKER(ShotNumberSincePowerUp, 0);
}
 
void EXIF::get_lensinfo(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_lensinfo()" << std::endl;

    {
        auto const& mn(m_processor->imgdata.lens);
        MAKER(MinFocal, 0.0f);
        MAKER(MaxFocal, 0.0f);
        MAKER(MaxAp4MinFocal, 0.0f);
        MAKER(MaxAp4MaxFocal, 0.0f);
        MAKER(EXIF_MaxAp, 0.0f);
        MAKER(LensMake, 0);
        MAKER(Lens, 0);
        MAKER(LensSerial, 0);
        MAKER(InternalLensSerial, 0);
        MAKER(FocalLengthIn35mmFormat, 0);
    }
    {
        auto const& mn(m_processor->imgdata.lens.makernotes);
        MAKER(LensID, 0ULL);
        MAKER(Lens, 0);
        MAKER(LensFormat, 0);
        MAKER(LensMount, 0);
        MAKER(CamID, 0ULL);
        MAKER(CameraFormat, 0);
        MAKER(CameraMount, 0);
        MAKER(body, 0);
        MAKER(FocalType, 0);
        MAKER(LensFeatures_pre, 0);
        MAKER(LensFeatures_suf, 0);
        MAKER(MinFocal, 0.0f);
        MAKER(MaxFocal, 0.0f);
        MAKER(MaxAp4MinFocal, 0.0f);
        MAKER(MaxAp4MaxFocal, 0.0f);
        MAKER(MinAp4MinFocal, 0.0f);
        MAKER(MinAp4MaxFocal, 0.0f);
        MAKER(MaxAp, 0.0f);
        MAKER(MinAp, 0.0f);
        MAKER(CurFocal, 0.0f);
        MAKER(CurAp, 0.0f);
        MAKER(MaxAp4CurFocal, 0.0f);
        MAKER(MinAp4CurFocal, 0.0f);
        MAKER(MinFocusDistance, 0.0f);
        MAKER(FocusRangeIndex, 0.0f);
        MAKER(LensFStops, 0.0f);
        MAKER(TeleconverterID, 0ULL);
        MAKER(Teleconverter, 0);
        MAKER(AdapterID, 0ULL);
        MAKER(Adapter, 0);
        MAKER(AttachmentID, 0ULL);
        MAKER(Attachment, 0);
        MAKER(FocalUnits, 0);
        MAKER(FocalLengthIn35mmFormat, 0.0f);
    }

    if (OIIO::Strutil::iequals(m_make, "Nikon")) {
        auto const& mn(m_processor->imgdata.lens.nikon);
        add(m_make, "EffectiveMaxAp", mn.EffectiveMaxAp);
        add(m_make, "LensIDNumber", mn.LensIDNumber);
        add(m_make, "LensFStops", mn.LensFStops);
        add(m_make, "MCUVersion", mn.MCUVersion);
        add(m_make, "LensType", mn.LensType);
    }
    if (OIIO::Strutil::iequals(m_make, "DNG")) {
        auto const& mn(m_processor->imgdata.lens.dng);
        MAKER(MaxAp4MaxFocal, 0.0f);
        MAKER(MaxAp4MinFocal, 0.0f);
        MAKER(MaxFocal, 0.0f);
        MAKER(MinFocal, 0.0f);
    }
}

void EXIF::get_shootinginfo(std::unique_ptr<LibRaw>& m_processor, std::string& m_make)
{
	LOG(trace) << "EXIF::get_shootinginfo()" << std::endl;

    auto const& mn(m_processor->imgdata.shootinginfo);
    MAKER(DriveMode, -1);
    MAKER(FocusMode, -1);
    MAKER(MeteringMode, -1);
    MAKERF(AFPoint);
    MAKER(ExposureMode, -1);
    MAKERF(ImageStabilization);
    MAKER(BodySerial, 0);
    MAKER(InternalBodySerial, 0);
}

void EXIF::get_colorinfo(std::unique_ptr<LibRaw>& m_processor)
{
	LOG(trace) << "EXIF::get_colorinfo()" << std::endl;

    add("raw", "pre_mul",
        OIIO::cspan<float>(&(m_processor->imgdata.color.pre_mul[0]),
            &(m_processor->imgdata.color.pre_mul[4])),
        false, 0.f);
    add("raw", "cam_mul",
        OIIO::cspan<float>(&(m_processor->imgdata.color.cam_mul[0]),
            &(m_processor->imgdata.color.cam_mul[4])),
        false, 0.f);
    add("raw", "rgb_cam",
        OIIO::cspan<float>(&(m_processor->imgdata.color.rgb_cam[0][0]),
            &(m_processor->imgdata.color.rgb_cam[2][4])),
        false, 0.f);
    add("raw", "cam_xyz",
        OIIO::cspan<float>(&(m_processor->imgdata.color.cam_xyz[0][0]),
            &(m_processor->imgdata.color.cam_xyz[3][3])),
        false, 0.f);
}
