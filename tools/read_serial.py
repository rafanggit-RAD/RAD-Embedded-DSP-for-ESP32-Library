import serial
import time

log_file_path = "serial_log.txt"
try:
    # Menggunakan baud rate 115200 sesuai dengan kecepatan komunikasi serial aktual
    ser = serial.Serial('COM12', 115200, timeout=1)
    print("Opened COM12 at 115200 baud, reading silently for 8 seconds...")
    start_time = time.time()
    
    # Reset the chip via DTR/RTS
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.1)
    ser.setRTS(False)
    time.sleep(0.1)
    
    with open(log_file_path, "wb") as log_file:
        while time.time() - start_time < 8:
            line = ser.readline()
            if line:
                log_file.write(line)
                log_file.flush()
                
    ser.close()
    print(f"Logged successfully in binary mode to {log_file_path}")
except Exception as e:
    print(f"Error: {e}")
