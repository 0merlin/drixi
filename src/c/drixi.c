#include <pebble.h>

// Default value
#define STEPS_DEFAULT 1000

#define R0OX 72
#define R0OY 3
#define R0IX 72
#define R0IY 33
#define R0MX 72
#define R0MY 53

#define R1OX 102
#define R1OY 11
#define R1IX 87
#define R1IY 37
#define R1MX 77
#define R1MY 54

#define R2OX 123
#define R2OY 33
#define R2IX 97
#define R2IY 48
#define R2MX 80
#define R2MY 58

#define R3OX 132
#define R3OY 63
#define R3IX 102
#define R3IY 63
#define R3MX 82
#define R3MY 63

#define R4OX 123
#define R4OY 92
#define R4IX 97
#define R4IY 78
#define R4MX 80
#define R4MY 68

#define R5OX 102
#define R5OY 114
#define R5IX 87
#define R5IY 88
#define R5MX 77
#define R5MY 71

#define R6OX 72
#define R6OY 123
#define R6IX 72
#define R6IY 93
#define R6MX 72
#define R6MY 73

#define R7OX 42
#define R7OY 114
#define R7IX 57
#define R7IY 88
#define R7MX 67
#define R7MY 71

#define R8OX 20
#define R8OY 93
#define R8IX 46
#define R8IY 78
#define R8MX 63
#define R8MY 68

#define R9OX 12
#define R9OY 63
#define R9IX 42
#define R9IY 63
#define R9MX 62
#define R9MY 63

#define R10OX 20
#define R10OY 33
#define R10IX 46
#define R10IY 48
#define R10MX 63
#define R10MY 58

#define R11OX 41
#define R11OY 11
#define R11IX 56
#define R11IY 37
#define R11MX 67
#define R11MY 54

static AppSync s_sync;

static Window *s_main_window;

static Layer *clock_layer, *battery_layer;

static GFont s_time_font;

static TextLayer *top_left, *top_right, *top_middle, *bottom_left, *bottom_right;

static int current_steps = 0, steps_day_average; //, steps_average_now
static int battery_level, phone_battery = -1;
static bool bluetooth_connected = false, battery_charging = false, phone_battery_charging = false, flick_showing = false;
static int hour_hand = 0, minute_hand = 0;

static char s_time[8], steps_avg_buffer[10], date_buffer[25], battery_buffer[10], steps_buffer[25];

static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment);
static void battery_callback(BatteryChargeState state);
static void bluetooth_callback(bool connected);
static void clock_update_proc(Layer *layer, GContext *ctx);
static void battery_layer_update_proc(Layer *layer, GContext *ctx);
static void draw_line(GContext *ctx, int line, bool inner);
static void format_number(char *str, int size, int number);
static void hide_text();
static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void show_text();
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void update_watch();

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *tup;

  tup = dict_find(iter, MESSAGE_KEY_PhoneBattery);
  if(tup) {
    phone_battery = tup->value->int32;
  }

  tup = dict_find(iter, MESSAGE_KEY_PhoneBatteryCharging);
  if(tup) {
    phone_battery_charging = tup->value->int32 == 1;
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction)
{
  show_text();
}

int main(void)
{
  setlocale(LC_ALL, "");

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers)
  {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_set_background_color(s_main_window, GColorBlack);
  window_stack_push(s_main_window, true);
  bluetooth_callback(connection_service_peek_pebble_app_connection());

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });

  app_message_open(64, 64);
  app_message_register_inbox_received(inbox_received_callback);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  steps_day_average = STEPS_DEFAULT;

  battery_callback(battery_state_service_peek());
  battery_state_service_subscribe(battery_callback);
  accel_tap_service_subscribe(accel_tap_handler);

  update_watch();

  app_event_loop();

  window_destroy(s_main_window);
  app_sync_deinit(&s_sync);
}

