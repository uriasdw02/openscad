#include "gui/PipePanel.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>

#include <initializer_list>
#include <utility>

#include "gui/Editor.h"
#include "gui/MainWindow.h"

namespace {

const char *kMarker = "// Built with the Pipe Builder";

const QStringList kTypes = {"Leg", "Valve", "Flanged joint", "Reducer", "Field weld", "Shop weld"};
const QStringList kDirs = {"N",  "S",  "E",  "W",  "U",  "D",  "NE", "NW", "SE", "SW",
                           "NU", "ND", "SU", "SD", "EU", "ED", "WU", "WD"};
const QStringList kNps = {"0.5", "0.75", "1",  "1.25", "1.5", "2",  "2.5", "3",  "3.5", "4",
                          "5",   "6",    "8",  "10",   "12",  "14", "16",  "18", "20",  "24"};
const QStringList kValves = {"gate", "globe", "ball", "check"};

QComboBox *combo(const QStringList& items, const QString& current, bool editable = false)
{
  auto *c = new QComboBox;
  c->addItems(items);
  c->setEditable(editable);
  c->setCurrentText(current);
  return c;
}

}  // namespace

void PipePanel::install(MainWindow *window)
{
  auto *panel = new PipePanel(window);
  window->addDockWidget(Qt::RightDockWidgetArea, panel);
  panel->hide();
  QAction *toggle = panel->toggleViewAction();
  toggle->setText(tr("Pipe Builder"));
  const auto actions = window->menuWindow->actions();
  window->menuWindow->insertAction(actions.isEmpty() ? nullptr : actions.first(), toggle);
}

