# ESP32 IoT Engine Monitoring 
_Devil's Invent Hackathon Project_  
Predictive Maintenance – IoT system using ESP32, DHT11, and MPU6050 to track engine temperature, humidity, and vibration (RMS &amp; Peak in g). Data is sent to ThingSpeak for real-time visualization, helping detect anomalies and support predictive maintenance.


## Overview  
This project was built as part of the **Honeywell's Hackathon**.  
It uses an **ESP32 microcontroller** connected with:  
- **DHT11 sensor** → to measure temperature & humidity  
- **MPU6050 sensor** → to measure vibration (RMS and peak acceleration in g)  

The ESP32 reads sensor data, processes it, and uploads it to **ThingSpeak** for real-time visualization.  

## Features  
- Temperature, humidity, and heat index monitoring  
- Vibration analysis using RMS and peak values  
- Aggregated data sampled every 5 seconds  
- Upload to ThingSpeak every 20 seconds  
- Real-time IoT dashboard with graphs  

## Hardware Connections  

### ESP32 to DHT11  
- DHT11 **DATA** → GPIO 15  
- VCC → 3.3V  
- GND → GND  

### ESP32 to MPU6050  
- SDA → GPIO 21  
- SCL → GPIO 22  
- VCC → 3.3V  
- GND → GND  

## Software  
- **Arduino IDE** with ESP32 board support  
- Required libraries:  
  - `WiFi.h`  
  - `HTTPClient.h`  
  - `DHT.h`  
  - `MPU6050_light.h`  

## ThingSpeak Setup  
1. Create a ThingSpeak channel.  
2. Add 5 fields:  
   - Field 1: Temperature (°C)  
   - Field 2: Humidity (%)  
   - Field 3: Heat Index (°C)  
   - Field 4: Vibration RMS (g)  
   - Field 5: Vibration Peak (g)  
3. Copy your API key and place it in the code.  

## Data Flow  
1. ESP32 collects data from DHT11 and MPU6050.  
2. MPU6050 vibration data is sampled for **5 seconds** to compute RMS and Peak.  
3. Every **20 seconds**, ESP32 uploads data to ThingSpeak.  
4. ThingSpeak visualizes vibration and environmental conditions on live charts.  

## Applications  
- Industrial vibration monitoring  
- Environmental sensing  
- Predictive maintenance  
- Smart factories & IoT dashboards  

## Team  
Built by **Ishan Srivastava, Aadi Kadam, and team**  
Arizona State University – Robotics & Autonomous Systems (RAS)  
Honeywell Hackathon, Tempe  

---  
