# Nordic_power_control_and_path_loss_monitoring
During my internship at Nordic Semiconductor, I developed two samples that establish a Bluetooth Low Energy (BLE) connection between a central and a peripheral device.

The first sample, a power control demo, monitors the Received Signal Strength Indicator (RSSI) and adjusts the transmit (Tx) power of the peripheral accordingly.

The second sample monitors path loss—defined as the difference between the transmitted signal strength and the RSSI (i.e., path loss = Tx - RSSI). Based on the path loss value, the system indicates the current state using LEDs:

    Low path loss: LED0 lights up

    Medium path loss: LED1 lights up

    High path loss: LED2 lights up

Both samples were tested using two nRF54L15 Development Kits (DKs).
