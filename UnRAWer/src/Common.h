/*
 * UnRAWer - camera raw batch processor
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

#if 0
/* Is Windows OS? */
#define OS_WIN \
	defined _WINDOWS || defined WIN32 || defined _WIN32 || defined __WIN32__ || defined _WIN64

 // Is Mac OS?
#define OS_MAC \
	defined __APPLE__

 // Is Linux OS?
#define OS_LINUX \
	defined __linux__

// Is OS supported?
#define OS_SUPPORTED \
	OS_WIN /* || OS_MAC || OS_LINUX */

// Misc macro utility/
#define MINLINE static __forceinline

// Stringify code
#define STRINGIFY0(v) #v
#define STRINGIFY(v) STRINGIFY0(v)
#endif

//extern const char* APP_NAME;
//extern const char* APP_AUTHOR;
//extern const int APP_VERSION[];