PipePanel::PipePanel(MainWindow *window) : QDockWidget(tr("Pipe Builder"), window), window_(window)
{
  setObjectName("PipeBuilderDock");
  auto *root = new QWidget;
  auto *v = new QVBoxLayout(root);

  // ---- line settings ----
  auto *form = new QGridLayout;
  line_ = new QLineEdit("LINE-1");
  nps_ = combo(kNps, "4");
  sch_ = combo({"5S", "10S", "40", "STD", "40S", "80", "XS", "80S", "160", "XXS"}, "40");
  material_ = combo({"CS", "SS", "CHROME", "AL", "CU", "CUNI", "NI", "TI", "DUPLEX", "GALV"}, "CS");
  rating_ = combo({"150", "300"}, "150");
  units_ = combo({"mm", "in"}, "in");
  start_ = combo({"open", "flange"}, "flange");
  end_ = combo({"open", "flange"}, "flange");
  elbow_ = combo({"LR", "SR", "3D", "5D"}, "LR", true);
  int r = 0;
  form->addWidget(new QLabel(tr("Line")), r, 0);
  form->addWidget(line_, r, 1, 1, 3);
  ++r;
  form->addWidget(new QLabel(tr("NPS")), r, 0);
  form->addWidget(nps_, r, 1);
  form->addWidget(new QLabel(tr("Sch")), r, 2);
  form->addWidget(sch_, r, 3);
  ++r;
  form->addWidget(new QLabel(tr("Material")), r, 0);
  form->addWidget(material_, r, 1);
  form->addWidget(new QLabel(tr("Class")), r, 2);
  form->addWidget(rating_, r, 3);
  ++r;
  form->addWidget(new QLabel(tr("Units")), r, 0);
  form->addWidget(units_, r, 1);
  form->addWidget(new QLabel(tr("Elbows")), r, 2);
  form->addWidget(elbow_, r, 3);
  ++r;
  form->addWidget(new QLabel(tr("Start")), r, 0);
  form->addWidget(start_, r, 1);
  form->addWidget(new QLabel(tr("End")), r, 2);
  form->addWidget(end_, r, 3);
  v->addLayout(form);

  // ---- quick add: one tap per direction ----
  auto *dirRow = new QHBoxLayout;
  dirRow->addWidget(new QLabel(tr("Add leg:")));
  for (const QString& d : {"N", "S", "E", "W", "U", "D"}) {
    auto *b = new QPushButton(d);
    b->setMaximumWidth(34);
    b->setToolTip(tr("Add a leg going %1").arg(d));
    connect(b, &QPushButton::clicked, this, [this, d] {
      addRow("Leg", d);
      table_->setCurrentCell(table_->rowCount() - 1, ColLength);
      if (auto *len = qobject_cast<QLineEdit *>(table_->cellWidget(table_->rowCount() - 1, ColLength))) {
        len->setFocus();
      }
    });
    dirRow->addWidget(b);
  }
  dirRow->addStretch();
  v->addLayout(dirRow);

  auto *fitRow = new QHBoxLayout;
  for (const auto& [label, type] : std::initializer_list<std::pair<QString, QString>>{
         {tr("+ Valve"), "Valve"},
         {tr("+ Flange"), "Flanged joint"},
         {tr("+ Reducer"), "Reducer"},
         {tr("+ Field weld"), "Field weld"}}) {
    auto *b = new QPushButton(label);
    connect(b, &QPushButton::clicked, this, [this, type] { addRow(type); });
    fitRow->addWidget(b);
  }
  v->addLayout(fitRow);

  // ---- run table ----
  table_ = new QTableWidget(0, ColCount);
  table_->setHorizontalHeaderLabels({tr("Item"), tr("Direction"), tr("Length"), tr("Detail")});
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table_->verticalHeader()->setDefaultSectionSize(26);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setToolTip(tr("Lengths are work point to work point: elbow centerlines, flange faces, valve faces.\n"
                        "Type numbers (in the chosen units) or feet-inches like 10'-6 3/4\"."));
  v->addWidget(table_, 1);

  auto *editRow = new QHBoxLayout;
  auto *up = new QPushButton(tr("Up"));
  auto *down = new QPushButton(tr("Down"));
  auto *del = new QPushButton(tr("Delete"));
  connect(up, &QPushButton::clicked, this, [this] { moveRow(-1); });
  connect(down, &QPushButton::clicked, this, [this] { moveRow(1); });
  connect(del, &QPushButton::clicked, this, &PipePanel::deleteRow);
  editRow->addWidget(up);
  editRow->addWidget(down);
  editRow->addWidget(del);
  editRow->addStretch();
  v->addLayout(editRow);

  auto *applyRow = new QHBoxLayout;
  live_ = new QCheckBox(tr("Live preview"));
  live_->setChecked(true);
  drawing_ = new QCheckBox(tr("Iso drawing"));
  drawing_->setToolTip(tr("Show a clean 2D isometric drawing with dimensions and cut list instead of the 3D model.\n"
                          "Export it with File > Export > Export as PDF / SVG / DXF."));
  connect(drawing_, &QCheckBox::toggled, this, &PipePanel::scheduleLive);
  auto *build = new QPushButton(tr("Build model"));
  build->setDefault(true);
  build->setStyleSheet("font-weight: bold;");
  connect(build, &QPushButton::clicked, this, [this] { apply(true); });
  applyRow->addWidget(live_);
  applyRow->addWidget(drawing_);
  applyRow->addStretch();
  applyRow->addWidget(build);
  v->addLayout(applyRow);

  setWidget(root);

  liveTimer_.setSingleShot(true);
  liveTimer_.setInterval(700);
  connect(&liveTimer_, &QTimer::timeout, this, [this] { apply(false); });
  for (QComboBox *c : {nps_, sch_, material_, rating_, units_, start_, end_, elbow_}) {
    connect(c, &QComboBox::currentTextChanged, this, &PipePanel::scheduleLive);
  }
  connect(line_, &QLineEdit::textChanged, this, &PipePanel::scheduleLive);

  // A starting point: the example from the PipeTrack README.
  addRow("Leg", "N", "3'-6\"");
  addRow("Leg", "E", "2'-8\"");
  addRow("Leg", "U", "2'-6\"");
  addRow("Valve", "", "", "gate");
  addRow("Leg", "U", "1'-2\"");
}

