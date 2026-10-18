#pragma once

#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class DiagnosticPlot : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList series READ series WRITE setSeries NOTIFY changed)
    Q_PROPERTY(bool stacked READ stacked WRITE setStacked NOTIFY changed)
    Q_PROPERTY(bool laneScales READ laneScales WRITE setLaneScales NOTIFY changed)
    Q_PROPERTY(bool integerTicks READ integerTicks WRITE setIntegerTicks NOTIFY changed)
    Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText NOTIFY changed)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY changed)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY changed)
    Q_PROPERTY(double xStart READ xStart WRITE setXStart NOTIFY changed)
    Q_PROPERTY(double xStep READ xStep WRITE setXStep NOTIFY changed)
    Q_PROPERTY(QString xLabel READ xLabel WRITE setXLabel NOTIFY changed)
    Q_PROPERTY(double uiScale READ uiScale WRITE setUiScale NOTIFY changed)
public:
    explicit DiagnosticPlot(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    QVariantList series() const { return m_series; }
    bool stacked() const { return m_stacked; }
    bool laneScales() const { return m_laneScales; }
    bool integerTicks() const { return m_integerTicks; }
    QString emptyText() const { return m_emptyText; }
    double minimum() const { return m_minimum; }
    double maximum() const { return m_maximum; }
    double xStart() const { return m_xStart; }
    double xStep() const { return m_xStep; }
    QString xLabel() const { return m_xLabel; }
    double uiScale() const { return m_uiScale; }
    void setSeries(const QVariantList &value);
    void setStacked(bool value);
    void setLaneScales(bool value);
    void setIntegerTicks(bool value);
    void setEmptyText(const QString &value);
    void setMinimum(double value);
    void setMaximum(double value);
    void setXStart(double value);
    void setXStep(double value);
    void setXLabel(const QString &value);
    void setUiScale(double value);
signals:
    void changed();
private:
    QVariantList m_series;
    bool m_stacked = false;
    bool m_laneScales = false, m_integerTicks = false;
    QString m_emptyText = tr("Waiting for measurement data");
    double m_minimum = -1, m_maximum = 1, m_xStart = 0, m_xStep = 1, m_uiScale = 1;
    QString m_xLabel = QStringLiteral("Samples");
};
