.. _bluetooth_power_control:

Bluetooth: Power Control
#############################

.. contents::
   :local:
   :depth: 2

The Power Control Sample demonstrates how to monitor Bluetooth® LE signal strength (RSSI) and dynamically adjust the transmit (Tx) power of a connected Peripheral device. The Central collects RSSI data and sends power control commands to optimize the connection quality.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

You must use two compatible development kits to run this sample: one programmed with the Central application, and one with the Peripheral application.

Overview
********

This sample illustrates Bluetooth® LE power control by having the Central read the RSSI of an active connection and use it to adjust the Peripheral's transmit power. The Central directly invokes SoftDevice Controller (SDC) APIs to configure power control behavior based on RSSI measurements. This approach only works on single-core platforms where the Bluetooth Controller and application run on the same core.

There are two ways to monitor the connection parameters:

- **Terminal Output:** Connect to the Central device's VCOM port (e.g., ``VCOM1``) to view RSSI and Tx power logs in real time.
- **Graphical Plotting:** Use the provided script to visualize signal strength over time.

.. code-block:: console

   python3 tools/plot_path_loss.py

.. note::

   Make sure no terminal is open on the same serial port when using the Python plotting script, as it requires exclusive access.

   Also, ensure that line 9 in the script correctly points to the active serial port:

   .. code-block:: python

      ser = serial.Serial('/dev/ttyACM1', 115200, timeout=1)

   Adjust ``/dev/ttyACM1`` if necessary (e.g., ``/dev/ttyACM0``).

.. note::

   **Single-Core Platforms Only (e.g., nRF52840)**

   This sample only works on single-core SoCs because it directly calls ``sdc_*`` functions (e.g., ``sdc_hci_cmd_vs_set_power_control_request_params()``), which are only accessible on the core running the Bluetooth Controller.

   On dual-core platforms like nRF5340 or nRF54h20, the Bluetooth Controller runs on the **network core**, making these calls inaccessible from the application core and resulting in linker errors.

Workaround for Dual-Core SoCs (e.g., nRF5340)
=============================================

To adapt this sample for dual-core SoCs:

- **Remove all** ``sdc_*`` **function calls** from the application.
- **Use Zephyr HCI commands** to interface with the controller:

.. code-block:: c

   struct net_buf *buf = bt_hci_cmd_create(...);
   bt_hci_cmd_send_sync(...);

Refer to the ``hci_pwr_ctrl`` sample or Nordic DevZone for reference implementations.

User interface
**************

Central:

- **VCOM output:** Displays live RSSI and Tx power data.
- **Optional plotting:** Provides a visual graph of signal variation over time.

Peripheral:

- No direct user interface. Automatically accepts connections and responds to Tx power control commands.

Building and running
********************

.. |sample path| replace:: :file:`samples/bluetooth/power_control_demo`
.. include:: /includes/build_and_run.txt

.. |sample_or_app| replace:: sample
.. |ipc_radio_dir| replace:: :file:`sysbuild/ipc_radio`
.. include:: /includes/ipc_radio_conf.txt

Testing
=======

To test the sample:

1. Program one DK with the **Peripheral Power Control** firmware.
2. Program another DK with the **Central Power Control** firmware.
3. Open a terminal on the Central's VCOM port or run the plotting script.
4. Power both devices.
5. The Central will scan and connect to the Peripheral automatically.
6. Observe RSSI and Tx power data printed or plotted.
7. Move the devices or add obstructions to see dynamic power adjustment.

Sample output
*************

Typical logs from the Central device:

.. code-block:: console

   [00:00:01.000,000] I: Starting BLE Central
   [00:00:01.500,000] I: Scanning for peripherals...
   [00:00:03.020,000] I: Found peripheral: FF:EE:DD:CC:BB:AA (random)
   [00:00:03.050,000] I: Connection to FF:EE:DD:CC:BB:AA (random)
   [00:00:03.800,000] I: Connected to FF:EE:DD:CC:BB:AA (random)
   [00:00:05.010,000] I: Successful initialization of power control
   [00:00:06.010,000] I: Tx: 0 dBm, RSSI: -33 dBm
   [00:00:07.010,000] I: Tx: -9 dBm, RSSI: -45 dBm

Configuration
*************

|config|

Configuration options
*********************

:kconfig:option:`CONFIG_BT_CENTRAL`
    Enables the Bluetooth® LE Central role for the Central application.

:kconfig:option:`CONFIG_BT_PERIPHERAL`
    Enables the Bluetooth® LE Peripheral role for the Peripheral application.

:kconfig:option:`CONFIG_BT_DEVICE_NAME`
    Sets the advertised name of the Peripheral (default: ``power_control_peripheral``).

:kconfig:option:`CONFIG_BT_SCAN`
    Enables scanning for Bluetooth LE advertisements (Central only).

:kconfig:option:`CONFIG_BT_SCAN_FILTER_ENABLE`
    Enables name-based filtering for Bluetooth scan results (Central only).

:kconfig:option:`CONFIG_BT_CTLR_CONN_RSSI`
    Enables controller-side RSSI reporting for active connections.

:kconfig:option:`CONFIG_BT_TRANSMIT_POWER_CONTROL`
    Enables support for Bluetooth LE transmit power control.

:kconfig:option:`CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL`
    Allows dynamic adjustment of transmit power based on RSSI feedback.

:kconfig:option:`CONFIG_BT_HCI_RAW`
    Enables raw HCI interface for communication between the application and controller cores (used in IPC radio configurations).

:kconfig:option:`CONFIG_LOG`
    Enables logging via Zephyr's logging subsystem (optional but useful for debugging).

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`nrf_bt_scan_readme`

It also relies on Zephyr's Bluetooth and HCI layers:

* :ref:`zephyr:bluetooth_api`
* :ref:`zephyr:logging_api`
* :ref:`zephyr:hci_driver`