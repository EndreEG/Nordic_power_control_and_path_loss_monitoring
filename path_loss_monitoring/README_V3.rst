.. _bluetooth_path_loss_monitoring:

Bluetooth: Path Loss Monitoring
###############################

.. contents::
   :local:
   :depth: 2

The Path Loss Monitoring sample demonstrates how to evaluate signal quality between two Bluetooth® LE devices based on path loss measurements. It uses visual feedback to indicate connection quality between a Central and a Peripheral device.

This sample consists of two applications:

- **Central Path Loss Monitoring** – Connects to a Peripheral and calculates path loss in real time.
- **Peripheral Path Loss Monitoring** – Advertises itself and supports path loss monitoring functionality.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

You need two compatible development kits to test this sample. It was tested with two **nRF54L15 DKs**.

Overview
********

The sample demonstrates the use of the Bluetooth® LE Path Loss Monitoring feature to classify signal quality. Path loss is calculated as:

::

   path loss = Tx - RSSI

The Central device scans for and connects to a Peripheral named `Path_Loss_Peripheral`. After establishing a connection, it monitors the path loss in real time and categorizes the signal into three zones:

- **Low path loss** (good signal): ``LED 0`` is turned on
- **Medium path loss**: ``LED 1`` is turned on
- **High path loss** (poor signal): ``LED 2`` is turned on

This provides a visual indicator for use cases like range testing, signal diagnostics, or location-aware applications.

User interface
**************

Central DK LED behavior:

- **LED 0**: Low path loss (strong signal)
- **LED 1**: Medium path loss
- **LED 2**: High path loss (weak signal)

No buttons are used in this sample.

Building and running
********************

.. |sample path| replace:: :file:`samples/bluetooth/path_loss_monitoring`

.. include:: /includes/build_and_run.txt

.. |sample_or_app| replace:: sample
.. |ipc_radio_dir| replace:: :file:`sysbuild/ipc_radio`

.. include:: /includes/ipc_radio_conf.txt

Testing
=======

After programming the sample to your development kits, follow these steps:

1. Program one DK with the **Peripheral Path Loss Monitoring** application.
2. Program a second DK with the **Central Path Loss Monitoring** application.
3. Power both devices.
4. The Central scans for a device named `Path_Loss_Peripheral` and connects automatically.
5. Observe the LED behavior on the Central DK to evaluate signal quality:
   - ``LED 0`` lights up when signal is strong (low path loss)
   - ``LED 1`` lights up with medium signal
   - ``LED 2`` lights up when signal is weak (high path loss)
6. Change the distance between the DKs or introduce obstacles to observe how the LEDs react.

Sample output
*************

The following output may be seen on the RTT console of the Central device:

.. code-block:: console

   [INFO] Scanning for devices...
   [INFO] Connected to Path_Loss_Peripheral
   [INFO] Path loss: 42 dB → Zone: MEDIUM
   [INFO] Path loss: 58 dB → Zone: HIGH
   [INFO] Path loss: 31 dB → Zone: LOW

Configuration
*************

|config|

Configuration options
*********************

Check and configure the following Kconfig options:

:kconfig:option:`CONFIG_BT_CENTRAL`
    Enables the Bluetooth® LE Central role (Central app only).

:kconfig:option:`CONFIG_BT_PERIPHERAL`
    Enables the Bluetooth® LE Peripheral role (Peripheral app only).

:kconfig:option:`CONFIG_BT_SCAN`
    Enables scanning for Bluetooth LE advertisements.

:kconfig:option:`CONFIG_BT_SCAN_FILTER_ENABLE`
    Enables name-based scan filtering.

:kconfig:option:`CONFIG_BT_DEVICE_NAME`
    Sets the device name to `Path_Loss_Peripheral`.

:kconfig:option:`CONFIG_BT_PATH_LOSS_MONITORING`
    Enables support for the Bluetooth LE Path Loss Monitoring feature.

:kconfig:option:`CONFIG_DK_LIBRARY`
    Enables use of LEDs and buttons on the development kit.

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`nrf_bt_scan_readme`
* :ref:`dk_buttons_and_leds_readme`

It uses the following Zephyr libraries:

* :ref:`zephyr:bluetooth_api`:
  * :file:`include/bluetooth/bluetooth.h`
  * :file:`include/bluetooth/conn.h`
  * :file:`include/bluetooth/gatt.h`
  * :file:`include/bluetooth/uuid.h`
  * :file:`include/bluetooth/hci.h`
* :ref:`zephyr:kernel_api`:
  * :file:`include/kernel.h`

Standard headers used:

* :file:`include/zephyr.h`
* :file:`include/sys/printk.h`
* :file:`include/sys/byteorder.h`

