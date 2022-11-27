#include "colourfmt.h"
#include <exception> // std::exception
#include <cstdlib> // std::abort
#include <cstdio> // std::snprintf
#include <algorithm> // std::min, std::max

struct HsvColour {
	int h, s, v, a;
};

struct HslColour {
	int h, s, l, a;
};

struct CmykColour {
	int c, m, y, k, a;
};

struct FormatError : std::exception { };

template<typename... Args>
static std::string Format(const char* format, Args... args) {
	int size = std::snprintf(nullptr, 0, format, args...);
	if (size <= 0)
		throw FormatError();
	std::string s;
	s.resize((size_t)size);
	std::snprintf(s.data(), (size_t)size + 1, format, args...);
	return s;
}

static std::string ToString2dp(float f) {
	return Format("%.2ff", f);
}

static HsvColour RgbToHsv(const SDL_Colour& colour) {
	float r = colour.r / 255.0f;
	float g = colour.g / 255.0f;
	float b = colour.b / 255.0f;
	int a = int(colour.a / 255.0f * 100 + 0.5f);
	
	float cmin = std::min(r, std::min(g, b));
	float cmax = std::max(r, std::max(g, b));
	float delta = cmax - cmin;
	if (delta == 0) {
		return { 0, 0, int(100 * cmax), a };
	}
	
	float h = 0;
	if (cmax == r) {
		h = 60 * ((g - b) / delta);
	} else if (cmax == g) {
		h = 60 * (((b - r) / delta) + 2);
	} else if (cmax == b) {
		h = 60 * (((r - g) / delta) + 4);
	}

	float s = cmax ? (delta / cmax) : 0;

	return { int(h + 0.5f), int(100 * s + 0.5f), int(100 * cmax + 0.5f), a };
}

static HslColour RgbToHsl(const SDL_Colour& colour) {
	float r = colour.r / 255.0f;
	float g = colour.g / 255.0f;
	float b = colour.b / 255.0f;
	int a = int(colour.a / 255.0f * 100 + 0.5f);

	float cmin = std::min(r, std::min(g, b));
	float cmax = std::max(r, std::max(g, b));
	float delta = cmax - cmin;
	
	float el = 50 * (cmin + cmax);

	float s = 0;
	if (cmin == cmax) {
		return { 0, 0, int(el), a };
	} else if (el < 50) {
		s = 100 * delta / (cmax + cmin);
	} else {
		s = 100 * delta / (2 - cmax - cmin);
	}

	float h = 0;
	if (cmax == r) {
		h = 60 * (g - b) / delta;
	} else if (cmax == g) {
		h = 60 * (b - r) / delta + 120;
	} else if (cmax == b) {
		h = 60 * (r - g) / delta + 240;
	}
	
	if (h < 0) {
		h += 360;
	}

	return { int(h + 0.5f), int(s + 0.5f), int(el + 0.5f), a };
}

static CmykColour RgbToCmyk(const SDL_Colour& colour) {
	float r = colour.r / 255.0f;
	float g = colour.g / 255.0f;
	float b = colour.b / 255.0f;
	int a = int(colour.a / 255.0f * 100 + 0.5f);

	float c = 1 - r;
	float m = 1 - g;
	float y = 1 - b;
	float k = std::min(c, std::min(m, y));

	if (k == 1) {
		return { 0, 0, 0, 100, a };
	}

	c = (c - k) / (1 - k);
	m = (m - k) / (1 - k);
	y = (y - k) / (1 - k);

	return {
		int(100 * c + 0.5f),
		int(100 * m + 0.5f),
		int(100 * y + 0.5f),
		int(100 * k + 0.5f),
		a
	};
}

static const char* HEX_CHARS = "0123456789ABCDEF";

static std::string HexA(const SDL_Colour& colour) {
	return std::string()
		+ HEX_CHARS[(colour.r >> 4) & 0xF] + HEX_CHARS[colour.r & 0xF]
		+ HEX_CHARS[(colour.g >> 4) & 0xF] + HEX_CHARS[colour.g & 0xF]
		+ HEX_CHARS[(colour.b >> 4) & 0xF] + HEX_CHARS[colour.b & 0xF]
		+ HEX_CHARS[(colour.a >> 4) & 0xF] + HEX_CHARS[colour.a & 0xF];
}

static std::string Hex(const SDL_Colour& colour) {
	return std::string()
		+ HEX_CHARS[(colour.r >> 4) & 0xF] + HEX_CHARS[colour.r & 0xF]
		+ HEX_CHARS[(colour.g >> 4) & 0xF] + HEX_CHARS[colour.g & 0xF]
		+ HEX_CHARS[(colour.b >> 4) & 0xF] + HEX_CHARS[colour.b & 0xF];
}

