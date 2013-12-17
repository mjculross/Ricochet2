/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* *                                                                 * */
/* *                          Ricochet2                              * */
/* *                                                                 * */
/* *      A watchface/app where the date and time bounce around      * */
/* *           & ricochet off of each other and the walls            * */
/* *                                                                 * */
/* *                 [ SDK 2.0 compatible version ]                  * */
/* *                                                                 * */
/* *                    by Mark J Culross, KD5RXT                    * */
/* *                                                                 * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include <pebble.h>

// This is a custom defined key for saving the night_enabled flag
#define PKEY_NIGHT_ENABLED 21359
#define NIGHT_ENABLED_DEFAULT false

// This is a custom defined key for saving the clock_24h_style flag
#define PKEY_CLOCK_24H_STYLE 13592
#define CLOCK_24H_STYLE_DEFAULT false

// This is a custom defined key for saving the date_month_first flag
#define PKEY_DATE_MONTH_FIRST 35921
#define DATE_MONTH_FIRST_DEFAULT true

// This is a custom defined key for saving the time_on_top flag
#define PKEY_TIME_ON_TOP 59213
#define TIME_ON_TOP_DEFAULT false

#define TOTAL_TIME_DIGITS 6
#define TOTAL_DATE_DIGITS 8
#define TOTAL_BATT_DIGITS 4

static Window *window;
Layer *window_layer;

static GBitmap *time_digits_image[TOTAL_TIME_DIGITS];
static GBitmap *day_image;
static GBitmap *date_image[TOTAL_DATE_DIGITS];
static GBitmap *splash_image;
static GBitmap *batt_image[TOTAL_BATT_DIGITS];

static BitmapLayer *time_layer;

bool night_enabled = NIGHT_ENABLED_DEFAULT;
bool clock_24h_style = CLOCK_24H_STYLE_DEFAULT;
bool date_month_first = DATE_MONTH_FIRST_DEFAULT;
bool time_on_top = TIME_ON_TOP_DEFAULT;
bool light_on = false;

int splash_timer = 3;
int freeze_timer = 4;

int time_x_max = 0;

int time_x_delta = 2;
int time_y_delta = 3;

int time_x_offset = 0;
int time_y_offset = 0;

int date_x_max = 104;

int date_x_delta = -3;
int date_y_delta = -2;

int date_x_offset = 0;
int date_y_offset = 0;

BatteryChargeState batt_state;


const int BIG_DIGIT_IMAGE_RESOURCE_IDS[] =
{
   RESOURCE_ID_IMAGE_NUM_0,
   RESOURCE_ID_IMAGE_NUM_1,
   RESOURCE_ID_IMAGE_NUM_2,
   RESOURCE_ID_IMAGE_NUM_3,
   RESOURCE_ID_IMAGE_NUM_4,
   RESOURCE_ID_IMAGE_NUM_5,
   RESOURCE_ID_IMAGE_NUM_6,
   RESOURCE_ID_IMAGE_NUM_7,
   RESOURCE_ID_IMAGE_NUM_8,
   RESOURCE_ID_IMAGE_NUM_9,
};


const int DATENUM_IMAGE_RESOURCE_IDS[] =
{
   RESOURCE_ID_IMAGE_DATENUM_0,
   RESOURCE_ID_IMAGE_DATENUM_1,
   RESOURCE_ID_IMAGE_DATENUM_2,
   RESOURCE_ID_IMAGE_DATENUM_3,
   RESOURCE_ID_IMAGE_DATENUM_4,
   RESOURCE_ID_IMAGE_DATENUM_5,
   RESOURCE_ID_IMAGE_DATENUM_6,
   RESOURCE_ID_IMAGE_DATENUM_7,
   RESOURCE_ID_IMAGE_DATENUM_8,
   RESOURCE_ID_IMAGE_DATENUM_9,
};


const int DAY_IMAGE_RESOURCE_IDS[] =
{
   RESOURCE_ID_IMAGE_DAY_SUN,
   RESOURCE_ID_IMAGE_DAY_MON,
   RESOURCE_ID_IMAGE_DAY_TUE,
   RESOURCE_ID_IMAGE_DAY_WED,
   RESOURCE_ID_IMAGE_DAY_THU,
   RESOURCE_ID_IMAGE_DAY_FRI,
   RESOURCE_ID_IMAGE_DAY_SAT,
};



static void click_config_provider(void *context);
static void deinit(void);
static void down_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void handle_accel_tap(AccelAxisType axis, int32_t direction);
static void handle_second_tick(struct tm *tick, TimeUnits units_changed);
static void init(void);
static void select_long_click_handler(ClickRecognizerRef recognizer, void *context);
static void select_long_release_handler(ClickRecognizerRef recognizer, void *context);
static void select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void set_bitmap_image(GContext *ctx, GBitmap **bmp_image, const int resource_id, GPoint this_origin, bool invert);
static void up_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void update_date(Layer *layer, GContext *ctx);
static void update_display(Layer *layer, GContext *ctx);
static void update_moves(void);
static void update_time(Layer *layer, GContext *ctx);



static void click_config_provider(void *context)
{
   window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
   window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
   window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
   window_long_click_subscribe(BUTTON_ID_SELECT, 250, select_long_click_handler, select_long_release_handler);
}  // click_config_provider()


static void deinit(void)
{
   // Save all settings into persistent storage on app exit
   persist_write_int(PKEY_NIGHT_ENABLED, night_enabled);
   persist_write_int(PKEY_CLOCK_24H_STYLE, clock_24h_style);
   persist_write_int(PKEY_DATE_MONTH_FIRST, date_month_first);
   persist_write_int(PKEY_TIME_ON_TOP, time_on_top);

   layer_remove_from_parent(bitmap_layer_get_layer(time_layer));
   bitmap_layer_destroy(time_layer);

   for (int i = 0; i < TOTAL_TIME_DIGITS; i++)
   {
      gbitmap_destroy(time_digits_image[i]);
   }

   for (int i = 0; i < TOTAL_DATE_DIGITS; i++)
   {
      gbitmap_destroy(date_image[i]);
   }

   for (int i = 0; i < TOTAL_BATT_DIGITS; i++)
   {
      gbitmap_destroy(batt_image[i]);
   }

   gbitmap_destroy(day_image);

   gbitmap_destroy(splash_image);

   tick_timer_service_unsubscribe();
   accel_tap_service_unsubscribe();
   window_destroy(window);
}  // deinit()


static void down_single_click_handler(ClickRecognizerRef recognizer, void *context)
{
   if (splash_timer == 0)
   {
      clock_24h_style = !clock_24h_style;

      // Save clock_24h_style setting into persistent storage
      persist_write_int(PKEY_CLOCK_24H_STYLE, clock_24h_style);

      if (clock_24h_style)
      {
         time_x_max = 93;
      }
      else
      {
         time_x_max = 103;
      }

      layer_mark_dirty(window_layer);
   }
}  // down_single_click_handler()


static void handle_accel_tap(AccelAxisType axis, int32_t direction)
{
   freeze_timer = 4;
   splash_timer = 0;

   light_on = !light_on;

   if (light_on)
   {
      light_enable(true);
   }
   else
   {
      light_enable(false);
   }

   layer_mark_dirty(window_layer);
}  // accel_tap_handler()


void handle_second_tick(struct tm *tick_time, TimeUnits units_changed)
{
   if (splash_timer > 0)
   {
      splash_timer--;
   }
   else
   {
      if (freeze_timer > 0)
      {
         freeze_timer--;

         if (freeze_timer == 0)
         {
            light_enable(false);
            light_on = false;
         }
      }
   }

   layer_mark_dirty(window_layer);
}  // handle_second_tick()


static void init(void)
{
   GRect dummy_frame = { {0, 0}, {0, 0} };

   window = window_create();
   if (window == NULL)
   {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "...couldn't allocate window memory...");
      return;
   }

   window_layer = window_get_root_layer(window);

   // Get 24-hour setting from persistent storage, else use global clock_is_24h_style setting
   if (persist_exists(PKEY_CLOCK_24H_STYLE))
   {
      clock_24h_style = persist_read_int(PKEY_CLOCK_24H_STYLE);
   }
   else
   {
      if (clock_is_24h_style())
      {
         clock_24h_style = true;
      }
      else
      {
         clock_24h_style = false;
      }
   }

   if (clock_24h_style)
   {
      time_x_max = 93;
   }
   else
   {
      time_x_max = 103;
   }

   // Get all settings from persistent storage for use if they exist, otherwise use the default
   night_enabled = persist_exists(PKEY_NIGHT_ENABLED) ? persist_read_int(PKEY_NIGHT_ENABLED) : NIGHT_ENABLED_DEFAULT;
   date_month_first = persist_exists(PKEY_DATE_MONTH_FIRST) ? persist_read_int(PKEY_DATE_MONTH_FIRST) : DATE_MONTH_FIRST_DEFAULT;
   time_on_top = persist_exists(PKEY_TIME_ON_TOP) ? persist_read_int(PKEY_TIME_ON_TOP) : TIME_ON_TOP_DEFAULT;

   window_set_fullscreen(window, true);
   window_stack_push(window, true /* Animated */);

   window_set_click_config_provider(window, click_config_provider);
   layer_set_update_proc(window_layer, update_display);

   splash_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPLASH);
 
   day_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DAY_SUN);
 
   for (int i = 0; i < TOTAL_BATT_DIGITS; i++)
   {
      batt_image[i] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DATENUM_0); 
   }

   for (int i = 0; i < TOTAL_DATE_DIGITS; i++)
   {
      date_image[i] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DATENUM_0); 
   }

   for (int i = 0; i < TOTAL_TIME_DIGITS; i++)
   {
      time_digits_image[i] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NUM_0); 
   }

   time_layer = bitmap_layer_create(dummy_frame);
   layer_add_child(window_layer, bitmap_layer_get_layer(time_layer));

   accel_tap_service_subscribe(&handle_accel_tap);
   tick_timer_service_subscribe(SECOND_UNIT, &handle_second_tick);
}  // init()


