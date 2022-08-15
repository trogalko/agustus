#include "routing.h"

#include "building/building.h"
#include "core/time.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_aqueduct.h"
#include "map/routing_data.h"
#include "map/terrain.h"

#include <stdlib.h>

#define MAX_QUEUE GRID_SIZE * GRID_SIZE
#define GUARD 50000

#define UNTIL_STOP 0
#define UNTIL_CONTINUE 1

typedef enum {
    DIRECTIONS_NO_DIAGONALS = 4,
    DIRECTIONS_DIAGONALS = 8
} max_directions;

static const int ROUTE_OFFSETS[] = { -162, 1, 162, -1, -161, 163, 161, -163 };
static const int ROUTE_OFFSETS_X[] = { 0, 1, 0, -1,  1, 1, -1, -1 };
static const int ROUTE_OFFSETS_Y[] = { -1, 0, 1,  0, -1, 1,  1, -1 };

static struct {
    grid_i16 possible;
    grid_i16 determined;
    int dst_x;
    int dst_y;
} distance;

static struct {
    int total_routes_calculated;
    int enemy_routes_calculated;
} stats;

static struct {
    int head;
    int tail;
    int items[MAX_QUEUE];
} queue;

static grid_u8 water_drag;

static struct {
    grid_u8 status;
    time_millis last_check;
} fighting_data;

static struct {
    int through_building_id;
} state;

static void reset_fighting_status(void)
{
    time_millis current_time = time_get_millis();
    if (current_time != fighting_data.last_check) {
        map_grid_clear_u8(fighting_data.status.items);
        fighting_data.last_check = current_time;
    }
}

static void clear_data(void)
{
    reset_fighting_status();
    map_grid_clear_i16(distance.possible.items);
    map_grid_clear_i16(distance.determined.items);
    queue.head = 0;
    queue.tail = 0;
}

static inline void enqueue(int next_offset, int dist)
{
    distance.determined.items[next_offset] = dist;
    queue.items[queue.tail++] = next_offset;
    if (queue.tail >= MAX_QUEUE) {
        queue.tail = 0;
    }
}

static inline int queue_pop(void)
{
    int result = queue.items[queue.head];
    if (++queue.head >= MAX_QUEUE) {
        queue.head = 0;
    }
    return result;
}

static inline int ordered_queue_parent(int index)
{
    return (index - 1) / 2;
}

static inline void ordered_queue_swap(int first, int second)
{
    int temp = queue.items[first];
    queue.items[first] = queue.items[second];
    queue.items[second] = temp;
}

void ordered_queue_reorder(int start_index)
{
    int left_child = 2 * start_index + 1;
    if (left_child >= queue.tail) {
        return;
    }
    int right_child = left_child + 1;
    int smallest = start_index;
    int16_t *offset_smallest = &distance.possible.items[queue.items[smallest]];
    if (distance.possible.items[queue.items[left_child]] < *offset_smallest) {
        smallest = left_child;
        offset_smallest = &distance.possible.items[queue.items[smallest]];
    }
    if (right_child < queue.tail &&
        distance.possible.items[queue.items[right_child]] < *offset_smallest) {
        smallest = right_child;
    }
    if (smallest != start_index) {
        ordered_queue_swap(start_index, smallest);
        ordered_queue_reorder(smallest);
    }
}

static inline int ordered_queue_pop(void)
{
    int min = queue.items[0];
    queue.items[0] = queue.items[--queue.tail];
    ordered_queue_reorder(0);
    return min;
}

static inline void ordered_queue_reduce_index(int index, int offset, int dist)
{
    queue.items[index] = offset;
    while (index && distance.possible.items[queue.items[ordered_queue_parent(index)]] > dist) {
        ordered_queue_swap(index, ordered_queue_parent(index));
        index = ordered_queue_parent(index);
    }
}

static void ordered_enqueue(int next_offset, int current_dist, int remaining_dist)
{
    int possible_dist = remaining_dist + current_dist;
    int index = queue.tail;
    if (distance.possible.items[next_offset]) {
        if (distance.possible.items[next_offset] <= possible_dist) {
            return;
        } else {
            for (int i = 0; i < queue.tail; i++) {
                if (queue.items[i] == next_offset) {
                    index = i;
                    break;
                }
            }
        }
    } else {
        queue.tail++;
    }
    distance.determined.items[next_offset] = current_dist;
    distance.possible.items[next_offset] = possible_dist;

    ordered_queue_reduce_index(index, next_offset, possible_dist);
}

static inline int valid_offset(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) && distance.determined.items[grid_offset] == 0;
}

static inline int distance_left(int x, int y)
{
    return abs(distance.dst_x - x) + abs(distance.dst_y - y);
}

