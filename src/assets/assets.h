#ifndef ASSETS_H
#define ASSETS_H

#include "core/image.h"

#define ASSETS_DIR_NAME "assets"
#define ASSETS_DIRECTORY "***" ASSETS_DIR_NAME "***"

#define PATH_ROTATE_OFFSET 56

void assets_init(int force_reload, color_t **main_images, int *main_image_widths);

int assets_load_single_group(const char *file_name, color_t **main_images, int *main_image_widths);

int assets_get_group_id(const char *assetlist_name);

int assets_get_image_id(const char *assetlist_name, const char *image_name);

const image *assets_get_image(int image_id);

void assets_load_unpacked_asset(int image_id);

#endif // ASSETS_H
