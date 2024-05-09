#include "led.h"

//-----------------------------------------------
// LED main
//-----------------------------------------------

int main(int argc, char* argv[]) {
    led_init(argc, argv);

    if (led.opt.help)
        led_help();
    else
        while (led_file_next()) {
            bool isline = false;
            do {
                isline = led_process_read();
                if (led_process_selector()) {
                    led_process_functions();
                    if (led.opt.exec)
                        led_process_exec();
                    else
                        led_process_write();
                }
            } while(isline);
        }
    if (led.opt.report)
        led_report();
    led_free();
    return 0;
}
