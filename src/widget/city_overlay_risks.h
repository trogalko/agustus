#ifndef WIGET_CITY_OVERLAY_RISKS_H
#define WIGET_CITY_OVERLAY_RISKS_H

#include "city_overlay.h"

void city_overlay_problems_prepare_building(building *b);

const city_overlay *city_overlay_for_fire(void);

const city_overlay *city_overlay_for_damage(void);

const city_overlay *city_overlay_for_crime(void);

const city_overlay *city_overlay_for_problems(void);

const city_overlay *city_overlay_for_native(void);

const city_overlay *city_overlay_for_enemy(void);

#endif // WIGET_CITY_OVERLAY_RISKS_H
