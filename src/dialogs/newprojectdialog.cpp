#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>

#include "src/irisgl/src/core/irisutils.h"
#include "../core/settingsmanager.h"
#include "../constants.h"

#include "newprojectdialog.h"
#include "ui_newprojectdialog.h"

#include <QStandardPaths>
#include <QMessageBox>

NewProjectDialog::NewProjectDialog(QDialog *parent) : QDialog(parent), ui(new Ui::NewProjectDialog)
{
    ui->setupUi(this);

    this->setWindowTitle("New World");

//    QFile fontFile(IrisUtils::getAbsoluteAssetPath("app/fonts/OpenSans-Bold.ttf"));
//    if (fontFile.exists()) {
//        fontFile.open(QIODevice::ReadOnly);
//        QFontDatabase::addApplicationFontFromData(fontFile.readAll());
//        QApplication::setFont(QFont("Open Sans", 9));
//    }

    settingsManager = SettingsManager::getDefaultManager();

    ui->projectPath->setDisabled(true);

//    connect(ui->browseProject, SIGNAL(pressed()), SLOT(setProjectPath()));
    connect(ui->createProject, SIGNAL(pressed()), SLOT(confirmProjectCreation()));

//    lastValue = settingsManager->getValue("last_wd", "").toString();
//    if (!lastValue.isEmpty()) {
//        projectPath = lastValue;
//        ui->projectPath->setText(lastValue);
//    } else {
        auto path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                    + Constants::PROJECT_FOLDER;
        projectPath = settingsManager->getValue("default_directory", path).toString();
        ui->projectPath->setText(projectPath);
//    }
}

NewProjectDialog::~NewProjectDialog()
{
    delete ui;
}

ProjectInfo NewProjectDialog::getProjectInfo()
{
    ProjectInfo pInfo = { projectName, projectPath };
    return pInfo;
}

void NewProjectDialog::setProjectPath()
{
    QFileDialog projectDir;
    projectPath = projectDir.getExistingDirectory(nullptr, "Select project dir", lastValue);
    ui->projectPath->setText(projectPath);
}

void NewProjectDialog::createNewProject()
{
    projectName = ui->projectName->text();
}

void NewProjectDialog::confirmProjectCreation()
{
    if (QDir(projectPath + '/' + ui->projectName->text()).exists()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Project Path not Empty", "Project already Exists! Overwrite?",
                                        QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            createNewProject();
            this->close();
            emit accepted();
        }
    } else {
        createNewProject();
        this->close();
        emit accepted();
    }
}
