// Resistor Time
// 
// a watchface based on ks-clock-face from https://github.com/pebble-examples/ks-clock-face
//
// this watchface draws a common carbon resistor with four bands representing the four
// digits of the 24 hour time.  Below the resistor, the time is also drawn in digits with
// an omega (the symbol for ohms), and the current date is indicated by the
// white silkscreen of Rmmdd
//
// There are two alternate modes -- there's a surface mount resistor mode that
// uses an image of a large SMT resistor and draws the time in numbers and there's a
// mode that shows the NYC Resistor logo.

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
#define STORAGE_KEY_BG_COLOR        (0)
#define STORAGE_KEY_SILK_COLOR      (1)
#define STORAGE_KEY_VIBE_ON_BT      (2)
#define STORAGE_KEY_RESISTOR_TYPE   (3)
#define STORAGE_KEY_LOWER_LABEL     (4)

#if PBL_ROUND
#define Y_OFFSET (20)
#else
#define Y_OFFSET (4)
#endif

#define OHM "\xE2\x84\xA6"

typedef enum ResistorType {
    THROUGH_HOLE = 0,
    SURFACE_MOUNT = 1,
    NYC_RESISTOR = 2,
    CYCLE_RESISTORS = 3
} ResistorType;

typedef enum LowerLabelType {
    STANDARD_TIME = 0,
    BEATS = 1,
    ALTERNATE = 2,
    SWITCH_ON_SHAKE = 3
} LowerLabelType;

static GColor pcb_background = { .argb = GColorKellyGreenARGB8 };
static GColor pcb_silkscreen = { .argb = GColorWhiteARGB8 };
static ConnectionVibesState vibe_on_bt = ConnectionVibesStateDisconnect;
static uint8_t resistor_type = THROUGH_HOLE;
static bool cycle_resistors = false;
static LowerLabelType lower_label = STANDARD_TIME;

static GFont s_ocra_font;
static uint8_t s_ocra_height;

static GFont s_smt_font;
static GSize s_smt_size;

static GBitmap *s_resistor_img;
static GBitmap *s_smt_resistor_img;
static GBitmap *s_nyc_resistor_img;

static struct tm s_last_time;

static bool s_show_beats = false;

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

static int current_time_in_beats(void) {
    // compute beats by taking current time, converting to UTC+1,
    // then converting to UTC struct tm and adding up second count
    time_t now = time(NULL);
    now += (60 * 60);
    struct tm ref = *gmtime(&now);
    int beats = (ref.tm_sec + (ref.tm_min * 60) + (ref.tm_hour * 60 * 60)) * 10 / 864;
    return beats;
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
        persist_write_int(STORAGE_KEY_VIBE_ON_BT, vibe_on_bt);
        connection_vibes_set_state(vibe_on_bt);
    }
    if (packet_contains_key(iter, MESSAGE_KEY_RESISTOR_TYPE)) {
        resistor_type = packet_get_integer(iter, MESSAGE_KEY_RESISTOR_TYPE);
        persist_write_int(STORAGE_KEY_RESISTOR_TYPE, resistor_type);
 
        if (resistor_type == CYCLE_RESISTORS) {
            cycle_resistors = true;
            resistor_type = THROUGH_HOLE;
        }
        else {
            cycle_resistors = false;
        }
 
        if (s_canvas_layer) {
            layer_mark_dirty(s_canvas_layer);
        }
    }
    if (packet_contains_key(iter, MESSAGE_KEY_LOWER_LABEL)) {
        lower_label = packet_get_integer(iter, MESSAGE_KEY_LOWER_LABEL);
        persist_write_int(STORAGE_KEY_LOWER_LABEL, lower_label);
        if (s_canvas_layer) {
            layer_mark_dirty(s_canvas_layer);
        }
    }
}

/************************************ UI **************************************/

