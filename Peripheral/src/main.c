#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/sys/byteorder.h>


//Declare connection variables
static struct bt_conn *default_conn;
static uint16_t default_conn_handle;

//Advertising parameter for connectable advertising
static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
    (BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY), //Connectable advertising and use identity address
    800,                                               //Min Advertising Interval 500ms (800*0.625ms)
    801,                                               //Max Advertising Interval 500.625ms (801*0.625ms)
    NULL                                               //Set to NULL for undirected advertising
);

LOG_MODULE_REGISTER(Peripheral_Unit, LOG_LEVEL_INF); //Sets up logging for peripheral module. Log level set to info

//Defines for name
#define DEVICE_NAME              CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN          (sizeof(DEVICE_NAME)-1)

//#define RUN_STATUS_LED           DK_LED1
//#define RUN_LED_BLINK_INTERVAL   1000

static struct k_work adv_work;   //Structure used to submit work

//Advertising data
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)), //Discoverable and only support for BLE
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),         //Full name of device
};

//Scan response data. Complementary data to the advertising data, if the central wants more information
static const struct bt_data sd[] = {
    //16-bytes UUID of the LBS service
    BT_DATA_BYTES(
        BT_DATA_UUID128_ALL,  //Type of data being sent
        BT_UUID_128_ENCODE(   //The 16-bytes or 128-bit representing the UUID LBS service
            0x00001523,
            0x1212,
            0xefde,
            0x1523,
            0x785feabcd123)),
};



//Function to resume advertising after a disconnection
static void adv_work_handler(struct k_work *work){
    int err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if(err){
        printk("Advertising failed to start (err: %d)\n", err);
        return;
    }
    printk("Advertising successfully started\n");
}



//Function to submit the work item adv_work to the system work queue
static void advertising_start(void){
    k_work_submit(&adv_work);
}



//Function for notifying when a connection is stopped and the resource becomes available again
//Reinitiates advertising
static void recycled_cb(void){
    printk("Connection object available from previous conn. Disconnect is complete!\n");
    advertising_start();
}



//Function to set tx power
static void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
{
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(struct bt_hci_cp_vs_write_tx_power_level));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;
	cp->tx_power_level = tx_pwr_lvl;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				   buf, &rsp);
	if (err) {
		printk("Set Tx power err: %d\n", err);
		return;
	}

	rp = (void *)rsp->data;
	//printk("Actual Tx Power: %d\n", rp->selected_tx_power);

	net_buf_unref(rsp);
}



//Function to get Tx power
static void get_tx_power(uint8_t handle_type, uint16_t handle, int8_t *tx_pwr_lvl)
{
	struct bt_hci_cp_vs_read_tx_power_level *cp;
	struct bt_hci_rp_vs_read_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	*tx_pwr_lvl = 0xFF;
	buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, sizeof(struct bt_hci_cp_vs_read_tx_power_level));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_TX_POWER_LEVEL,
				   buf, &rsp);
	if (err) {
		printk("Read Tx power err: %d\n", err);
		return;
	}

	rp = (void *)rsp->data;
	*tx_pwr_lvl = rp->tx_power_level;

	net_buf_unref(rsp);
}



//Function to read rssi value
static void read_conn_rssi(uint16_t handle, int8_t *rssi)
{
	struct net_buf *buf, *rsp = NULL;
	struct bt_hci_cp_read_rssi *cp;
	struct bt_hci_rp_read_rssi *rp;

	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(struct bt_hci_cp_read_rssi));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
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



//Function for connection
static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int8_t txp;
	int ret;

	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
	} else {
		default_conn = bt_conn_ref(conn);
		ret = bt_hci_get_conn_handle(default_conn,
					     &default_conn_handle);
		if (ret) {
			printk("No connection handle (err %d)\n", ret);
		} else {
			//Setting initial tx power
			set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, default_conn_handle, 0);
			get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, default_conn_handle, &txp);
			printk("Connection (%d) - Tx Power = %d\n", default_conn_handle, txp);
		}
	}
}



//Function for disconnection
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}



//Define the callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
    .recycled = recycled_cb,
};




int main(void){
    int err;

    LOG_INF("Starting the peripheral unit");

    //Change the random static address
    bt_addr_le_t addr;
    //Defining address and buffer location
    err = bt_addr_le_from_str("FF:EE:DD:CC:BB:AA", "random", &addr);
    if(err < 0){
        printk("Invalid BT address (err: %d)\n", err);
    }
    //Setting the address
    err = bt_id_create(&addr, NULL);
    if(err < 0){
        printk("Creating new ID failed (err: %d)\n", err);
    }

    //Initialize bluetooth subsystem
    err = bt_enable(NULL);
    if(err){
        LOG_ERR("Bluetooth init failed (err: %d)\n", err); 
        return -1;
    }
    LOG_INF("Bluetooth initialized\n");

    //Start connectable advertising
    k_work_init(&adv_work, adv_work_handler);
    advertising_start();

    LOG_INF("Advertising successfully started\n");

    int8_t rssi = 0xFF;
    int8_t txp;
    while(1){
        read_conn_rssi(default_conn_handle, &rssi);
        get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, default_conn_handle, &txp);
        printk("Connected (%d), RSSI: %d, Tx: %d\n", default_conn_handle, rssi, txp);
        k_sleep(K_SECONDS(2));
    }
}