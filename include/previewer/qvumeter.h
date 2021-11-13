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

#ifndef QVUMETER_H
#define QVUMETER_H

#include <QtGui>
#include <QWidget>
#include <QtDesigner/QDesignerExportWidget>

class QVUMeter : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor colorBg READ colorBg WRITE setColorBg)
    Q_PROPERTY(QColor colorValue READ colorValue WRITE setColorValue)
    Q_PROPERTY(QColor colorLow READ colorLow WRITE setColorLow)
    Q_PROPERTY(QColor colorHigh READ colorHigh WRITE setColorHigh)
    Q_PROPERTY(double leftValue READ leftValue WRITE setLeftValue)
    Q_PROPERTY(double rightValue READ rightValue WRITE setRightValue)
    Q_PROPERTY(double minValue READ minValue WRITE setMinValue)
    Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue)
    Q_PROPERTY(int dimValue READ valueDim WRITE setValueDim)

    QColor colorBg() const
    {
        return colBack;
    }
    QColor colorValue() const
    {
        return colValue;
    }
    QColor colorLow() const
    {
        return colLow;
    }
    QColor colorHigh() const
    {
        return colHigh;
    }
    double leftValue() const
    {
        return leftVal;
    }
    double rightValue() const
    {
        return rightVal;
    }
    double minValue() const
    {
        return min;
    }
    double maxValue() const
    {
        return max;
    }
    int valueDim() const
    {
        return dimVal;
    }

    public:
        QVUMeter(QWidget* parent = 0);
        QSize minimumSizeHint() const;
        QSize sizeHint() const;
        void mouseDoubleClickEvent(QMouseEvent* event);

    private:
        void resetCh();

    signals:
        void valueLChanged(double);
        void valueRChanged(double);

    public slots:
        void setColorBg(QColor);
        void setColorValue(QColor);
        void setColorHigh(QColor);
        void setColorLow(QColor);
        void setValueDim(int);
        void setLeftValue(const double);
        void setRightValue(const double);
        void setMinValue(const double);
        void setMaxValue(const double);
        void setValues(const QByteArray& audioBuffer);

    protected:
        void paintEvent(QPaintEvent *);
        void paintBorder();
        void paintBar();
        void paintValue();

    private:
        int min;
        int max;
        int leftVal;
        int rightVal;
        int leftCh;
        int rightCh;
        int leftCh1;
        int leftCh2;
        int rightCh1;
        int rightCh2;
        int dimVal;
        QColor colBack;
        QColor colValue;
        QColor colHigh;
        QColor colLow;
};

#endif
