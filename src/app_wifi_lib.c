
#include "zephyr/sys/util_macro.h"
#define APP_WIFI_LIB_PROV_ENABLED                                              \
  (defined(CONFIG_APP_BT_LIB) && defined(CONFIG_APP_NVS_LIB))
#define APP_WIFI_LIB_PROV_ENABLED 1
#include <app_wifi_lib/app_wifi_lib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/wifi_mgmt.h>
#if APP_WIFI_LIB_PROV_ENABLED
#include <app_bt_lib/app_bt_lib.h>
#include <app_nvs_lib/app_nvs_lib.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#endif

LOG_MODULE_REGISTER(app_wifi_lib, LOG_LEVEL_DBG);

#define NET_EVENT_WIFI_MASK                                                    \
  (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |          \
   NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |        \
   NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)
static struct net_if *sta_iface;
static struct wifi_connect_req_params sta_config;
static struct net_mgmt_event_callback cb;
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
  LOG_DBG("Got psk: %d", buf);
  app_nvs_write_utf8(NVS_PWD_KEY, buf, len);
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

static char *get_wifi_ssid() {
  static char ssid_buf[256];
  if (IS_ENABLED(APP_WIFI_LIB_PROV_ENABLED)) {
    int rs = app_nvs_read_utf8(NVS_SSID_KEY, ssid_buf, sizeof(ssid_buf));
    if (rs > 0) {
      return ssid_buf;
    }
  }
  return CONFIG_APP_WIFI_DEFAULT_SSID;
}

static char *get_wifi_psk() {
  static char psk_buf[256];

  if (IS_ENABLED(APP_WIFI_LIB_PROV_ENABLED)) {
    int rs = app_nvs_read_utf8(NVS_PWD_KEY, psk_buf, sizeof(psk_buf));
    if (rs > 0) {
      return psk_buf;
    }
  }
  return CONFIG_APP_WIFI_DEFAULT_PSK;
}
static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                               uint64_t mgmt_event, struct net_if *iface) {
  switch (mgmt_event) {
  case NET_EVENT_WIFI_CONNECT_RESULT: {
    const struct wifi_status *status = (const struct wifi_status *)cb->info;
    LOG_INF("Connected xx to %s: status=%d, conn_status=%d, disconn_status=%d",
            get_wifi_ssid(), status->status, status->conn_status,
            status->disconn_reason);
    break;
  }
  case NET_EVENT_WIFI_DISCONNECT_RESULT: {
    const struct wifi_status *status = (const struct wifi_status *)cb->info;
    LOG_INF(
        "Disconnected xx to %s: status=%d, conn_status=%d, disconn_status=%d",
        get_wifi_ssid(), status->status, status->conn_status,
        status->disconn_reason);
    // LOG_INF("Disconnected from %s", CONFIG_WIFI_SAMPLE_SSID);
    // connect_to_wifi();
    break;
  }
  // case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
  //   LOG_INF("AP Mode is enabled. Waiting for station to connect");
  //   break;
  // }
  // case NET_EVENT_WIFI_AP_DISABLE_RESULT: {
  //   LOG_INF("AP Mode is disabled.");
  //   break;
  // }
  // case NET_EVENT_WIFI_AP_STA_CONNECTED: {
  //   struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;
  //
  //   LOG_INF("station: " MACSTR " joined ", sta_info->mac[0],
  //   sta_info->mac[1],
  //           sta_info->mac[2], sta_info->mac[3], sta_info->mac[4],
  //           sta_info->mac[5]);
  //   break;
  // }
  // case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
  //   struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;
  //
  //   LOG_INF("station: " MACSTR " leave ", sta_info->mac[0], sta_info->mac[1],
  //           sta_info->mac[2], sta_info->mac[3], sta_info->mac[4],
  //           sta_info->mac[5]);
  //   break;
  // }
  default:
    break;
  }
}
static int connect_to_wifi(void) {
  if (!sta_iface) {
    LOG_INF("STA: interface no initialized");
    return -EIO;
  }

  sta_config.ssid = get_wifi_ssid();
  sta_config.ssid_length = strlen(sta_config.ssid);
  sta_config.psk = get_wifi_psk();
  sta_config.psk_length = strlen(sta_config.psk);
  sta_config.security = WIFI_SECURITY_TYPE_PSK;
  sta_config.channel = WIFI_CHANNEL_ANY;
  sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

  LOG_INF("Connecting to SSID: ssid=%s, psk=%s\n", sta_config.ssid,
          sta_config.psk);

  int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
                     sizeof(struct wifi_connect_req_params));
  if (ret) {
    LOG_ERR("Unable to Connect to (%s), %d", CONFIG_APP_WIFI_DEFAULT_SSID, ret);
  }

  return ret;
}
int app_wifi_lib_init(void) {
  LOG_INF("app_wifi_lib init");
  /* Get STA interface in AP-STA mode. */
  sta_iface = net_if_get_wifi_sta();
  if (IS_ENABLED(APP_WIFI_LIB_PROV_ENABLED)) {
    app_nvs_lib_init();
    app_bt_lib_init();
  }

  // enable_ap_mode();
  net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
  net_mgmt_add_event_callback(&cb);
  connect_to_wifi();
  return 0;
}

int app_wifi_lib_do_something(int value) {
  LOG_INF("Doing something with %d", value);
  return value * 2;
}
