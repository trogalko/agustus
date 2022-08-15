#ifndef CITY_GAMES_H
#define CITY_GAMES_H

#include "city/resource.h"
#include "translation/translation.h"

#define MAX_GAMES 3

typedef struct {
    int id;
    int header_key;
    int description_key;
    int message_planning;
    int cost_base;
    int cost_scaling;
    int delay_months;
    int duration_days;
    int bonus_duration;
    int building_id_required;
    int water_access_required;
    int resource_cost[RESOURCE_MAX];
    void (*games_start_function)(int id);
} games_type;

int city_games_money_cost(int game_type_id);
int city_games_resource_cost(int game_type_id, resource_type resource);

void city_games_schedule(int game_id);
void city_games_decrement_month_counts(void);
void city_games_decrement_duration(void);

int city_games_naval_battle_active(void);
int city_games_executions_active(void);
int city_games_imperial_festival_active(void);
int city_games_naval_battle_distant_battle_bonus_active(void);
void city_games_remove_naval_battle_distant_battle_bonus(void);

games_type *city_games_get_game_type(int id);

#endif // CITY_GAMES_H
