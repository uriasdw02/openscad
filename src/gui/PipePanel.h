#pragma once

// PipePanel - "Pipe Builder" dock (Window > Pipe Builder).
//
// Point-and-click front end for the PipeTrack library: pick size, schedule,
// material and class, add legs (direction + length, feet-inches OK), valves,
// flanged joints, reducers and welds, and it writes the pipe_run() code into
// the editor and previews it. It only ever rewrites files it created
// (they start with a "Built with the Pipe Builder" line).

#include <QDockWidget>
#include <QString>
#include <QTimer>

class MainWindow;
class QComboBox;
class QLineEdit;
class QTableWidget;
class QCheckBox;

class PipePanel : public QDockWidget
{
  Q_OBJECT

public:
  explicit PipePanel(MainWindow *window);

  static void install(MainWindow *window);

private:
  enum Column { ColType = 0, ColDir, ColLength, ColDetail, ColCount };

  void addRow(const QString& type, const QString& dir = "N", const QString& length = "",
              const QString& detail = "");
  void updateRowState(int row);
  void moveRow(int delta);
  void deleteRow();
  void scheduleLive();
  void apply(bool interactive);
  QString generateCode(QStringList& problems) const;
  QString cellText(int row, int col) const;
  static QString lengthLiteral(const QString& text, bool& ok);

  MainWindow *window_;
  QLineEdit *line_;
  QComboBox *nps_, *sch_, *material_, *rating_, *units_, *start_, *end_, *elbow_;
  QTableWidget *table_;
  QCheckBox *live_;
  QCheckBox *drawing_;
  QTimer liveTimer_;
};
