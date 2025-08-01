#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <cctype>

#include "fami32_pin.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &SPI, DISPLAY_DC, DISPLAY_RESET, DISPLAY_CS);

#undef B0
#undef _abs
#undef sq

#define IN_GIAC
#include "giac/gen.h"
#include "giac/global.h"

using namespace giac;

static const uart_port_t UART_PORT = UART_NUM_0;

static void initialize_console()
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    esp_vfs_dev_uart_use_driver(UART_PORT);

    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(32);
    linenoiseAllowEmpty(0);
}

// static inline void heap_barrier(const char *tag) {
//     bool ok = heap_caps_check_integrity_all(true);
//     printf("[heap_barrier] %s: %s\n", tag, ok ? "OK" : "BROKEN");
// }

static void trim_inplace(char* s) {
    size_t n = strlen(s);
    size_t i = 0;
    while (i < n && isspace((unsigned char)s[i])) ++i;
    size_t j = n;
    while (j > i && isspace((unsigned char)s[j - 1])) --j;
    if (i) memmove(s, s + i, j - i);
    s[j - i] = '\0';
}

static bool giac_eval_line(const char* line)
{
    static giac::context s_ct;
    static bool s_inited = false;
    if (!s_inited) {
        init_context(&s_ct);
        s_inited = true;
    }

    // heap_barrier("before gen");
    try {
        giac::gen g(line, &s_ct);
        giac::gen result = g.eval(1, &s_ct);
        std::string out = result.print(&s_ct);
        printf("result.type=%d result.subtype=%d\n", result.type, result.subtype);
        printf("== %s\n", out.c_str());
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        // heap_barrier("after gen (exception)");
        return false;
    }
    // heap_barrier("after gen");
    return true;
}

static void giac_task(void*)
{
    printf("\nESP_GIAC Demo\n"
           "By libchara-dev\n"
           "Type an expression to calculate. type 'exit' to reset.\n");
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    printf("\n");

    for(;;) {
        char *line = linenoise(">>> ");
        if (!line) continue;

        size_t len = strlen(line);
        while (len && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        trim_inplace(line);
        if (line[0] == '\0') { linenoiseFree(line); continue; }

        if (strcmp(line, "exit") == 0) {
            linenoiseFree(line);
            break;
        }

        linenoiseHistoryAdd(line);

        // heap_barrier("before eval");
        giac_eval_line(line);
        // heap_barrier("after eval");

        linenoiseFree(line);
    }

    esp_restart();
}

extern "C" void app_main()
{
    SPI.begin((gpio_num_t)DISPLAY_SCL, (gpio_num_t)-1, (gpio_num_t)DISPLAY_SDA);
    display.begin();
    display.clearDisplay();
    display.display();
    initialize_console();
    xTaskCreatePinnedToCore(giac_task, "giac_task", 64 * 1024, NULL, 5, NULL, 0);
}
