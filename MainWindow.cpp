#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QtUiTools>
#include <QFileDialog>
#include <QFormBuilder>

#include "QConvertFig.h"

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_ui(new Ui::MainWindow)
{
	m_ui->setupUi(this);
}

MainWindow::~MainWindow()
{
	delete m_ui;
}


void MainWindow::on_action_load_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, "Load MATLAB FIG", "", "MATLAB fig (*.fig)");
	if (fileName.isEmpty())
		return;
	QConvertFig conv(fileName);
	if (conv.convert()) {
		QUiLoader loader;
		loader.setWorkingDirectory(QFileInfo(conv.outputFileName()).absolutePath());

		QFile file(conv.outputFileName());
		file.open(QFile::ReadOnly);

		QWidget *formWidget = loader.load(&file);
		file.close();

		if (formWidget) {
			QSize size = formWidget->geometry().size();
			QMdiSubWindow *subWindow = m_ui->mdiArea->addSubWindow(formWidget);
			subWindow->resize(size);
			subWindow->setAttribute(Qt::WA_DeleteOnClose);
			subWindow->showMaximized();
		}
	}
}
