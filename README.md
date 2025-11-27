# capstone-gesture-controlled-drone
GROUP 24 CAPSTONE PROJECT: Gesture-Controlled Drone with Object-Carrying Capabilities

PHASE A - SOFTWARE GOALS:
- MAIN GOAL: Implement gesture mapping/recognition algorithm using data from the glove (flex sensors + MPU6050 IMU) -> Python
- GLOVE SOFTWARE: Implement glove firmware on Arduino to get readings -> C/C++
- The main goal will be implemented on Python so that it:
    - receives glove data,
    - recognizes hand shapes and specific gestures,
    - maps those gestures to drone movement (based on gesture mapping table in docs/gesture_mapping_table).
 
PHASE B - SOFTWARE GOALS:
- to be updated

REPO CLASSIFICATION:
- 'glove_firmware/': Arduino C++ code for flex sensors, IMU, and communication.
- 'software/': Python code for gesture mapping, data I/O, and logging data.
- 'docs/': Research, files, diagrams.
