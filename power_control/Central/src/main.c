#include <bluetooth/scan.h>
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
#define POWER_CONTROL_SENSITIVITY 0
#define ENABLED 1
#define DISABLED 0
#define LOWER_LIMIT_RSSI -60
#define UPPER_LIMIT_RSSI -40
#define LOWER_TARGET_RSSI -55
#define UPPER_TARGET_RSSI -45
#define WAIT_TIME_MS 1000

int8_t peripheral_tx_power = 0;

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
      BT_GAP_SCAN_FAST_INTERVAL /* Same value, but now for the scan window. How
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

/* Function to initialize autonomous power control */
static void init_power_control() {
  sdc_hci_cmd_vs_set_power_control_request_params_t p_params = {
      .beta = POWER_CONTROL_SENSITIVITY,
      .auto_enable = ENABLED,
      .lower_limit = LOWER_LIMIT_RSSI,
      .upper_limit = UPPER_LIMIT_RSSI,
      .lower_target_rssi = LOWER_TARGET_RSSI,
      .upper_target_rssi = UPPER_TARGET_RSSI,
      .wait_period_ms = WAIT_TIME_MS

  };

  int err = sdc_hci_cmd_vs_set_power_control_request_params(&p_params);
  if (err) {
    printk("Failed to initialize power control\n");
    return;
  }
  printk("Successful initialization of power control\n");
}

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

  /* Enable transmit power reporting for both local and remote devices */
  int err = bt_conn_le_set_tx_power_report_enable(conn, true, true);
  if (err) {
    printk("Failed to enable TX power reporting (err: %d)\n", err);
    return;
  }

  init_power_control();
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
      .connect_if_match = DISABLED, /* Disable automatic connection. Instead handle
                                     * connection manually through scan_filter_match()
                                     */
      .scan_param =
          &scan_param, /* Links to a bt_le_scan_param structure, defined earlier */
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

/* Functionality to read the RSSI value */
static void read_conn_rssi(uint16_t handle, int8_t *rssi) {
  struct net_buf *buf, *rsp = NULL;
  struct bt_hci_cp_read_rssi *cp;
  struct bt_hci_rp_read_rssi *rp;

  int err;

  buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
  if (!buf) {
    printk("Unnable to allocate command buffer\n");
    return;
  }

  cp = net_buf_add(buf, sizeof(*cp));
  cp->handle = sys_cpu_to_le16(handle);

  err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
  if (err) {
    printk("Read RSSI err: %d\n", err);
    return;
  }

  rp = (void *)rsp->data;
  *rssi = rp->rssi;

  net_buf_unref(rsp);
}

/* Work handler to poll Tx and RSSI periodically */
static void tx_rssi_work_handler(struct k_work *work) {
  if (!default_conn) {
    printk("No connection, do not reschedule");
    return;
  }

  uint16_t conn_handle;
  int rc = bt_hci_get_conn_handle(default_conn, &conn_handle);
  if (rc) {
    printk("Failed to get connection handle\n");
    return;
  }

  int8_t rssi = 0;
  read_conn_rssi(conn_handle, &rssi);
  printk("Tx: %d dBm, RSSI: %d dBm\n", peripheral_tx_power, rssi);

  /* Reschedule the work */
  k_work_reschedule(&tx_rssi_work, K_MSEC(POLLING_INTERVAL_MS));
}

/* Function activated when power report callback occurs */
static void tx_power_report_cb(struct bt_conn *conn,
                               struct bt_conn_le_tx_power_report *report) {
  peripheral_tx_power = report->tx_power_level;
}

/* Register the callback functions */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

BT_CONN_CB_DEFINE(tx_power_cb) = {
    .tx_power_report = tx_power_report_cb,
};

int main(void) {
  int err;

  printk("Starting BLE Central\n");

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

  while (1) {
    if (is_connected && !polling_started) {
      k_work_init_delayable(&tx_rssi_work, tx_rssi_work_handler);
      k_work_reschedule(&tx_rssi_work, K_MSEC(POLLING_INTERVAL_MS));
      polling_started = true;
    }
    k_sleep(K_MSEC(POLLING_INTERVAL_MS));
  }
}
