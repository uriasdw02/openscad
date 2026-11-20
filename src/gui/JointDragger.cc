#include "gui/JointDragger.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <deque>
#include <optional>

#include <QAction>
#include <QCursor>
#include <QKeySequence>
#include <QMenu>
#include <QRegularExpression>
#include <QStatusBar>
#include <QToolBar>

#include "core/Assignment.h"
#include "core/Expression.h"
#include "core/LocalScope.h"
#include "core/ModuleInstantiation.h"
#include "core/SourceFile.h"
#include "core/TransformNode.h"
#include "core/UserModule.h"
#include "core/customizer/ParameterObject.h"
#include "core/node.h"
#include "gui/MainWindow.h"
#include "gui/QGLView.h"
#include "gui/ScintillaEditor.h"
#include "gui/parameter/ParameterWidget.h"

namespace {

// If `e` is a simple linear use of one variable (v, -v, v + 5, 5 - v, 2 * v, v / 2),
// return the variable name and its coefficient.
bool linearRef(const Expression *e, std::string& name, double& coef)
{
  if (!e) return false;
  if (const auto *lookup = dynamic_cast<const Lookup *>(e)) {
    name = lookup->get_name();
    coef = 1.0;
    return true;
  }
  if (const auto *unary = dynamic_cast<const UnaryOp *>(e)) {
    if (unary->getOp() == UnaryOp::Op::Negate && linearRef(unary->getExpr().get(), name, coef)) {
      coef = -coef;
      return true;
    }
    return false;
  }
  if (const auto *binary = dynamic_cast<const BinaryOp *>(e)) {
    const Expression *l = binary->getLeft().get();
    const Expression *r = binary->getRight().get();
    auto number = [](const Expression *x, double& out) {
      const auto *lit = dynamic_cast<const Literal *>(x);
      if (!lit || !lit->isDouble()) return false;
      out = lit->toDouble();
      return true;
    };
    double k = 0;
    switch (binary->getOp()) {
    case BinaryOp::Op::Plus:
      if (r && r->isLiteral() && linearRef(l, name, coef)) return true;
      if (l && l->isLiteral() && linearRef(r, name, coef)) return true;
      return false;
    case BinaryOp::Op::Minus:
      if (r && r->isLiteral() && linearRef(l, name, coef)) return true;
      if (l && l->isLiteral() && linearRef(r, name, coef)) {
        coef = -coef;
        return true;
      }
      return false;
    case BinaryOp::Op::Multiply:
      if (number(r, k) && k != 0 && linearRef(l, name, coef)) { coef *= k; return true; }
      if (number(l, k) && k != 0 && linearRef(r, name, coef)) { coef *= k; return true; }
      return false;
    case BinaryOp::Op::Divide:
      if (number(r, k) && k != 0 && linearRef(l, name, coef)) { coef /= k; return true; }
      return false;
    default: return false;
    }
  }
  return false;
}

// Get argument `name` (or positional index `pos`) of a module call.
std::shared_ptr<Expression> argument(const ModuleInstantiation& inst, const std::string& name, int pos)
{
  int positional = 0;
  std::shared_ptr<Expression> byPos;
  for (const auto& arg : inst.arguments) {
    if (arg->getName().empty()) {
      if (positional == pos) byPos = arg->getExpr();
      ++positional;
    } else if (arg->getName() == name) {
      return arg->getExpr();
    }
  }
  return byPos;
}

// Variable names that mean "this translate() is a motion", not a size.
bool looksLikeSlide(const std::string& name)
{
  static const QRegularExpression re(
    "(slide|travel|extend|extension|stroke|lift|pos|position|open|drawer|carriage|shift|reach)",
    QRegularExpression::CaseInsensitiveOption);
  return re.match(QString::fromStdString(name)).hasMatch();
}

}  // namespace