static void select_long_click_handler(ClickRecognizerRef recognizer, void *context)
{
   if (splash_timer == 0)
   {
      night_enabled = !night_enabled;

      // Save night_enabled setting into persistent storage
      persist_write_int(PKEY_NIGHT_ENABLED, night_enabled);

      layer_mark_dirty(window_layer);
   }
}  // select_long_click_handler()


static void select_long_release_handler(ClickRecognizerRef recognizer, void *context)
{
}  // select_long_release_handler()


static void select_single_click_handler(ClickRecognizerRef recognizer, void *context)
{
   if (splash_timer == 0)
   {
      if (freeze_timer == 4)
      {
         time_on_top = !time_on_top;

         // Save time_on_top setting into persistent storage
         persist_write_int(PKEY_TIME_ON_TOP, time_on_top);

         light_on = true;
         light_enable(true);
      }
      else
      {
         freeze_timer = 4;

         light_on = !light_on;

         if (light_on)
         {
            light_enable(true);
         }
         else
         {
            light_enable(false);
         }
      }
   }
   else
   {
      splash_timer = 0;

      freeze_timer = 4;

      light_on = !light_on;

      if (light_on)
      {
         light_enable(true);
      }
      else
      {
         light_enable(false);
      }
   }

   layer_mark_dirty(window_layer);
}  // select_single_click_handler()


