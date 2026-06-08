#include <pebble.h>

#define WEATHER_KEY 0

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;

static GBitmap *s_bg_bitmap;
static BitmapLayer *s_bg_layer;

static GBitmap *s_fg_bitmap;
static BitmapLayer *s_fg_layer;

static GBitmap *s_dial_bitmap;
static RotBitmapLayer *s_dial_layer;

// Seizoenen lagen
static GBitmap *s_season_bitmap;
static BitmapLayer *s_season_layer;
static int s_current_season = -1;

// Weer lagen
static GBitmap *s_weather_bitmap;
static BitmapLayer *s_weather_layer;
static int s_current_weather = -1; 

// Festival lagen
static GBitmap *s_festival_bitmap;
static BitmapLayer *s_festival_layer;
static bool s_is_festival_day = false;

static GFont s_time_font;
static GFont s_date_font;

// Functie om de exacte datum van het echte Pasen te berekenen (Meeus/Jones/Butcher algoritme)
static bool is_easter(int year, int month, int day) {
  int a = year % 19;
  int b = year / 100;
  int c = year % 100;
  int d = b / 4;
  int e = b % 4;
  int f = (b + 8) / 25;
  int g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30;
  int i = c / 4;
  int k = c % 4;
  int L = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * L) / 451;
  
  int easter_month = (h + L - 7 * m + 114) / 31; // 3 = Maart, 4 = April
  int easter_day = ((h + L - 7 * m + 114) % 31) + 1;

  return (month == (easter_month - 1) && day == easter_day);
}

// Functie om te bepalen of het vandaag een festivaldag is
static bool check_festival_day(int year, int month, int day) {
  if (is_easter(year, month, day)) return true;  
  if (month == 11 && day == 25) return true; // Kerstmis (25 December)
  if (month == 0 && day == 1) return true;   // Nieuwjaar (1 Januari)
  return false;
}

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  int year = tick_time->tm_year + 1900;
  int month = tick_time->tm_mon; 
  int day = tick_time->tm_mday;   

  // --- TIJD EN DATUM ---
  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);

  static char s_date_buffer[16];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%a. %d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);

  // --- SEIZOENEN LOGICA ---
  int detected_season = 0;
  if ((month == 11 && day >= 21) || month == 0 || month == 1 || (month == 2 && day < 21)) {
    detected_season = 0; // Winter
  } else if ((month == 2 && day >= 21) || month == 3 || month == 4 || (month == 5 && day < 21)) {
    detected_season = 1; // Lente
  } else if ((month == 5 && day >= 21) || month == 6 || month == 7 || (month == 8 && day < 23)) {
    detected_season = 2; // Zomer
  } else {
    detected_season = 3; // Herfst
  }

  if (detected_season != s_current_season) {
    s_current_season = detected_season;
    if (s_season_bitmap != NULL) gbitmap_destroy(s_season_bitmap);

    switch (s_current_season) {
      case 0: s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_WINTER); break;
      case 1: s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_SPRING); break;
      case 2: s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_SUMMER); break;
      case 3: s_season_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEASON_FALL); break;
    }
    if (s_season_layer != NULL) {
      bitmap_layer_set_bitmap(s_season_layer, s_season_bitmap);
      layer_mark_dirty(bitmap_layer_get_layer(s_season_layer));
    }
  }

  // --- FESTIVAL EN SEIZOEN WISSEL LOGICA ---
  bool today_festival = check_festival_day(year, month, day);
  if (today_festival != s_is_festival_day) {
    s_is_festival_day = today_festival;
    
    if (s_festival_bitmap != NULL) {
      gbitmap_destroy(s_festival_bitmap);
      s_festival_bitmap = NULL;
    }

    if (s_is_festival_day) {
      s_festival_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BADGE_FESTIVAL);
      bitmap_layer_set_bitmap(s_festival_layer, s_festival_bitmap);
      
      // Wissel om: Verberg seizoen, laat festival zien
      layer_set_hidden(bitmap_layer_get_layer(s_season_layer), true);
      layer_set_hidden(bitmap_layer_get_layer(s_festival_layer), false);
    } else {
      // Wissel terug: Laat seizoen zien, verberg festival
      layer_set_hidden(bitmap_layer_get_layer(s_festival_layer), true);
      layer_set_hidden(bitmap_layer_get_layer(s_season_layer), false);
    }
  }

  // --- ROTATIE LOGICA ---
  int hours = tick_time->tm_hour;
  int minutes = tick_time->tm_min;
  int32_t angle = 0; 
  if (hours >= 6) {
    int minutes_since_six = ((hours - 6) * 60) + minutes;
    int total_minutes_interval = 1080;
    int32_t start_angle = TRIG_MAX_ANGLE / 2; 
    int32_t total_rotation = TRIG_MAX_ANGLE / 2; 
    angle = start_angle + ((minutes_since_six * total_rotation) / total_minutes_interval);
  } else {
    angle = 0; 
  }
  rot_bitmap_layer_set_angle(s_dial_layer, angle);
}

