#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <assert.h>
#include <functional>
#include <locale.h>
#include <string>

#pragma warning(push)
#if defined(_MSC_VER)
// Reduce noisy warnings across the project (mostly third-party headers)
#pragma warning(disable:4100)   // unreferenced formal parameter
#pragma warning(disable:4244)   // conversion, possible loss of data
#pragma warning(disable:4267)   // conversion from 'size_t' to smaller type
#pragma warning(disable:4996)   // deprecated CRT functions
#pragma warning(disable:26812)  // unscoped enum (static analyzer)
#pragma warning(disable:26495)  // uninitialized member (static analyzer)
#pragma warning(disable:6011)   // dereferencing NULL pointer (false positives in third-party)
#pragma warning(disable:6385)   // reading invalid data from buffer (third-party false positives)
#pragma warning(disable:28182)  // dereferencing NULL pointer (stb false positives)
#pragma warning(disable:4456)   // declaration hides previous local declaration
#pragma warning(disable:4706)   // assignment within conditional expression
#pragma warning(disable:26451)  // arithmetic overflow: use wider type
#pragma warning(disable:5054)   // deprecated enum bitwise operation
#endif

constexpr uint32_t SCREEN_WIDTH = 1280;
constexpr uint32_t SCREEN_HEIGHT = 720;
