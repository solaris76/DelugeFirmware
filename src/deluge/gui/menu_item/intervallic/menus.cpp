#include "gui/menu_item/intervallic/menus.h"
#include "definitions.h"
#include "gui/l10n/strings.h"

namespace deluge::gui::menu_item::intervallic {

using enum l10n::String;
using enum PartialField;

#define DECL_PARTIAL(n)                                                                                                \
	PLACE_SDRAM_BSS PartialParam p##n##Level{STRING_FOR_VOLUME_LEVEL, n, Level};                                       \
	PLACE_SDRAM_BSS PartialParam p##n##Interval{STRING_FOR_TRANSPOSE, n, Interval};                                    \
	PLACE_SDRAM_BSS PartialWave p##n##Wave{STRING_FOR_WAVEFORM, n};                                                    \
	PLACE_SDRAM_BSS PartialParam p##n##Detune{STRING_FOR_UNISON_DETUNE, n, Detune};                                    \
	PLACE_SDRAM_BSS PartialParam p##n##Pan{STRING_FOR_PAN, n, Pan};                                                    \
	PLACE_SDRAM_BSS PartialParam p##n##LfoLvl{STRING_FOR_DEPTH, n, LfoDepthLevel};                                     \
	PLACE_SDRAM_BSS PartialParam p##n##LfoDet{STRING_FOR_AMOUNT, n, LfoDepthDetune};                                   \
	PLACE_SDRAM_BSS PartialParam p##n##LfoIdx{STRING_FOR_LFO_TYPE, n, LfoIndex};                                       \
	PLACE_SDRAM_BSS PartialPhiZone p##n##ZoneA{STRING_FOR_PHI_ZONE_A, n, 0};                                           \
	PLACE_SDRAM_BSS PartialPhiZone p##n##ZoneB{STRING_FOR_PHI_ZONE_B, n, 1};                                           \
	PLACE_SDRAM_BSS PartialWaveIndex p##n##WaveIdx{STRING_FOR_WAVE_INDEX, n};                                          \
	PLACE_SDRAM_BSS PartialFile p##n##File{STRING_FOR_FILE_BROWSER, n};                                                \
	PLACE_SDRAM_BSS PartialMenu p##n##Menu{{&p##n##Level, &p##n##Interval, &p##n##Wave, &p##n##Detune, &p##n##Pan,     \
	                                        &p##n##LfoLvl, &p##n##LfoDet, &p##n##LfoIdx, &p##n##ZoneA, &p##n##ZoneB,   \
	                                        &p##n##WaveIdx, &p##n##File},                                              \
	                                       n};

DECL_PARTIAL(0)
DECL_PARTIAL(1)
DECL_PARTIAL(2)
DECL_PARTIAL(3)
DECL_PARTIAL(4)
DECL_PARTIAL(5)
DECL_PARTIAL(6)
DECL_PARTIAL(7)

#undef DECL_PARTIAL

PLACE_SDRAM_BSS LatticePreset latticePresetMenu{STRING_FOR_SHAPE};
PLACE_SDRAM_BSS LatticeInversion latticeInversionMenu{STRING_FOR_MODE};
PLACE_SDRAM_BSS LatticeVoiceSpread latticeSpreadMenu{STRING_FOR_SPREAD_OCTAVE};
PLACE_SDRAM_BSS LatticePairFm latticePairFmMenu{STRING_FOR_FEEDBACK};
PLACE_SDRAM_BSS LatticePlayMode latticePlayModeMenu{STRING_FOR_PLAY};
PLACE_SDRAM_BSS LatticeLfoRate latticeLfo0Menu{STRING_FOR_LFO1_RATE, 0};
PLACE_SDRAM_BSS LatticeLfoRate latticeLfo1Menu{STRING_FOR_LFO2_RATE, 1};
PLACE_SDRAM_BSS LatticeLfoRate latticeLfo2Menu{STRING_FOR_LFO3_RATE, 2};
PLACE_SDRAM_BSS LatticeLfoRate latticeLfo3Menu{STRING_FOR_LFO4_RATE, 3};

