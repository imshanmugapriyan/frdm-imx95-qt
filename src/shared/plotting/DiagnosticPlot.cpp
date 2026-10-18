#include "DiagnosticPlot.h"

#include <QPainter>
#include <QPen>
#include <QVariantMap>
#include <algorithm>
#include <cmath>

DiagnosticPlot::DiagnosticPlot(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &DiagnosticPlot::changed, this, [this] { update(); });
}
void DiagnosticPlot::setSeries(const QVariantList &v) { m_series = v; emit changed(); }
void DiagnosticPlot::setStacked(bool v) { if (m_stacked != v) { m_stacked = v; emit changed(); } }
void DiagnosticPlot::setLaneScales(bool v) { if (m_laneScales != v) { m_laneScales = v; emit changed(); } }
void DiagnosticPlot::setIntegerTicks(bool v) { if (m_integerTicks != v) { m_integerTicks = v; emit changed(); } }
void DiagnosticPlot::setEmptyText(const QString &v) { if (m_emptyText != v) { m_emptyText = v; emit changed(); } }
void DiagnosticPlot::setMinimum(double v) { if (std::isfinite(v) && m_minimum != v) { m_minimum = v; emit changed(); } }
void DiagnosticPlot::setMaximum(double v) { if (std::isfinite(v) && m_maximum != v) { m_maximum = v; emit changed(); } }
void DiagnosticPlot::setXStart(double v) { if (std::isfinite(v) && m_xStart != v) { m_xStart = v; emit changed(); } }
void DiagnosticPlot::setXStep(double v) { if (std::isfinite(v) && m_xStep != v) { m_xStep = v; emit changed(); } }
void DiagnosticPlot::setXLabel(const QString &v) { if (m_xLabel != v) { m_xLabel = v; emit changed(); } }
void DiagnosticPlot::setUiScale(double v) { if (std::isfinite(v) && v > 0 && m_uiScale != v) { m_uiScale = v; emit changed(); } }