static void route_queue_from_to(int src_x, int src_y, int dst_x, int dst_y, int max_tiles,
    void (*callback)(int next_offset, int dist, int remaining_dist))
{
    clear_data();
    distance.dst_x = dst_x;
    distance.dst_y = dst_y;
    int dest = map_grid_offset(dst_x, dst_y);
    ordered_enqueue(map_grid_offset(src_x, src_y), 1, 0);
    int tiles = 0;
    while (queue.tail) {
        int offset = ordered_queue_pop();
        if (offset == dest || (max_tiles && ++tiles > max_tiles)) {
            break;
        }
        int x = map_grid_offset_to_x(offset);
        int y = map_grid_offset_to_y(offset);
        int dist = 1 + distance.determined.items[offset];
        distance.possible.items[offset] = 1;
        for (int i = 0; i < 4; i++) {
            if (valid_offset(offset + ROUTE_OFFSETS[i])) {
                callback(offset + ROUTE_OFFSETS[i], dist,
                    distance_left(x + ROUTE_OFFSETS_X[i], y + ROUTE_OFFSETS_Y[i]));
            }
        }
    }
}

static void route_queue_all_from(int source, max_directions directions, int (*callback)(int next_offset, int dist), int is_boat)
{
    clear_data();
    map_grid_clear_u8(water_drag.items);
    enqueue(source, 1);
    int tiles = 0;
    while (queue.head != queue.tail) {
        if (++tiles > GUARD) {
            break;
        }
        int offset = queue_pop();
        int drag = is_boat && terrain_water.items[offset] == WATER_N2_MAP_EDGE ? 4 : 0;
        if (water_drag.items[offset] < drag) {
            water_drag.items[offset]++;
            queue.items[queue.tail++] = offset;
            if (queue.tail >= MAX_QUEUE) {
                queue.tail = 0;
            }
        } else {
            int dist = 1 + distance.determined.items[offset];
            for (int i = 0; i < directions; i++) {
                if (valid_offset(offset + ROUTE_OFFSETS[i])) {
                    if (callback(offset + ROUTE_OFFSETS[i], dist) == UNTIL_STOP) {
                        break;
                    }
                }
            }
        }
    }
}

static int callback_calc_distance(int next_offset, int dist)
{
    if (terrain_land_citizen.items[next_offset] >= CITIZEN_0_ROAD) {
        enqueue(next_offset, dist);
    }
    return 1;
}

void map_routing_calculate_distances(int x, int y)
{
    ++stats.total_routes_calculated;
    route_queue_all_from(map_grid_offset(x, y), DIRECTIONS_NO_DIAGONALS, callback_calc_distance, 0);
}

static int callback_calc_distance_water_boat(int next_offset, int dist)
{
    if (terrain_water.items[next_offset] != WATER_N1_BLOCKED &&
        terrain_water.items[next_offset] != WATER_N3_LOW_BRIDGE) {
        enqueue(next_offset, dist);
        if (terrain_water.items[next_offset] == WATER_N2_MAP_EDGE) {
            distance.determined.items[next_offset] += 4;
        }
    }
    return 1;
}

void map_routing_calculate_distances_water_boat(int x, int y)
{
    int grid_offset = map_grid_offset(x, y);
    if (terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
        clear_data();
    } else {
        route_queue_all_from(grid_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_water_boat, 1);
    }
}

static int callback_calc_distance_water_flotsam(int next_offset, int dist)
{
    if (terrain_water.items[next_offset] != WATER_N1_BLOCKED) {
        enqueue(next_offset, dist);
    }
    return 1;
}

void map_routing_calculate_distances_water_flotsam(int x, int y)
{
    int grid_offset = map_grid_offset(x, y);
    if (terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
        clear_data();
    } else {
        route_queue_all_from(grid_offset, DIRECTIONS_DIAGONALS, callback_calc_distance_water_flotsam, 0);
    }
}

static int callback_calc_distance_build_wall(int next_offset, int dist)
{
    if (terrain_land_citizen.items[next_offset] == CITIZEN_4_CLEAR_TERRAIN) {
        enqueue(next_offset, dist);
    }
    return 1;
}

static int callback_calc_distance_build_road(int next_offset, int dist)
{
    int blocked = 0;
    switch (terrain_land_citizen.items[next_offset]) {
        case CITIZEN_N3_AQUEDUCT:
            if (!map_can_place_road_under_aqueduct(next_offset)) {
                distance.determined.items[next_offset] = -1;
                blocked = 1;
            }
            break;
        case CITIZEN_2_PASSABLE_TERRAIN: // rubble, garden, access ramp
        case CITIZEN_N1_BLOCKED: // non-empty land
            blocked = 1;
            break;
        default:
            if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
                blocked = 1;
            }
            break;
    }
    if (!blocked) {
        enqueue(next_offset, dist);
    }
    return 1;
}

