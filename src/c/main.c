#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;

static GBitmap *s_bg_bitmap;
static BitmapLayer *s_bg_layer;

static GBitmap *s_fg_bitmap;
static BitmapLayer *s_fg_layer;

// Custom fonts
static GFont s_time_font;
static GFont s_date_font;

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);

  static char s_date_buffer[16];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a. %d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  s_bg_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bg_layer));

  s_fg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FOREGROUND);
  
  int fg_w = 180;
  int fg_h = 148;
  
  int fg_x = (bounds.size.w / 2) - (fg_w / 2); 
  int fg_y = 40;

  s_fg_layer = bitmap_layer_create(GRect(fg_x, fg_y, fg_w, fg_h)); 
  bitmap_layer_set_bitmap(s_fg_layer, s_fg_bitmap);
  bitmap_layer_set_compositing_mode(s_fg_layer, GCompOpSet); 
  layer_add_child(window_layer, bitmap_layer_get_layer(s_fg_layer));

  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SV_28));
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SV_28));

  int date_height = 30;

  // Time TextLayer
  s_time_layer = text_layer_create(GRect(28, 100, bounds.size.w, 60));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);


  // Date TextLayer
  s_date_layer = text_layer_create(GRect(28, 45, bounds.size.w, date_height));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
}

static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);

  bitmap_layer_destroy(s_fg_layer);
  gbitmap_destroy(s_fg_bitmap);

  bitmap_layer_destroy(s_bg_layer);
  gbitmap_destroy(s_bg_bitmap);

  // Unload custom fonts
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_date_font);
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  window_stack_push(s_main_window, true);
  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}