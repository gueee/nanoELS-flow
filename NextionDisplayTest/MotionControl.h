#ifndef MOTIONCONTROL_H
#define MOTIONCONTROL_H

class MotionControl {
public:
  MotionControl() {}
  void update() {}
  int getEmergencyStop() { return 0; }
  int isMoving() { return 0; }
  int getPosition(int) { return 0; }
  int getSpindleRPM() { return 0; }
  int getSpindlePosition() { return 0; }
};

#endif // MOTIONCONTROL_H 