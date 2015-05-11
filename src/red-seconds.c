// Copyright (c) 2015 Marcus Fritzsch
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <pebble.h>

typedef struct {
   char day[3];
   uint8_t mon;
   uint8_t dow;
} DateInfo;

static Window *window;
static Layer *effect_layer;
static GRect bounds;
static GPoint center;
static GFont date_font;
static GFont day_font;
static GBitmap *bg;
static BitmapLayer *bg_layer;

static char *day_of_week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static DateInfo date_info;

static void update_date_info(struct tm *tm) {
   strftime(date_info.day, sizeof date_info.day, "%d", tm);
   date_info.mon = (uint8_t) tm->tm_mon;
   date_info.dow = (uint8_t) tm->tm_wday;
}

typedef struct {
   int32_t width;
   int32_t main_len;
   int32_t tail_len;
   int32_t angle;
   GPoint center;
   GColor color;
} HandInfo;

static void draw_simple_hand(GContext *ctx, HandInfo hi) {
   int32_t sin = sin_lookup(hi.angle);
   int32_t cos = cos_lookup(hi.angle);

   GPoint p0 = {
       sin * hi.main_len / TRIG_MAX_RATIO + center.x,
      -cos * hi.main_len / TRIG_MAX_RATIO + center.y,
   };

   GPoint p1 = {
      -sin * hi.tail_len / TRIG_MAX_RATIO + center.x,
       cos * hi.tail_len / TRIG_MAX_RATIO + center.y,
   };

   graphics_context_set_stroke_width(ctx, hi.width);
   graphics_context_set_stroke_color(ctx, hi.color);
   graphics_draw_line(ctx, p0, p1);

   // draw another tick on the hand for better visibility
   graphics_context_set_fill_color(ctx, GColorWhite);
   graphics_fill_circle(ctx, p0, 1);
}

static void update_effect_layer(Layer *l, GContext *ctx) {
   time_t t = time(NULL);
   struct tm *tm = localtime(&t);
   int32_t sec = tm->tm_sec;
   int32_t min = tm->tm_min;
   int32_t hr = tm->tm_hour;
   graphics_context_set_stroke_width(ctx, 1);

   // draw date
   graphics_context_set_text_color(ctx, GColorLightGray);

   // origin is also dependent on text rectangle width,
   // shifting y coordingate up by 1 for proper centering, easier on the eye
   GPoint date_origin = {144 / 3 * 2 - 2, 168 / 2 - 1};
   graphics_draw_text(ctx, day_of_week[date_info.dow], date_font,
                      GRect(date_origin.x, date_origin.y - 10 - 14, 30, 14),
                      GTextOverflowModeFill, GTextAlignmentCenter, NULL);
   graphics_draw_text(ctx, month[date_info.mon], date_font,
                      GRect(date_origin.x, date_origin.y + 10, 30, 14),
                      GTextOverflowModeFill, GTextAlignmentCenter, NULL);

   graphics_context_set_text_color(ctx, GColorRajah);
   graphics_draw_text(ctx, date_info.day, day_font,
                      GRect(date_origin.x, date_origin.y - 10, 30, 20),
                      GTextOverflowModeFill, GTextAlignmentCenter, NULL);

   // draw hands
   draw_simple_hand(ctx,
         (HandInfo){
            .width = 3,
            .main_len = 144 / 2 - 30,
            .tail_len = 7,
            .angle = TRIG_MAX_ANGLE * (hr * 60 + min) / (12 * 60),
            .center = center,
            .color = GColorDarkGray});

   draw_simple_hand(ctx,
         (HandInfo){
            .width = 3,
            .main_len = 144 / 2 - 15,
            .tail_len = 9,
#ifdef SUBMINUTE_MINUTE_HAND
            .angle = TRIG_MAX_ANGLE * (min * 60 + sec) / 3600,
#else
            .angle = TRIG_MAX_ANGLE * min / 60,
#endif
            .center = center,
            .color = GColorDarkGray});

   draw_simple_hand(ctx,
         (HandInfo){
            .width = 3,
            .main_len = 144 / 2 - 10,
            .tail_len = 11,
            .angle = TRIG_MAX_ANGLE * sec / 60,
            .center = center,
            .color = GColorDarkCandyAppleRed});

   // center screw...
   graphics_context_set_fill_color(ctx, GColorWhite);
   graphics_fill_circle(ctx, center, 3);

   layer_mark_dirty(l);
}

static void window_load(Window *window) {
   Layer *window_layer = window_get_root_layer(window);
   bounds = layer_get_bounds(window_layer);
   center = grect_center_point(&bounds);
   bg = gbitmap_create_with_resource(RESOURCE_ID_BG);

   bg_layer = bitmap_layer_create(bounds);
   Layer *bgl = bitmap_layer_get_layer(bg_layer);
   bitmap_layer_set_bitmap(bg_layer, bg);
   bitmap_layer_set_compositing_mode(bg_layer, GCompOpSet);
   layer_add_child(window_layer, bgl);

   effect_layer = layer_create(bounds);
   layer_set_update_proc(effect_layer, update_effect_layer);
   layer_insert_above_sibling(effect_layer, bgl);
}

static void window_unload(Window *window) {
   bitmap_layer_destroy(bg_layer);
   gbitmap_destroy(bg);
   layer_destroy(effect_layer);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
   if (units_changed & DAY_UNIT) {
      update_date_info(tick_time);
   }
   layer_mark_dirty(bitmap_layer_get_layer(bg_layer));
   layer_mark_dirty(effect_layer);
}

static void init(void) {
   window = window_create();
   window_set_window_handlers(window,
                              (WindowHandlers){
                                 .load = window_load, .unload = window_unload,
                              });

   date_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
   day_font = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);

   time_t t = time(NULL);
   struct tm *tm = localtime(&t);
   update_date_info(tm);
   window_stack_push(window, true);
   tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}

static void deinit(void) { window_destroy(window); }

int main(void) {
   init();

   APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

   app_event_loop();
   deinit();
}
