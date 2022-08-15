#ifndef CITY_SENTIMENT_H
#define CITY_SENTIMENT_H

int city_sentiment(void);

int city_sentiment_low_mood_cause(void);

void city_sentiment_change_happiness(int amount);

void city_sentiment_set_max_happiness(int max);
void city_sentiment_set_min_happiness(int min);

int city_sentiment_get_population_below_happiness(int happiness);

void city_sentiment_reset_protesters_criminals(void);

void city_sentiment_add_protester(void);
void city_sentiment_add_criminal(void);

int city_sentiment_protesters(void);
int city_sentiment_criminals(void);
int city_sentiment_crime_cooldown(void);
void city_sentiment_set_crime_cooldown(void);
void city_sentiment_reduce_crime_cooldown(void);
int city_sentiment_get_blessing_festival_boost(void);
void city_sentiment_decrement_blessing_boost(void);

void city_sentiment_update(void);

#endif // CITY_SENTIMENT_H
