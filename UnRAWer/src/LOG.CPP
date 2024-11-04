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

#include "stdafx.h"

#include "Common.h"
#include "log.h"

//#include <boost/log/expressions.hpp>
//#include <boost/log/utility/setup.hpp>
//#include <boost/log/utility/setup/console.hpp>

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;

std::string format_thread_id(attrs::current_thread_id::value_type::native_type id)
{
	std::ostringstream os;
	os << std::hex << std::setw(8) << std::setfill('0') << id;
	return os.str();
}

// TODO: Make into a class with enhancements maybe?
void Log_Init()
{
	boost::log::add_common_attributes();
	boost::log::add_console_log
	(
		std::cout,
		boost::log::keywords::format = "[%Severity%]<%ThreadID%> %Message%"
		//boost::log::keywords::format = "[%TimeStamp%] [%Severity%] %File%(%Line%): %Message%"
	);
}

void Log_SetVerbosity(int l)
{
	boost::log::core::get()->set_filter(
		boost::log::trivial::severity >= (boost::log::trivial::fatal - l)
		// log level is 0-5, 0 is most verbose
		// 0 = trace, 1 = debug, 2 = info, 3 = warning, 4 = error, 5 = fatal
		// fatal - 0 = only fatal errors, 
		// fatal - 1 = error,
		// fatal - 2 = warning, 
		// fatal - 3 = info (default), 
		// fatal - 4 = debug, 
		// fatal - 5 = trace (most outputs)
	);

	//LOG(debug) << "DEBUGMSG";
	//LOG(info) << "INFOMSG";
}
