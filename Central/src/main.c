#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <bluetooth/scan.h>
#include <zephyr/sys/byteorder.h>
#include <sdc_hci_vs.h>
#include <dk_buttons_and_leds.h>

//Define what functionalities should be active
//Comment out to deactivate functionality
#define RSSI_power_control
#define path_loss_monitoring

//Define RSSI value that the system should converge towards
#define STRONG_RSSI   -50

//Polling interval
#define POLLING_INTERVAL_MS 500

//Declare the connection variable
static struct bt_conn *default_conn;

//Flags for connection status
volatile bool polling_started = false;
volatile bool is_connected = false;

//The peripheral's 128-bit UUID (In this case LBS). Note: It is little endian format (backwards)
static struct bt_uuid_128 lbs_uuid = BT_UUID_INIT_128(
    0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15, 0xde, 0xef, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00
);

//Creating a delayed work item
static struct k_work_delayable tx_rssi_work;



//Function to read TX using HCI command
static void get_tx_power(uint8_t handle_type, uint16_t handle, int8_t *tx_pwr_lvl){
    struct bt_hci_cp_vs_read_tx_power_level *cp;
    struct bt_hci_rp_vs_read_tx_power_level *rp;
    struct net_buf *buf, *rsp = NULL;
    int err;

    *tx_pwr_lvl = 0xFF;  //Default value

    buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, sizeof(*cp));

    if(!buf){
        printk("Unable to allocate command buffer\n");
        return;
    }

    cp = net_buf_add(buf, sizeof(*cp));
    cp->handle = sys_cpu_to_le16(handle);
    cp->handle_type = handle_type;

    err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, buf, &rsp);

    if(err){
        printk("Read Tx power err: %d\n", err);
        return;
    }

    rp = (void *)rsp->data;
    *tx_pwr_lvl = rp->tx_power_level;

    net_buf_unref(rsp);
}



//Function to set Tx power remotely for the peripheral
uint8_t set_remote_tx_power(int8_t delta){
    //Get the connection handle
    uint16_t conn_handle;
    int rc = bt_hci_get_conn_handle(default_conn, &conn_handle);
    if(rc){
        printk("Failed to get connection handle\n");
        return rc;
    }

    //Prepare the parameters for the remote TX power command
    sdc_hci_cmd_vs_write_remote_tx_power_t params = {
        .conn_handle = conn_handle,
        .phy = 2,
        .delta = delta
    };

    //Call the HCI command to request a TX power change
    int err = sdc_hci_cmd_vs_write_remote_tx_power(&params);
    if(err){
        printk("Failed to request remote TX power change (err: %d)\n", err);
        return err;
    }
    else{
        //printk("Request sent to adjust remote TX power by %d dBm\n", delta);
        return 0;
    }
}



//Function to set tx power for the central
static void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl){
    struct bt_hci_cp_vs_write_tx_power_level *cp;
    struct bt_hci_rp_vs_write_tx_power_level *rp;
    struct net_buf *buf, *rsp = NULL;
    int err;

    buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(*cp));
     if(!buf){
        printk("Unable to allocate command buffer\n");
        return;
     }

     cp = net_buf_add(buf, sizeof(*cp));
     cp->handle = sys_cpu_to_le16(handle);
     cp->handle_type = handle_type;
     cp->tx_power_level = tx_pwr_lvl;

     err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);

     if(err){
        printk("Set Tx power err: %d\n", err);
        return;
     }

     rp = (void *)rsp->data;

     net_buf_unref(rsp);
}



//Functionality to read the RSSI value
static void read_conn_rssi(uint16_t handle, int8_t *rssi){
    struct net_buf *buf, *rsp = NULL;
    struct bt_hci_cp_read_rssi *cp;
    struct bt_hci_rp_read_rssi *rp;

    int err;

    buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
    if(!buf){
        printk("Unnable to allocate command buffer\n");
        return;
    }

    cp = net_buf_add(buf, sizeof(*cp));
    cp->handle = sys_cpu_to_le16(handle);

    err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
    if(err){
        printk("Read RSSI err: %d\n", err);
        return;
    }

    rp = (void *)rsp->data;
    *rssi = rp->rssi;

    net_buf_unref(rsp);
}



