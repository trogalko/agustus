#ifndef BUILDING_COUNT_H
#define BUILDING_COUNT_H

#include "core/buffer.h"
#include "building/type.h"
#include "game/resource.h"

/**
 * @file
 * Building totals
 */

/**
 * Updates the building counts and does some extra work on the side
 */
void building_count_update(void);

/**
 * Returns the active building count for the type
 * @param type Building type
 * @return Number of active buildings
 */
int building_count_active(building_type type);

/**
 * Returns the building count for the type
 * @param type Building type
 * @return Total number of buildings
 */
int building_count_total(building_type type);

/**
 * Returns the upgraded building count for the type
 * @param type Building type
 * @return Number of upgraded buildings
 */

int building_count_upgraded(building_type type);


/**
 * Returns the active building count for the resource type
 * @param resource Resource type
 * @return Number of active buildings
 */
int building_count_industry_active(resource_type resource);

int building_count_colosseum(void);

int building_count_grand_temples(void);

int building_count_grand_temples_active(void);

/**
 * Returns the building count for the resource type
 * @param resource Resource type
 * @return Total number of buildings
 */
int building_count_industry_total(resource_type resource);

/**
 * Save the building counts
 * @param industry Buffer for industry
 * @param culture1 Culture part 1
 * @param culture2 Culture part 2 (schools)
 * @param culture3 Culture part 3 (temples)
 * @param military Military
 * @param support Market and water
 */
void building_count_save_state(
    buffer *industry, buffer *culture1, buffer *culture2, buffer *culture3, buffer *military, buffer *support);

/**
 * Load the building counts
 * @param industry Buffer for industry
 * @param culture1 Culture part 1
 * @param culture2 Culture part 2 (schools)
 * @param culture3 Culture part 3 (temples)
 * @param military Military
 * @param support Market and water
 */
void building_count_load_state(
    buffer *industry, buffer *culture1, buffer *culture2, buffer *culture3, buffer *military, buffer *support, int includes_buffer_size);

#endif // BUILDING_COUNT_H