void DiagnosticPlot::paint(QPainter *p)
{
    const double s = m_uiScale;
    const bool scales = m_stacked && m_laneScales;
    const QRectF plot((scales ? 108 : 76)*s, 24*s, width()-(scales ? 132 : 100)*s, height()-82*s);
    if (plot.width() <= 0 || plot.height() <= 0 || m_maximum <= m_minimum) return;
    QFont font = p->font(); font.setPixelSize(std::max(8, int(16*s))); p->setFont(font);
    p->setPen(QColor("#3d3650"));
    for (int i=0; i<=8; ++i) {
        const double x = plot.left()+plot.width()*i/8;
        p->drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    const int rows = m_stacked ? std::max(1, int(m_series.size())) : 1;
    qsizetype longest = 0;
    for (qsizetype seriesIndex=0; seriesIndex<m_series.size(); ++seriesIndex) {
        const auto data = m_series.at(seriesIndex).toMap();
        const auto values = data.value("values").toList();
        longest = std::max(longest, values.size());
        const double rowHeight = plot.height()/rows;
        const double top = plot.top()+(m_stacked ? seriesIndex*rowHeight : 0);
        const double bottom = m_stacked ? top+rowHeight : plot.bottom();
        const double amplitude = m_stacked ? rowHeight*0.85 : plot.height();
        p->setPen(QColor("#4b415c"));
        p->drawLine(QPointF(plot.left(), bottom), QPointF(plot.right(), bottom));
        if (scales) {
            QFont tickFont = font; tickFont.setPixelSize(std::max(8,int(std::min(12*s,rowHeight*0.26))));
            p->setFont(tickFont); p->setPen(QColor("#a69cb9"));
            const double middle = (m_minimum+m_maximum)/2;
            const auto tick = [&](double value, double y) {
                p->drawLine(QPointF(plot.left()-4*s,y),QPointF(plot.left(),y));
                p->drawText(QRectF(67*s,y-7*s,32*s,14*s),Qt::AlignRight|Qt::AlignVCenter,QString::number(value,'g',3));
            };
            tick(m_minimum,bottom); tick(middle,bottom-amplitude/2);
            if (seriesIndex == 0) tick(m_maximum,bottom-amplitude);
            p->setPen(QPen(QColor("#302a41"),1,Qt::DashLine));
            p->drawLine(QPointF(plot.left(),bottom-amplitude/2),QPointF(plot.right(),bottom-amplitude/2));
            p->setFont(font);
        }
        const QColor requestedColor(data.value("color").toString());
        p->setPen(requestedColor.isValid() ? requestedColor : seriesIndex % 2 ? QColor("#75caff") : QColor("#c4a3ff"));
        p->drawText(QRectF(0, m_stacked ? top : plot.top()+seriesIndex*24*s, 65*s, m_stacked ? rowHeight : 26*s),
                    Qt::AlignRight | Qt::AlignVCenter, data.value("label").toString());
        if (values.isEmpty()) continue;
        // Bin min/max rather than skipping samples: narrow peaks remain visible.
        const qsizetype bins = std::max<qsizetype>(1, std::min<qsizetype>(values.size(), qsizetype(plot.width())));
        QPointF previous;
        bool previousValid = false;
        p->save(); p->setClipRect(QRectF(plot.left(), top, plot.width(), rowHeight));
        for (qsizetype bin=0; bin<bins; ++bin) {
            const qsizetype first = bin*values.size()/bins, end = (bin+1)*values.size()/bins;
            double low=0, high=0; bool valid=false;
            for (qsizetype index=first; index<end; ++index) {
                const auto &v = values.at(index);
                bool ok=false; const double raw = v.toDouble(&ok);
                if (v.isNull() || !ok || !std::isfinite(raw)) continue;
                const double value = m_stacked ? std::abs(raw) : raw;
                if (!valid) { low=value; high=value; valid=true; }
                else { low=std::min(low,value); high=std::max(high,value); }
            }
            if (!valid) { previousValid=false; continue; }
            const double x = plot.left()+bin*plot.width()/std::max<qsizetype>(1,bins-1);
            const auto y = [&](double value) { return bottom-(std::clamp(value,m_minimum,m_maximum)-m_minimum)
                                                                /(m_maximum-m_minimum)*amplitude; };
            if (m_stacked) p->drawLine(QPointF(x,bottom),QPointF(x,y(high)));
            else {
                p->drawLine(QPointF(x,y(low)),QPointF(x,y(high)));
                const QPointF current(x,y((low+high)/2));
                if (previousValid) p->drawLine(previous,current);
                previous=current; previousValid=true;
            }
        }
        p->restore();
    }
    p->setPen(QColor("#bfb5d0"));
    const int ticks = int(std::min<qsizetype>(4,std::max<qsizetype>(0,longest-1)));
    for (int i=0; longest > 0 && i<=ticks; ++i) {
        double index = (longest-1)*double(i)/std::max(1,ticks);
        if (m_integerTicks) index=std::round(index);
        const double x=plot.left()+plot.width()*index/std::max<qsizetype>(1,longest-1);
        const double value=m_xStart+index*m_xStep;
        p->drawText(QRectF(x-55*s,plot.bottom()+8*s,110*s,24*s),Qt::AlignCenter,
                    m_integerTicks ? QString::number(value,'f',0) : QString::number(value,'g',5));
    }
    if (!m_stacked) {
        p->drawText(QRectF(0,plot.top()-20*s,65*s,20*s),Qt::AlignRight,QString::number(m_maximum,'g',4));
        p->drawText(QRectF(0,plot.bottom()-20*s,65*s,20*s),Qt::AlignRight,QString::number(m_minimum,'g',4));
    }
    p->drawText(QRectF(plot.left(),height()-27*s,plot.width(),24*s),Qt::AlignCenter,m_xLabel);
    if (longest == 0) {
        const QRectF message(plot.center().x()-240*s,plot.center().y()-24*s,480*s,48*s);
        p->fillRect(message,QColor("#1b1627"));
        p->drawText(message,Qt::AlignCenter,m_emptyText);
    }
}
