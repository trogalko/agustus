#ifndef BUILDING_WAREHOUSE_H
#define BUILDING_WAREHOUSE_H

#include "building/building.h"
#include "map/point.h"

#define FULL_WAREHOUSE 32
#define THREEQ_WAREHOUSE 24
#define HALF_WAREHOUSE 16
#define QUARTER_WAREHOUSE 8

enum {
    WAREHOUSE_ROOM = 0,
    WAREHOUSE_FULL = 1,
    WAREHOUSE_SOME_ROOM = 2
};

enum {
    WAREHOUSE_TASK_NONE = -1,
    WAREHOUSE_TASK_GETTING = 0,
    WAREHOUSE_TASK_DELIVERING = 1
};

int building_warehouse_get_space_info(building *warehouse);

int building_warehouse_get_amount(building *warehouse, int resource);

int building_warehouse_add_resource(building *b, int resource);

int building_warehouse_is_accepting(int resource, building *b);
int building_warehouse_is_getting(int resource, building *b);
int building_warehouse_is_not_accepting(int resource, building *b);

int building_warehouse_remove_resource(building *warehouse, int resource, int amount);

void building_warehouse_remove_resource_curse(building *warehouse, int amount);

void building_warehouse_space_set_image(building *space, int resource);

void building_warehouse_space_add_import(building *space, int resource, int land_trader);

void building_warehouse_space_remove_export(building *space, int resource, int land_trader);

int building_warehouses_remove_resource(int resource, int amount);

int building_warehouses_send_resources_to_rome(int resource, int amount);

int building_warehouse_accepts_storage(building *b, int resource, int *understaffed);

int building_warehouse_for_storing(int src_building_id, int x, int y, int resource, int road_network_id,
                                   int *understaffed, map_point *dst);

int building_warehouse_amount_can_get_from(building *destination, int resource);

int building_warehouse_for_getting(building *src, int resource, map_point *dst);

int building_warehouse_with_resource(int src_building_id, int x, int y, int resource,
    int distance_from_entry, int road_network_id, int* understaffed,
    map_point* dst);

int building_warehouse_determine_worker_task(building *warehouse, int *resource);

#endif // BUILDING_WAREHOUSE_H
