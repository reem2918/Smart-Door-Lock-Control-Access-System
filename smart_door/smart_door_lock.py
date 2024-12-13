#importing necessary modules
import serial
import time
import requests

#ThingSpeak API URL and Write API Key
THINGSPEAK_URL = "https://api.thingspeak.com/update"
API_KEY = "KZAIJ07PJONKHPCB"

#Serial connection with port COM7 at 9600 baud
ser = serial.Serial('COM11', 9600)

#send data to ThingSpeak
def send_to_thingspeak(door_state):
    try:
        payload = {
            'api_key': API_KEY,
            'field1': door_state  #Field 1 on ThingSpeak for door state
        }
        response = requests.get(THINGSPEAK_URL, params=payload)
        if response.status_code == 200:
            print(f"Data sent to ThingSpeak: Door State = {door_state}")
        else:
            print("Error sending data to ThingSpeak")
    except Exception as e:
        print(f"Error: {e}")

#loop to listen for serial input from Arduino
while True:
    if ser.in_waiting > 0:
        message = ser.readline().decode('utf-8').strip()
        if "Unlocked" in message:
            send_to_thingspeak(1)  #Door is unlocked
        elif "Locked" in message:
            send_to_thingspeak(0)  #Door is locked
    time.sleep(1)
