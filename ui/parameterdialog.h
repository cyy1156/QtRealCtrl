#ifndef PARAMETERDIALOG_H
#define PARAMETERDIALOG_H

#include <QDialog>
#include <QVariantMap>
#include <QMap>
#include <QWidget>
#include "algorithm/IAlgorithm.h"

class ParameterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ParameterDialog(const QString& algName,
                             const QList<AlgorithmParamDef>& schema,
                             const QVariantMap& currentParams,
                             const QVariantMap& defaultParams,
                             QWidget *parent = nullptr);

    QVariantMap parameterValues() const;

private:
    void applyParams(const QVariantMap& params);

private:
    QVariantMap m_defaultParams;
    QMap<QString, QWidget*> m_editorByKey;
};

#endif // PARAMETERDIALOG_H