JointDragger::JointDragger(MainWindow *window) : QObject(window), window_(window)
{
  toggleAction_ = new QAction(tr("Move Parts"), this);
  toggleAction_->setObjectName("viewActionMoveParts");
  toggleAction_->setCheckable(true);
  toggleAction_->setShortcut(QKeySequence(tr("Ctrl+Shift+M")));
  toggleAction_->setToolTip(
    tr("Move Parts (Ctrl+Shift+M): drag a part to move it at its hinge or slide.\n"
       "Works on rotate()/translate() driven by a top-level variable."));
  window->menu_View->addSeparator();
  window->menu_View->addAction(toggleAction_);
  window->viewerToolBar->addAction(toggleAction_);
  connect(toggleAction_, &QAction::toggled, this, [this](bool on) {
    window_->statusBar()->showMessage(
      on ? tr("Move Parts on: press F5, then drag a part. Drag empty space to orbit as usual.")
         : tr("Move Parts off."),
      5000);
  });

  flushTimer_.setSingleShot(true);
  flushTimer_.setInterval(15);
  connect(&flushTimer_, &QTimer::timeout, this, &JointDragger::flush);

  QGLView *view = window->qglview;
  view->partGrabBegin = [this](QPoint p) { return begin(p); };
  view->partGrabMove = [this](QPoint p) { move(p); };
  view->partGrabEnd = [this]() { end(); };
}

bool JointDragger::isEnabled() const { return toggleAction_->isChecked(); }

QPointF JointDragger::project(const Vector3d& w) const
{
  const auto& m = modelview_;
  const auto& p = projection_;
  double eye[4], clip[4];
  for (int i = 0; i < 4; ++i) {
    eye[i] = m[0 * 4 + i] * w.x() + m[1 * 4 + i] * w.y() + m[2 * 4 + i] * w.z() + m[3 * 4 + i];
  }
  for (int i = 0; i < 4; ++i) {
    clip[i] = p[0 * 4 + i] * eye[0] + p[1 * 4 + i] * eye[1] + p[2 * 4 + i] * eye[2] + p[3 * 4 + i] * eye[3];
  }
  if (std::abs(clip[3]) < 1e-12) return {};
  const double nx = clip[0] / clip[3], ny = clip[1] / clip[3];
  const QGLView *view = window_->qglview;
  return {(nx + 1.0) * 0.5 * view->width(), (1.0 - ny) * 0.5 * view->height()};
}

bool JointDragger::findJoint(QPoint pos, Joint& joint, std::string& why)
{
  QGLView *view = window_->qglview;
  if (!view->renderer || !window_->rootProduct || !window_->rootNode || !window_->rootFile) {
    why = "Press F5 (Preview) first, then drag a part.";
    return false;
  }
  const int index = view->pickObject(pos);
  if (index < 0) return false;  // clicked empty space: orbit as usual
  std::deque<std::shared_ptr<const AbstractNode>> path;  // [0] = clicked part ... back() = root
  if (!window_->rootNode->getNodeByID(index, path)) return false;

  ParameterWidget *params = window_->activeEditor ? window_->activeEditor->parameterWidget : nullptr;
  if (!params) return false;
  const LocalScope& topScope = *window_->rootFile->scope;

  // Follow a variable from where it's used up through module parameters to a
  // top-level number variable (Customizer parameter).
  auto resolve = [&](std::string name, double coef, size_t from, std::string& param,
                     double& totalCoef) -> bool {
    for (size_t j = from + 1; j < path.size(); ++j) {
      const auto& inst = path[j]->modinst;
      if (!inst) continue;
      auto mod = topScope.lookup<UserModule *>(inst->name());
      if (!mod || !*mod) continue;
      int pidx = -1;
      for (size_t k = 0; k < (*mod)->parameters.size(); ++k) {
        if ((*mod)->parameters[k]->getName() == name) pidx = static_cast<int>(k);
      }
      if (pidx < 0) continue;
      auto expr = argument(*inst, name, pidx);
      if (!expr) expr = (*mod)->parameters[pidx]->getExpr();  // default value
      std::string next;
      double c = 1;
      if (!linearRef(expr.get(), next, c)) return false;
      name = next;
      coef *= c;
    }
    if (!params->findNumberParameter(name)) return false;
    param = name;
    totalCoef = coef;
    return true;
  };

  for (size_t i = 0; i < path.size(); ++i) {
    const auto& node = path[i];
    const auto tnode = std::dynamic_pointer_cast<const TransformNode>(node);
    if (!tnode || !node->modinst) continue;
    const std::string kind = node->modinst->name();
    const bool isRotate = kind == "rotate";
    const bool isTranslate = kind == "translate";
    if (!isRotate && !isTranslate) continue;

    std::string param;
    double coef = 1;
    Vector3d localAxis(0, 0, 1);
    bool found = false;

    const auto mainArg = argument(*node->modinst, isRotate ? "a" : "v", 0);
    if (const auto *vec = dynamic_cast<const Vector *>(mainArg.get())) {
      const auto& kids = vec->getChildren();
      for (size_t idx = 0; idx < kids.size() && idx < 3 && !found; ++idx) {
        std::string name;
        double c = 1;
        if (!linearRef(kids[idx].get(), name, c)) continue;
        if (!resolve(name, c, i, param, coef)) continue;
        if (isTranslate && !looksLikeSlide(param)) continue;
        if (isTranslate) {
          localAxis = Vector3d::Unit(static_cast<int>(idx));
        } else if (idx == 2) {
          localAxis = Vector3d::UnitZ();
        } else if (idx == 0) {
          localAxis = tnode->matrix.linear() * Vector3d::UnitX();
        } else {
          const Vector3d ea = tnode->matrix.linear().eulerAngles(2, 1, 0);
          localAxis = Eigen::AngleAxisd(ea[0], Vector3d::UnitZ()) * Vector3d::UnitY();
        }
        found = true;
      }
    } else if (isRotate && mainArg) {
      std::string name;
      double c = 1;
      if (linearRef(mainArg.get(), name, c) && resolve(name, c, i, param, coef)) {
        localAxis = Vector3d::UnitZ();
        if (const auto *v = dynamic_cast<const Vector *>(argument(*node->modinst, "v", 1).get())) {
          Vector3d axis(0, 0, 0);
          const auto& kids = v->getChildren();
          for (size_t idx = 0; idx < kids.size() && idx < 3; ++idx) {
            const auto *lit = dynamic_cast<const Literal *>(kids[idx].get());
            if (lit && lit->isDouble()) axis[static_cast<int>(idx)] = lit->toDouble();
          }
          if (axis.norm() > 1e-9) localAxis = axis.normalized();
        }
        found = true;
      }
    }
    if (!found) continue;

    // World frame of the joint = all transforms above it.
    Transform3d parent = Transform3d::Identity();
    for (size_t k = path.size(); k-- > i + 1;) {
      if (const auto t = std::dynamic_pointer_cast<const TransformNode>(path[k])) parent = parent * t->matrix;
    }
    joint.param = param;
    joint.coef = coef;
    joint.rotational = isRotate;
    joint.pivot = parent * Vector3d(0, 0, 0);
    joint.axis = parent.linear() * localAxis;
    if (isRotate) joint.axis.normalize();
    return true;
  }
  why = "This part has no movable joint. Drive its rotate() with a top-level variable, "
        "e.g. hinge_angle = 30; // [-90:90]";
  return false;
}