PLACE_SDRAM_BSS LatticeMenu intervallicLatticeMenu{
    STRING_FOR_INTERVAL,
    {&latticePresetMenu, &latticeInversionMenu, &latticeSpreadMenu, &latticePairFmMenu, &latticePlayModeMenu,
     &latticeLfo0Menu, &latticeLfo1Menu, &latticeLfo2Menu, &latticeLfo3Menu},
};

PLACE_SDRAM_BSS HorizontalMenuGroup intervallicMenuGroup{
    {&p0Menu, &p1Menu, &p2Menu, &p3Menu, &p4Menu, &p5Menu, &p6Menu, &p7Menu, &intervallicLatticeMenu}};

MenuItem* partialChildForRow(uint8_t partialId, int32_t row) {
	// Row map matches Shift pad rows: 0 level, 1 int, 2 wave, 3 lfo lvl, 4 lfo phs stub→waveidx,
	// 5 detune, 6 lfo det, 7 pan
	struct Kids {
		MenuItem* level;
		MenuItem* interval;
		MenuItem* wave;
		MenuItem* detune;
		MenuItem* pan;
		MenuItem* lfoLvl;
		MenuItem* lfoDet;
		MenuItem* waveIdx;
	};
	static Kids kids[8] = {
	    {&p0Level, &p0Interval, &p0Wave, &p0Detune, &p0Pan, &p0LfoLvl, &p0LfoDet, &p0WaveIdx},
	    {&p1Level, &p1Interval, &p1Wave, &p1Detune, &p1Pan, &p1LfoLvl, &p1LfoDet, &p1WaveIdx},
	    {&p2Level, &p2Interval, &p2Wave, &p2Detune, &p2Pan, &p2LfoLvl, &p2LfoDet, &p2WaveIdx},
	    {&p3Level, &p3Interval, &p3Wave, &p3Detune, &p3Pan, &p3LfoLvl, &p3LfoDet, &p3WaveIdx},
	    {&p4Level, &p4Interval, &p4Wave, &p4Detune, &p4Pan, &p4LfoLvl, &p4LfoDet, &p4WaveIdx},
	    {&p5Level, &p5Interval, &p5Wave, &p5Detune, &p5Pan, &p5LfoLvl, &p5LfoDet, &p5WaveIdx},
	    {&p6Level, &p6Interval, &p6Wave, &p6Detune, &p6Pan, &p6LfoLvl, &p6LfoDet, &p6WaveIdx},
	    {&p7Level, &p7Interval, &p7Wave, &p7Detune, &p7Pan, &p7LfoLvl, &p7LfoDet, &p7WaveIdx},
	};
	if (partialId >= 8) {
		return kids[0].level;
	}
	auto& k = kids[partialId];
	switch (row) {
	case 1:
		return k.interval;
	case 2:
		return k.wave;
	case 3:
		return k.lfoLvl;
	case 4:
		return k.waveIdx;
	case 5:
		return k.detune;
	case 6:
		return k.lfoDet;
	case 7:
		return k.pan;
	case 0:
	default:
		return k.level;
	}
}

HorizontalMenu* partialMenu(uint8_t partialId) {
	HorizontalMenu* menus[8] = {&p0Menu, &p1Menu, &p2Menu, &p3Menu, &p4Menu, &p5Menu, &p6Menu, &p7Menu};
	return menus[partialId & 7];
}

HorizontalMenu* latticeMenu() {
	return &intervallicLatticeMenu;
}

MenuItem* latticeChildForRow(int32_t row) {
	MenuItem* kids[] = {
	    &latticePresetMenu,   &latticeInversionMenu, &latticeSpreadMenu, &latticePairFmMenu,
	    &latticePlayModeMenu, &latticeLfo0Menu,      &latticeLfo1Menu,   &latticeLfo2Menu,
	};
	return kids[std::clamp(row, int32_t{0}, int32_t{7})];
}

HorizontalMenuGroup* menuGroup() {
	return &intervallicMenuGroup;
}

} // namespace deluge::gui::menu_item::intervallic