// Callback wanneer het weer via de telefoon binnenkomt
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *weather_tuple = dict_find(iterator, WEATHER_KEY);
  if (weather_tuple) {
    int received_weather = weather_tuple->value->int32;
    
    if (received_weather != s_current_weather) {
      s_current_weather = received_weather;
      if (s_weather_bitmap != NULL) gbitmap_destroy(s_weather_bitmap);

      switch (s_current_weather) {
        case 0: s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WEATHER_SUN); break;
        case 1: s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WEATHER_RAIN); break;
        case 2: s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WEATHER_SNOW); break;
        case 3: s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WEATHER_THUNDER); break;
        case 4: s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WEATHER_WIND); break;
      }

      if (s_weather_layer != NULL && s_weather_bitmap != NULL) {
        bitmap_layer_set_bitmap(s_weather_layer, s_weather_bitmap);
        layer_mark_dirty(bitmap_layer_get_layer(s_weather_layer));
      }
    }
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  
  if (tick_time->tm_min % 30 == 0) {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    if (iter != NULL) {
      dict_write_uint8(iter, 0, 0);
      app_message_outbox_send();
    }
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // 1. Achtergrond
  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  s_bg_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bg_layer));

  int item_w = 30;
  int item_h = 20;
  
  int season_x = 142;
  int season_y = 80;
  int badge_spacing_x = 60; 

  // 2. Seizoenen laag
  s_season_layer = bitmap_layer_create(GRect(season_x, season_y, item_w, item_h));
  bitmap_layer_set_compositing_mode(s_season_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_season_layer));

  // 3. Weer laag
  int weather_x = season_x - badge_spacing_x;
  s_weather_layer = bitmap_layer_create(GRect(weather_x, season_y, item_w, item_h));
  bitmap_layer_set_compositing_mode(s_weather_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_layer));

  // Standaard error-afbeelding zolang er geen weerdata is ontvangen
  s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WEATHER_ERROR);
  bitmap_layer_set_bitmap(s_weather_layer, s_weather_bitmap);

  // 4. Festival laag
  s_festival_layer = bitmap_layer_create(GRect(season_x, season_y, item_w, item_h));
  bitmap_layer_set_compositing_mode(s_festival_layer, GCompOpSet);
  layer_set_hidden(bitmap_layer_get_layer(s_festival_layer), true);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_festival_layer));

  // 5. Voorgrond
  s_fg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FOREGROUND);
  int fg_w = 180;
  int fg_h = 148;
  int fg_x = (bounds.size.w / 2) - (fg_w / 2); 
  int fg_y = 40;
  
  s_fg_layer = bitmap_layer_create(GRect(fg_x, fg_y, fg_w, fg_h)); 
  bitmap_layer_set_bitmap(s_fg_layer, s_fg_bitmap);
  bitmap_layer_set_compositing_mode(s_fg_layer, GCompOpSet); 
  layer_add_child(window_layer, bitmap_layer_get_layer(s_fg_layer));

  // 6. Roterende Dial
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

  // Textlagen
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
  if (s_season_bitmap != NULL) gbitmap_destroy(s_season_bitmap);

  bitmap_layer_destroy(s_weather_layer);
  if (s_weather_bitmap != NULL) gbitmap_destroy(s_weather_bitmap);

  bitmap_layer_destroy(s_festival_layer);
  if (s_festival_bitmap != NULL) gbitmap_destroy(s_festival_bitmap);

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
  
  update_time();
  
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(64, 64);
  
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