bool JointDragger::begin(QPoint pos)
{
  if (!isEnabled() || GuiLocker::isLocked()) return false;
  QGLView *view = window_->qglview;
  std::copy(std::begin(view->modelview), std::end(view->modelview), modelview_.begin());
  std::copy(std::begin(view->projection), std::end(view->projection), projection_.begin());

  Joint joint;
  std::string why;
  if (!findJoint(pos, joint, why)) {
    if (!why.empty()) window_->statusBar()->showMessage(QString::fromStdString(why), 6000);
    return false;
  }
  ParameterWidget *params = window_->activeEditor->parameterWidget;
  NumberParameter *np = params->findNumberParameter(joint.param);
  if (!np) return false;

  joint_ = joint;
  startValue_ = currentValue_ = np->value;
  hasMin_ = static_cast<bool>(np->minimum);
  hasMax_ = static_cast<bool>(np->maximum);
  minValue_ = hasMin_ ? *np->minimum : 0;
  maxValue_ = hasMax_ ? *np->maximum : 0;
  step_ = np->step ? *np->step : 0;
  pressPos_ = pos;
  pivotScreen_ = project(joint_.pivot);

  if (joint_.rotational) {
    const auto& m = modelview_;
    const double eyeZ = m[2] * joint_.axis.x() + m[6] * joint_.axis.y() + m[10] * joint_.axis.z();
    sign_ = eyeZ >= 0 ? 1.0 : -1.0;
    const QPointF d = QPointF(pos) - pivotScreen_;
    edgeOn_ = std::abs(eyeZ) < 0.2 || std::hypot(d.x(), d.y()) < 10;
    lastAngle_ = std::atan2(-d.y(), d.x()) * 180.0 / M_PI;
    totalAngle_ = 0;
  } else {
    slideDir_ = project(joint_.pivot + joint_.axis) - pivotScreen_;
    if (std::hypot(slideDir_.x(), slideDir_.y()) < 1e-3) {
      window_->statusBar()->showMessage(tr("That slide points straight at you - turn the view to drag it."), 5000);
      return false;
    }
  }

  dragging_ = true;
  view->setCursor(Qt::ClosedHandCursor);
  window_->statusBar()->showMessage(
    tr("Moving %1 = %2").arg(QString::fromStdString(joint_.param), formatValue(currentValue_)));
  return true;
}