static std::string DecA(const SDL_Colour& colour) {
	return "("
		+ std::to_string(colour.r) + ", "
		+ std::to_string(colour.g) + ", "
		+ std::to_string(colour.b) + ", "
		+ std::to_string(colour.a) + ")";
}

static std::string Dec(const SDL_Colour& colour) {
	return "("
		+ std::to_string(colour.r) + ", "
		+ std::to_string(colour.g) + ", "
		+ std::to_string(colour.b) + ")";
}

static std::string FloatA(const SDL_Colour& colour) {
	return std::string("(")
		+ ToString2dp(colour.r / 255.0f) + ", "
		+ ToString2dp(colour.g / 255.0f) + ", "
		+ ToString2dp(colour.b / 255.0f) + ", "
		+ ToString2dp(colour.a / 255.0f) + ")";
}

static std::string Float(const SDL_Colour& colour) {
	return "("
		+ ToString2dp(colour.r / 255.0f) + ", "
		+ ToString2dp(colour.g / 255.0f) + ", "
		+ ToString2dp(colour.b / 255.0f) + ")";
}

static std::string HsvA(const SDL_Colour& colour) {
	HsvColour hsv = RgbToHsv(colour);
	return "("
		+ std::to_string(hsv.h) + ", "
		+ std::to_string(hsv.s) + ", "
		+ std::to_string(hsv.v) + ", "
		+ std::to_string(hsv.a) + ")";
}

static std::string Hsv(const SDL_Colour& colour) {
	HsvColour hsv = RgbToHsv(colour);
	return "("
		+ std::to_string(hsv.h) + ", "
		+ std::to_string(hsv.s) + ", "
		+ std::to_string(hsv.v) + ")";
}

static std::string HslA(const SDL_Colour& colour) {
	HslColour hsl = RgbToHsl(colour);
	return "("
		+ std::to_string(hsl.h) + ", "
		+ std::to_string(hsl.s) + ", "
		+ std::to_string(hsl.l) + ", "
		+ std::to_string(hsl.a) + ")";
}

static std::string Hsl(const SDL_Colour& colour) {
	HslColour hsl = RgbToHsl(colour);
	return "("
		+ std::to_string(hsl.h) + ", "
		+ std::to_string(hsl.s) + ", "
		+ std::to_string(hsl.l) + ")";
}

static std::string CmykA(const SDL_Colour& colour) {
	CmykColour cmyk = RgbToCmyk(colour);
	return "("
		+ std::to_string(cmyk.c) + ", "
		+ std::to_string(cmyk.m) + ", "
		+ std::to_string(cmyk.y) + ", "
		+ std::to_string(cmyk.k) + ", "
		+ std::to_string(cmyk.a) + ")";
}

static std::string Cmyk(const SDL_Colour& colour) {
	CmykColour cmyk = RgbToCmyk(colour);
	return "("
		+ std::to_string(cmyk.c) + ", "
		+ std::to_string(cmyk.m) + ", "
		+ std::to_string(cmyk.y) + ", "
		+ std::to_string(cmyk.k) + ")";
}

struct Fmt {
	const char* label;
	std::string(*format)(const SDL_Colour&);
};

static const Fmt FORMATS[] = {
	{ "RGBA", HexA }, { "RGB", Hex },
	{ "RGBA", DecA }, { "RGB", Dec },
	{ "RGBA", FloatA }, { "RGB", Float },
	{ "HSVA", HsvA }, { "HSV", Hsv },
	{ "HSLA", HslA }, { "HSL", Hsl },
	{ "CMYKA", CmykA }, { "CMYK", Cmyk },
};

static_assert(std::size(FORMATS) % 2 == 0);

int ColourFormatter::GetFormat() const {
	return format;
}

void ColourFormatter::SetFormat(int format) {
	if (format >= 0 || format < (int)std::size(FORMATS) / 2) {
		this->format = format;
	}
}

void ColourFormatter::SwitchFormat() {
	format = (format + 1) % ((int)std::size(FORMATS) / 2);
}

const char* ColourFormatter::GetLabel() const {
	if (alphaEnabled) {
		return FORMATS[format * 2].label;
	} else {
		return FORMATS[format * 2 + 1].label;
	}
}

std::string ColourFormatter::FormatColour(const SDL_Colour& colour) const {
	try {
		if (alphaEnabled) {
			return FORMATS[format * 2].format(colour);
		} else {
			return FORMATS[format * 2 + 1].format(colour);
		}
	} catch (FormatError&) {
		return "-";
	}
}
