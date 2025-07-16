There are two ways to monitor the Tx and RSSI data:

    Using the Terminal: Open the VCOM port of the central unit, where the data is printed in real time. Which port will

    vary depending on the platform that is used, but during testing, it was VCOM1.

    Using a Graphical Plot: Navigate to the tools folder and run the following command in a terminal to visualize the data:

    python3 plot_path_loss.py

Note: When using the Python script to plot the graph, make sure the terminal to the VCOM port where the data is being printed is closed, as the script requires exclusive access to the port.

Also, ensure that line 9 in the script:

    ser = serial.Serial('/dev/ttyACM1', 115200, timeout=1)

is referencing the correct serial port. In some cases, the device may appear as /dev/ttyACM0 instead of /dev/ttyACM1.
