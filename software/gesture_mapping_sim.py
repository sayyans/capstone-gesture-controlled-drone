from dataclasses import dataclass
from typing import List

# class definitions
@dataclass
class GloveData:
    flex: List[float]
    pitch: float
    roll: float
    isStill: bool

class Gesture():
    MOVE_BACKWARD = "MOVE_BACKWARD"
    MOVE_FORWARD = "MOVE_FORWARD"
    MOVE_RIGHT = "MOVE_RIGHT"
    MOVE_LEFT = "MOVE_LEFT"
    MOVE_UP = "MOVE_UP"
    MOVE_DOWN = "MOVE_DOWN"
    OPEN_GRIPPER = "OPEN_GRIPPER"
    CLOSE_GRIPPER = "CLOSE_GRIPPER"
    NONE = "NONE"

class HandPattern():
    OPEN_HAND = "OPEN_HAND"
    FIST = "FIST"
    TWO_FINGERS_OUT = "TWO_FINGERS_OUT"
    OTHER = "OTHER"

class IMUCondition():
    NEUTRAL = "NEUTRAL"
    FORWARD_TILT = "FORWARD_TILT"
    BACKWARD_TILT = "BACKWARD_TILT"
    RIGHT_TILT = "RIGHT_TILT"
    LEFT_TILT = "LEFT_TILT"
    POINT_TWO_UP = "POINT_TWO_UP"
    POINT_TWO_DOWN = "POINT_TWO_DOWN"
    NONE = "NONE"

# thresholds defined in gesture mapping table
PITCH_BACKWARD_THRESHOLD = 15
PITCH_FORWARD_THRESHOLD = -15
ROLL_RIGHT_THRESHOLD = 20
ROLL_LEFT_THRESHOLD = -20
PITCH_ASCEND_THRESHOLD = 80
PITCH_DESCEND_THRESHOLD = -80
NEUTRAL_PITCH_THRESHOLD = 10
NEUTRAL_ROLL_THRESHOLD = 10

# hand pattern recognition
def detect_hand_pattern(flex: List[float]) -> HandPattern:
    straight_threshold = 0.3
    bent_threshold = 0.7

    if len(flex) < 5:
        return HandPattern.OTHER
    
    thumb  = flex[0]
    index  = flex[1]
    middle = flex[2]
    ring   = flex[3]
    pinky  = flex[4]

    if all(v < straight_threshold for v in flex[:5]):
        return HandPattern.OPEN_HAND

    if all(v > bent_threshold for v in flex[:5]):
        return HandPattern.FIST

    if (index  < straight_threshold and
        middle < straight_threshold and
        thumb  > bent_threshold and
        ring   > bent_threshold and
        pinky  > bent_threshold
    ):
        return HandPattern.TWO_FINGERS_OUT

    return HandPattern.OTHER

def detect_imu_state(pitch: float, roll: float, isStill: bool) -> IMUCondition:
    if (abs(pitch) < NEUTRAL_PITCH_THRESHOLD and abs(roll) < NEUTRAL_ROLL_THRESHOLD and isStill):
        return IMUCondition.NEUTRAL
    
    if pitch > PITCH_ASCEND_THRESHOLD:
        return IMUCondition.POINT_TWO_UP
    
    if pitch < PITCH_DESCEND_THRESHOLD:
        return IMUCondition.POINT_TWO_DOWN
    
    if pitch < PITCH_FORWARD_THRESHOLD:
        return IMUCondition.FORWARD_TILT
    
    if pitch > PITCH_BACKWARD_THRESHOLD:
        return IMUCondition.BACKWARD_TILT
    
    if roll > ROLL_RIGHT_THRESHOLD:
        return IMUCondition.RIGHT_TILT
    
    if roll < ROLL_LEFT_THRESHOLD:
        return IMUCondition.LEFT_TILT
    
    return IMUCondition.NONE

