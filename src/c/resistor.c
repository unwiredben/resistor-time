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

#include <pebble-events/pebble-events.h>
#include <pebble-connection-vibes/connection-vibes.h>
#include <pebble-packet/pebble-packet.h>

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

// used for persistent storage
#define STORAGE_KEY_BG_COLOR   (0)
#define STORAGE_KEY_SILK_COLOR (1)
#define STORAGE_KEY_VIBE_ON_BT (2)

#define TEXT_HEIGHT  (24)
#if PBL_ROUND
#define Y_OFFSET (20)
#else
#define Y_OFFSET (4)
#endif

static GColor pcb_background = { .argb = GColorKellyGreenARGB8 };
static GColor pcb_silkscreen = { .argb = GColorWhiteARGB8 };
static ConnectionVibesState vibe_on_bt = ConnectionVibesStateDisconnect;

static GFont s_ocra_font;
static GBitmap *s_resistor_img;

static struct tm s_last_time;

static Window *s_main_window;
static Layer *s_canvas_layer;

/********************************** UTILITY ************************************/

static int persist_read_int_with_default(const uint32_t key, const int32_t defaultValue) {
    if (!persist_exists(key)) {
        persist_write_int(key, defaultValue);
        return defaultValue;
    }
    else {
        return persist_read_int(key);
    }    
}

/********************************** CONFIG ************************************/

static void in_recv_handler(DictionaryIterator *iter, void *context) {
    if (packet_contains_key(iter, MESSAGE_KEY_BG_COLOR)) {
        pcb_background = GColorFromHEX(packet_get_integer(iter, MESSAGE_KEY_BG_COLOR));
        persist_write_int(STORAGE_KEY_BG_COLOR, pcb_background.argb);
        if (s_canvas_layer) {
            layer_mark_dirty(s_canvas_layer);
        }
    }
    if (packet_contains_key(iter, MESSAGE_KEY_SILK_COLOR)) {
        pcb_silkscreen = GColorFromHEX(packet_get_integer(iter, MESSAGE_KEY_SILK_COLOR));
        persist_write_int(STORAGE_KEY_SILK_COLOR, pcb_silkscreen.argb);
        if (s_canvas_layer) {
            layer_mark_dirty(s_canvas_layer);
        }
    }
    if (packet_contains_key(iter, MESSAGE_KEY_VIBE_ON_BT)) {
        vibe_on_bt = packet_get_integer(iter, MESSAGE_KEY_VIBE_ON_BT);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "vibe_on_bt %d", vibe_on_bt);
        persist_write_int(STORAGE_KEY_VIBE_ON_BT, vibe_on_bt);
        connection_vibes_set_state(vibe_on_bt);
    }
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

static void HandleObstructedAreaChange(AnimationProgress progress, void *context) {
    
}

static void update_proc(Layer *layer, GContext *ctx) {
    GRect rect = layer_get_unobstructed_bounds(layer);
    
    graphics_context_set_fill_color(ctx, pcb_background);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);

    graphics_context_set_text_color(ctx, pcb_silkscreen);
    char label[8];

    // adjust drawing locations
    uint8_t resistor_base_x = (rect.size.w - 144) / 2;
    uint8_t resistor_base_y = (rect.size.h - 43) / 2;
    GRect bitmap_box  = {{0, resistor_base_y}, {rect.size.w, 43}};
    GRect date_box    = {{0, Y_OFFSET}, {rect.size.w, TEXT_HEIGHT}};
    GRect time_box    = {{0, rect.size.h - TEXT_HEIGHT - Y_OFFSET}, {rect.size.w, TEXT_HEIGHT}};
    GRect stripe1_box = {{resistor_base_x + 29, resistor_base_y + 2}, {8, 39}};
    GRect stripe2_box = {{resistor_base_x + 54, resistor_base_y + 8}, {9, 27}};
    GRect stripe3_box = {{resistor_base_x + 70, resistor_base_y + 8}, {8, 27}};
    GRect stripe4_box = {{resistor_base_x + 85, resistor_base_y + 8}, {8, 27}};
    
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

EventHandle sTicksHandler;
EventHandle sAppMessageRecv;

static void init() {
    pcb_background.argb = persist_read_int_with_default(STORAGE_KEY_BG_COLOR, pcb_background.argb);
    pcb_silkscreen.argb = persist_read_int_with_default(STORAGE_KEY_SILK_COLOR, pcb_silkscreen.argb);
    vibe_on_bt          = persist_read_int_with_default(STORAGE_KEY_VIBE_ON_BT, vibe_on_bt);
    
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

    sTicksHandler = events_tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    connection_vibes_init();
    connection_vibes_set_state(vibe_on_bt);

    events_app_message_request_inbox_size(64);
    sAppMessageRecv = events_app_message_register_inbox_received(in_recv_handler, NULL);
    events_app_message_open();
}

static void deinit() {
    events_app_message_unsubscribe(sAppMessageRecv);
    connection_vibes_deinit();
    events_tick_timer_service_unsubscribe(sTicksHandler);
    window_destroy(s_main_window);
    gbitmap_destroy(s_resistor_img);
    fonts_unload_custom_font(s_ocra_font);
}

int main() {
    init();
    app_event_loop();
    deinit();
}