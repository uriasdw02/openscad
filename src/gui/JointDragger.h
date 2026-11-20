#pragma once

// JointDragger - "Move Parts" mode.
//
// Click a part in the 3D view and drag it to move it the way the real
// mechanism moves. A part is movable when one of the rotate()/translate()
// calls that place it is driven by a top-level number variable (a Customizer
// parameter), for example:
//
//     arm_angle = 30;          // [-90:90]
//     rotate([0, 0, arm_angle]) arm();
//
// Dragging changes that variable live (the Customizer slider follows), keeps it
// inside its [min:max] range, and on release writes the new value back into the
// code. The variable may also be passed through module parameters
// (`module arm(a) rotate(a) ...; arm(arm_angle);`) or used as `-v`, `v + 5`,
// `2 * v`. Nothing is specific to one model.
//
// rotate() joints work automatically. translate() joints (slides) are only used
// when the variable name says it is a motion, e.g. `lift_slide`, `drawer_travel`,
// `ram_extend`, `carriage_pos`, so that size variables never become "handles".

#include <array>
#include <memory>
#include <string>

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QTimer>

#include "geometry/linalg.h"

class MainWindow;
class QAction;
class AbstractNode;
class Expression;

class JointDragger : public QObject
{
  Q_OBJECT

public:
  explicit JointDragger(MainWindow *window);

  QAction *toggleAction() const { return toggleAction_; }
  bool isEnabled() const;

private:
  struct Joint {
    std::string param;       // customizer variable that drives the joint
    double coef = 1.0;       // joint value = coef * param (+ constant)
    bool rotational = true;  // rotate() vs translate()
    Vector3d pivot{0, 0, 0}; // world coordinates
    Vector3d axis{0, 0, 1};  // world direction (unit)
  };

  bool begin(QPoint pos);
  void move(QPoint pos);
  void end();
  void flush();

  bool findJoint(QPoint pos, Joint& joint, std::string& why);
  QPointF project(const Vector3d& world) const;
  void writeBackToCode(const std::string& param, double value);
  QString formatValue(double value) const;

  MainWindow *window_;
  QAction *toggleAction_;
  QTimer flushTimer_;

  // Drag state
  bool dragging_ = false;
  Joint joint_;
  double startValue_ = 0;
  double currentValue_ = 0;
  double minValue_ = 0, maxValue_ = 0;
  bool hasMin_ = false, hasMax_ = false;
  double step_ = 0;
  QPoint pressPos_;
  QPointF pivotScreen_;
  double lastAngle_ = 0, totalAngle_ = 0;
  double sign_ = 1;          // +1 if the rotation axis points toward the viewer
  bool edgeOn_ = false;      // axis nearly in the screen plane -> use plain left/right drag
  QPointF slideDir_;         // screen pixels per model unit along a slide axis
  std::array<double, 16> modelview_{}, projection_{};
  bool pendingPreview_ = false;
};
