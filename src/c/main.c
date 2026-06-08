#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;

static GBitmap *s_bg_bitmap;
static BitmapLayer *s_bg_layer;

static GBitmap *s_fg_bitmap;
static BitmapLayer *s_fg_layer;

// Variabelen voor de roterende wijzer (Sun/Moon dial)
static GBitmap *s_dial_bitmap;
static RotBitmapLayer *s_dial_layer;

static GBitmap *s_season_bitmap;
static BitmapLayer *s_season_layer;
static int s_current_season = -1;

// Custom fonts
static GFont s_time_font;
static GFont s_date_font;

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // --- TIJD EN DATUM ---
  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);

  static char s_date_buffer[16];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a. %d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);

// --- SEIZOENEN LOGICA ---
  int month = tick_time->tm_mon;
  int day = tick_time->tm_mday;
  int detected_season = 0;

  if ((month == 11 && day >= 21) || month == 0 || month == 1 || (month == 2 && day < 21)) {
    detected_season = 0; // Winter: 21 dec t/m 20 mrt
  } else if ((month == 2 && day >= 21) || month == 3 || month == 4 || (month == 5 && day < 21)) {
    detected_season = 1; // Lente: 21 mrt t/m 20 jun
  } else if ((month == 5 && day >= 21) || month == 6 || month == 7 || (month == 8 && day < 23)) {
    detected_season = 2; // Zomer: 21 jun t/m 22 sep
  } else {
    detected_season = 3; // Herfst: 23 sep t/m 20 dec
  }

  if (detected_season != s_current_season) {
    s_current_season = detected_season;

    if (s_season_bitmap != NULL) {
      gbitmap_destroy(s_season_bitmap);
    }

    switch (s_current_season) {
      case 0:
        s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_WINTER);
        break;
      case 1:
        s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_SPRING);
        break;
      case 2:
        s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_SUMMER);
        break;
      case 3:
        s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_FALL);
        break;
    }

    if (s_season_layer != NULL) {
      bitmap_layer_set_bitmap(s_season_layer, s_season_bitmap);
      layer_mark_dirty(bitmap_layer_get_layer(s_season_layer));
    }
  }

  // --- ROTATIE LOGICA (DIAL) ---
  int hours = tick_time->tm_hour;
  int minutes = tick_time->tm_min;
  int32_t angle = 0; 

  if (hours >= 6) {
    int minutes_since_six = ((hours - 6) * 60) + minutes;
    int total_minutes_interval = 1080;
    
    int32_t start_angle = TRIG_MAX_ANGLE / 2; // 180 graden
    int32_t total_rotation = TRIG_MAX_ANGLE / 2; // Nog 180 graden te gaan
    
    angle = start_angle + ((minutes_since_six * total_rotation) / total_minutes_interval);
  } else {
    angle = 0; 
  }

  rot_bitmap_layer_set_angle(s_dial_layer, angle);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // 1. Achtergrond aanmaken
  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  s_bg_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bg_layer));

  // Voorgrond afmetingen bepalen (nodig voor positionering seizoen)
  int fg_w = 180;
  int fg_h = 148;
  int fg_x = (bounds.size.w / 2) - (fg_w / 2); 
  int fg_y = 40;

  int season_w = 30;
  int season_h = 20;
  int season_x = 142;
  int season_y = 80;

  s_season_layer = bitmap_layer_create(GRect(season_x, season_y, season_w, season_h));
  bitmap_layer_set_compositing_mode(s_season_layer, GCompOpSet);
  // We voegen hem NU al toe, zodat de voorgrond er dadelijk overheen valt!
  layer_add_child(window_layer, bitmap_layer_get_layer(s_season_layer));

  // 3. Voorgrond aanmaken (valt over de seizoensafbeelding heen)
  s_fg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FOREGROUND);
  s_fg_layer = bitmap_layer_create(GRect(fg_x, fg_y, fg_w, fg_h)); 
  bitmap_layer_set_bitmap(s_fg_layer, s_fg_bitmap);
  bitmap_layer_set_compositing_mode(s_fg_layer, GCompOpSet); 
  layer_add_child(window_layer, bitmap_layer_get_layer(s_fg_layer));

  // 4. Roterende Dial aanmaken
  s_dial_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DIAL);
  s_dial_layer = rot_bitmap_layer_create(s_dial_bitmap);
  rot_bitmap_set_compositing_mode(s_dial_layer, GCompOpSet);
  
  GRect bitmap_bounds = gbitmap_get_bounds(s_dial_bitmap);
  GPoint pivot_point = GPoint(bitmap_bounds.size.w / 2, bitmap_bounds.size.h - 3);
  rot_bitmap_set_src_ic(s_dial_layer, pivot_point);
  
  GRect dial_frame = layer_get_frame((Layer *)s_dial_layer);
  dial_frame.origin.x = 28;
  dial_frame.origin.y = 55;
  layer_set_frame((Layer *)s_dial_layer, dial_frame);
  layer_add_child(window_layer, (Layer *)s_dial_layer);

  // 5. Tekstlagen en lettertypen laden
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SV_28));
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SV_28));

  s_time_layer = text_layer_create(GRect(28, 100, bounds.size.w, 60));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  s_date_layer = text_layer_create(GRect(28, 45, bounds.size.w, 30));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);

  bitmap_layer_destroy(s_fg_layer);
  gbitmap_destroy(s_fg_bitmap);

  bitmap_layer_destroy(s_season_layer);
  if (s_season_bitmap != NULL) {
    gbitmap_destroy(s_season_bitmap);
  }

  rot_bitmap_layer_destroy(s_dial_layer);
  gbitmap_destroy(s_dial_bitmap);

  bitmap_layer_destroy(s_bg_layer);
  gbitmap_destroy(s_bg_bitmap);

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
  update_time(); // Dit laadt ook direct het juiste seizoen in bij de start!
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