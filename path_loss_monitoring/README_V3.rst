.. _bluetooth_path_loss_monitoring:

Bluetooth: Path Loss Monitoring
###############################

.. contents::
   :local:
   :depth: 2

The Path Loss Monitoring sample demonstrates how to evaluate Bluetooth® LE signal quality using the Path Loss Monitoring feature. It consists of a Central and a Peripheral device. The Central continuously monitors the signal strength and classifies the connection quality using onboard LEDs.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

You must use two compatible development kits to run this sample: one programmed with the Central application, and one with the Peripheral application.

Overview
********

This sample illustrates Bluetooth® LE Path Loss Monitoring by calculating the signal degradation between two connected devices. The Central application scans for a Peripheral named ``Path_Loss_Peripheral`` and connects to it. After a successful connection, the Central measures path loss using the formula:

::

   path loss = Tx Power - RSSI

The application defines three signal quality zones based on the path loss value:

- **Low path loss (strong signal):** LED0 is turned on.
- **Medium path loss:** LED1 is turned on.
- **High path loss (weak signal):** LED2 is turned on.

The Peripheral simply advertises its presence and accepts connections.

User interface
**************

Central:

- **LED0:** Indicates **Low** path loss (good signal).
- **LED1:** Indicates **Medium** path loss.
- **LED2:** Indicates **High** path loss (weak signal).

No buttons are used.

Peripheral:

- No direct user interface. The device advertises under the name ``Path_Loss_Peripheral``.

Building and running
********************

.. |sample path| replace:: :file:`samples/bluetooth/path_loss_monitoring`
.. include:: /includes/build_and_run.txt

.. |sample_or_app| replace:: sample
.. |ipc_radio_dir| replace:: :file:`sysbuild/ipc_radio`
.. include:: /includes/ipc_radio_conf.txt

Testing
=======

To test the sample:

1. Program one DK with the **Peripheral Path Loss Monitoring** firmware.
2. Program another DK with the **Central Path Loss Monitoring** firmware.
3. Power both devices.
4. The Central will start scanning and connect automatically to the Peripheral.
5. Once connected, observe the LEDs on the Central to evaluate signal strength:
   - Bring the devices closer or move them farther apart to change the signal quality.
   - Optionally place an obstruction between the devices to see how this affects signal quality.
6. Monitor the RTT console for logs showing connection status and real-time path loss.

Sample output
*************

Typical logs from the Central device:

.. code-block:: console

   [00:00:01.000,000] <inf> Central_Unit: Starting Path Loss Monitoring sample
   [00:00:01.500,000] <inf> Central_Unit: Scanning for peripherals...
   [00:00:03.020,000] <inf> Central_Unit: Found peripheral: FF:EE:DD:CC:BB:AA
   [00:00:03.050,000] <inf> Central_Unit: Connection to FF:EE:DD:CC:BB:AA
   [00:00:03.800,000] <inf> Central_Unit: Connected to FF:EE:DD:CC:BB:AA
   [00:00:03.810,000] <inf> Central_Unit: Path loss monitoring enabled
   [00:00:05.010,000] <inf> Central_Unit: Zone 0. Path loss: 30 dBm
   [00:00:06.010,000] <inf> Central_Unit: Zone 1. Path loss: 45 dBm
   [00:00:07.010,000] <inf> Central_Unit: Zone 2. Path loss: 63 dBm

Configuration
*************

|config|

Configuration options
*********************

Check and configure the following Kconfig options:

:kconfig:option:`CONFIG_BT_CENTRAL`
    Enables the Bluetooth® LE Central role (Central application).

:kconfig:option:`CONFIG_BT_PERIPHERAL`
    Enables the Bluetooth® LE Peripheral role (Peripheral application).

:kconfig:option:`CONFIG_BT_DEVICE_NAME`
    Sets the advertising name of the Peripheral. Default is ``Path_Loss_Peripheral``.

:kconfig:option:`CONFIG_BT_SCAN`
    Enables scanning for Bluetooth LE advertisements (Central only).

:kconfig:option:`CONFIG_BT_SCAN_FILTER_ENABLE`
    Enables name-based filtering for scan results (Central only).

:kconfig:option:`CONFIG_BT_PATH_LOSS_MONITORING`
    Enables the Bluetooth® LE Path Loss Monitoring feature on both devices.

:kconfig:option:`CONFIG_DK_LIBRARY`
    Enables the LEDs on the development kits.

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`nrf_bt_scan_readme`
* :ref:`dk_buttons_and_leds_readme`

It uses the following Zephyr libraries:

* :ref:`zephyr:bluetooth_api`
