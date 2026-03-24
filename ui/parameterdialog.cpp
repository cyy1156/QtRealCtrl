#include "ui/parameterdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <algorithm>

ParameterDialog::ParameterDialog(const QString& algName,
                                 const QList<AlgorithmParamDef>& schema,
                                 const QVariantMap& currentParams,
                                 const QVariantMap& defaultParams,
                                 QWidget *parent)
    : QDialog(parent)
    , m_defaultParams(defaultParams)
{
    setWindowTitle(QStringLiteral("参数设置 - %1").arg(algName));
    resize(420, 300);

    auto *root = new QVBoxLayout(this);
    auto *tip = new QLabel(QStringLiteral("请根据算法需求设置参数，点击确定后立即生效。"), this);
    root->addWidget(tip);

    QMap<QString, QFormLayout*> formByGroup;
    for (const auto& def : schema)
    {
        if (!formByGroup.contains(def.group))
        {
            auto *box = new QGroupBox(def.group, this);
            auto *layout = new QFormLayout(box);
            layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
            box->setLayout(layout);
            root->addWidget(box);
            formByGroup.insert(def.group, layout);
        }
        auto *form = formByGroup.value(def.group);

        if (def.isInteger)
        {
            auto *edit = new QSpinBox(this);
            edit->setRange(static_cast<int>(def.minValue), static_cast<int>(def.maxValue));
            edit->setSingleStep(std::max(1, static_cast<int>(def.step)));
            edit->setValue(currentParams.value(def.key).toInt());
            form->addRow(def.label, edit);
            m_editorByKey.insert(def.key, edit);
        }
        else
        {
            auto *edit = new QDoubleSpinBox(this);
            edit->setDecimals(std::max(0, def.decimals));
            edit->setRange(def.minValue, def.maxValue);
            edit->setSingleStep(def.step);
            edit->setValue(currentParams.value(def.key).toDouble());
            form->addRow(def.label, edit);
            m_editorByKey.insert(def.key, edit);
        }
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *btnReset = buttons->addButton(QStringLiteral("恢复默认参数"), QDialogButtonBox::ResetRole);
    connect(btnReset, &QPushButton::clicked, this, [this]() {
        applyParams(m_defaultParams);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QVariantMap ParameterDialog::parameterValues() const
{
    QVariantMap out;
    for (auto it = m_editorByKey.begin(); it != m_editorByKey.end(); ++it)
    {
        if (auto *intEdit = qobject_cast<QSpinBox*>(it.value()))
        {
            out[it.key()] = intEdit->value();
            continue;
        }
        if (auto *dblEdit = qobject_cast<QDoubleSpinBox*>(it.value()))
        {
            out[it.key()] = dblEdit->value();
        }
    }
    return out;
}

void ParameterDialog::applyParams(const QVariantMap& params)
{
    for (auto it = m_editorByKey.begin(); it != m_editorByKey.end(); ++it)
    {
        if (!params.contains(it.key()))
        {
            continue;
        }
        if (auto *intEdit = qobject_cast<QSpinBox*>(it.value()))
        {
            intEdit->setValue(params.value(it.key()).toInt());
            continue;
        }
        if (auto *dblEdit = qobject_cast<QDoubleSpinBox*>(it.value()))
        {
            dblEdit->setValue(params.value(it.key()).toDouble());
        }
    }
}
