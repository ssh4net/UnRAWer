/*
 * UnRAWer - camera raw batch processor
 * Copyright (c) 2024 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#    include <shellapi.h>
#    include <windows.h>
#endif

inline std::filesystem::path
pathFromUtf8(const std::string& utf8Path)
{
#if defined(__cpp_char8_t)
    std::u8string text;
    text.reserve(utf8Path.size());
    for (unsigned char c : utf8Path) {
        text.push_back(static_cast<char8_t>(c));
    }
    return std::filesystem::path(text);
#else
    return std::filesystem::u8path(utf8Path);
#endif
}

inline std::string
pathToUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_char8_t)
    const std::u8string text = path.u8string();
    std::string result;
    result.reserve(text.size());
    for (char8_t c : text) {
        result.push_back(static_cast<char>(c));
    }
    return result;
#else
    return path.u8string();
#endif
}

inline std::string
pathFilenameUtf8(const std::string& utf8Path)
{
    return pathToUtf8(pathFromUtf8(utf8Path).filename());
}

#ifdef _WIN32
inline std::string
wideToUtf8(const wchar_t* text)
{
    if (text == nullptr || text[0] == L'\0') {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

inline std::vector<std::string>
commandLineArgsUtf8(int argc, char* argv[])
{
    int wide_argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (wide_argv == nullptr) {
        std::vector<std::string> fallback;
        fallback.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            fallback.emplace_back(argv[i] != nullptr ? argv[i] : "");
        }
        return fallback;
    }

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(wide_argc));
    for (int i = 0; i < wide_argc; ++i) {
        args.push_back(wideToUtf8(wide_argv[i]));
    }
    LocalFree(wide_argv);
    return args;
}
#else
inline std::vector<std::string>
commandLineArgsUtf8(int argc, char* argv[])
{
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i] != nullptr ? argv[i] : "");
    }
    return args;
}
#endif
