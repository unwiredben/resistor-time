// Resistor Time
// 
// a watchface based on ks-clock-face from https://github.com/pebble-examples/ks-clock-face
//
// this watchface draws a common carbon resistor with four bands representing the four
// digits of the 24 hour time.  Below the resistor, the time is also drawn in digits with
// an omega (the symbol for ohms), and the current date is indicated by the
// white silkscreen of Rmmdd

#include <pebble.h>

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

#define RESISTOR_BASE_Y (56)
static const GRect stripe1_box = { { 29, RESISTOR_BASE_Y + 2 }, { 8, 39 } };
static const GRect stripe2_box = { { 54, RESISTOR_BASE_Y + 8 }, { 9, 27 } };
static const GRect stripe3_box = { { 70, RESISTOR_BASE_Y + 8 }, { 8, 27 } };
static const GRect stripe4_box = { { 85, RESISTOR_BASE_Y + 8 }, { 8, 27 } };

static const GColor8 pcb_background = { .argb = GColorKellyGreenARGB8 };
static const GColor8 pcb_silkscreen = { .argb = GColorWhiteARGB8 };

static GFont s_ocra_font;
static GBitmap *s_resistor_img;

static struct tm s_last_time;
    
static Window *s_main_window;
static Layer *s_canvas_layer;

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
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);

    graphics_context_set_text_color(ctx, pcb_silkscreen);
    char label[8];

    // draw "Rdate"
    snprintf(label, 8, "R%02d%02d", s_last_time.tm_mon + 1, s_last_time.tm_mday);
    graphics_draw_text(
        ctx, label, s_ocra_font, GRect(4, 4, 144 - 8, 20),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    // draw "time R"
    snprintf(label, 8, "%d%02d R", s_last_time.tm_hour, s_last_time.tm_min);
    graphics_draw_text(
        ctx, label, s_ocra_font, GRect(4, 168 - 24 - 4, 144 - 8, 20),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    
    // draw the resistor and the stripes
    graphics_draw_bitmap_in_rect(ctx, s_resistor_img, GRect(0, RESISTOR_BASE_Y, 144, 43));

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
}

static void deinit() {
    window_destroy(s_main_window);
    gbitmap_destroy(s_resistor_img);
    fonts_unload_custom_font(s_ocra_font);
}

int main() {
    init();
    app_event_loop();
    deinit();
}
