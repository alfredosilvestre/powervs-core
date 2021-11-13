/***************************************************************************
 *   Copyright (C) 2008 - Giuseppe Cigala                                  *
 *   g_cigala@virgilio.it                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include "qvumeter.h"

QVUMeter::QVUMeter(QWidget *parent) : QWidget(parent)
{
    colBack = QColor(60, 60, 60);
    colValue = Qt::white;
    colHigh = Qt::red;
    colLow = Qt::green;
    dimVal = 9;
    min = 0;
    max = 100;
    leftVal = 0;
    rightVal = 0;
    resetCh();
}

void QVUMeter::resetCh()
{
    leftCh = 1;
    rightCh = 2;
    leftCh1 = 0;
    leftCh2 = 1;
    rightCh1 = 2;
    rightCh2 = 3;
}

void QVUMeter::paintEvent(QPaintEvent *)
{
    paintBorder();
    paintBar();
    paintValue();

}

void QVUMeter::paintBorder()
{
    QPainter painter(this);
    painter.setWindow(0, 0, 100, 540);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(colBack, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    // Paint label
    painter.setPen(QPen(colValue, 2));
    QRectF Left(17, 515, 30, 20);
    QRectF Right(52, 515, 30, 20);
    QFont valFont("Arial", 12, QFont::Bold);
    painter.setFont(valFont);
    painter.drawText(Left, Qt::AlignCenter, "Ch" + QString::number(leftCh));
    painter.drawText(Right, Qt::AlignCenter, "Ch" + QString::number(rightCh));
}

void QVUMeter::paintBar()
{
    QPainter painter(this);
    painter.setWindow(0, 0, 100, 540);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient linGrad(50, 0, 50, 500);
    linGrad.setColorAt(0, colHigh);
    linGrad.setColorAt(1, colLow);
    linGrad.setSpread(QGradient::PadSpread);
    painter.setBrush(linGrad);

    // Draw color bar
    QRectF bar3(20, 50, 25, 450);
    painter.drawRect(bar3);
    QRectF bar4(55, 50, 25, 450);
    painter.drawRect(bar4);
    
    // Draw background bar
    painter.setBrush(QColor(40, 40, 40));
    
    double length = 450.0;
    double leftBar = abs(length * (min-leftVal)/(max-min));
    double rightBar = abs(length * (min-rightVal)/(max-min));
    QRectF bar1(20, 50, 25, 450-leftBar);
    painter.drawRect(bar1);
    QRectF bar2(55, 50, 25, 450-rightBar);
    painter.drawRect(bar2);
    painter.setPen(QPen(Qt::black, 2));

    for (int i = 0; i <=60; i++)
    {
        painter.drawLine(21, 500-450*i/60, 44, 500-450*i/60);
        painter.drawLine(56, 500-450*i/60, 79, 500-450*i/60);
    }

}

void QVUMeter::paintValue()
{
    QPainter painter(this);
    painter.setWindow(0, 0, 100, 540);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(Qt::black);
    painter.drawRect(20, 15, 25, 25);
    painter.drawRect(55, 15, 25, 25);
    painter.setPen(Qt::gray);
    painter.drawLine(20, 40, 45, 40);
    painter.drawLine(45, 15, 45, 40);
    painter.drawLine(55, 40, 80, 40);
    painter.drawLine(80, 15, 80, 40);

    painter.setPen(QPen(colValue, 1));
    QFont valFont("Arial", dimVal, QFont::Bold);
    painter.setFont(valFont);

    QRectF leftR(20, 15, 25, 25);
    QString lVal = QString("%1").arg(leftVal);
    painter.drawText(leftR, Qt::AlignCenter, lVal);
    QRectF rightR(55, 15, 25, 25);
    QString rVal = QString("%1").arg(rightVal);
    painter.drawText(rightR, Qt::AlignCenter, rVal);

    emit valueLChanged(leftVal);
    emit valueRChanged(rightVal);
}


void QVUMeter::setValueDim(int dim)
{
    dimVal = dim;
    update();
}

void QVUMeter::setColorBg(QColor color)
{
    colBack = color;
    update();
}

void QVUMeter::setColorValue(QColor color)
{
    colValue = color;
    update();
}

void QVUMeter::setColorHigh(QColor color)
{
    colHigh = color;
    update();
}


void QVUMeter::setColorLow(QColor color)
{
    colLow = color;
    update();
}

void QVUMeter::setLeftValue(const double leftValue)
{
    if (leftValue > max)
        leftVal = max;
    else if (leftValue < min)
        leftVal = min;
    else leftVal = leftValue;

    update();
}

void QVUMeter::setRightValue(const double rightValue)
{
    if (rightValue > max)
        rightVal = max;
    else if (rightValue < min)
        rightVal = min;
    else rightVal = rightValue;

    update();
}

void QVUMeter::setMinValue(const double minValue)
{
    if (minValue > max)
    {
        min = max;
        max = minValue;
        update();
    }
    else
    {
        min = minValue;
        update();
    }
}

void QVUMeter::setMaxValue(const double maxValue)
{
    if (maxValue < min)
    {
        max = min;
        min = maxValue;
        update();
    }
    else
    {
        max = maxValue;
        update();
    }
}

void QVUMeter::setValues(const QByteArray& audioBuffer)
{
    if(audioBuffer.size() == 0)
    {
        this->setLeftValue(0);
        this->setRightValue(0);
        return;
    }

    for(int i=0; i<audioBuffer.size(); i+=16)
    {
        leftVal = abs(audioBuffer[i+leftCh1] + audioBuffer[i+leftCh2]) >> 2;
        rightVal = abs(audioBuffer[i+rightCh1] + audioBuffer[i+rightCh2]) >> 2;
        update();
    }
}

QSize QVUMeter::minimumSizeHint() const
{
    return QSize(10, 54);
}

QSize QVUMeter::sizeHint() const
{
    return QSize(100, 540);
}

void QVUMeter::mouseDoubleClickEvent(QMouseEvent* event)
{
    QWidget::mouseDoubleClickEvent(event);
    if(event->button() == Qt::LeftButton)
    {
        leftCh += 2;
        rightCh += 2;
        leftCh1 += 4;
        leftCh2 += 4;
        rightCh1 += 4;
        rightCh2 += 4;
    }
    else if(event->button() == Qt::RightButton)
    {
        leftCh -= 2;
        rightCh -= 2;
        leftCh1 -= 4;
        leftCh2 -= 4;
        rightCh1 -= 4;
        rightCh2 -= 4;
    }

    if(leftCh < 1 || leftCh > 8)
        resetCh();

    update();
}
