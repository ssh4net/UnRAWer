/*
 * UnRAWer - camera raw batch processor on top of OpenImageIO
 * Copyright (c) 2024 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
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