static void update_resistor_type(void) {
    if (cycle_resistors) {
        if (s_last_time.tm_sec == 0) {
            resistor_type = THROUGH_HOLE;
        }
        else if (s_last_time.tm_sec == 20) {
            resistor_type = SURFACE_MOUNT;
        }
        else if (s_last_time.tm_sec == 40) {
            resistor_type = NYC_RESISTOR;
        }
    }
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
    // Store time
    s_last_time = *tick_time;

    int sec = s_last_time.tm_sec;

    if (s_canvas_layer &&
        (sec == 0 ||
        (cycle_resistors && (sec == 20 || sec == 40)) ||
        (lower_label == ALTERNATE && sec == 30))) {
        layer_mark_dirty(s_canvas_layer);
    }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    if (lower_label == SWITCH_ON_SHAKE) {
        s_show_beats = !s_show_beats;
        if (s_canvas_layer) {
            layer_mark_dirty(s_canvas_layer);
        }    
    }
}

static void draw_through_hole(Layer *layer, GContext *ctx) {
    GRect rect = layer_get_unobstructed_bounds(layer);

    // adjust drawing locations
    uint8_t resistor_base_x = (rect.size.w - 144) / 2;
    GRect bitmap_box  = gbitmap_get_bounds(s_resistor_img);
    uint8_t resistor_base_y = (rect.size.h - bitmap_box.size.h) / 2;
    bitmap_box.origin.x = (rect.size.w - bitmap_box.size.w) / 2;
    bitmap_box.origin.y = resistor_base_y;

    GRect stripe1_box = {{resistor_base_x + 29, resistor_base_y + 2}, {8, 39}};
    GRect stripe2_box = {{resistor_base_x + 54, resistor_base_y + 8}, {9, 27}};
    GRect stripe3_box = {{resistor_base_x + 70, resistor_base_y + 8}, {8, 27}};
    GRect stripe4_box = {{resistor_base_x + 85, resistor_base_y + 8}, {8, 27}};
        
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

static void draw_surface_mount(Layer *layer, GContext *ctx) {
    GRect rect = layer_get_unobstructed_bounds(layer);

    // adjust drawing locations
    GRect bitmap_box  = gbitmap_get_bounds(s_smt_resistor_img);
    uint8_t resistor_base_y = (rect.size.h - bitmap_box.size.h) / 2;
    bitmap_box.origin.x = (rect.size.w - bitmap_box.size.w) / 2;
    bitmap_box.origin.y = resistor_base_y;
        
    // draw the surface mount resistor
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_smt_resistor_img, bitmap_box);

    // draw the digits on the surface mount resistor
    bitmap_box.origin.x -= 1;
    bitmap_box.origin.y += 12;
    char label[6];
    snprintf(label, 6, "%04d", s_last_time.tm_hour * 100 + s_last_time.tm_min);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(
        ctx, label, s_smt_font, bitmap_box,
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_nyc_resistor(Layer *layer, GContext *ctx) {
    GRect rect = layer_get_unobstructed_bounds(layer);

    // adjust drawing locations
    GRect bitmap_box  = gbitmap_get_bounds(s_nyc_resistor_img);
    uint8_t resistor_base_y = (rect.size.h - bitmap_box.size.h) / 2;
    bitmap_box.origin.x = (rect.size.w - bitmap_box.size.w) / 2;
    bitmap_box.origin.y = resistor_base_y;
        
    // draw the NYC resistor logo
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_nyc_resistor_img, bitmap_box);
}

static void update_proc(Layer *layer, GContext *ctx) {
    update_resistor_type();

    GRect rect = layer_get_unobstructed_bounds(layer);
 
    graphics_context_set_fill_color(ctx, pcb_background);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);

    graphics_context_set_text_color(ctx, pcb_silkscreen);
    char label[12];

    // adjust drawing locations
    GRect date_box    = {{0, Y_OFFSET}, {rect.size.w, s_ocra_height}};
    GRect time_box    = {{0, rect.size.h - s_ocra_height - Y_OFFSET - 4}, {rect.size.w, s_ocra_height}};

#ifdef PBL_ROUND
    if (resistor_type == NYC_RESISTOR) {
        date_box.origin.y -= 10;
        time_box.origin.y += 6;
    }
#endif
    
    if (resistor_type == THROUGH_HOLE || rect.size.h > 130) {
        // draw "Rdate"
        snprintf(label, 12, "R%02d%02d", s_last_time.tm_mon + 1, s_last_time.tm_mday);
        graphics_draw_text(
            ctx, label, s_ocra_font, date_box,
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

        if (lower_label == BEATS ||
            (lower_label == ALTERNATE && s_last_time.tm_sec >= 30) ||
            (lower_label == SWITCH_ON_SHAKE && s_show_beats)) {
            // draw ".beats"
            snprintf(label, 12, "@ %d", current_time_in_beats());
            graphics_draw_text(
                ctx, label, s_ocra_font, time_box,
                GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        }
        else {
            // draw "time ohm"
            snprintf(label, 12, "%d " OHM, s_last_time.tm_hour * 100 + s_last_time.tm_min);
            graphics_draw_text(
                ctx, label, s_ocra_font, time_box,
                GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        }
    }

    switch (resistor_type) {
    case THROUGH_HOLE:
        draw_through_hole(layer, ctx);
        break;
    case SURFACE_MOUNT:
        draw_surface_mount(layer, ctx);
        break;
    case NYC_RESISTOR:
        draw_nyc_resistor(layer, ctx);
        break;
    }
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
EventHandle sAccelHandler;

static void init(void) {
    pcb_background.argb = persist_read_int_with_default(STORAGE_KEY_BG_COLOR, pcb_background.argb);
    pcb_silkscreen.argb = persist_read_int_with_default(STORAGE_KEY_SILK_COLOR, pcb_silkscreen.argb);
    vibe_on_bt          = persist_read_int_with_default(STORAGE_KEY_VIBE_ON_BT, vibe_on_bt);
    resistor_type       = persist_read_int_with_default(STORAGE_KEY_RESISTOR_TYPE, resistor_type);
    lower_label         = persist_read_int_with_default(STORAGE_KEY_LOWER_LABEL, lower_label);

    if (resistor_type == CYCLE_RESISTORS) {
        cycle_resistors = true;
        resistor_type = THROUGH_HOLE;
    }

    time_t t = time(NULL);
    struct tm *time_now = localtime(&t);
    tick_handler(time_now, SECOND_UNIT);

    s_ocra_font = fonts_load_custom_font(
        resource_get_handle(RESOURCE_ID_UBUNTU_MONO_22));
    GSize size = graphics_text_layout_get_content_size(
        "R89 " OHM, s_ocra_font, GRect(0, 0, 140, 140),
        GTextOverflowModeWordWrap, GTextAlignmentLeft);
    s_ocra_height = size.h;

    s_smt_font = fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
    s_smt_size = graphics_text_layout_get_content_size(
        "0000", s_smt_font, GRect(0, 0, 140, 140),
        GTextOverflowModeWordWrap, GTextAlignmentLeft);

    s_resistor_img = gbitmap_create_with_resource(RESOURCE_ID_RESISTOR_IMG);
    s_smt_resistor_img = gbitmap_create_with_resource(RESOURCE_ID_SURFACE_MOUNT_IMG);
    s_nyc_resistor_img = gbitmap_create_with_resource(RESOURCE_ID_NYCR_IMG);

    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_main_window, true);

    sTicksHandler = events_tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

    connection_vibes_init();
    connection_vibes_set_state(vibe_on_bt);

    events_app_message_request_inbox_size(64);
    sAppMessageRecv = events_app_message_register_inbox_received(in_recv_handler, NULL);
    events_app_message_open();

    sAccelHandler = events_accel_tap_service_subscribe(tap_handler);
}

static void deinit(void) {
    events_accel_tap_service_unsubscribe(sAccelHandler);
    events_app_message_unsubscribe(sAppMessageRecv);
    connection_vibes_deinit();
    events_tick_timer_service_unsubscribe(sTicksHandler);
    window_destroy(s_main_window);
    gbitmap_destroy(s_resistor_img);
    gbitmap_destroy(s_smt_resistor_img);
    gbitmap_destroy(s_nyc_resistor_img);
    fonts_unload_custom_font(s_ocra_font);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}