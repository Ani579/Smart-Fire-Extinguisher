# Dual-Model Fire Detection System

This project is a real-time fire detection system that dynamically switches between two YOLO models based on network connectivity.

## Features
- **Dynamic Model Switching**: Monitors network connectivity. If the connection drops for more than 5 seconds, it automatically switches from the primary AI model to a local "light" AI model to ensure continuous operation. When the connection is restored, it switches back.
- **Fire Type Classification**: Architecture provided to classify specific fire types (e.g., short-circuit, spark).

## Setup Instructions

1. **Install Dependencies**:
   Open your terminal in this directory and run:
   ```bash
   pip install -r requirements.txt
   ```

2. **Custom Weights Needed**:
   The script currently uses standard YOLOv8 pre-trained weights (`yolov8s.pt` and `yolov8n.pt`) as placeholders for the main and light models. Standard YOLO models are trained on the COCO dataset, which does **not** include "fire" classes.
   
   To detect fire and its types, you will need to:
   - Train a custom YOLOv8 model on a dataset containing images of fires (labeled with classes like `fire`, `short_circuit_fire`, `spark_fire`).
   - Replace the `MAIN_MODEL_PATH` and `LIGHT_MODEL_PATH` in `main.py` with the paths to your custom trained `.pt` files.

3. **Run the System**:
   ```bash
   python main.py
   ```
   Press `q` to quit the video stream.

## How it works
A background thread continuously pings a reliable server (like Google DNS) to check internet connectivity. If the ping fails for 5 seconds, a thread-safe lock swaps the active inference model from the main model to the lightweight local model. The video processing thread uses whichever model is currently active, drawing bounding boxes and labels on the detected fire instances.