static int callback_calc_distance_build_aqueduct(int next_offset, int dist)
{
    int blocked = 0;
    switch (terrain_land_citizen.items[next_offset]) {
        case CITIZEN_N3_AQUEDUCT:
        case CITIZEN_2_PASSABLE_TERRAIN: // rubble, garden, access ramp
        case CITIZEN_N1_BLOCKED: // non-empty land
            blocked = 1;
            break;
        default:
            if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
                if (terrain_land_citizen.items[next_offset] != CITIZEN_N4_RESERVOIR_CONNECTOR) {
                    blocked = 1;
                }
            }
            break;
    }
    if (map_terrain_is(next_offset, TERRAIN_ROAD) && !map_can_place_aqueduct_on_road(next_offset)) {
        distance.determined.items[next_offset] = -1;
        blocked = 1;
    }
    if (!blocked) {
        enqueue(next_offset, dist);
    }
    return 1;
}

static int map_can_place_initial_road_or_aqueduct(int grid_offset, int is_aqueduct)
{
    if (terrain_land_citizen.items[grid_offset] == CITIZEN_N1_BLOCKED) {
        // not open land, can only if:
        // - aqueduct should be placed, and:
        // - land is a reservoir building OR an aqueduct
        if (!is_aqueduct) {
            return 0;
        }
        if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
            return 1;
        }
        if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
            if (building_get(map_building_at(grid_offset))->type == BUILDING_RESERVOIR) {
                return 1;
            }
        }
        return 0;
    } else if (terrain_land_citizen.items[grid_offset] == CITIZEN_2_PASSABLE_TERRAIN) {
        // rubble, access ramp, garden
        return 0;
    } else if (terrain_land_citizen.items[grid_offset] == CITIZEN_N3_AQUEDUCT) {
        if (is_aqueduct) {
            return 0;
        }
        if (map_can_place_road_under_aqueduct(grid_offset)) {
            return 1;
        }
        return 0;
    } else {
        return 1;
    }
}

int map_routing_calculate_distances_for_building(routed_building_type type, int x, int y)
{
    if (type == ROUTED_BUILDING_WALL) {
        route_queue_all_from(map_grid_offset(x, y), DIRECTIONS_NO_DIAGONALS, callback_calc_distance_build_wall, 0);
        return 1;
    }
    clear_data();
    int source_offset = map_grid_offset(x, y);
    if (!map_can_place_initial_road_or_aqueduct(source_offset, type != ROUTED_BUILDING_ROAD)) {
        return 0;
    }
    if (map_terrain_is(source_offset, TERRAIN_ROAD) &&
        type != ROUTED_BUILDING_ROAD && !map_can_place_aqueduct_on_road(source_offset)) {
        return 0;
    }
    ++stats.total_routes_calculated;
    if (type == ROUTED_BUILDING_ROAD) {
        route_queue_all_from(source_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_build_road, 0);
    } else {
        route_queue_all_from(source_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_build_aqueduct, 0);
    }
    return 1;
}

static int callback_delete_wall_aqueduct(int next_offset, int dist)
{
    if (terrain_land_citizen.items[next_offset] < CITIZEN_0_ROAD) {
        if (map_terrain_is(next_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
            map_terrain_remove(next_offset, TERRAIN_CLEARABLE);
            return UNTIL_STOP;
        }
    } else {
        enqueue(next_offset, dist);
    }
    return UNTIL_CONTINUE;
}

void map_routing_delete_first_wall_or_aqueduct(int x, int y)
{
    ++stats.total_routes_calculated;
    route_queue_all_from(map_grid_offset(x, y), DIRECTIONS_NO_DIAGONALS, callback_delete_wall_aqueduct, 0);
}

static int is_fighting_friendly(figure *f)
{
    return f->is_friendly && f->action_state == FIGURE_ACTION_150_ATTACK;
}

static inline int has_fighting_friendly(int grid_offset)
{
    if (!(fighting_data.status.items[grid_offset] & 0x80)) {
        fighting_data.status.items[grid_offset] |= 0x80 | map_figure_foreach_until(grid_offset, is_fighting_friendly);
    }
    return fighting_data.status.items[grid_offset] & 1;
}

static int is_fighting_enemy(figure *f)
{
    return !f->is_friendly && f->action_state == FIGURE_ACTION_150_ATTACK;
}

static inline int has_fighting_enemy(int grid_offset)
{
    if (!(fighting_data.status.items[grid_offset] & 0x40)) {
        fighting_data.status.items[grid_offset] |= 0x40 | (map_figure_foreach_until(grid_offset, is_fighting_enemy) << 1);
    }
    return fighting_data.status.items[grid_offset] & 2;
}

static void callback_travel_citizen_land(int next_offset, int dist, int remaining_dist)
{
    if (terrain_land_citizen.items[next_offset] >= 0 && !has_fighting_friendly(next_offset)) {
        ordered_enqueue(next_offset, dist, remaining_dist);
    }
}

int map_routing_citizen_can_travel_over_land(int src_x, int src_y, int dst_x, int dst_y)
{
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, 0, callback_travel_citizen_land);
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

static void callback_travel_citizen_road_garden(int next_offset, int dist, int remaining_dist)
{
    if (terrain_land_citizen.items[next_offset] >= CITIZEN_0_ROAD &&
        terrain_land_citizen.items[next_offset] <= CITIZEN_2_PASSABLE_TERRAIN) {
        ordered_enqueue(next_offset, dist, remaining_dist);
    }
}

int map_routing_citizen_can_travel_over_road_garden(int src_x, int src_y, int dst_x, int dst_y)
{
    int dst_offset = map_grid_offset(dst_x, dst_y);
    if (terrain_land_citizen.items[dst_offset] < CITIZEN_0_ROAD ||
        terrain_land_citizen.items[dst_offset] > CITIZEN_2_PASSABLE_TERRAIN) {
        return 0;
    }
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, 0, callback_travel_citizen_road_garden);
    return distance.determined.items[dst_offset] != 0;
}

