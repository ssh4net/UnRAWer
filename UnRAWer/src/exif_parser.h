#pragma once

#include <memory>
#include <string>
#include <OpenImageIO/imageio.h>
#include <libraw/libraw.h>

namespace EXIF
{
	bool get_exif(std::shared_ptr<LibRaw>& m_processor, OIIO::ImageSpec& inp_spec);
	void get_makernotes(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_canon(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_nikon(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_olympus(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_panasonic(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_pentax(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_kodak(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_fuji(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_makernotes_sony(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);

	void get_lensinfo(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);
	void get_shootinginfo(std::shared_ptr<LibRaw>& m_processor, std::string& m_make);

	void get_colorinfo(std::shared_ptr<LibRaw>& m_processor);
}