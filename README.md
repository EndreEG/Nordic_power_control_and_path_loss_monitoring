# Nordic_power_control_and_path_loss_monitoring
A sample I developed during my internship at Nordic Semiconductor. It sets up a BLE connection between a central and a peripheral, and monitors the RSSI value to control the Tx power. Path loss monitoring is also enabled, and the states are indicated by different LEDs. Path loss is defined as signal lost from emitter to receiver, i.e path loss = Tx - RSSI. If the path loss is low, LED2 will light up, path loss medium LED0, and path loss high LED1.

The setup that was used were two nRF54L15 DKs.