void PipePanel::addRow(const QString& type, const QString& dir, const QString& length,
                       const QString& detail)
{
  int row = table_->rowCount();
  // insert after the selected row, if any
  if (table_->currentRow() >= 0 && table_->currentRow() < row - 1) row = table_->currentRow() + 1;
  table_->insertRow(row);

  auto *typeBox = combo(kTypes, type);
  auto *dirBox = combo(kDirs, dir.isEmpty() ? "N" : dir, true);
  dirBox->setToolTip(tr("N S E W U D, combinations like NEU for rolling offsets, or a vector [1,1,0]"));
  auto *lenEdit = new QLineEdit(length);
  lenEdit->setPlaceholderText(tr("e.g. 1050 or 3'-6\""));
  auto *detailBox = new QComboBox;
  detailBox->setEditable(true);
  table_->setCellWidget(row, ColType, typeBox);
  table_->setCellWidget(row, ColDir, dirBox);
  table_->setCellWidget(row, ColLength, lenEdit);
  table_->setCellWidget(row, ColDetail, detailBox);

  // locate the row from the widget each time - rows move
  auto rowOf = [this, typeBox] {
    for (int r = 0; r < table_->rowCount(); ++r) {
      if (table_->cellWidget(r, ColType) == typeBox) return r;
    }
    return -1;
  };
  connect(typeBox, &QComboBox::currentTextChanged, this, [this, rowOf] {
    updateRowState(rowOf());
    scheduleLive();
  });
  connect(dirBox, &QComboBox::currentTextChanged, this, &PipePanel::scheduleLive);
  connect(lenEdit, &QLineEdit::textChanged, this, &PipePanel::scheduleLive);
  connect(detailBox, &QComboBox::currentTextChanged, this, &PipePanel::scheduleLive);

  updateRowState(row);
  if (!detail.isEmpty()) detailBox->setCurrentText(detail);
  table_->setCurrentCell(row, ColType);
  scheduleLive();
}

void PipePanel::updateRowState(int row)
{
  if (row < 0) return;
  const QString type = cellText(row, ColType);
  auto *dir = table_->cellWidget(row, ColDir);
  auto *len = table_->cellWidget(row, ColLength);
  auto *detail = qobject_cast<QComboBox *>(table_->cellWidget(row, ColDetail));
  const bool leg = type == "Leg";
  dir->setEnabled(leg);
  len->setEnabled(leg);
  const QString keep = detail->currentText();
  detail->blockSignals(true);
  detail->clear();
  if (type == "Valve") {
    detail->addItems(kValves);
    detail->setCurrentText(kValves.contains(keep) ? keep : "gate");
  } else if (type == "Reducer") {
    detail->addItems(kNps);
    detail->setCurrentText(kNps.contains(keep) ? keep : "2");
  }
  detail->setEnabled(type == "Valve" || type == "Reducer");
  detail->blockSignals(false);
}

QString PipePanel::cellText(int row, int col) const
{
  QWidget *w = table_->cellWidget(row, col);
  if (auto *c = qobject_cast<QComboBox *>(w)) return c->currentText().trimmed();
  if (auto *e = qobject_cast<QLineEdit *>(w)) return e->text().trimmed();
  return {};
}

void PipePanel::moveRow(int delta)
{
  const int row = table_->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= table_->rowCount()) return;
  // swap the four values (cell widgets can't be moved between rows)
  const QStringList a = {cellText(row, 0), cellText(row, 1), cellText(row, 2), cellText(row, 3)};
  const QStringList b = {cellText(target, 0), cellText(target, 1), cellText(target, 2), cellText(target, 3)};
  auto set = [this](int r, const QStringList& vals) {
    qobject_cast<QComboBox *>(table_->cellWidget(r, ColType))->setCurrentText(vals[0]);
    updateRowState(r);
    qobject_cast<QComboBox *>(table_->cellWidget(r, ColDir))->setCurrentText(vals[1]);
    qobject_cast<QLineEdit *>(table_->cellWidget(r, ColLength))->setText(vals[2]);
    qobject_cast<QComboBox *>(table_->cellWidget(r, ColDetail))->setCurrentText(vals[3]);
  };
  set(row, b);
  set(target, a);
  table_->setCurrentCell(target, ColType);
}

void PipePanel::deleteRow()
{
  const int row = table_->currentRow();
  if (row < 0) return;
  table_->removeRow(row);
  scheduleLive();
}

void PipePanel::scheduleLive()
{
  if (live_ && live_->isChecked()) liveTimer_.start();
}