static void main_window_load(Window *window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);

  top_middle = text_layer_create(GRect(0, 56, 144, 40));
  text_layer_set_font(top_middle, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS));
  add_text_layer(window_layer, top_middle, GTextAlignmentCenter);

  // Create clock Layer
  clock_layer = layer_create(GRect(0, 20, 144, 126));
  layer_set_update_proc(clock_layer, clock_update_proc);
  layer_add_child(window_layer, clock_layer);

  battery_layer = layer_create(GRect(104, 0, 144, 20));
  layer_set_update_proc(battery_layer, battery_layer_update_proc);
  layer_add_child(window_layer, battery_layer);

  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_LECO_15));

  top_left = text_layer_create(GRect(0, 0, 144, 20));
  text_layer_set_font(top_left, s_time_font);
  add_text_layer(window_layer, top_left, GTextAlignmentLeft);

  top_right = text_layer_create(GRect(0, 0, 144, 20));
  text_layer_set_font(top_right, s_time_font);
  add_text_layer(window_layer, top_right, GTextAlignmentRight);

  bottom_left = text_layer_create(GRect(0, 146, 144, 20));
  text_layer_set_font(bottom_left, s_time_font);
  add_text_layer(window_layer, bottom_left, GTextAlignmentLeft);

  bottom_right = text_layer_create(GRect(0, 146, 144, 20));
  text_layer_set_font(bottom_right, s_time_font);
  add_text_layer(window_layer, bottom_right, GTextAlignmentRight);
}

static void main_window_unload(Window *window)
{
  text_layer_destroy(top_middle);
  text_layer_destroy(top_left);
  text_layer_destroy(top_right);
  text_layer_destroy(bottom_left);
  text_layer_destroy(bottom_right);
  layer_destroy(clock_layer);
  fonts_unload_custom_font(s_time_font);
}

static void battery_callback(BatteryChargeState state)
{
  battery_level = state.charge_percent;
  battery_charging = state.is_charging;
  layer_mark_dirty(battery_layer);
}

static void bluetooth_callback(bool connected)
{
  bluetooth_connected = connected;
  layer_mark_dirty(clock_layer);

  if(!connected)
    vibes_double_pulse();
}

static void battery_layer_update_proc(Layer *layer, GContext *ctx)
{
  if (battery_level > 60) { // draw first dot
    graphics_context_set_fill_color(ctx, GColorBlueMoon);
    graphics_fill_circle(ctx, GPoint(5, 10), 3);
  } else {
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_circle(ctx, GPoint(5, 10), 1);
  }
  if (battery_level > 40) { // draw second dot
    graphics_context_set_fill_color(ctx, GColorBlueMoon);
    graphics_fill_circle(ctx, GPoint(15, 10), 3);
  } else {
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_circle(ctx, GPoint(15, 10), 1);
  }
  if (battery_level > 20) { // draw third dot
    graphics_context_set_fill_color(ctx, GColorBlueMoon);
    graphics_fill_circle(ctx, GPoint(25, 10), 3);
  } else {
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_circle(ctx, GPoint(25, 10), 1);
  }
  if (battery_level > 0) { // draw fourth dot
    graphics_context_set_fill_color(ctx, GColorBlueMoon);
    graphics_fill_circle(ctx, GPoint(35, 10), 3);
  } else {
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_circle(ctx, GPoint(35, 10), 1);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  update_watch();
}

static void clock_update_proc(Layer *layer, GContext *ctx)
{
  // draw background
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, bluetooth_connected ? GColorDarkGray : GColorRed); //GColorBlueMoon
  draw_line(ctx, 0, true);
  draw_line(ctx, 1, true);
  draw_line(ctx, 2, true);
  draw_line(ctx, 3, true);
  draw_line(ctx, 4, true);
  draw_line(ctx, 5, true);
  draw_line(ctx, 6, true);
  draw_line(ctx, 7, true);
  draw_line(ctx, 8, true);
  draw_line(ctx, 9, true);
  draw_line(ctx, 10, true);
  draw_line(ctx, 11, true);

  graphics_context_set_stroke_width(ctx, 7);

  // draw minute
  graphics_context_set_stroke_color(ctx, GColorFashionMagenta);
  draw_line(ctx, minute_hand, false);

  // draw hour
  graphics_context_set_stroke_color(ctx, GColorVividCerulean);
  draw_line(ctx, hour_hand, true);


  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, 15, 0, (1.0*current_steps/steps_day_average) * DEG_TO_TRIGANGLE(360));
}