static void set_bitmap_image(GContext *ctx, GBitmap **bmp_image, const int resource_id, GPoint this_origin, bool invert)
{
   gbitmap_destroy(*bmp_image);

   *bmp_image = gbitmap_create_with_resource(resource_id);

   GRect frame = (GRect)
   {
      .origin = this_origin,
      .size = (*bmp_image)->bounds.size
   };

   if (invert)
   {
      graphics_context_set_compositing_mode(ctx, GCompOpAssignInverted);
   }
   else
   {
      graphics_context_set_compositing_mode(ctx, GCompOpAssign);
   }

   graphics_draw_bitmap_in_rect(ctx, *bmp_image, frame);

   layer_mark_dirty(window_layer);
}  // set_bitmap_image()


static void up_single_click_handler(ClickRecognizerRef recognizer, void *context)
{
   if (splash_timer == 0)
   {
      date_month_first = !date_month_first;

      // Save date_month_first setting into persistent storage
      persist_write_int(PKEY_DATE_MONTH_FIRST, date_month_first);

      layer_mark_dirty(window_layer);
   }
}  // up_single_click_handler()


static void update_date(Layer *layer, GContext *ctx)
{
   time_t t = time(NULL);
   struct tm *current_time = localtime(&t);

   if (freeze_timer > 0)
   {
      if (time_on_top)
      {
         date_x_offset = 20;
         date_y_offset = 75;
      }
      else
      {
         date_x_offset = 20;
         date_y_offset = 10;
      }
   }

   // display date
   set_bitmap_image(ctx, &day_image, DAY_IMAGE_RESOURCE_IDS[current_time->tm_wday], GPoint(date_x_offset, date_y_offset), night_enabled);

   batt_state = battery_state_service_peek();
   if (batt_state.charge_percent < 100)
   {
      if (batt_state.is_charging)
      {
         set_bitmap_image(ctx, &batt_image[0], RESOURCE_ID_IMAGE_DATENUM_PLUS, GPoint(date_x_offset+52, date_y_offset), night_enabled);
      }
      else
      {
         set_bitmap_image(ctx, &batt_image[0], RESOURCE_ID_IMAGE_DATENUM_BLANK, GPoint(date_x_offset+52, date_y_offset), night_enabled);
      }
   }
   else
   {
      set_bitmap_image(ctx, &batt_image[0], RESOURCE_ID_IMAGE_DATENUM_1, GPoint(date_x_offset+52, date_y_offset), night_enabled);
   }

   batt_state.charge_percent %= 100;

   if (batt_state.charge_percent < 10)
   {
      set_bitmap_image(ctx, &batt_image[1], RESOURCE_ID_IMAGE_DATENUM_BLANK, GPoint(date_x_offset+65, date_y_offset), night_enabled);
   }
   else
   {
      set_bitmap_image(ctx, &batt_image[1], DATENUM_IMAGE_RESOURCE_IDS[batt_state.charge_percent / 10], GPoint(date_x_offset+65, date_y_offset), night_enabled);
   }

   set_bitmap_image(ctx, &batt_image[2], DATENUM_IMAGE_RESOURCE_IDS[batt_state.charge_percent % 10], GPoint(date_x_offset+78, date_y_offset), night_enabled);
   set_bitmap_image(ctx, &batt_image[3], RESOURCE_ID_IMAGE_DATENUM_PERCENT, GPoint(date_x_offset+91, date_y_offset), night_enabled);

   if (date_month_first)
   {
      set_bitmap_image(ctx, &date_image[0], DATENUM_IMAGE_RESOURCE_IDS[(current_time->tm_mon + 1) / 10], GPoint(date_x_offset, date_y_offset + 23), night_enabled);
      set_bitmap_image(ctx, &date_image[1], DATENUM_IMAGE_RESOURCE_IDS[(current_time->tm_mon + 1) % 10], GPoint(date_x_offset + 13, date_y_offset + 23), night_enabled);
      set_bitmap_image(ctx, &date_image[3], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday / 10], GPoint(date_x_offset + 39, date_y_offset + 23), night_enabled);
      set_bitmap_image(ctx, &date_image[4], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday % 10], GPoint(date_x_offset + 52, date_y_offset + 23), night_enabled);
   }
   else
   {
      set_bitmap_image(ctx, &date_image[3], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday / 10], GPoint(date_x_offset, date_y_offset + 23), night_enabled);
      set_bitmap_image(ctx, &date_image[4], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_mday % 10], GPoint(date_x_offset + 13, date_y_offset + 23), night_enabled);
      set_bitmap_image(ctx, &date_image[0], DATENUM_IMAGE_RESOURCE_IDS[(current_time->tm_mon + 1) / 10], GPoint(date_x_offset + 39, date_y_offset + 23), night_enabled);
      set_bitmap_image(ctx, &date_image[1], DATENUM_IMAGE_RESOURCE_IDS[(current_time->tm_mon + 1) % 10], GPoint(date_x_offset + 52, date_y_offset + 23), night_enabled);
   }

   set_bitmap_image(ctx, &date_image[6], DATENUM_IMAGE_RESOURCE_IDS[(current_time->tm_year / 10) % 10], GPoint(date_x_offset + 78, date_y_offset + 23), night_enabled);
   set_bitmap_image(ctx, &date_image[7], DATENUM_IMAGE_RESOURCE_IDS[current_time->tm_year % 10], GPoint(date_x_offset + 91, date_y_offset + 23), night_enabled);
   set_bitmap_image(ctx, &date_image[2], RESOURCE_ID_IMAGE_DATENUM_SLASH, GPoint(date_x_offset + 26, date_y_offset + 23), night_enabled);
   set_bitmap_image(ctx, &date_image[5], RESOURCE_ID_IMAGE_DATENUM_SLASH, GPoint(date_x_offset + 65, date_y_offset + 23), night_enabled);
}  // update_date()


