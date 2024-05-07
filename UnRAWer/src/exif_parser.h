#pragma once

#include <memory>
#include <string>
#include <OpenImageIO/imageio.h>
#include <libraw/libraw.h>

namespace EXIF
{
	bool get_exif(std::unique_ptr<LibRaw>& m_processor, OIIO::ImageSpec& inp_spec);

	void get_makernotes(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_canon(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_nikon(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_olympus(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_panasonic(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_pentax(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_kodak(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_fuji(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_makernotes_sony(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);

	void get_lensinfo(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);
	void get_shootinginfo(std::unique_ptr<LibRaw>& m_processor, std::string& m_make, OIIO::ImageSpec& m_spec);

	void get_colorinfo(std::unique_ptr<LibRaw>& m_processor, OIIO::ImageSpec& m_spec);
}