/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <AzQtComponents/AzQtComponentsAPI.h>

#include <QColor>
#include <QObject>
#include <QPixmap>
#include <QString>

class QPainter;
class QRect;

namespace AzQtComponents
{
    struct DockBarColors
    {
        QColor text;
        QColor frame;
        QColor firstLine;
        QColor gradientStart;
        QColor gradientEnd;
    };

    class AZ_QT_COMPONENTS_API DockBar
        : public QObject
    {
        Q_OBJECT
    public:
        enum
        {
            Height = 32,
            HandleLeftMargin = 3,
            TitleLeftMargin = 8,
            TitleRightMargin = 18,
            CloseButtonRightMargin = 2,
            ButtonsSpacing = 5,
            ResizeTopMargin = 4,
        };

        static void drawFrame(QPainter* painter, const QRect& area, bool drawSideBorders, const DockBarColors& colors);
        static void drawSolidFrame(QPainter* painter, const QRect& area, const QColor& backgroundColor);
        static void drawTabContents(QPainter* painter, const QRect& area, const DockBarColors& colors, const QString& title);
        static int getTitleMinWidth(const QString& title, bool enableTear = true);
        static DockBarColors getColors(bool active);

        explicit DockBar(QObject* parent = nullptr);
        DockBarColors GetColors(bool active) { return DockBar::getColors(active); }
        int GetTitleMinWidth(const QString& title, bool enableTear = true) { return DockBar::getTitleMinWidth(title, enableTear); }
        void DrawSegment(QPainter& painter, const QRect& area,
            int buttonsX, bool enableTear, bool drawSideBorders, const DockBarColors& colors,
            const QString& title = QString());
        void DrawSolidBackgroundSegment(QPainter& painter, const QRect& area,
            int buttonsX, bool drawAppIcon, const QColor& backgroundColor, const QColor& textColor, const QString& title = QString());

    private:
        static int drawIcon(QPainter* painter, int x, const QPixmap& icon);
        static void drawTitle(QPainter* painter, int leftContentWidth, const QRect& area,
            int buttonsX, const QColor& color, const QString& title);
        QPixmap m_tearIcon;
        QPixmap m_applicationIcon;
    };
} // namespace AzQtComponents