static void update_display(Layer *layer, GContext *ctx)
{
   if (splash_timer == 0)
   {
      if (freeze_timer == 0)
      {
         update_moves();
      }

      set_bitmap_image(ctx, &splash_image, RESOURCE_ID_IMAGE_WHITE_BACK, GPoint (0, 0), night_enabled);
      update_date(layer, ctx);
      update_time(layer, ctx);
   }
   else
   {
      set_bitmap_image(ctx, &splash_image, RESOURCE_ID_IMAGE_SPLASH, GPoint (0, 0), night_enabled);
   }
}  // update_display()


static void update_moves(void)
{
   date_x_offset += date_x_delta;
   date_y_offset += date_y_delta;

   time_x_offset += time_x_delta;
   time_y_offset += time_y_delta;

   // total date field is 104w x 39h
   if ((date_x_offset + date_x_delta) < 0)
   {
      // generate a pseudo random number from 2, 4, & 6
      date_x_delta = ((rand() % 3) + 1) * 2;
   }
   else
   {
      if ((date_x_offset + date_x_delta + date_x_max) >= 143)
      {
         // generate a pseudo random number from -2, -4, & -6
         date_x_delta = ((rand() % 3) + 1) * 2;
         date_x_delta = -date_x_delta;
      }
   }

   // total time field is 103w x 52h for 12-hour clock & 93w x 52h for 24-hour clock
   if ((time_x_offset + time_x_delta) < 0)
   {
      // generate a pseudo random number from 2, 4, & 6
      time_x_delta = ((rand() % 3) + 1) * 2;
   }
   else
   {
      if ((time_x_offset + time_x_delta + time_x_max) >= 143)
      {
         // generate a pseudo random number from -2, -4, & -6
         time_x_delta = ((rand() % 3) + 1) * 2;
         time_x_delta = -time_x_delta;
      }
   }

   if (time_on_top == true)
   {
      if ((time_y_offset + time_y_delta) < 0)
      {
         // generate a pseudo random number from 3, 6, & 9
         time_y_delta = ((rand() % 3) + 1) * 3;
      }

      if ((date_y_offset + date_y_delta + 39) >= 168)
      {
         // generate a pseudo random number from -4, -8, & -12
         date_y_delta = ((rand() % 3) + 1) * 4;
         date_y_delta = -date_y_delta;
      }

      if (((date_y_offset + date_y_delta) - (time_y_offset + time_y_delta)) <= 52)
      {
         // generate a pseudo random number from -3, -6, & -9
         time_y_delta = ((rand() % 3) + 1) * 3;
         time_y_delta = -time_y_delta;

         // generate a pseudo random number from 4, 8, & 12
         date_y_delta = ((rand() % 3) + 1) * 4;
      }
   }
   else
   {
      if ((date_y_offset + date_y_delta) < 0)
      {
         // generate a pseudo random number from 4, 8, & 12
         date_y_delta = ((rand() % 3) + 1) * 4;
      }

      if ((time_y_offset + time_y_delta + 52) >= 168)
      {
         // generate a pseudo random number from -3, -6, & -9
         time_y_delta = ((rand() % 3) + 1) * 3;
         time_y_delta = -time_y_delta;
      }

      if (((time_y_offset + time_y_delta) - (date_y_offset + date_y_delta)) <= 39)
      {
         // generate a pseudo random number from 3, 6, & 9
         time_y_delta = ((rand() % 3) + 1) * 3;

         // generate a pseudo random number from -4, -8, & -12
         date_y_delta = ((rand() % 3) + 1) * 4;
         date_y_delta = -date_y_delta;
      }
   }
}  // update_moves()