//Work handler to poll Tx and RSSI periodically
static void tx_rssi_work_handler(struct k_work *work){
    if(!default_conn){
        //No connection, do not reschedule
        printk("No connection, do not reschedule");
        return;
    }

    uint16_t conn_handle;
    int rc = bt_hci_get_conn_handle(default_conn, &conn_handle);
    if(rc){
        printk("Failed to get connection handle\n");
        return;
    }

    int8_t tx;
    int8_t rssi = 0;
    int8_t path_loss;
    get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle, &tx);
    read_conn_rssi(conn_handle, &rssi);
    path_loss = tx - rssi;
    //printk("Tx: %d dBm, Path loss: %d dBm\n", tx, path_loss);
    printk("Tx: %d dBm, RSSI: %d dBm\n", tx, rssi);
    //printk("Path loss(%d) = Tx(%d) - RSSI(%d)\n", path_loss, tx, rssi);

    //Reschedule the work
    k_work_reschedule(&tx_rssi_work, K_MSEC(POLLING_INTERVAL_MS));
}



//After a UUID match has occured, a connection between central and peripheral is established
static void scan_filter_match(struct bt_scan_device_info *device_info, struct bt_scan_filter_match *filter_match, bool connectable){
    //Converts the Bluetooth address of the found device into a readable string (addr). Practical for logging and debugging.
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
    printk("Found peripheral: %s\n", addr);

    bt_scan_stop(); //Stops scanning for other peripherals

    //Default connection creation parameters
    struct bt_conn_le_create_param *conn_params = 
        BT_CONN_LE_CREATE_PARAM(
            BT_CONN_LE_OPT_NONE,       //No special option for baudrate. Central-peripheral connection uses the highest value that both support, in this case 2M
            BT_GAP_SCAN_FAST_INTERVAL, //How often the central scans while trying to connect
            BT_GAP_SCAN_FAST_INTERVAL  //Same value, but now for the scan window. How long the radio stays active to listen during each interval
        );  
    
    //Creates connection between central and peripheral
    int err = bt_conn_le_create(device_info->recv_info->addr, conn_params, BT_LE_CONN_PARAM_DEFAULT, &default_conn);
    if(err){
        printk("Connection failed (err: %d)\n", err);
        bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
        return;
    }
    printk("Connection to %s\n", addr);
}

//Initialize a scan callback structure
BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);



//Initialize a path loss callback structure
static void path_loss_threshold_report(struct bt_conn *conn, const struct bt_conn_le_path_loss_threshold_report *report){
    static uint16_t default_conn_handle;
    int ret = bt_hci_get_conn_handle(conn, &default_conn_handle);
    if (ret){
        printk("No connection handle (err %d)\n", ret);
        return;
    }
    if(report->zone == 0){
        dk_set_led(DK_LED1, 0);
        dk_set_led(DK_LED2, 0);
        dk_set_led(DK_LED3, 1);
    }
    else if(report->zone == 1){
        dk_set_led(DK_LED1, 1);
        dk_set_led(DK_LED2, 0);
        dk_set_led(DK_LED3, 0);
    }
    else if(report->zone == 2){
        dk_set_led(DK_LED1, 0);
        dk_set_led(DK_LED2, 1);
        dk_set_led(DK_LED3, 0);
    }
}