static void draw_line(GContext *ctx, int line, bool inner)
{
  switch (line) {
    case 1: graphics_draw_line(ctx, GPoint(R1OX, R1OY), inner ? GPoint(R1IX, R1IY) : GPoint(R1MX, R1MY)); break;
    case 2: graphics_draw_line(ctx, GPoint(R2OX, R2OY), inner ? GPoint(R2IX, R2IY) : GPoint(R2MX, R2MY)); break;
    case 3: graphics_draw_line(ctx, GPoint(R3OX, R3OY), inner ? GPoint(R3IX, R3IY) : GPoint(R3MX, R3MY)); break;
    case 4: graphics_draw_line(ctx, GPoint(R4OX, R4OY), inner ? GPoint(R4IX, R4IY) : GPoint(R4MX, R4MY)); break;
    case 5: graphics_draw_line(ctx, GPoint(R5OX, R5OY), inner ? GPoint(R5IX, R5IY) : GPoint(R5MX, R5MY)); break;
    case 6: graphics_draw_line(ctx, GPoint(R6OX, R6OY), inner ? GPoint(R6IX, R6IY) : GPoint(R6MX, R6MY)); break;
    case 7: graphics_draw_line(ctx, GPoint(R7OX, R7OY), inner ? GPoint(R7IX, R7IY) : GPoint(R7MX, R7MY)); break;
    case 8: graphics_draw_line(ctx, GPoint(R8OX, R8OY), inner ? GPoint(R8IX, R8IY) : GPoint(R8MX, R8MY)); break;
    case 9: graphics_draw_line(ctx, GPoint(R9OX, R9OY), inner ? GPoint(R9IX, R9IY) : GPoint(R9MX, R9MY)); break;
    case 10: graphics_draw_line(ctx, GPoint(R10OX, R10OY), inner ? GPoint(R10IX, R10IY) : GPoint(R10MX, R10MY)); break;
    case 11: graphics_draw_line(ctx, GPoint(R11OX, R11OY), inner ? GPoint(R11IX, R11IY) : GPoint(R11MX, R11MY)); break;
    case 0:
    default: graphics_draw_line(ctx, GPoint(R0OX, R0OY), inner ? GPoint(R0IX, R0IY) : GPoint(R0MX, R0MY)); break;
  }
}

static void update_watch()
{
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static int th,tm,c,m;

  m = tick_time->tm_min;

  strftime(s_time, sizeof(s_time), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  strftime(date_buffer, sizeof(date_buffer), "%a, %d %b", tick_time);

  th = tick_time->tm_hour % 12;
  if (m < 57) {
    c = m % 5;
    tm = (m < 55) ? (c >= 3 ? m - c + 5 : m - c) / 5 : 11;
  } else {
    th = (th + 1) % 12;
    tm = 0;
  }

  if (th != hour_hand || tm != minute_hand) {
    hour_hand = th;
    minute_hand = tm;
    layer_mark_dirty(clock_layer);
  }


  current_steps = (int)health_service_sum_today(HealthMetricStepCount);

  format_number(steps_buffer, sizeof(steps_buffer), current_steps);
  text_layer_set_text(top_left, date_buffer);
  text_layer_set_text(top_middle, s_time);
  if (!flick_showing) {
    text_layer_set_text(bottom_left, steps_buffer);
  }
}

static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  text_layer_set_text_alignment(text_layer, alignment);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void show_text()
{
  if (flick_showing) return;
  flick_showing = true;

  int start = time_start_of_today();

  steps_day_average = (int)health_service_sum_averaged(HealthMetricStepCount, start, start + SECONDS_PER_DAY, HealthServiceTimeScopeDailyWeekdayOrWeekend);
  if (steps_day_average < 1) steps_day_average = STEPS_DEFAULT;

  // steps_average_now = (int)health_service_sum_averaged(HealthMetricStepCount, start, time(NULL), HealthServiceTimeScopeDailyWeekdayOrWeekend);
  // if (steps_average_now < 1) steps_average_now = STEPS_DEFAULT;

  format_number(steps_buffer, sizeof(steps_buffer), current_steps);
  format_number(steps_avg_buffer, sizeof(steps_avg_buffer), steps_day_average);
  static char tmp_str[25];
  snprintf(tmp_str, sizeof(tmp_str), "%s/%s", steps_buffer, steps_avg_buffer);
  text_layer_set_text(bottom_left, tmp_str);
  app_timer_register(10000, hide_text, NULL);

  if (phone_battery > -1) {
    snprintf(battery_buffer, sizeof(battery_buffer), "%s%d/%s%d", battery_charging ? "+" : "", battery_level, phone_battery_charging ? "+" : "", phone_battery);
  } else {
    snprintf(battery_buffer, sizeof(battery_buffer), "%s%d", battery_charging ? "+" : "", battery_level);
  }
  text_layer_set_text(bottom_right, battery_buffer);
}

static void hide_text()
{
  flick_showing = false;
  text_layer_set_text(bottom_right, NULL);

  format_number(steps_buffer, sizeof(steps_buffer), current_steps);
  text_layer_set_text(bottom_left, steps_buffer);
}

static void format_number(char *str, int size, int number)
{
  if (number > 1000)
    snprintf(str, size, "%d.%dk", number / 1000, (number % 1000) / 100);
  else
    snprintf(str, size, "%d", number);
}