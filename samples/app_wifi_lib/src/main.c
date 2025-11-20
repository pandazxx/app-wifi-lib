#include <app_wifi_lib/app_wifi_lib.h>
#include <zephyr/kernel.h>

int main(void) {
  app_wifi_lib_init();

  int result = app_wifi_lib_do_something(21);
  printk("Result = %d\n", result);

  while (1) {
    k_sleep(K_SECONDS(1));
  }
}
