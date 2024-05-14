
#include "led.h"

#define test(NAME) printf("-- %s...\n", #NAME);NAME();printf("OK\n")

void led_test_char_app() {
    led_u8str_decl(test, 16);
    led_u8str_app_str(&test,"a test");
    led_u8str_app_char(&test, 'A');
    led_u8str_app_char(&test, 'â');
    led_debug("%s",led_u8str_str(&test));
    led_assert(led_u8str_equal_str(&test, "a testAâ"), LED_ERR_INTERNAL, "led_test_char_app");
}

void led_test_char_last() {
    led_u8str_decl(test, 16);
    led_u8str_app_str(&test,"test=à");
    u8chr_t c = led_u8str_char_last(&test);
    led_u8str_app_char(&test, c);
    led_debug("%s",led_u8str_str(&test));
    led_assert(led_u8str_equal_str(&test, "test=àà"), LED_ERR_INTERNAL, "led_test_char_last");
}

void led_test_trunk_char() {
    led_u8str_decl(test, 16);
    led_u8str_app_str(&test,"test=àa");
    led_str_trunk_char(&test, 'à');
    led_debug("%s",led_u8str_str(&test));
    led_str_trunk_char(&test, 'a');
    led_debug("%s",led_u8str_str(&test));
    led_str_trunk_char(&test, 'à');
    led_debug("%s",led_u8str_str(&test));
    led_assert(led_u8str_equal_str(&test, "test="), LED_ERR_INTERNAL, "led_test_trunk_char");
}

void led_test_cut_next() {
    led_u8str_decl(test, 256);
    led_u8str_app_str(&test, "chara/charà/charÂ");
    led_str_t tok;

    led_debug("%s",led_u8str_str(&test));
    led_u8str_cut_next(&test, '/', &tok);
    led_debug("%s -> tok=%s",led_u8str_str(&test), led_u8str_str(&tok));
    led_assert(led_u8str_equal_str(&test, "charà/charÂ"), LED_ERR_INTERNAL, "led_test_cut_next");
    led_assert(led_u8str_equal_str(&tok, "chara"), LED_ERR_INTERNAL, "led_test_cut_next");
}
//-----------------------------------------------
// LEDTEST main
//-----------------------------------------------

int main(int , char* []) {
    led.opt.verbose = true;
    test(led_test_char_app);
    test(led_test_char_last);
    test(led_test_trunk_char);
    test(led_test_cut_next);
    return 0;
}
