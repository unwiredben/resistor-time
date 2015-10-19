// Resistor Time
// 
// a watchface based on ks-clock-face from https://github.com/pebble-examples/ks-clock-face
//
// this watchface draws a common carbon resistor with four bands representing the four
// digits of the 24 hour time.  Below the resistor, the time is also drawn in digits with
// an omega (the symbol for ohms), and the current date is indicated by the
// white silkscreen of Rmmdd

#include <pebble.h>
#include <inttypes.h>
    
// used for slots in AppSync and Persistant Storage
#define BACKGROUND_COLOR_KEY    0
#define SILKSCREEN_COLOR_KEY    1
    
static const GColor8 resistor_colors[10] = {
    { /* 0 */ .argb = GColorBlackARGB8 },
    { /* 1 */ .argb = GColorWindsorTanARGB8 },
    { /* 2 */ .argb = GColorRedARGB8 },
    { /* 3 */ .argb = GColorChromeYellowARGB8 },
    { /* 4 */ .argb = GColorYellowARGB8 },
    { /* 5 */ .argb = GColorGreenARGB8 },
    { /* 6 */ .argb = GColorBlueARGB8 },
    { /* 7 */ .argb = GColorVividVioletARGB8 },
    { /* 8 */ .argb = GColorLightGrayARGB8 },
    { /* 9 */ .argb = GColorWhiteARGB8 },
};

#define TEXT_HEIGHT  (24)
#if PBL_ROUND
    #define RECT_WIDTH  (180)
    #define RECT_HEIGHT (180)
    #define Y_OFFSET (20)
#else
    #define RECT_WIDTH  (144)
    #define RECT_HEIGHT (168)
    #define Y_OFFSET (4)
#endif

#define RESISTOR_BASE_X ((int)((RECT_WIDTH - 144) / 2))
#define RESISTOR_BASE_Y ((int)((RECT_HEIGHT - 43) / 2))
static const GRect bitmap_box  = {{0, RESISTOR_BASE_Y}, {RECT_WIDTH, 43}};
static const GRect date_box    = {{0, Y_OFFSET}, {RECT_WIDTH, TEXT_HEIGHT}};
static const GRect time_box    = {{0, RECT_HEIGHT - TEXT_HEIGHT - Y_OFFSET}, {RECT_WIDTH, TEXT_HEIGHT}};
static const GRect stripe1_box = {{RESISTOR_BASE_X + 29, RESISTOR_BASE_Y + 2}, {8, 39}};
static const GRect stripe2_box = {{RESISTOR_BASE_X + 54, RESISTOR_BASE_Y + 8}, {9, 27}};
static const GRect stripe3_box = {{RESISTOR_BASE_X + 70, RESISTOR_BASE_Y + 8}, {8, 27}};
static const GRect stripe4_box = {{RESISTOR_BASE_X + 85, RESISTOR_BASE_Y + 8}, {8, 27}};
    
static GColor8 pcb_background = { .argb = GColorKellyGreenARGB8 };
static GColor8 pcb_silkscreen = { .argb = GColorWhiteARGB8 };

static GFont s_ocra_font;
static GBitmap *s_resistor_img;

static struct tm s_last_time;
    
static Window *s_main_window;
static Layer *s_canvas_layer;

/********************************** CONFIG ************************************/

static const char *translate_error(AppMessageResult result) {
    switch (result) {
        case APP_MSG_OK: return "APP_MSG_OK";
        case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
        case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
        case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
        case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
        case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
        case APP_MSG_BUSY: return "APP_MSG_BUSY";
        case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
        case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
        case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
        case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
        case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
        case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
        case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
        default: return "UNKNOWN ERROR";
    }
}

static AppSync s_sync;
static uint8_t s_sync_buffer[32];

static void sync_changed_handler(const uint32_t key,
                                 const Tuple *new_tuple,
                                 const Tuple *old_tuple,
                                 void *context) {
    bool dirty = false;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "sync_changed: key %" PRIu32, key);
    switch (key) {
        case BACKGROUND_COLOR_KEY:
            if (pcb_background.argb != new_tuple->value->uint8) {
                pcb_background.argb = new_tuple->value->uint8;
                persist_write_int(BACKGROUND_COLOR_KEY, pcb_background.argb);
                dirty = true;
            }
            break;

        case SILKSCREEN_COLOR_KEY:
            if (pcb_silkscreen.argb != new_tuple->value->uint8) {
                pcb_silkscreen.argb = new_tuple->value->uint8;
                persist_write_int(SILKSCREEN_COLOR_KEY, pcb_silkscreen.argb);
                dirty = true;
            }
            break;

        default:
            // ignore unknown keys
            break;
    }
    if (dirty && s_canvas_layer) {
        layer_mark_dirty(s_canvas_layer);
    }
}