// 1050 -> 1050 ; 3'-6" -> "3'-6\"" ; rejects anything else
QString PipePanel::lengthLiteral(const QString& text, bool& ok)
{
  static const QRegularExpression number(R"(^\d+(\.\d+)?$)");
  static const QRegularExpression ftin(R"(^(\d+(\.\d+)?\s*')?\s*-?\s*(\d+(\.\d+)?)?(\s*[- ]?\s*\d+/\d+)?\s*"?$)");
  ok = true;
  if (number.match(text).hasMatch()) return text;
  if (!text.isEmpty() && ftin.match(text).hasMatch() && (text.contains('\'') || text.contains('"') || text.contains('/'))) {
    QString escaped = text;
    escaped.replace("\"", "\\\"");
    return "\"" + escaped + "\"";
  }
  ok = false;
  return {};
}

QString PipePanel::generateCode(QStringList& problems) const
{
  QStringList items;
  for (int r = 0; r < table_->rowCount(); ++r) {
    const QString type = cellText(r, ColType);
    if (type == "Leg") {
      QString dir = cellText(r, ColDir).toUpper();
      if (dir.startsWith("[")) {
        dir = cellText(r, ColDir);  // vector, keep as typed
      } else {
        if (!QRegularExpression("^[NSEWUD]{1,3}$").match(dir).hasMatch()) {
          problems << tr("Row %1: direction \"%2\" isn't N/S/E/W/U/D").arg(r + 1).arg(dir);
          continue;
        }
        dir = "\"" + dir + "\"";
      }
      bool ok = false;
      const QString len = lengthLiteral(cellText(r, ColLength), ok);
      if (!ok) {
        problems << tr("Row %1: length \"%2\" isn't a number or feet-inches").arg(r + 1).arg(cellText(r, ColLength));
        continue;
      }
      items << QString("    [%1, %2]").arg(dir, len);
    } else if (type == "Valve") {
      items << QString("    [\"valve\", \"%1\"]").arg(cellText(r, ColDetail).toLower());
    } else if (type == "Flanged joint") {
      items << "    [\"flange\"]";
    } else if (type == "Reducer") {
      items << QString("    [\"reducer\", %1]").arg(cellText(r, ColDetail));
    } else if (type == "Field weld") {
      items << "    [\"fw\"]";
    } else if (type == "Shop weld") {
      items << "    [\"sw\"]";
    }
  }
  if (items.isEmpty()) problems << tr("Add at least one leg");

  QString code;
  code += QString(kMarker) + " (Window > Pipe Builder). Edits made here are replaced when the panel rebuilds.\n";
  code += "use <pipetrack/pipe.scad>\n\n";
  code += "pipe_run([\n" + items.join(",\n") + "\n  ],\n";
  code += QString("  nps = %1, sch = \"%2\", material = \"%3\", rating = %4, units = \"%5\",\n")
            .arg(nps_->currentText(), sch_->currentText(), material_->currentText(), rating_->currentText(),
                 units_->currentText());
  code += QString("  start = \"%1\", end = \"%2\", elbow = \"%3\", line = \"%4\"%5);\n")
            .arg(start_->currentText(), end_->currentText(), elbow_->currentText(),
                 line_->text().replace("\"", "'"), QString(drawing_->isChecked() ? ", drawing = true" : ""));
  return code;
}

void PipePanel::apply(bool interactive)
{
  if (!interactive && !isVisible()) return;  // live updates only while the panel is open
  QStringList problems;
  const QString code = generateCode(problems);
  if (!problems.isEmpty()) {
    if (interactive) {
      QMessageBox::warning(this, tr("Pipe Builder"), problems.join("\n"));
    } else {
      window_->statusBar()->showMessage(tr("Pipe Builder: ") + problems.first(), 5000);
    }
    return;
  }
  if (!window_->activeEditor) return;

  const QString current = window_->activeEditor->toPlainText();
  const bool ours = current.trimmed().isEmpty() || current.startsWith(kMarker);
  if (!ours) {
    if (!interactive) return;  // live updates never touch someone else's file
    const auto choice = QMessageBox::question(
      this, tr("Pipe Builder"),
      tr("The editor has other code in it. Open the pipe run in a new tab?"),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
    if (choice != QMessageBox::Yes) return;
    window_->fileActionNew->trigger();
    if (!window_->activeEditor) return;
  }
  if (window_->activeEditor->toPlainText() != code) {
    window_->activeEditor->setPlainText(code);
  }
  window_->actionRenderPreview();
}