static void update_time(Layer *layer, GContext *ctx)
{
   time_t t = time(NULL);
   struct tm *current_time = localtime(&t);

   if (freeze_timer > 0)
   {
      if (time_on_top)
      {
         time_x_offset = 20;
         time_y_offset = 10;
      }
      else
      {
         time_x_offset = 20;
         time_y_offset = 75;
      }
   }

   // display time hour
   if (clock_24h_style)
   {
      set_bitmap_image(ctx, &time_digits_image[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_hour / 10], GPoint(time_x_offset, time_y_offset), night_enabled);
      set_bitmap_image(ctx, &time_digits_image[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_hour % 10], GPoint(21 + time_x_offset, time_y_offset), night_enabled);

      // display blank in place of AM/PM
      set_bitmap_image(ctx, &time_digits_image[5], RESOURCE_ID_IMAGE_BLANK_MODE, GPoint(93 + time_x_offset, time_y_offset), night_enabled);
   }
   else
   {
      // display AM/PM
      if (current_time->tm_hour >= 12)
      {
         set_bitmap_image(ctx, &time_digits_image[5], RESOURCE_ID_IMAGE_PM_MODE, GPoint(93 + time_x_offset, time_y_offset), night_enabled);
      }
      else
      {
         set_bitmap_image(ctx, &time_digits_image[5], RESOURCE_ID_IMAGE_AM_MODE, GPoint(93 + time_x_offset, time_y_offset), night_enabled);
      }

      if ((current_time->tm_hour % 12) == 0)
      {
         set_bitmap_image(ctx, &time_digits_image[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[1], GPoint(time_x_offset, time_y_offset), night_enabled);
         set_bitmap_image(ctx, &time_digits_image[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[2], GPoint(21 + time_x_offset, time_y_offset), night_enabled);
      }
      else
      {
         set_bitmap_image(ctx, &time_digits_image[0], BIG_DIGIT_IMAGE_RESOURCE_IDS[(current_time->tm_hour % 12) / 10], GPoint(time_x_offset, time_y_offset), night_enabled);
         set_bitmap_image(ctx, &time_digits_image[1], BIG_DIGIT_IMAGE_RESOURCE_IDS[(current_time->tm_hour % 12) % 10], GPoint(21 + time_x_offset, time_y_offset), night_enabled);

         if ((current_time->tm_hour % 12) < 10)
         {
            set_bitmap_image(ctx, &time_digits_image[0], RESOURCE_ID_IMAGE_NUM_BLANK, GPoint(time_x_offset, time_y_offset), night_enabled);
         }
      }
   }

   // display colon & time minute
   set_bitmap_image(ctx, &time_digits_image[2], RESOURCE_ID_IMAGE_COLON, GPoint(42 + time_x_offset, time_y_offset), night_enabled);
   set_bitmap_image(ctx, &time_digits_image[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min / 10], GPoint(51 + time_x_offset, time_y_offset), night_enabled);
   set_bitmap_image(ctx, &time_digits_image[4], BIG_DIGIT_IMAGE_RESOURCE_IDS[current_time->tm_min % 10], GPoint(72 + time_x_offset, time_y_offset), night_enabled);
}  // update_time()


int main(void) {
   init();
   app_event_loop();
   deinit();
}









