/**
 * Copyright (C) 2017 - Marcos Cardinot
 * @author Marcos Cardinot <mcardinot@gmail.com>
 */

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVariant>

#include "attributeswidget.h"
#include "ui_attributeswidget.h"

#define STRING_NULL_PLUGINID "--"
// let's make it easier to find the type of the field
#define CHECK_BOX "0"
#define COMBO_BOX "1"
#define DOUBLE_SPIN_BOX "2"
#define LINE_EDIT "3"
#define SPIN_BOX "4"

namespace evoplex {

AttributesWidget::AttributesWidget(Project* project, QWidget *parent)
    : QDockWidget(parent)
    , m_project(project)
    , m_selectedGraphId(STRING_NULL_PLUGINID)
    , m_selectedModelId(STRING_NULL_PLUGINID)
    , m_ui(new Ui_AttributesWidget)
{
    m_ui->setupUi(this);

    connect(m_ui->btnSubmit, SIGNAL(clicked(bool)), this, SLOT(slotCreateExperiment()));
    m_ui->treeWidget->setFocusPolicy(Qt::NoFocus);

    //
    // setup the tree widget: general attributes
    //
    m_treeItemGeneral = new QTreeWidgetItem(m_ui->treeWidget);
    m_treeItemGeneral->setText(0, "General");
    m_treeItemGeneral->setExpanded(true);

    auto addGeneralAttr = [this](const QString& label, const QVariant& field) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_treeItemGeneral);
        m_widgetFields.insert(label, field);
        item->setText(0, label);
        m_ui->treeWidget->setItemWidget(item, 1, field.value<QWidget*>());
    };

    // add custom widget -- agents from file
    QLineEdit* agentsPath = new QLineEdit(m_project->getDir());
    agentsPath->setObjectName(LINE_EDIT);
    QPushButton* btBrowseFile = new QPushButton("...");
    btBrowseFile->setMaximumWidth(20);
    connect(btBrowseFile, SIGNAL(clicked(bool)), this, SLOT(slotAgentFile()));
    QHBoxLayout* agentsLayout = new QHBoxLayout(new QWidget(m_ui->treeWidget));
    agentsLayout->setMargin(0);
    agentsLayout->insertWidget(0, agentsPath);
    agentsLayout->insertWidget(1, btBrowseFile);
    m_widgetFields.insert(GENERAL_ATTRIBUTE_AGENTS, QVariant::fromValue(agentsPath));
    QTreeWidgetItem* item = new QTreeWidgetItem(m_treeItemGeneral);
    item->setText(0, GENERAL_ATTRIBUTE_AGENTS);
    m_ui->treeWidget->setItemWidget(item, 1, agentsLayout->parentWidget());
    // seed
    QSpinBox* sp = new QSpinBox(m_ui->treeWidget);
    sp->setObjectName(SPIN_BOX);
    sp->setMaximum(INT32_MAX);
    sp->setMinimum(0);
    addGeneralAttr(GENERAL_ATTRIBUTE_SEED, QVariant::fromValue(sp));
    // stop at
    sp = new QSpinBox(m_ui->treeWidget);
    sp->setObjectName(SPIN_BOX);
    sp->setMaximum(EVOPLEX_MAX_STEPS);
    sp->setMinimum(1);
    addGeneralAttr(GENERAL_ATTRIBUTE_STOPAT, QVariant::fromValue(sp));
    // trials
    sp = new QSpinBox(m_ui->treeWidget);
    sp->setObjectName(SPIN_BOX);
    sp->setMaximum(EVOPLEX_MAX_TRIALS);
    sp->setMinimum(1);
    addGeneralAttr(GENERAL_ATTRIBUTE_TRIALS, QVariant::fromValue(sp));
    // models available
    QComboBox* cb = new QComboBox(m_ui->treeWidget);
    cb->setObjectName(COMBO_BOX);
    connect(cb, SIGNAL(currentIndexChanged(QString)), this, SLOT(slotModelSelected(QString)));
    addGeneralAttr(GENERAL_ATTRIBUTE_MODELID, QVariant::fromValue(cb));
    // add the graphId combo box
    cb = new QComboBox(m_ui->treeWidget);
    cb->setObjectName(COMBO_BOX);
    cb->setHidden(true);
    connect(cb, SIGNAL(currentIndexChanged(QString)), this, SLOT(slotGraphSelected(QString)));
    addGeneralAttr(GENERAL_ATTRIBUTE_GRAPHID, QVariant::fromValue(cb));
    // auto delete
    QCheckBox* chb = new QCheckBox(m_ui->treeWidget);
    chb->setObjectName(CHECK_BOX);
    chb->setChecked(true);
    addGeneralAttr(GENERAL_ATTRIBUTE_AUTODELETE, QVariant::fromValue(chb));

    // create the trees with the plugin stuff
    slotUpdateModelPlugins();
    slotUpdateGraphPlugins();
}

