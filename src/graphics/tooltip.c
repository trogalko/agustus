#include "tooltip.h"

#include "city/labor.h"
#include "city/ratings.h"
#include "city/view.h"
#include "core/lang.h"
#include "core/string.h"
#include "core/time.h"
#include "game/settings.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "map/grid.h"
#include "map/property.h"
#include "scenario/criteria.h"
#include "scenario/property.h"
#include "translation/translation.h"
#include "window/advisors.h"

#include <stdlib.h>

#define COMPOSED_TOOLTIP_TEXT_MAX 1000

static const int DEFAULT_TEXT_GROUP = 68;
static const time_millis TOOLTIP_DELAY_MILLIS = 150;

static time_millis last_update = 0;
static uint8_t composed_tooltip_text[COMPOSED_TOOLTIP_TEXT_MAX];
static struct {
    int is_active;
    int x;
    int y;
    int width;
    int height;
    int buffer_id;
} button_tooltip_info;

static void reset_timer(void)
{
    last_update = time_get_millis();
}

static int should_draw_tooltip(tooltip_context *c)
{
    if (c->type == TOOLTIP_NONE) {
        reset_timer();
        return 0;
    }
    if (!c->high_priority && setting_tooltips() != TOOLTIPS_FULL) {
        reset_timer();
        return 0;
    }
    if (time_get_millis() - last_update < TOOLTIP_DELAY_MILLIS) { // delay drawing tooltip
        return 0;
    }
    return 1;
}

static void reset_tooltip(tooltip_context *c)
{
    if (c->type != TOOLTIP_NONE) {
        c->type = TOOLTIP_NONE;
    }
}

static void restore_window_under_tooltip_from_buffer(void)
{
    if (button_tooltip_info.is_active) {
        graphics_draw_from_image(button_tooltip_info.buffer_id, button_tooltip_info.x, button_tooltip_info.y);
    }
}

static void save_window_under_tooltip_to_buffer(int x, int y, int width, int height)
{
    if (button_tooltip_info.is_active &&
        x == button_tooltip_info.x && y == button_tooltip_info.y &&
        width == button_tooltip_info.width && height == button_tooltip_info.height) {
        return;
    }
    restore_window_under_tooltip_from_buffer();
    button_tooltip_info.is_active = 1;
    button_tooltip_info.x = x;
    button_tooltip_info.y = y;
    button_tooltip_info.width = width;
    button_tooltip_info.height = height;
    button_tooltip_info.buffer_id = graphics_save_to_image(button_tooltip_info.buffer_id, x, y, width, height);
}

static const uint8_t *get_tooltip_text(const tooltip_context *c)
{
    const uint8_t* text;
    if (c->precomposed_text) {
        return c->precomposed_text;
    }
    if (c->translation_key) {
        text = translation_for(c->translation_key);
    } else {
        text = lang_get_string(c->text_group, c->text_id);
    }
    if (c->has_numeric_prefix) {
        int offset = string_from_int(composed_tooltip_text, c->numeric_prefix, 0);
        string_copy(text, &composed_tooltip_text[offset], COMPOSED_TOOLTIP_TEXT_MAX - offset);
        text = composed_tooltip_text;
    } else if (c->num_extra_texts > 0) {
        string_copy(text, composed_tooltip_text, COMPOSED_TOOLTIP_TEXT_MAX);
        int offset = string_length(composed_tooltip_text);
        int is_comma_separated = c->extra_text_type == TOOLTIP_EXTRA_TEXT_COMMA_SEPARATED;
        if (is_comma_separated) {
            composed_tooltip_text[offset++] = ':';
            composed_tooltip_text[offset++] = '\n';
        } else {
            composed_tooltip_text[offset++] = ' ';
        }
        for (int i = 0; i < c->num_extra_texts; i++) {
            if (i) {
                if (is_comma_separated) {
                    composed_tooltip_text[offset++] = ',';
                }
                composed_tooltip_text[offset++] = ' ';
            }
            const uint8_t *extra_value = lang_get_string(c->extra_text_groups[i], c->extra_text_ids[i]);
            string_copy(extra_value, &composed_tooltip_text[offset], COMPOSED_TOOLTIP_TEXT_MAX - offset);
            offset += string_length(extra_value);
        }
        text = composed_tooltip_text;
    }
    return text;
}

