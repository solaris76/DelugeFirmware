#include "gui/menu_item/value_scaling.h"
#include "definitions_cxx.hpp"

#include <cstdint>

int32_t computeCurrentValueForStandardMenuItem(int32_t value) {
	return (((int64_t)value + 2147483648) * kMaxMenuValue + 2147483648) >> 32;
}

int32_t computeCurrentValueForHalfPrecisionMenuItem(int32_t value) {
	return ((int64_t)value * (kMaxMenuValue * 2) + 2147483648) >> 32;
}

int32_t computeCurrentValueForPan(int32_t value) {
	return ((int64_t)value * (kMaxMenuRelativeValue * 2) + 2147483648) >> 32;
}

int32_t computeCurrentValueForUnsignedMenuItem(uint32_t value) {
	return ((int64_t)value * kMaxMenuValue + 2147483648) >> 32;
}

int32_t computeFinalValueForStandardMenuItem(int32_t value) {
	if (value == kMaxMenuValue) {
		return 2147483647;
	}
	else if (value == kMinMenuValue) {
		return -2147483648;
	}
	else {
		// (2147483648 / kMidMenuValue) == 85899345
		return (uint32_t)value * 85899345 - 2147483648;
	}
}

int32_t computeFinalValueForHalfPrecisionMenuItem(int32_t value) {
	// comp params and osc pulse width aren't set up for negative inputs
	if (value == kMaxMenuValue) {
		return 2147483647;
	}
	else if (value == kMinMenuValue) {
		return 0;
	}
	else {
		return (uint32_t)value * (2147483648 / kMidMenuValue) >> 1;
	}
}

int32_t computeFinalValueForPan(int32_t value) {
	if (value == kMaxMenuRelativeValue) {
		return 2147483647;
	}
	else if (value == kMinMenuRelativeValue) {
		return -2147483648;
	}
	else {
		return ((int32_t)value * (2147483648 / (kMaxMenuRelativeValue * 2)) * 2);
	}
}

uint32_t computeFinalValueForUnsignedMenuItem(int32_t value) {
	return (uint32_t)value * 85899345;
}

int32_t computeCurrentValueForTranspose(int32_t transpose, int32_t cents) {
	return transpose * 100 + cents;
}

void computeFinalValuesForTranspose(int32_t value, int32_t* transpose, int32_t* cents) {
	int32_t currentValue = value + 25600;
	int32_t semitones = (currentValue + 50) / 100;
	*cents = currentValue - semitones * 100;
	*transpose = semitones - 256;
}

int32_t computeCurrentValueForMachineDial(int32_t value) {
	int32_t v = static_cast<int32_t>((((int64_t)value + 2147483648LL) * 127 + 2147483648LL) >> 32);
	if (v < 0) {
		return 0;
	}
	if (v > 127) {
		return 127;
	}
	return v;
}

int32_t computeFinalValueForMachineDial(int32_t value) {
	if (value >= 127) {
		return 2147483647;
	}
	if (value <= 0) {
		return -2147483648;
	}
	return static_cast<int32_t>(static_cast<uint32_t>(value) * 33818641u - 2147483648u);
}

int32_t computeCurrentValueForMachineDialHybrid(int32_t finalValue) {
	int64_t recovered = (int64_t)finalValue << 1;
	if (recovered > 2147483647LL) {
		recovered = 2147483647LL;
	}
	if (recovered < -2147483648LL) {
		recovered = -2147483648LL;
	}
	return computeCurrentValueForMachineDial(static_cast<int32_t>(recovered));
}