void JointDragger::move(QPoint pos)
{
  if (!dragging_) return;
  double jointDelta = 0;
  if (joint_.rotational) {
    if (edgeOn_) {
      jointDelta = (pos.x() - pressPos_.x()) * 0.5;
    } else {
      const QPointF d = QPointF(pos) - pivotScreen_;
      const double a = std::atan2(-d.y(), d.x()) * 180.0 / M_PI;
      double step = a - lastAngle_;
      while (step > 180) step -= 360;
      while (step < -180) step += 360;
      totalAngle_ += step;
      lastAngle_ = a;
      jointDelta = sign_ * totalAngle_;
    }
  } else {
    const QPointF md = QPointF(pos - pressPos_);
    const double len2 = slideDir_.x() * slideDir_.x() + slideDir_.y() * slideDir_.y();
    jointDelta = (md.x() * slideDir_.x() + md.y() * slideDir_.y()) / len2;
  }

  double value = startValue_ + jointDelta / joint_.coef;
  const double snap = step_ > 0 ? step_ : (joint_.rotational ? 0.5 : 0.01);
  value = std::round(value / snap) * snap;
  if (hasMin_) value = std::max(value, minValue_);
  if (hasMax_) value = std::min(value, maxValue_);
  if (value == currentValue_) return;
  currentValue_ = value;
  if (!flushTimer_.isActive()) flushTimer_.start();
}

void JointDragger::flush()
{
  if (GuiLocker::isLocked()) {  // a preview is still running; try again shortly
    flushTimer_.start(30);
    return;
  }
  if (!window_->activeEditor) return;
  window_->activeEditor->parameterWidget->setNumberParameter(joint_.param, currentValue_);
  window_->statusBar()->showMessage(
    tr("Moving %1 = %2").arg(QString::fromStdString(joint_.param), formatValue(currentValue_)));
  window_->actionRenderPreview();
}

void JointDragger::end()
{
  if (!dragging_) return;
  dragging_ = false;
  window_->qglview->unsetCursor();
  flushTimer_.stop();
  if (!window_->activeEditor) return;
  if (currentValue_ != startValue_) {
    window_->activeEditor->parameterWidget->setNumberParameter(joint_.param, currentValue_);
    writeBackToCode(joint_.param, currentValue_);
  }
  window_->actionRenderPreview();
}

QString JointDragger::formatValue(double value) const
{
  int decimals = 2;
  if (step_ > 0) {
    decimals = 0;
    double s = step_;
    while (decimals < 6 && std::abs(s - std::round(s)) > 1e-9) {
      s *= 10;
      ++decimals;
    }
  }
  QString text = QString::number(value, 'f', decimals);
  if (text.contains('.')) {
    while (text.endsWith('0')) text.chop(1);
    if (text.endsWith('.')) text.chop(1);
  }
  if (text == "-0") text = "0";
  return text;
}

void JointDragger::writeBackToCode(const std::string& param, double value)
{
  auto *editor = dynamic_cast<ScintillaEditor *>(window_->activeEditor);
  if (!editor || !window_->rootFile) return;

  int line = -1;
  for (const auto& assignment : window_->rootFile->scope->assignments) {
    if (assignment->getName() == param) line = assignment->location().firstLine() - 1;
  }
  if (line < 0) return;

  const QString text = editor->qsci->text(line);
  const QRegularExpression re(QString(R"(^(\s*%1\s*=\s*)(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?))")
                                .arg(QRegularExpression::escape(QString::fromStdString(param))));
  const auto match = re.match(text);
  if (!match.hasMatch()) {
    window_->statusBar()->showMessage(
      tr("Moved %1 (couldn't update its line in the code automatically)").arg(QString::fromStdString(param)),
      6000);
    return;
  }
  int curLine = 0, curIndex = 0;
  editor->qsci->getCursorPosition(&curLine, &curIndex);
  editor->qsci->setSelection(line, match.capturedStart(2), line, match.capturedEnd(2));
  editor->qsci->replaceSelectedText(formatValue(value));
  editor->qsci->setCursorPosition(curLine, curIndex);
  window_->statusBar()->showMessage(
    tr("%1 = %2 (saved in code - Ctrl+S to keep it)").arg(QString::fromStdString(param), formatValue(value)),
    6000);
}