AttributesWidget::~AttributesWidget()
{
    delete m_ui;
}

void AttributesWidget::slotAgentFile()
{
    QLineEdit* lineedit = m_widgetFields.value(GENERAL_ATTRIBUTE_AGENTS).value<QLineEdit*>();
    QString path = QFileDialog::getOpenFileName(this, "Initial Population", lineedit->text(), "Text Files (*.csv *.txt)");
    if (!path.isEmpty()) {
        lineedit->setText(path);
    }
}

void AttributesWidget::slotCreateExperiment()
{
    if (m_selectedModelId == STRING_NULL_PLUGINID) {
        QMessageBox::warning(this, "Experiment", "Please, select a valid 'modelId'.");
        return;
    } else if (m_selectedGraphId == STRING_NULL_PLUGINID) {
        QMessageBox::warning(this, "Experiment", "Please, select a valid 'graphId'.");
        return;
    }

    QStringList header;
    QStringList values;
    QVariantHash::iterator it = m_widgetFields.begin();
    while (it != m_widgetFields.end()) {
        header << it.key();
        QWidget* widget = it.value().value<QWidget*>();
        if (widget->objectName() == CHECK_BOX) {
            values << QString::number(qobject_cast<QCheckBox*>(widget)->isChecked());
        } else if (widget->objectName() == COMBO_BOX) {
            values << qobject_cast<QComboBox*>(widget)->currentText();
        } else if (widget->objectName() == DOUBLE_SPIN_BOX) {
            values << QString::number(qobject_cast<QDoubleSpinBox*>(widget)->value(), 'f', 2);
        } else if (widget->objectName() == SPIN_BOX) {
            values << QString::number(qobject_cast<QSpinBox*>(widget)->value());
        } else if (widget->objectName() == LINE_EDIT) {
            values << qobject_cast<QLineEdit*>(widget)->text();
        } else {
            qFatal("[AttributesWidget]: unable to know the widget type.");
        }
        ++it;
    }

    QString errorMsg;
    int expId = m_project->newExperiment(header, values, errorMsg);
    if (expId >= 0) {
        emit (experimentCreated(expId));
    } else {
        QMessageBox::warning(this, "Experiment",
                "Unable to create the experiment.\nError: \"" + errorMsg + "\"");
    }
}

void AttributesWidget::slotGraphSelected(const QString& graphId)
{
    // hide last and show the selected graph tree
    QTreeWidgetItem* item = m_treeItemGraphs.value(m_selectedGraphId);
    if (item) { item->setHidden(true); }
    item = m_treeItemGraphs.value(graphId);
    if (item) {
        item->setHidden(false);
        item->setExpanded(true);
    }
    m_selectedGraphId = graphId;
}

void AttributesWidget::slotModelSelected(const QString& modelId)
{
    QTreeWidgetItem* itemModel = m_treeItemModels.value(m_selectedModelId);
    if (itemModel) itemModel->setHidden(true);

    itemModel = m_treeItemModels.value(modelId);
    if (itemModel) {
        QComboBox* cb = m_widgetFields.value(GENERAL_ATTRIBUTE_GRAPHID).value<QComboBox*>();
        cb->setCurrentIndex(0); // reset graphId
        itemModel->setHidden(false);
        itemModel->setExpanded(true);
    }
    m_selectedModelId = modelId;
}