static void draw_button_tooltip(tooltip_context *c)
{
    const uint8_t *text = get_tooltip_text(c);
    int width = 200;
    int lines = text_measure_multiline(text, width - 5, FONT_SMALL_PLAIN);
    if (lines > 2) {
        width = 300;
        lines = text_measure_multiline(text, width - 5, FONT_SMALL_PLAIN);
    }
    int height = 16 * lines + 10;

    int x, y;
    if (c->mouse_x < screen_dialog_offset_x() + width + 100) {
        if (window_is(WINDOW_ADVISORS)) {
            x = c->mouse_x + 50;
        } else {
            x = c->mouse_x + 20;
        }
    } else {
        x = c->mouse_x - width - 20;
    }
    if (x + width > screen_width() - 20) {
        x = c->mouse_x - width - 20;
    }
    if (x < 20) {
        x = 20;
    }

    switch (window_get_id()) {
        case WINDOW_ADVISORS:
            if (c->mouse_y < screen_dialog_offset_y() + 432) {
                y = c->mouse_y;
                switch (window_advisors_get_advisor()) {
                    case ADVISOR_LABOR: y -= 74; break;
                    case ADVISOR_TRADE: y -= 54; break;
                    case ADVISOR_POPULATION: y -= 58; break;
                    default: y -= 64; break;
                }
            } else {
                y = screen_dialog_offset_y() + 432;
            }
            break;
        case WINDOW_TRADE_PRICES: // FIXED used to cause ghosting
            y = c->mouse_y - 42;
            break;
        case WINDOW_DONATE_TO_CITY:
            y = c->mouse_y - 52;
            break;
        case WINDOW_LABOR_PRIORITY:
            x = c->mouse_x - width / 2 - 10;
            if (c->mouse_y < screen_dialog_offset_y() + 200) {
                y = c->mouse_y + 40;
            } else {
                y = c->mouse_y - 72;
            }
            break;
        default:
            if (c->mouse_y < screen_dialog_offset_y() + 200) {
                y = c->mouse_y + 40;
            } else {
                y = c->mouse_y - 62;
            }
            break;
    }

    if (y + height > screen_height() - 1) {
        y = screen_height() - 1 - height;
    }
    if (y < 20) {
        y = 20;
    }

    save_window_under_tooltip_to_buffer(x, y, width, height);

    graphics_draw_rect(x, y, width, height, COLOR_BLACK);
    graphics_fill_rect(x + 1, y + 1, width - 2, height - 2, COLOR_WHITE);
    text_draw_multiline(text, x + 5, y + 7, width - 5, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
}

static void draw_overlay_tooltip(tooltip_context *c)
{
    const uint8_t *text = get_tooltip_text(c);
    int width = 200;
    int lines = text_measure_multiline(text, width - 5, FONT_SMALL_PLAIN);
    if (lines > 2) {
        width = 300;
        lines = text_measure_multiline(text, width - 5, FONT_SMALL_PLAIN);
    }
    int height = 16 * lines + 10;

    int x, y;
    if (c->mouse_x < width + 20) {
        x = c->mouse_x + 20;
    } else {
        x = c->mouse_x - width - 20;
    }
    if (c->mouse_y < 200) {
        y = c->mouse_y + 50;
    } else if (c->mouse_y + height - 72 > screen_height()) {
        y = screen_height() - height;
    } else {
        y = c->mouse_y - 72;
    }

    save_window_under_tooltip_to_buffer(x, y, width, height);

    graphics_draw_rect(x, y, width, height, COLOR_BLACK);
    graphics_fill_rect(x + 1, y + 1, width - 2, height - 2, COLOR_WHITE);
    text_draw_multiline(text, x + 5, y + 7, width - 5, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
}

static void draw_senate_tooltip(tooltip_context *c)
{
    int x, y;
    int width = 220;
    int height = 80;
    if (c->mouse_x < width + 20) {
        x = c->mouse_x + 20;
    } else {
        x = c->mouse_x - width - 20;
    }
    if (c->mouse_y < 200) {
        y = c->mouse_y + 10;
    } else if (c->mouse_y + height - 32 > screen_height()) {
        y = screen_height() - height;
    } else {
        y = c->mouse_y - 32;
    }

    save_window_under_tooltip_to_buffer(x, y, width, height);

    graphics_draw_rect(x, y, width, height, COLOR_BLACK);
    graphics_fill_rect(x + 1, y + 1, width - 2, height - 2, COLOR_WHITE);

    // unemployment
    lang_text_draw_colored(68, 148, x + 5, y + 5, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    width = text_draw_number(city_labor_unemployment_percentage(), '@', "%",
        x + 140, y + 5, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    text_draw_number(city_labor_workers_unemployed() - city_labor_workers_needed(), '(', ")",
        x + 140 + width, y + 5, FONT_SMALL_PLAIN, COLOR_TOOLTIP);

    // ratings
    lang_text_draw_colored(68, 149, x + 5, y + 19, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    text_draw_number(city_rating_culture(), '@', " ",
        x + 140, y + 19, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    if (!scenario_is_open_play() && scenario_criteria_culture_enabled()) {
        text_draw_number(scenario_criteria_culture(), '(', ")",
            x + 140 + width, y + 19, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    }

    lang_text_draw_colored(68, 150, x + 5, y + 33, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    text_draw_number(city_rating_prosperity(), '@', " ",
        x + 140, y + 33, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    if (!scenario_is_open_play() && scenario_criteria_prosperity_enabled()) {
        text_draw_number(scenario_criteria_prosperity(), '(', ")",
            x + 140 + width, y + 33, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    }

    lang_text_draw_colored(68, 151, x + 5, y + 47, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    text_draw_number(city_rating_peace(), '@', " ",
        x + 140, y + 47, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    if (!scenario_is_open_play() && scenario_criteria_peace_enabled()) {
        text_draw_number(scenario_criteria_peace(), '(', ")",
            x + 140 + width, y + 47, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    }

    lang_text_draw_colored(68, 152, x + 5, y + 61, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    text_draw_number(city_rating_favor(), '@', " ",
        x + 140, y + 61, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    if (!scenario_is_open_play() && scenario_criteria_favor_enabled()) {
        text_draw_number(scenario_criteria_favor(), '(', ")",
            x + 140 + width, y + 61, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    }
}

static void draw_tile_tooltip(tooltip_context * c){
    view_tile view;
    if (city_view_pixels_to_view_tile(c->mouse_x, c->mouse_y, &view)) {
        int grid_offset = city_view_tile_to_grid_offset(&view);
        city_view_set_selected_view_tile(&view);
        int x_tile = map_grid_offset_to_x(grid_offset);
        int y_tile = map_grid_offset_to_y(grid_offset);

        int x, y;
        int width = 60;
        int height = 40;
        if (c->mouse_x < width + 20) {
            x = c->mouse_x + 20;
        } else {
            x = c->mouse_x - width - 20;
        }
        if (c->mouse_y < 40) {
            y = c->mouse_y + 10;
        } else if (c->mouse_y + height - 32 > screen_height()) {
            y = screen_height() - height;
        } else {
            y = c->mouse_y - 32;
        }

        save_window_under_tooltip_to_buffer(x, y, width, height);

        graphics_draw_rect(x, y, width, height, COLOR_BLACK);
        graphics_fill_rect(x + 1, y + 1, width - 2, height - 2, COLOR_WHITE);
        text_draw_label_and_number(string_from_ascii("x: "), x_tile, " ", x + 2, y + 5, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
        text_draw_label_and_number(string_from_ascii("y: "), y_tile, " ", x + 2 , y + 19, FONT_SMALL_PLAIN, COLOR_TOOLTIP);
    }
}

static void draw_tooltip(tooltip_context *c)
{
    if (c->type == TOOLTIP_BUTTON) {
        draw_button_tooltip(c);
    } else if (c->type == TOOLTIP_OVERLAY) {
        draw_overlay_tooltip(c);
    } else if(c->type == TOOLTIP_TILES) {
        draw_tile_tooltip(c);
    } else if (c->type == TOOLTIP_SENATE) {
        draw_senate_tooltip(c);
    }
}

void tooltip_invalidate(void)
{
    button_tooltip_info.is_active = 0;
}

void tooltip_handle(const mouse *m, void (*func)(tooltip_context *))
{
    if (m->is_touch && !m->left.is_down) {
        reset_timer();
        return;
    }
    tooltip_context context = {m->x, m->y};
    context.text_group = DEFAULT_TEXT_GROUP;
    if (setting_tooltips() && func) {
        func(&context);
    }
    if (should_draw_tooltip(&context)) {
        draw_tooltip(&context);
        reset_tooltip(&context);
    } else {
        restore_window_under_tooltip_from_buffer();
        button_tooltip_info.is_active = 0;
    }
}