//This function runs after a successful or failed connection attempt
static void connected(struct bt_conn *conn, uint8_t conn_err){
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if(conn_err){
        printk("Failed to connect to %s (err 0x%02x)\n", addr, conn_err);
        bt_conn_unref(default_conn);
        default_conn = NULL;
        bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
        is_connected = false;
        return;
    }

    printk("Connected to %s\n", addr);
    is_connected = true;

    // Enable transmit power reporting for both local and remote devices
    int err = bt_conn_le_set_tx_power_report_enable(conn, true, true);
    if (err) {
        printk("Failed to enable TX power reporting (err: %d)\n", err);
        return;
    }

    //Define path loss monitoring parameters
    struct bt_conn_le_path_loss_reporting_param pl_param = {
        .high_threshold = 60,
        .high_hysteresis = 5,
        .low_threshold = 40,
        .low_hysteresis = 5,
        .min_time_spent = 5
    };

    //Set the parameters
    err = bt_conn_le_set_path_loss_mon_param(conn, &pl_param);
    if(err){
        printk("Failed to set path loss monitoring parameters (er: %d)\n", err);
        return;
    }

    //Enable path loss monitoring
    err = bt_conn_le_set_path_loss_mon_enable(conn, true);
    if(err){
        printk("Failed to enable path loss monitoring (err: %d)\n", err);
        return;
    }
    printk("Path loss monitoring enabled\n");

    //Set initial tx power
    uint16_t conn_handle;
    int rc = bt_hci_get_conn_handle(default_conn, &conn_handle);
    if(rc){
        printk("Failed to get connection handle\n");
        return;
    }
    set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle, 0);
}



//Function when disconnection occurs
static void disconnected(struct bt_conn *conn, uint8_t reason){
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected from %s (reason: 0x%02x)\n", addr, reason);

    if(default_conn == conn){
        bt_conn_unref(default_conn);
        default_conn = NULL;
        bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    }
    is_connected = false;
    polling_started = false;
}



//Register the callback functions
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
#ifdef path_loss_monitoring
    .path_loss_threshold_report = path_loss_threshold_report,
#endif
};



//Initializes the scanning process
static void scan_init(void){
    //Scan parameters with fast scanning intervals and windows
    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    //Struct to initialize the scan module
    struct bt_scan_init_param scan_init = {
        .connect_if_match = 0,      //Disable automatic connection. Instead handle connection manually through scan_filter_match()
        .scan_param = &scan_param,  //Links to a bt_le_scan_param structure, defined earlier
    };

    bt_scan_init(&scan_init);
    bt_scan_cb_register(&scan_cb);

    int err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, &lbs_uuid.uuid);
    if(err){
        printk("Failed to add UUID filter (err %d)\n", err);
        return;
    }

    err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
    if(err){
        printk("Failed to enable filter (err: %d)\n", err);
        return;
    }
}


//Function for power control based on RSSI
static void power_control(){
    //Get connection handle
    uint16_t conn_handle;
    int rc = bt_hci_get_conn_handle(default_conn, &conn_handle);
    if(rc){
        printk("Failed to get connection handle\n");
        return;
    }

    //Get Tx and RSSI value
    int8_t rssi = 0;
    int8_t link_margin = 0;
    int8_t tx;
    get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle, &tx);
    read_conn_rssi(conn_handle, &rssi);

    //Set new Tx for central and peripheral based on deviation from optimal RSSI
    link_margin = STRONG_RSSI - rssi;
    //printk("Link margin(%d), tx(%d)\n", link_margin, tx);
    if(20 > link_margin && link_margin > 10 && tx < 8){
        set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle, tx+1);
        set_remote_tx_power(1);
    }
    else if(link_margin > 20 && tx < 8){
        set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle, tx+5);
        set_remote_tx_power(5);
    }
    else if(-20 < link_margin && link_margin < -10 && tx > -46){
        set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle, tx-1);
        set_remote_tx_power(-1);
    }
    else if(link_margin < -20 && tx > -46){
        set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle, tx-5);
        set_remote_tx_power(-5);
    }
}






int main(void){
    int err;

    printk("Starting BLE Central\n");

    err = dk_leds_init();
	if (err) {
		printk("LEDs init failed (err %d)\n", err);
		return 0;
	}
    
    err = bt_enable(NULL);
    if(err){
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    else{
        printk("LED initialization successful\n");
    }

    scan_init();

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if(err){
        printk("Scanning failed to start (err: %d)\n", err);
        return 0;
    }

    printk("Scanning for peripherals...\n");

    while(1){
        if(is_connected && !polling_started){
            k_work_init_delayable(&tx_rssi_work, tx_rssi_work_handler);
            k_work_reschedule(&tx_rssi_work, K_MSEC(500));
            polling_started = true;
        }
        #ifdef RSSI_power_control
            power_control();
        #endif
        k_sleep(K_MSEC(500));
    }
}