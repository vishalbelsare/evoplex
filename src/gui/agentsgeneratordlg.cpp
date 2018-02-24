/**
 * Copyright (C) 2017 - Marcos Cardinot
 * @author Marcos Cardinot <mcardinot@gmail.com>
 */

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <functional>

#include "agentsgeneratordlg.h"
#include "ui_agentsgeneratordlg.h"
#include "agent.h"

namespace evoplex
{

AgentsGeneratorDlg::AgentsGeneratorDlg(const AttributesSpace& agentAttrsSpace, QWidget* parent, AgentsGenerator* ag)
    : QDialog(parent)
    , m_ui(new Ui_AgentsGeneratorDlg)
    , m_agentAttrsSpace(agentAttrsSpace)
{
    setWindowModality(Qt::ApplicationModal);
    m_ui->setupUi(this);

    connect(m_ui->bFromFile, SIGNAL(toggled(bool)), m_ui->wFromFile, SLOT(setVisible(bool)));
    connect(m_ui->bSameData, SIGNAL(toggled(bool)), m_ui->wSameData, SLOT(setVisible(bool)));
    connect(m_ui->bDiffData, SIGNAL(toggled(bool)), m_ui->wDiffData, SLOT(setVisible(bool)));
    m_ui->wFromFile->setVisible(false);
    m_ui->wSameData->setVisible(false);
    m_ui->wDiffData->setVisible(false);
    m_ui->bSameData->setChecked(true);

    connect(m_ui->saveAs, SIGNAL(pressed()), SLOT(slotSaveAs()));
    connect(m_ui->cancel, SIGNAL(pressed()), SLOT(reject()));
    connect(m_ui->ok, SIGNAL(pressed()), SLOT(accept()));

    connect(m_ui->browseFile, &QPushButton::pressed, [this]() {
        QString path = QFileDialog::getOpenFileName(this,
                                                    "Initial Population",
                                                    m_ui->filepath->text(),
                                                    "Text Files (*.csv *.txt)");
        if (!path.isEmpty()) {
            m_ui->filepath->setText(path);
        }
    });

    connect(m_ui->func, &QComboBox::currentTextChanged, [this]() {
        bool notRand = m_ui->func->currentData() != AgentsGenerator::F_Rand;
        m_ui->lseed->setHidden(notRand);
        m_ui->fseed->setHidden(notRand);
    });
    m_ui->func->insertItem(0, AgentsGenerator::enumToString(AgentsGenerator::F_Min), AgentsGenerator::F_Min);
    m_ui->func->insertItem(1, AgentsGenerator::enumToString(AgentsGenerator::F_Max), AgentsGenerator::F_Max);
    m_ui->func->insertItem(2, AgentsGenerator::enumToString(AgentsGenerator::F_Rand), AgentsGenerator::F_Rand);
    m_ui->func->setCurrentIndex(0);

    m_ui->table->setRowCount(m_agentAttrsSpace.size());
    for (const ValueSpace* vs : m_agentAttrsSpace) {
        m_ui->table->setItem(vs->id(), 0, new QTableWidgetItem(vs->attrName()));

        QComboBox* cb = new QComboBox(m_ui->table);
        cb->insertItem(0, AgentsGenerator::enumToString(AgentsGenerator::F_Min), AgentsGenerator::F_Min);
        cb->insertItem(1, AgentsGenerator::enumToString(AgentsGenerator::F_Max), AgentsGenerator::F_Max);
        cb->insertItem(2, AgentsGenerator::enumToString(AgentsGenerator::F_Rand), AgentsGenerator::F_Rand);
        cb->insertItem(3, AgentsGenerator::enumToString(AgentsGenerator::F_Value), AgentsGenerator::F_Value);
        m_ui->table->setCellWidget(vs->id(), 1, cb);

        QLineEdit* le = new QLineEdit();
        connect(cb, &QComboBox::currentTextChanged, [cb, le, vs](){
            if (cb->currentData() == AgentsGenerator::F_Min
                    || cb->currentData() == AgentsGenerator::F_Max) {
                le->setHidden(true);
                return;
            } else if (cb->currentData() == AgentsGenerator::F_Rand) {
                le->setToolTip("Type the PRG seed (integer).");
                le->setFocus();
            } else if (cb->currentData() == AgentsGenerator::F_Value) {
                le->setToolTip("Type a valid value for this attribute.\n"
                               "Expected: " + vs->space());
                le->setFocus();
            } else {
                Q_ASSERT(false);
            }
            le->setHidden(false);
        });
        m_ui->table->setCellWidget(vs->id(), 2, le);
        le->setHidden(true);
    }

    resize(width(), 250);
    fill(ag);
}

AgentsGeneratorDlg::~AgentsGeneratorDlg()
{
    delete m_ui;
}

void AgentsGeneratorDlg::slotSaveAs()
{
    QString path = QFileDialog::getSaveFileName(this,
                                                "Save Agents",
                                                m_ui->filepath->text(),
                                                "Text Files (*.csv)");
    if (path.isEmpty()) {
        return;
    }

    bool saved = false;
    if (m_ui->bFromFile->isChecked()) {
        saved = QFile::copy(m_ui->filepath->text(), path);
    } else {
        QString cmd = readCommand();
        if (cmd.isEmpty()) {
            return;
        }

        int numAgents = 0;
        if (m_ui->bSameData->isChecked()) {
            numAgents = m_ui->numAgents1->value();
        } else {
            numAgents = m_ui->numAgents2->value();
        }

        QProgressDialog progressDlg("Exporting Agents...", QString(), 0, 2 * numAgents, this);
        progressDlg.setWindowModality(Qt::WindowModal);
        progressDlg.setValue(0);

        QString errMsg;
        AgentsGenerator* ag = AgentsGenerator::parse(m_agentAttrsSpace, cmd, errMsg);
        Q_ASSERT(errMsg.isEmpty());

        int pValue = 0;
        std::function<void(int)> progress = [&progressDlg, &pValue](int p) { progressDlg.setValue(pValue + p); };
        Agents agents = ag->create(progress);
        Q_ASSERT(agents.size() > 0);

        pValue = numAgents;
        saved = AgentsGenerator::saveToFile(path, agents, progress);

        Utils::deleteAndShrink(agents);
    }

    if (saved) {
        QMessageBox::information(this, "Exporting Agents",
                "The set of agents was saved successfully!\n" + path);
    } else {
        QMessageBox::warning(this, "Exporting Agents",
                "ERROR! Unable to save the set of agents at:\n"
                + path + "\nPlease, make sure this directory is writable.");
    }
}

void AgentsGeneratorDlg::fill(AgentsGenerator* ag)
{
    if (!ag) {
        return;
    }

    AGFromFile* agff = dynamic_cast<AGFromFile*>(ag);
    if (agff) {
        m_ui->bFromFile->setChecked(true);
        m_ui->filepath->setText(agff->filePath());
        return;
    }

    AGSameFuncForAll* agsame = dynamic_cast<AGSameFuncForAll*>(ag);
    if (agsame) {
        m_ui->bSameData->setChecked(true);
        m_ui->numAgents1->setValue(agsame->numAgents());
        m_ui->func->setCurrentIndex(m_ui->func->findData(agsame->function()));
        Value v = agsame->functionInput();
        m_ui->fseed->setValue(v.type() == Value::INT ? v.toInt() : 0);
        return;
    }

    AGDiffFunctions* agdiff = dynamic_cast<AGDiffFunctions*>(ag);
    if (agdiff) {
        m_ui->bDiffData->setChecked(true);
        m_ui->numAgents2->setValue(agdiff->numAgents());
        for (const AGDiffFunctions::AttrCmd ac : agdiff->attrCmds()) {
            Q_ASSERT(m_ui->table->item(ac.attrId, 0)->text() == ac.attrName);
            QComboBox* cb = dynamic_cast<QComboBox*>(m_ui->table->cellWidget(ac.attrId, 1));
            cb->setCurrentIndex(cb->findData(ac.func));
            if (ac.func == AgentsGenerator::F_Rand || ac.func == AgentsGenerator::F_Value) {
                dynamic_cast<QLineEdit*>(m_ui->table->cellWidget(ac.attrId, 2))->setText(ac.funcInput.toQString());
            }
        }
    }
}

QString AgentsGeneratorDlg::readCommand()
{
    QString command;
    if (m_ui->bFromFile->isChecked()) {
        if (!QFileInfo::exists(m_ui->filepath->text())) {
            QMessageBox::warning(this, "Agents from csv file...",
                "The file does not exist.\nPlease, check the file path!");
            return QString();
        }
        command = m_ui->filepath->text();
    } else if (m_ui->bSameData->isChecked()) {
        command = QString("*%1;%2").arg(m_ui->numAgents1->text()).arg(m_ui->func->currentText());
        if (m_ui->func->currentData() == AgentsGenerator::F_Rand) {
            command += QString("_%1").arg(m_ui->fseed->value());
        }
    } else if (m_ui->bDiffData->isChecked()) {
        command = QString("#%1").arg(m_ui->numAgents2->text());
        for (const ValueSpace* vs : m_agentAttrsSpace) {
            Q_ASSERT(m_ui->table->item(vs->id(), 0)->text() == vs->attrName());
            QComboBox* cb = dynamic_cast<QComboBox*>(m_ui->table->cellWidget(vs->id(), 1));
            command += QString(";%1_%2").arg(vs->attrName()).arg(cb->currentText());

            QString valStr = dynamic_cast<QLineEdit*>(m_ui->table->cellWidget(vs->id(), 2))->text();
            if (cb->currentData() == AgentsGenerator::F_Rand) {
                bool isInt = false;
                valStr.toInt(&isInt);
                if (!isInt) {
                    QMessageBox::warning(this, "Agents Generator",
                        "The PRG seed for '" + vs->attrName() + "' should be an integer!\n");
                    return QString();
                }
                command += "_" + valStr;
            } else if (cb->currentData() == AgentsGenerator::F_Value) {
                if (!vs->validate(valStr).isValid()) {
                    QMessageBox::warning(this, "Agents Generator",
                        "The value of '" + vs->attrName() + "' is invalid!\n"
                         "Expected: " + vs->space());
                    return QString();
                }
                command += "_" + valStr;
            }
        }
    } else {
        qFatal("[AgentsGeneratorDlg::slotOk()]");
    }

    return command;
}

} // evoplex
