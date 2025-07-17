.. _bluetooth_path_loss_monitoring:

Bluetooth: Path Loss Monitoring
###############################

.. contents::
   :local:
   :depth: 2

This sample demonstrates Bluetooth® LE path loss monitoring between two devices — one acting as a Central, the other as a Peripheral.

Path loss is calculated as the difference between transmitted power and received signal strength indicator (RSSI):

::

   path loss = Tx - RSSI

Based on the calculated path loss, the Central device provides a visual indication of signal condition using onboard LEDs.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

You need **two development kits**, both supporting Bluetooth LE. This sample was tested with:

- Two **nRF54L15 DKs**

One kit runs the **Central Path Loss Monitoring** role, the other runs the **Peripheral Path Loss Monitoring** role.

Overview
********

This sample is split into two applications:

- :ref:`Central <bluetooth_path_loss_monitoring>`: Connects to a peripheral and calculates the path loss in real time.
- :ref:`Peripheral <bluetooth_path_loss_monitoring>`: Advertises itself for connection and supports Bluetooth Path Loss Monitoring.

### Central Role

The central device scans for peripherals advertising the name `Path_Loss_Peripheral`. When a connection is established, it starts monitoring path loss and categorizes the signal quality into three zones:

- **Low path loss (good signal)**: ``LED0`` is turned on
- **Medium path loss**: ``LED1`` is turned on
- **High path loss (poor signal)**: ``LED2`` is turned on

### Peripheral Role

The peripheral simply advertises with the name `Path_Loss_Peripheral` and enables path loss monitoring functionality.

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

1. Program one DK with the **Peripheral Path Loss Monitoring** application.
2. Program the second DK with the **Central Path Loss Monitoring** application.
3. Power both devices and reset if necessary.
4. The central device scans for a peripheral named `Path_Loss_Peripheral`.
5. Upon successful connection, observe the LEDs on the central DK:

   - ``LED0`` lights up when the signal is strong (low path loss)
   - ``LED1`` lights up for moderate signal (medium path loss)
   - ``LED2`` lights up when the signal is weak (high path loss)

6. Move the devices around or introduce obstacles to simulate signal degradation.

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`nrf_bt_scan_readme`
* :ref:`dk_buttons_and_leds_readme`

It also uses the following Zephyr libraries:

* :ref:`zephyr:bluetooth_api`:
  * :file:`include/bluetooth/bluetooth.h`
  * :file:`include/bluetooth/conn.h`
  * :file:`include/bluetooth/gatt.h`
  * :file:`include/bluetooth/uuid.h`
  * :file:`include/bluetooth/hci.h`
* :file:`include/zephyr.h`
* :file:`include/sys/printk.h`
* :file:`include/sys/byteorder.h`

Kconfig Options
===============

Central-specific (`central_path_loss_monitoring/prj.conf`):

* ``CONFIG_BT_CENTRAL`` – Enables Central role support
* ``CONFIG_BT_SCAN`` – Enables Bluetooth scanning
* ``CONFIG_BT_PATH_LOSS_MONITORING`` – Enables path loss monitoring
* ``CONFIG_BT_SCAN_FILTER_ENABLE`` – Enables scan filtering
* ``CONFIG_DK_LIBRARY`` – Enables LED output

Peripheral-specific (`peripheral_path_loss_monitoring/prj.conf`):

* ``CONFIG_BT_PERIPHERAL`` – Enables Peripheral role
* ``CONFIG_BT_PATH_LOSS_MONITORING`` – Enables path loss monitoring
* ``CONFIG_BT_DEVICE_NAME="Path_Loss_Peripheral"`` – Sets the advertising name
* ``CONFIG_DK_LIBRARY`` – Enables LED output