static void callback_travel_walls(int next_offset, int dist, int remaining_dist)
{
    if (terrain_walls.items[next_offset] >= WALL_0_PASSABLE &&
        terrain_walls.items[next_offset] <= 2) {
        ordered_enqueue(next_offset, dist, remaining_dist);
    }
}

int map_routing_can_travel_over_walls(int src_x, int src_y, int dst_x, int dst_y)
{
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, 0, callback_travel_walls);
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

static void callback_travel_noncitizen_land_through_building(int next_offset, int dist, int remaining_dist)
{
    if (!has_fighting_enemy(next_offset)) {
        if (terrain_land_noncitizen.items[next_offset] == NONCITIZEN_0_PASSABLE ||
            terrain_land_noncitizen.items[next_offset] == NONCITIZEN_2_CLEARABLE ||
            (terrain_land_noncitizen.items[next_offset] == NONCITIZEN_1_BUILDING &&
            map_building_at(next_offset) == state.through_building_id)) {
            ordered_enqueue(next_offset, dist, remaining_dist);
        }
    }
}

static void callback_travel_noncitizen_land(int next_offset, int dist, int remaining_dist)
{
    if (!has_fighting_enemy(next_offset)) {
        if (terrain_land_noncitizen.items[next_offset] >= NONCITIZEN_0_PASSABLE &&
            terrain_land_noncitizen.items[next_offset] < NONCITIZEN_5_FORT) {
            ordered_enqueue(next_offset, dist, remaining_dist);
        }
    }
}

int map_routing_noncitizen_can_travel_over_land(
    int src_x, int src_y, int dst_x, int dst_y, int only_through_building_id, int max_tiles)
{
    ++stats.total_routes_calculated;
    ++stats.enemy_routes_calculated;
    if (only_through_building_id) {
        state.through_building_id = only_through_building_id;
        route_queue_from_to(src_x, src_y, dst_x, dst_y, 0, callback_travel_noncitizen_land_through_building);
    } else {
        route_queue_from_to(src_x, src_y, dst_x, dst_y, max_tiles, callback_travel_noncitizen_land);
    }
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

static void callback_travel_noncitizen_through_everything(int next_offset, int dist, int remaining_dist)
{
    if (terrain_land_noncitizen.items[next_offset] >= NONCITIZEN_0_PASSABLE) {
        ordered_enqueue(next_offset, dist, remaining_dist);
    }
}

int map_routing_noncitizen_can_travel_through_everything(int src_x, int src_y, int dst_x, int dst_y)
{
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, 0, callback_travel_noncitizen_through_everything);
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

void map_routing_block(int x, int y, int size)
{
    if (!map_grid_is_inside(x, y, size)) {
        return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            distance.determined.items[map_grid_offset(x + dx, y + dy)] = 0;
        }
    }
}

int map_routing_distance(int grid_offset)
{
    return distance.determined.items[grid_offset];
}

void map_routing_save_state(buffer *buf)
{
    buffer_write_i32(buf, 0); // unused counter
    buffer_write_i32(buf, stats.enemy_routes_calculated);
    buffer_write_i32(buf, stats.total_routes_calculated);
    buffer_write_i32(buf, 0); // unused counter
}

void map_routing_load_state(buffer *buf)
{
    buffer_skip(buf, 4); // unused counter
    stats.enemy_routes_calculated = buffer_read_i32(buf);
    stats.total_routes_calculated = buffer_read_i32(buf);
    buffer_skip(buf, 4); // unused counter
}
