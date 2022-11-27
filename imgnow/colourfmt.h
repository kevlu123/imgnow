#pragma once
#include <string>
#include "SDL.h"

struct ColourFormatter {
	bool alphaEnabled = true;
	int GetFormat() const;
	void SetFormat(int format);
	void SwitchFormat(); // Go to the next format
	const char* GetLabel() const;
	std::string FormatColour(const SDL_Colour& colour) const;
private:
	int format = 0;
};
