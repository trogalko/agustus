#include "building/building.h"

#ifndef BUILDING_HOUSE_POPULATION_H
#define BUILDING_HOUSE_POPULATION_H

/**
 * Add people to the houses in the city, use for births
 * @param num_people Number of people to add
 * @return Number of people added
 */
int house_population_add_to_city(int num_people);

/**
 * Removes people from the houses in the city, use for births and troop requests
 * @param num_people Number of people to remove
 * @return Number of people removed
 */
int house_population_remove_from_city(int num_people);

/**
 * Update room available in houses
 */
void house_population_update_room(void);

/**
 * Update migration statistics and create immigrants/emigrants
 */
void house_population_update_migration(void);

/**
 * Evict people from overcrowded houses
 */
void house_population_evict_overcrowded(void);

/**
 * Create immigrants
 * @param num_people Number of people to immigrate
 * @return Number of people actually immigrated
 */
int house_population_create_immigrants(int num_people);

/**
 * Create emigrants
 * @param num_people Number of people to emigrate
 * @return Number of people actually emigrated
 */
int house_population_create_emigrants(int num_people);

int house_population_get_capacity(building *house);

#endif // BUILDING_HOUSE_POPULATION_H
