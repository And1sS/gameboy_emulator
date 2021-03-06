#pragma once

#include <cmath>
#include <array>

#include "Generator.h"

#include "..\util.h"

class ToneGenerator: public Generator
{
private:
	uint8_t duty = 0;
	double  sound_length = 1.0 / 256;

	uint8_t envelope_level = 0;
	uint8_t envelope_direction = 0;
	double  envelope_step = 0;

	uint8_t current_volume = 15;

	uint16_t frequency_specifier = 1792; // x; frequency = 131072/(2048-x) Hz
	double   period = 1.0 * (2048 - 1792) / 131072;

	bool infinite_sound = true;
	bool turned_on = false;

	static const std::array<uint8_t, 4> sound_data;

public:
	void process_sound_IO_write(uint16_t addr, uint8_t value) override;

	int16_t get_sample(double elapsed_time) override;
};