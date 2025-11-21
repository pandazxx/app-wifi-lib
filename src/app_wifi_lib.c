
#define APP_WIFI_LIB_PROV_ENABLED                                              \
  (defined(CONFIG_APP_BT_LIB) && defined(CONFIG_APP_NVS_LIB))
#define APP_WIFI_LIB_PROV_ENABLED 1
#include <app_wifi_lib/app_wifi_lib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if APP_WIFI_LIB_PROV_ENABLED
#include <app_nvs_lib/app_nvs_lib.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#endif

LOG_MODULE_REGISTER(app_wifi_lib, LOG_LEVEL_INF);

#if APP_WIFI_LIB_PROV_ENABLED
#define NVS_SSID_KEY 42
#define NVS_PWD_KEY 43

#define BT_UUID_PROV_SERVICE_VAL                                               \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x1234567890ab)

#define BT_UUID_PROV_SSID_VAL                                                  \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x1234567890ac)

#define BT_UUID_PROV_PASS_VAL                                                  \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x1234567890ad)

static struct bt_uuid_128 prov_svc_uuid =
    BT_UUID_INIT_128(BT_UUID_PROV_SERVICE_VAL);
static struct bt_uuid_128 prov_ssid_uuid =
    BT_UUID_INIT_128(BT_UUID_PROV_SSID_VAL);
static struct bt_uuid_128 prov_pass_uuid =
    BT_UUID_INIT_128(BT_UUID_PROV_PASS_VAL);

static int write_str_attr(const void *buf, uint16_t len, char *dst,
                          size_t dst_size, const char *label) {
  if (len >= dst_size) {
    LOG_WRN("%s too long (%u, max %zu)", label, len, dst_size - 1);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  memcpy(dst, buf, len);
  dst[len] = '\0';

  LOG_INF("Provisioning %s: \"%s\"", label, dst);
  return 0;
}

/* SSID write callback */
static ssize_t ssid_write_cb(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr, const void *buf,
                             uint16_t len, uint16_t offset, uint8_t flags) {
  ARG_UNUSED(conn);
  ARG_UNUSED(attr);
  ARG_UNUSED(offset);
  ARG_UNUSED(flags);

  // return write_str_attr(buf, len, ssid, sizeof(ssid), "SSID");
  app_nvs_write_utf8(NVS_SSID_KEY, buf, len);
  return 0;
}

/* Password write callback */
static ssize_t pass_write_cb(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr, const void *buf,
                             uint16_t len, uint16_t offset, uint8_t flags) {
  ARG_UNUSED(conn);
  ARG_UNUSED(attr);
  ARG_UNUSED(offset);
  ARG_UNUSED(flags);

  // return write_str_attr(buf, len, password, sizeof(password), "PASSWORD");
  app_nvs_write_utf8(NVS_SSID_KEY, buf, len);
  return 0;
}
BT_GATT_SERVICE_DEFINE(
    prov_svc, BT_GATT_PRIMARY_SERVICE(&prov_svc_uuid),

    /* SSID (write-only string) */
    BT_GATT_CHARACTERISTIC(&prov_ssid_uuid.uuid, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, ssid_write_cb, NULL),

    /* PASSWORD (write-only string) */
    BT_GATT_CHARACTERISTIC(&prov_pass_uuid.uuid, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, pass_write_cb, NULL),

);
#endif

int app_wifi_lib_init(void) {
  LOG_INF("app_wifi_lib init");
  return 0;
}

int app_wifi_lib_do_something(int value) {
  LOG_INF("Doing something with %d", value);
  return value * 2;
}