# gesture mapping
def classify_gesture(data: GloveData) -> Gesture:
    hand = detect_hand_pattern(data.flex)
    imu = detect_imu_state(data.pitch, data.roll, data.isStill)

    match (hand, imu):
        # all fingers straight (used in MOVE_FORWARD and MOVE_BACKWARD)
        case (HandPattern.OPEN_HAND, IMUCondition.FORWARD_TILT):
            return Gesture.MOVE_FORWARD
        
        case (HandPattern.OPEN_HAND, IMUCondition.BACKWARD_TILT):
            return Gesture.MOVE_BACKWARD

        # index and middle fingers out, the rest bent (used in MOVE_RIGHT, MOVE_LEFT, MOVE_DOWN, and MOVE_UP)
        case (HandPattern.TWO_FINGERS_OUT, IMUCondition.RIGHT_TILT):
            return Gesture.MOVE_RIGHT
        
        case (HandPattern.TWO_FINGERS_OUT, IMUCondition.LEFT_TILT):
            return Gesture.MOVE_LEFT
        
        case (HandPattern.TWO_FINGERS_OUT, IMUCondition.POINT_TWO_UP):
            return Gesture.MOVE_UP
        
        case (HandPattern.TWO_FINGERS_OUT, IMUCondition.POINT_TWO_DOWN):
            return Gesture.MOVE_DOWN

        # open hand (used in OPEN_GRIPPER)
        case (HandPattern.OPEN_HAND, IMUCondition.NEUTRAL):
            return Gesture.OPEN_GRIPPER
        
        # fist (used in CLOSE_GRIPPER)
        case (HandPattern.FIST, IMUCondition.NEUTRAL):
            return Gesture.CLOSE_GRIPPER

    return Gesture.NONE

# gesture simulator test     
def testing(name: str, data: GloveData, expected: Gesture):
    actual = classify_gesture(data)
    print(f"Test {name}")
    print(f"flex = {data.flex}")
    print(f"pitch = {data.pitch}")
    print(f"roll = {data.roll}")
    print(f"still = {data.isStill}")
    print(f"expected gesture = ", expected)
    print(f"actual gesture = ", actual)
    if actual == expected:
        print("RESULT: PASS\n")
    else:
        print("RESULT: FAIL\n")

if __name__ == "__main__":
    print("Gesture Mapping Tests: ")
    
    # 1: MOVE_FORWARD
    testing_forward = GloveData(flex = [0.1, 0.1, 0.1, 0.1, 0.1],
                                pitch = -30.0,
                                roll = 0.0,
                                isStill = False)
    testing("Move forward", testing_forward, Gesture.MOVE_FORWARD)

    # 2: MOVE_BACKWARD
    testing_backward = GloveData(flex = [0.1, 0.1, 0.1, 0.1, 0.1],
                                pitch = 30.0,
                                roll = 0.0,
                                isStill = False)
    testing("Move backward", testing_backward, Gesture.MOVE_BACKWARD)

    # 3: MOVE_RIGHT
    testing_right = GloveData(flex = [0.9, 0.1, 0.1, 0.9, 0.9],
                                pitch = 0.0,
                                roll = 30.0,
                                isStill = False)
    testing("Move right", testing_right, Gesture.MOVE_RIGHT)

    # 4: MOVE_LEFT
    testing_left = GloveData(flex = [0.9, 0.1, 0.1, 0.9, 0.9],
                                pitch = 0.0,
                                roll = -30.0,
                                isStill = False)
    testing("Move left", testing_left, Gesture.MOVE_LEFT)

    # 5: MOVE_UP
    testing_up = GloveData(flex = [0.9, 0.1, 0.1, 0.9, 0.9],
                                pitch = 85.0,
                                roll = 0.0,
                                isStill = False)
    testing("Move up", testing_up, Gesture.MOVE_UP)

    # 6: MOVE_DOWN
    testing_down = GloveData(flex = [0.9, 0.1, 0.1, 0.9, 0.9],
                                pitch = -85.0,
                                roll = 0.0,
                                isStill = False)
    testing("Move down", testing_down, Gesture.MOVE_DOWN)

    # 7: OPEN_GRIPPER
    testing_open = GloveData(flex = [0.1, 0.1, 0.1, 0.1, 0.1],
                                pitch = 0.0,
                                roll = 0.0,
                                isStill = True)
    testing("Open gripper", testing_open, Gesture.OPEN_GRIPPER)

    # 8: CLOSE_GRIPPER
    testing_close = GloveData(flex = [0.9, 0.9, 0.9, 0.9, 0.9],
                                pitch = 0.0,
                                roll = 0.0,
                                isStill = True)
    testing("Close gripper", testing_close, Gesture.CLOSE_GRIPPER)

    # 9: NONE (no matching gesture)
    testing_none = GloveData(flex = [0.4, 0.2, 0.8, 0.5, 0.6],
                                pitch = 3.0,
                                roll = 3.0,
                                isStill = False)
    testing("No gesture", testing_none, Gesture.NONE)