static void sync_error_handler(DictionaryResult dict_error,
                               AppMessageResult app_message_error, 
                               void *context) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "sync error: %s", translate_error(app_message_error));
}

static void config_init(void) {
    app_message_open(
        app_message_inbox_size_maximum(),
        app_message_outbox_size_maximum());

    Tuplet initial_values[] = {
        TupletInteger(BACKGROUND_COLOR_KEY, pcb_background.argb),
        TupletInteger(SILKSCREEN_COLOR_KEY, pcb_silkscreen.argb)
    };
    app_sync_init(
        &s_sync, s_sync_buffer, sizeof(s_sync_buffer), 
        initial_values, ARRAY_LENGTH(initial_values), 
        sync_changed_handler, sync_error_handler, NULL);
}

static void config_deinit(void) {
    app_sync_deinit(&s_sync);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
    // Store time
    s_last_time = *tick_time;

    // Redraw
    if (s_canvas_layer) {
        layer_mark_dirty(s_canvas_layer);
    }
}

static void update_proc(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, pcb_background);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

    graphics_context_set_text_color(ctx, pcb_silkscreen);
    char label[8];

    // draw "Rdate"
    snprintf(label, 8, "R%02d%02d", s_last_time.tm_mon + 1, s_last_time.tm_mday);
    //graphics_context_set_fill_color(ctx, GColorBlack);
    //graphics_fill_rect(ctx, date_box, 0, GCornerNone);
    graphics_draw_text(
        ctx, label, s_ocra_font, date_box,
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    // draw "time R"
    snprintf(label, 8, "%d%02d R", s_last_time.tm_hour, s_last_time.tm_min);
    //graphics_context_set_fill_color(ctx, GColorBlack);
    //graphics_fill_rect(ctx, time_box, 0, GCornerNone);
    graphics_draw_text(
        ctx, label, s_ocra_font, time_box,
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    
    // draw the resistor and the stripes
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_resistor_img, bitmap_box);

    graphics_context_set_fill_color(ctx, resistor_colors[s_last_time.tm_hour / 10]);
    graphics_fill_rect(ctx, stripe1_box, 0, GCornerNone);
    
    graphics_context_set_fill_color(ctx, resistor_colors[s_last_time.tm_hour % 10]);
    graphics_fill_rect(ctx, stripe2_box, 0, GCornerNone);
    
    graphics_context_set_fill_color(ctx, resistor_colors[s_last_time.tm_min / 10]);
    graphics_fill_rect(ctx, stripe3_box, 0, GCornerNone);

    graphics_context_set_fill_color(ctx, resistor_colors[s_last_time.tm_min % 10]);
    graphics_fill_rect(ctx, stripe4_box, 0, GCornerNone);
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect window_bounds = layer_get_bounds(window_layer);

    s_canvas_layer = layer_create(window_bounds);
    layer_set_update_proc(s_canvas_layer, update_proc);
    layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
    layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/

static void init() {
    if (!persist_exists(BACKGROUND_COLOR_KEY)) {
        persist_write_int(BACKGROUND_COLOR_KEY, pcb_background.argb);
        persist_write_int(SILKSCREEN_COLOR_KEY, pcb_silkscreen.argb);
    }
    else {
        pcb_background.argb = persist_read_int(BACKGROUND_COLOR_KEY);
        pcb_silkscreen.argb = persist_read_int(SILKSCREEN_COLOR_KEY);
    }

    time_t t = time(NULL);
    struct tm *time_now = localtime(&t);
    tick_handler(time_now, MINUTE_UNIT);

    s_ocra_font = fonts_load_custom_font(
        resource_get_handle(RESOURCE_ID_FONT_OCR_A_20));
    
    s_resistor_img = gbitmap_create_with_resource(RESOURCE_ID_RESISTOR_IMG);
    
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_main_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    config_init();
}

static void deinit() {
    config_deinit();
    window_destroy(s_main_window);
    gbitmap_destroy(s_resistor_img);
    fonts_unload_custom_font(s_ocra_font);
}

int main() {
    init();
    app_event_loop();
    deinit();
}
