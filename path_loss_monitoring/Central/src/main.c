#include <bluetooth/scan.h>
#include <dk_buttons_and_leds.h>
#include <sdc_hci_vs.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>

static struct bt_conn *default_conn;

/* Creating a delayed work item */
static struct k_work_delayable tx_rssi_work;

#define POLLING_INTERVAL_MS 500
#define BAUDRATE_2M 2
#define ENABLED 1
#define DISABLED 0
#define WAIT_TIME_MS 1000

#define LOW_PATH_LOSS_LED DK_LED1
#define MEDIUM_PATH_LOSS_LED DK_LED2
#define HIGH_PATH_LOSS_LED DK_LED3
#define HIGH_PATH_LOSS_BORDER 60
#define LOW_PATH_LOSS_BORDER 40
#define NUM_ITERATIONS 5
#define PATH_LOSS_BUFFER_ZONE 5
#define ACTIVE 1
#define UNACTIVE 0

/* Flags for connection status */
volatile bool polling_started = false;
volatile bool is_connected = false;

/* The peripheral's 128-bit UUID. Note: It is little endian
 * format (backwards)
 */
static struct bt_uuid_128 bt_uuid =
    BT_UUID_INIT_128(0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15, 0xde, 0xef,
                     0x12, 0x12, 0x23, 0x15, 0x00, 0x00);

/* After a UUID match has occured, this is the functionality that occurs: */
static void scan_filter_match(struct bt_scan_device_info *device_info,
                              struct bt_scan_filter_match *filter_match,
                              bool connectable) {
  /* Converts the Bluetooth address of the found device into a readable string
   * (addr). Practical for logging and debugging.
   */
  char addr[BT_ADDR_LE_STR_LEN];
  bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
  printk("Found peripheral: %s\n", addr);

  bt_scan_stop(); /* Stops scanning for other peripherals */

  /* Default connection creation parameters */
  struct bt_conn_le_create_param *conn_params = BT_CONN_LE_CREATE_PARAM(
      BT_CONN_LE_OPT_NONE, /* No special option for baudrate. Central-peripheral
                            * connection uses the highest value that both
                            * support, in this case 2M
                            */
      BT_GAP_SCAN_FAST_INTERVAL, /* How often the central scans while trying to
                                  * connect
                                  */
      BT_GAP_SCAN_FAST_INTERVAL  /* Same value, but now for the scan window. How
                                  * long the radio stays active to listen during
                                  * each interval
                                  */
  );

  /* Creates connection between central and peripheral */
  int err = bt_conn_le_create(device_info->recv_info->addr, conn_params,
                              BT_LE_CONN_PARAM_DEFAULT, &default_conn);
  if (err) {
    printk("Connection failed (err: %d)\n", err);
    bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    return;
  }
  printk("Connection to %s\n", addr);
}

/* Initialize a scan callback structure */
BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

/* This function runs after a successful or failed connection attempt */
static void connected(struct bt_conn *conn, uint8_t conn_err) {
  char addr[BT_ADDR_LE_STR_LEN];
  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (conn_err) {
    printk("Failed to connect to %s (err 0x%02x)\n", addr, conn_err);
    bt_conn_unref(default_conn);
    default_conn = NULL;
    bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    is_connected = false;
    return;
  }
  printk("Connected to %s\n", addr);
  is_connected = true;

  /* Define path loss monitoring parameters */
  struct bt_conn_le_path_loss_reporting_param pl_param = {
      .high_threshold = HIGH_PATH_LOSS_BORDER,
      .high_hysteresis = PATH_LOSS_BUFFER_ZONE,
      .low_threshold = LOW_PATH_LOSS_BORDER,
      .low_hysteresis = PATH_LOSS_BUFFER_ZONE,
      .min_time_spent = NUM_ITERATIONS};

  /* Set the parameters */
  int err = bt_conn_le_set_path_loss_mon_param(conn, &pl_param);
  if (err) {
    printk("Failed to set path loss monitoring parameters (er: %d)\n", err);
    return;
  }

  /* Enable path loss monitoring */
  err = bt_conn_le_set_path_loss_mon_enable(conn, true);
  if (err) {
    printk("Failed to enable path loss monitoring (err: %d)\n", err);
    return;
  }
  printk("Path loss monitoring enabled\n");
}

/* Function when disconnection occurs */
static void disconnected(struct bt_conn *conn, uint8_t reason) {
  char addr[BT_ADDR_LE_STR_LEN];
  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  printk("Disconnected from %s (reason: 0x%02x)\n", addr, reason);

  if (default_conn == conn) {
    bt_conn_unref(default_conn);
    default_conn = NULL;
    bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
  }
  is_connected = false;
  polling_started = false;
}

/* Initializes the scanning process */
static void scan_init(void) {
  /* Scan parameters with fast scanning intervals and windows */
  struct bt_le_scan_param scan_param = {
      .type = BT_LE_SCAN_TYPE_ACTIVE,
      .interval = BT_GAP_SCAN_FAST_INTERVAL,
      .window = BT_GAP_SCAN_FAST_WINDOW,
  };

  /* Struct to initialize the scan module */
  struct bt_scan_init_param scan_init = {
      .connect_if_match =
          DISABLED,              /* Disable automatic connection. Instead handle
                                  * connection manually through scan_filter_match()
                                  */
      .scan_param = &scan_param, /* Links to a bt_le_scan_param structure,
                                    defined earlier */
  };

  bt_scan_init(&scan_init);
  bt_scan_cb_register(&scan_cb);

  int err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, &bt_uuid.uuid);
  if (err) {
    printk("Failed to add UUID filter (err %d)\n", err);
    return;
  }

  err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
  if (err) {
    printk("Failed to enable filter (err: %d)\n", err);
    return;
  }
}

/* Initialize a path loss callback structure */
static void path_loss_threshold_report(
    struct bt_conn *conn,
    const struct bt_conn_le_path_loss_threshold_report *report) {
  static uint16_t default_conn_handle;
  int ret = bt_hci_get_conn_handle(conn, &default_conn_handle);
  if (ret) {
    printk("No connection handle (err %d)\n", ret);
    return;
  }
  if (report->zone == 0) {
    dk_set_led(LOW_PATH_LOSS_LED, ACTIVE);
    dk_set_led(MEDIUM_PATH_LOSS_LED, UNACTIVE);
    dk_set_led(HIGH_PATH_LOSS_LED, UNACTIVE);
  } else if (report->zone == 1) {
    dk_set_led(LOW_PATH_LOSS_LED, UNACTIVE);
    dk_set_led(MEDIUM_PATH_LOSS_LED, ACTIVE);
    dk_set_led(HIGH_PATH_LOSS_LED, UNACTIVE);
  } else if (report->zone == 2) {
    dk_set_led(LOW_PATH_LOSS_LED, UNACTIVE);
    dk_set_led(MEDIUM_PATH_LOSS_LED, UNACTIVE);
    dk_set_led(HIGH_PATH_LOSS_LED, ACTIVE);
  }
}

/* Register the callback functions */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .path_loss_threshold_report = path_loss_threshold_report,
};

int main(void) {
  int err;

  printk("Starting BLE Central\n");

  err = dk_leds_init();
  if (err) {
    printk("LEDs init failed (err %d)\n", err);
    return 0;
  }

  err = bt_enable(NULL);
  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return 0;
  }

  scan_init();

  err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
  if (err) {
    printk("Scanning failed to start (err: %d)\n", err);
    return 0;
  }

  printk("Scanning for peripherals...\n");
}
