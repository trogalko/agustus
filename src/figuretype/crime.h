#ifndef FIGURETYPE_CRIME_H
#define FIGURETYPE_CRIME_H

#include "figure/figure.h"

void figure_generate_criminals(void);

void figure_protestor_action(figure *f);

void figure_rioter_action(figure *f);

void figure_robber_action(figure *f);

void figure_looter_action(figure *f);

int figure_rioter_collapse_building(figure *f);

#endif // FIGURETYPE_CRIME_H