void AttributesWidget::slotUpdateGraphPlugins()
{
    // list all graphs
    QComboBox* cb = m_widgetFields.value(GENERAL_ATTRIBUTE_GRAPHID).value<QComboBox*>();
    cb->blockSignals(true);
    cb->clear();
    cb->insertItem(0, STRING_NULL_PLUGINID);
    cb->insertItems(1, m_project->getGraphs().keys());
    cb->blockSignals(false);

    // create the trees to hold the graphs' attributes
    foreach (MainApp::GraphPlugin* graph, m_project->getGraphs()) {
        if (graph->graphAttrMin.size() > 0 && m_treeItemGraphs.contains(graph->uid)) {
            continue;
        }

        // the root node
        QTreeWidgetItem* itemRoot = new QTreeWidgetItem(m_ui->treeWidget);
        itemRoot->setText(0, "Graph");
        itemRoot->setHidden(true);
        m_treeItemGraphs.insert(graph->uid, itemRoot);

        // the graph stuff
        insertPluginAttributes(itemRoot, graph->uid, graph->graphAttrMin, graph->graphAttrMax);
    }
}

void AttributesWidget::slotUpdateModelPlugins()
{
    // list all models
    QComboBox* cb = m_widgetFields.value(GENERAL_ATTRIBUTE_MODELID).value<QComboBox*>();
    cb->blockSignals(true);
    cb->clear();
    cb->insertItem(0, STRING_NULL_PLUGINID);
    cb->insertItems(1, m_project->getModels().keys());
    cb->blockSignals(false);

    // create the trees to hold the models' attributes
    foreach (MainApp::ModelPlugin* model, m_project->getModels()) {
        if (m_treeItemModels.contains(model->uid)) {
            continue;
        }

        // the root node
        QTreeWidgetItem* itemRoot = new QTreeWidgetItem(m_ui->treeWidget);
        itemRoot->setText(0, "Model");
        itemRoot->setHidden(true);
        m_treeItemModels.insert(model->uid, itemRoot);

        // the model stuff
        insertPluginAttributes(itemRoot, model->uid, model->modelAttrMin, model->modelAttrMax);
    }
}

void AttributesWidget::insertPluginAttributes(QTreeWidgetItem* itemRoot, const QString& uid,
                                              const Attributes& min, const Attributes& max)
{
    const QString& uid_ = uid + "_";
    for (int i = 0; i < min.size(); ++i) {
        QTreeWidgetItem* item = new QTreeWidgetItem(itemRoot);
        item->setText(0, min.name(i));

        QVariant widget;
        if (min.value(i).type == Value::DOUBLE) {
            QDoubleSpinBox* sp = new QDoubleSpinBox();
            sp->setObjectName(DOUBLE_SPIN_BOX);
            sp->setMinimum(min.value(i).toDouble);
            sp->setMaximum(max.value(i).toDouble);
            widget = QVariant::fromValue(sp);
            m_ui->treeWidget->setItemWidget(item, 1, sp);
        } else if (min.value(i).type == Value::INT) {
            QSpinBox* sp = new QSpinBox();
            sp->setObjectName(SPIN_BOX);
            sp->setMinimum(min.value(i).toInt);
            sp->setMaximum(max.value(i).toInt);
            widget = QVariant::fromValue(sp);
            m_ui->treeWidget->setItemWidget(item, 1, sp);
        } else {
            QLineEdit* le = new QLineEdit();
            le->setObjectName(LINE_EDIT);
            le->setText(min.value(i).toQString());
            widget = QVariant::fromValue(le);
            m_ui->treeWidget->setItemWidget(item, 1, le);
        }
        // add the uid as prefix to avoid clashes.
        m_widgetFields.insert(uid_ + min.name(i), widget);
    }
}
}
